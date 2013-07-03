/*
	Copyright 2011 Niels Brouwers

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.#include <string.h>
*/

//#include <Arduino.h>
#include "usb_adb.h"
#include "ch9_adb.h"
#include <rtdef.h>
#include "stm32_adb.h"
#include "usbh_hcs.h"
#include "usbh_def.h"
#include "stm32_adb.h"
#include "kservice.h"

static uint8_t usb_error = 0;
static uint8_t usb_task_state = USB_DETACHED_SUBSTATE_INITIALIZE;
static usb_eventHandler * eventHandler = RT_NULL;
HOST_State hState = HOST_INIT_ALL;

usb_device deviceTable[USB_NUMDEVICES + 1];

/**
 * Initialises the USB layer.
 */
void USB_init(void)
{
 	stm32_init();

	uint8_t i;

	// Initialise the USB state machine.
	usb_task_state = USB_DETACHED_SUBSTATE_INITIALIZE;

	// Initialise the device table.
	for (i = 0; i < (USB_NUMDEVICES + 1); i++)
		deviceTable[i].active = 0;

	// Address 0 is used to configure devices and assign them an address when they are first plugged in
	deviceTable[0].address = 0;
	USB_initEndPoint(&(deviceTable[0].control), 0, IO_EP, 
					USB_TRANSFER_TYPE_CONTROL, deviceTable[0].address, 8);
}

/**
 * Initialises a USB endpoint.
 * @param endpoint USB endpoint
 * @param address endpoint address
 */
void USB_initEndPoint(usb_endpoint * endpoint, uint8_t address,
						ep_dir_e ep_dir, uint8_t ep_type, uint8_t dev_addr,
						uint16_t max_pkt_size)
{
	endpoint->address = address;
	endpoint->sendToggle = 0;	   		//FIXME: check toggle bit
	endpoint->receiveToggle = 1;		//FIXME: check toggle bit

	// Allocate STM32 channel resource
	endpoint->stm32_out_chan_num = -1;
	endpoint->stm32_in_chan_num = -1;
	if ((ep_dir == OUT_EP) || (ep_dir == IO_EP)) {
		endpoint->stm32_out_chan_num = USBH_Alloc_Channel(&usb_host_reg, address);
		USBH_Open_Channel(&usb_host_reg, endpoint->stm32_out_chan_num, dev_addr,
						HPRT0_PRTSPD_FULL_SPEED, ep_type, max_pkt_size,
						(ep_type == USB_TRANSFER_TYPE_BULK)? 0:1);
	}
	if ((ep_dir == IN_EP) || (ep_dir == IO_EP)) {
		endpoint->stm32_in_chan_num = USBH_Alloc_Channel(&usb_host_reg, (0x80 | address));
		USBH_Open_Channel(&usb_host_reg, endpoint->stm32_in_chan_num, dev_addr,
						HPRT0_PRTSPD_FULL_SPEED, ep_type, max_pkt_size,
						(ep_type == USB_TRANSFER_TYPE_BULK)? 0:1);		
	}	
}

/**
 * Initialises a USB device and switches to the given configuration
 * @param USB device
 * @param configuration configuration to switch to
 * @return negative error code or zero on success.
 */
int USB_initDevice(usb_device * device, int configuration)
{
	char buf[4];
							
	uint8_t rcode;

	// Set the configuration for this USB device.
	rcode = USB_setConfiguration(device, configuration);
	if (rcode<0) return rcode;

	// Get the first supported language.
	rcode = USB_getString(device, 0, 0, 4, buf);
	if (rcode<0) return rcode;
    device->firstStringLanguage = (buf[3] << 8) | buf[2];

    return rcode;
}

/**
 * Sets the global USB event callback function. Use this to catch global events like device connect/disconnect.
 * @param handler event handler function.
 */
void USB_setEventHandler(usb_eventHandler * handler)
{
	eventHandler = handler;
}

/**
 * Fires a USB event. This calls the callback function set by setEventHandler.
 *
 * @param device the device the events relates to.
 * @param event event type (i.e. connected, disconnected)
 */
void USB_fireEvent(usb_device * device, usb_eventType event)
{
	eventHandler(device, event);
}

// private
uint8_t usb_getUsbTaskState()
{
	return (usb_task_state);
}

// private
void usb_setUsbTaskState(uint8_t state)
{
	usb_task_state = state;
}

/**
 * Gets the usb device at the given address, or RT_NULL is the address is out of range (greater than USB_NUMDEVICES+1,
 * as address zero is reserver).
 * @param address USB device address
 * @return USB device struct or RT_NULL on failure (address out of range)
 */
usb_device * usb_getDevice(uint8_t address)
{
	if (address>USB_NUMDEVICES+1) return RT_NULL;

	return &(deviceTable[address]);
}

/**
 * USB poll method. Performs enumeration/cleanup.
 */
void USB_poll ()
{
	uint8_t i;
	uint8_t rcode;			  
	uint8_t speed;
	usb_deviceDescriptor deviceDescriptor;
	
	switch (hState) {
	case HOST_INIT_ALL :
		/* This is board power on initialize */
		for (i = 1; i < USB_NUMDEVICES; i++) {
			if (deviceTable[i].active) {
				USB_fireEvent(&(deviceTable[i]), USB_DISCONNECT);
			}	
		}
		USB_init();		
		hState = HOST_IDLE;
		break;
		
	case HOST_ISSUE_CORE_RESET :
		if ( HCD_ResetPort(&usb_host_reg) == 0) {
			hState = HOST_IDLE;
		}
		break;
		
	case HOST_IDLE :	
		if (HCD_IsDeviceConnected(&usb_host_reg)) {
		  	/* Wait for USB Connect Interrupt void USBH_ISR_Connected(void) */	 
			hState = HOST_DEV_ATTACHED;
		}	   
		break;

	case HOST_DEV_ATTACHED :
		/* Reset USB Device */
		if (HCD_ResetPort(&usb_host_reg) == 0) {		   
			speed = HCD_GetCurrentSpeed(&usb_host_reg);
		   
		  	if ((speed != HPRT0_PRTSPD_FULL_SPEED)&&
				(speed != HPRT0_PRTSPD_LOW_SPEED)) {
			 	/* Should not be any value other than Full_Speed or Low_Speed */
			 	rt_kprintf("Not supported speed!\n");	  
		   	} else {
				hState = HOST_ENUMERATION;
			}
		}
		break;
									  
	case HOST_ENUMERATION :		 
		deviceTable[0].control.maxPacketSize = 8; 
				   
		rcode = USB_getDeviceDescriptor(&deviceTable[0], &deviceDescriptor); 
		if (rcode == 0) {
			deviceTable[0].control.maxPacketSize = deviceDescriptor.bMaxPacketSize0;
			USBH_Modify_Channel(&usb_host_reg, deviceTable[0].control.stm32_in_chan_num, 
								0, 0, 0, deviceTable[0].control.maxPacketSize);
			USBH_Modify_Channel(&usb_host_reg, deviceTable[0].control.stm32_out_chan_num, 
								0, 0, 0, deviceTable[0].control.maxPacketSize);			
		} else {
			hState = HOST_ERROR_STATE;
			break;
		}

		// Look for an empty spot
		for (i = 1; i < USB_NUMDEVICES; i++) {
			if (!deviceTable[i].active) {
				// Set correct MaxPktSize
				// deviceTable[i].epinfo = deviceTable[0].epinfo;
	
				deviceTable[i].address = i;
				deviceTable[i].active = 1;
	
				USB_initEndPoint(&(deviceTable[i].control), 0, IO_EP, 
							USB_TRANSFER_TYPE_CONTROL, deviceTable[i].address,
							deviceTable[0].control.maxPacketSize);
	
				// temporary record until plugged with real device endpoint structure
				rcode = USB_setAddress(&deviceTable[0], i);
	
				//if (rcode == 0)
				if (1)
				{
					USB_fireEvent(&deviceTable[i], USB_CONNECT);
					// usb_task_state = USB_STATE_CONFIGURING;
					// NB: I've bypassed the configuring state, because configuration should be handled
					// in the usb event handler.
					hState = HOST_STATE_RUNNING;
				} else {
					USB_fireEvent(&deviceTable[i], USB_ADRESSING_ERROR);
	
					// TODO remove usb_error at some point?
					//usb_error = USB_STATE_ADDRESSING;
					hState = HOST_STATE_ERROR;
				}
			}
			break; //break if address assigned or error occured during address assignment attempt
		}
		break;

	case HOST_STATE_RUNNING :
		break;
		
	default :
		/* this includes HOST_ERROR_STATE */
		while (1) {		
		}

	}				 
}

/**
 * Convenience method for getting a string. This is useful for printing manufacturer names, serial numbers, etc.
 * Language is defaulted to zero. Strings are returned in 16-bit unicode from the device, so this function converts the
 * result to ASCII by ignoring every second byte. However, since the target buffer is used as a temporary storage during
 * this process it must be twice as large as the desired maximum string size.
 *
 * @param device USB device.
 * @param index string index.
 * @param languageId language ID.
 * @param length buffer length.
 * @param str target buffer.
 * @return 0 on success, error code otherwise.
 */
int USB_getString(usb_device * device, uint8_t index, uint8_t languageId, uint16_t length, char * str)
{
	uint8_t stringLength = 0;
	int i, ret = 0;

    // Get string length;
	ret = USB_controlRequest(device, bmREQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, index, USB_DESCRIPTOR_STRING, languageId, sizeof(uint8_t), &stringLength);
    if (ret<0) return -1;

    // Trim string size to fit the target buffer.
    if (stringLength>length) stringLength = length;

	// Get the whole thing.
	ret = USB_controlRequest(device, bmREQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, index, USB_DESCRIPTOR_STRING, languageId, stringLength, (uint8_t *)str);
    if (ret<0) return -2;

	// Convert to 8-bit ASCII
	stringLength = (stringLength - 2) / 2;
	for (i=0; i<stringLength; i++) str[i] = str[2+i*2];
	str[stringLength] = 0;

	return 0;
}

/**
 * Performs an in transfer from a USB device from an arbitrary endpoint.
 *
 * @param device USB bulk device.
 * @param device length number of bytes to read.
 * @param data target buffer.
 * @return number of bytes read, or error code in case of failure.
 */
int USB_read(usb_device * device, usb_endpoint * endpoint, uint16_t length, uint8_t * data, unsigned int nakLimit)
{
	rt_sem_t thread_sem_p;
	usb_trans_record_t	*trans_record_p; 
	URB_STATE urb_result;
	HC_STATUS hc_status;

	do {				  
		
		thread_sem_p = USBH_BulkReceiveData(&usb_host_reg, data, length, 
											endpoint->stm32_in_chan_num);

		/* Sync wait */
		if (rt_sem_take(thread_sem_p, RT_WAITING_FOREVER) != RT_EOK) {
			rt_kprintf("%s listen to semaphore failed!\n", __FUNCTION__);
			return (-1);
		}

		trans_record_p = rt_list_entry(thread_sem_p, usb_trans_record_t, thread_wait);
		urb_result = trans_record_p->urb_status;
		hc_status = trans_record_p->hc_status;

		if (usb_trans_delete_record(&usb_host_reg, endpoint->stm32_in_chan_num, 1) != RT_EOK) {
			rt_kprintf("%s failed usb_trans_delete_record!\n", __FUNCTION__);
			return (-1);
		}

		if (urb_result == URB_DONE) {
			return (usb_host_reg.host.hc[endpoint->stm32_in_chan_num].xfer_Count);
		} else if (hc_status == HC_NAK || hc_status == HC_IDLE) {
			// move forward and check nakLimit
		} else {
			rt_kprintf("%s urb_status: %d\n", __FUNCTION__, urb_result);
			return (-1);
		}		
		
		nakLimit--;
	} while(nakLimit > 0);
	
	// Report error.
	return (-1);
}


/**
 * Performs a bulk in transfer from a USB device.
 *
 * @param device USB bulk device.
 * @param device length number of bytes to read.
 * @param data target buffer.
 *
 * @return number of bytes read, or error code in case of failure.
 */
int USB_bulkRead(usb_device * device, uint16_t length, uint8_t * data, uint8_t poll)
{
	return USB_read(device, &(device->bulk_in), length, data, poll ? 1 : USB_NAK_LIMIT);
} 


/**
 * Performs ab out transfer to a USB device on an arbitrary endpoint.
 *
 * @param device USB bulk device.
 * @param device length number of bytes to read.
 * @param data target buffer.
 * @return number of bytes written, or error code in case of failure.
 */
int USB_write(usb_device * device, usb_endpoint * endpoint, uint16_t length, uint8_t * data)
{
	rt_sem_t thread_sem_p;
	usb_trans_record_t	*trans_record_p; 
	URB_STATE urb_result;			

	thread_sem_p = USBH_BulkSendData(&usb_host_reg, data, length, 
											endpoint->stm32_out_chan_num);

	/* Sync wait */
	if (rt_sem_take(thread_sem_p, RT_WAITING_FOREVER) != RT_EOK) {
		rt_kprintf("%s listen to semaphore failed!\n", __FUNCTION__);
		return (-1);
	}

	trans_record_p = rt_list_entry(thread_sem_p, usb_trans_record_t, thread_wait);
	urb_result = trans_record_p->urb_status;

	if (usb_trans_delete_record(&usb_host_reg, endpoint->stm32_out_chan_num, 0) != RT_EOK) {
		rt_kprintf("%s failed usb_trans_delete_record!\n", __FUNCTION__);
		return (-1);
	}

	if (urb_result != URB_DONE) {
		rt_kprintf("%s urb_status: %d\n", __FUNCTION__, urb_result);
		return (-1);
	}			

	return (0);
}

/**
 * Performs a bulk out transfer to a USB device.
 *
 * @param device USB bulk device.
 * @param device length number of bytes to read.
 * @param data target buffer.
 * @return number of bytes read, or error code in case of failure.
 */
int USB_bulkWrite(usb_device * device, uint16_t length, uint8_t * data)
{
	return USB_write(device, &(device->bulk_out) , length, data);
}

/**
 * Sends a control request to a USB device.
 *
 * @param device USB device to send the control request to.
 * @param requestType request type (in/out).
 * @param request request.
 * @param valueLow low byte of the value parameter.
 * @param valueHigh high byte of the value parameter.
 * @param index index.
 * @param length number of bytes to transfer.
 * @param data data to send in case of output transfer, or reception buffer in case of input. If no data is to be exchanged this should be set to RT_NULL.
 * @return 0 on success, error code otherwise
 */
int USB_controlRequest(
		usb_device * device,
		uint8_t requestType,
		uint8_t request,
		uint8_t valueLow,
		uint8_t valueHigh,
		uint16_t index,
		uint16_t length,
		uint8_t * data)
{
	USB_Setup_TypeDef setup;
	setup.b.bmRequestType = requestType;
	setup.b.bRequest = request;
	setup.b.wValue.bw.msb = valueLow;
	setup.b.wValue.bw.lsb = valueHigh;
	setup.b.wIndex.w = index;
	setup.b.wLength.w = length;
	if (USBH_CtlReq(device, &usb_host_reg, &setup, data, length) != USBH_OK) {
		return -1;
	}

	return 0;
}

/**
 * Gets the device descriptor of a USB device.
 * @param device USB device
 * @param descriptor pointer to a usb_deviceDescriptor record that will be filled with the requested data.
 * @return 0 in case of success, error code otherwise
 */
int USB_getDeviceDescriptor(usb_device * device, usb_deviceDescriptor * descriptor)
{
	return(USB_controlRequest(device, bmREQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, 0x00, USB_DESCRIPTOR_DEVICE, 0x0000, /*sizeof(usb_deviceDescriptor)*/8, (uint8_t *)descriptor));
}

/**
 * Gets the configuration descriptor of a USB device as a byte array.
 * @param device USB device
 * @param conf configuration number
 * @param length length of the data buffer. This method will not write beyond this boundary.
 * @return number of bytes read, or negative number in case of error.
 */
int USB_getConfigurationDescriptor(usb_device * device, uint8_t conf, uint16_t length, uint8_t * data)
{
	uint16_t descriptorLength;
	int rcode;

	// Read the length of the configuration descriptor.
	rcode = (USB_controlRequest(device, bmREQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, conf, USB_DESCRIPTOR_CONFIGURATION, 0x0000, 4, data));
	if (rcode) return -1;

	descriptorLength = (data[3] << 8) | data[2];
	if (descriptorLength<length) length = descriptorLength;

	// Read the length of the configuration descriptor.
	rcode = (USB_controlRequest(device, bmREQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, conf, USB_DESCRIPTOR_CONFIGURATION, 0x0000, length, data));
	if (rcode) return -2;

	return length;
}

/**
 * Sets the address of a newly connected USB device.
 *
 * @param device the 'zero' usb device (address 0, endpoint 0)
 * @param address the address to set for the newly connected device
 * @return 0 in case of success, error code otherwise
 */
int USB_setAddress(usb_device * device, uint8_t address)
{
    return(USB_controlRequest(device, bmREQ_SET, USB_REQUEST_SET_ADDRESS, address, 0x00, 0x0000, 0x0000, RT_NULL));
}

/**
 * Switches a device to the given configuration.
 * @param device USB device
 * @param configuration configuration number to switch to
 * @param error code. Negative on error or zero on success.
 */
int USB_setConfiguration(usb_device * device, uint8_t configuration)
{
    return(USB_controlRequest(device, bmREQ_SET, USB_REQUEST_SET_CONFIGURATION, configuration, 0x00, 0x0000, 0x0000, RT_NULL));
}
