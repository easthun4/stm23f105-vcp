/*
 * File      : application.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2009-01-05     Bernard      the first version
 */

/**
 * @addtogroup STM32
 */
/*@{*/

#include <board.h>
#include <rtthread.h>

#ifdef CONNECT_TO_ANDROID
#include "usbh_core.h"
#endif

#include "exg_io.h"
#include "ad7947.h"
#include "adInternal.h"
#include "led.h"
#include "exg_api.h"
#include "exg_data_buffer.h"

#include "usbd_cdc_core.h"
#include "usbd_usr.h"
#include "usb_conf.h"
#include "usbd_desc.h"

#ifdef CONNECT_TO_ANDROID
#include "adb.h"
#endif
#ifdef CONNECT_TO_PC
#include "hw_config.h"
#include "usb_core.h"
#include "usb_init.h"
#include "otgd_fs_dev.h"
#include "usb_hid_pwr.h"
#endif

#ifdef USB_OTG_HS_INTERNAL_DMA_ENABLED
  #if defined ( __ICCARM__ ) /*!< IAR Compiler */
    #pragma data_alignment=4   
  #endif
#endif /* USB_OTG_HS_INTERNAL_DMA_ENABLED */
__ALIGN_BEGIN USB_OTG_CORE_HANDLE   USB_OTG_dev __ALIGN_END ;
extern uint8_t  APP_Rx_Buffer []; /* Write CDC received data in this buffer.
                                     These data will be sent over USB IN endpoint
                                     in the CDC core functions. */



#define MAX_CHARS 512

struct rt_semaphore rx_sem;
struct rt_semaphore usb_tx_sem;
rt_device_t app_dev = RT_NULL;
extern uint8_t connected;
extern CDC_IF_Prop_TypeDef  APP_FOPS;


/* current ADC buffer */
adc_buffer_t *adc_current_buf = RT_NULL;

/* command line buffer */
char lines[MAX_CHARS];
int lines_idx = 0;  

/* ADC sample updated by TIMER4 */
uint16_t adc_current;
int usb_connection_open = 0;
volatile int hz = 100;
volatile int hz_config = 100;

// debug PC send count
int pc_send_count = 0;
uint8_t u8VCPSendEnable = 0;


#ifdef CONNECT_TO_ANDROID
void adbEventHandler(Connection_t * connection, adb_eventType event, uint16_t length, uint8_t * data)
{
    int tmp_hz;
    if ((event == ADB_CONNECTION_RECEIVE) && (length > 0)) {
        if (data[0] == 0x03) {
            /* 32K switch */
            if (data[1] == 0x01) {
                exg_start_32k();
            } else {
                exg_stop_32k();
            }
        } else if (data[0] == 0x04) {
            /* Trigger */
            exg_trigger_start();
        } else if (data[0] == 0x05) {
            /* Sample rate */
            tmp_hz = (data[1] + (data[2] << 8));
            hz_config = tmp_hz;
        }
    }	
}
#endif

void adc_sample_handler (uint16_t input)
{
    //static int buf_seq = 0;

    /* debug: toggle bit */
    //static uint8_t i = 0;
    //exg_set_mode(i++ & 0x01);

    //adc_current = ad7947_sample();
    //	ADC_Cmd(ADC1, ENABLE);

    DBPushBuffer((uint8_t *)&adc_current, 2);
    /*
       if (adc_current_buf == RT_NULL) {
       adc_current_buf = ad7988_get_free_buf();
       RT_ASSERT(adc_current_buf != RT_NULL);
       adc_current_buf->seq_num = buf_seq++;
       }					 
       if (adc_current_buf->cur_idx < SAMPLES_PER_BUFFER) {
       adc_current_buf->samples[adc_current_buf->cur_idx] = adc_current;
       adc_current_buf->cur_idx = adc_current_buf->cur_idx+1;
       } else {
       adc_current_buf = ad7988_get_free_buf();
       RT_ASSERT(adc_current_buf != RT_NULL);
       adc_current_buf->seq_num = buf_seq++;
       }
       */
}

#ifdef CONNECT_TO_ANDROID
uint32_t USBH_OTG_ISR_Handler (USB_OTG_CORE_HANDLE *pdev)
{
    USBH_OTG_HOST_ISR_Handler(pdev);
}

void rt_usbh_process(void* parameter)
{
    Connection_t *conn_p;	   
    adc_buffer_t *send_buf;
    exg_msg_t dummy_msg;
    uint16_t type_size1, type_size2;					  

    /* initialize dummy message */
    dummy_msg.type = EXG2MNT_DATA;
    dummy_msg.length = 0;

    ADB_init();
    conn_p = ADB_addConnection("tcp:4568", 1, adbEventHandler);

    while (1) {			 
        if (conn_p->status == ADB_OPEN) {
            usb_connection_open = 1;   
        }
        //send_buf = ad7947_get_send_buf();
        if (send_buf != RT_NULL) {
            if (connected == 1) {
                exg_red_flip();
                Connection_write(conn_p, (2*SAMPLES_PER_BUFFER + 6 + sizeof(exg_msg_t)), 
                        &(send_buf->msg_hdr));
            } 
        } else {
            //Connection_write(conn_p, sizeof(exg_msg_t), &dummy_msg);
        }
        ADB_poll();		   
    }	 						  
}								 
#endif		

#ifdef CONNECT_TO_PC
USB_OTG_CORE_HANDLE usb_host_reg;	// dummy data structure
uint32_t USBH_OTG_ISR_Handler (USB_OTG_CORE_HANDLE *pdev)
{
    STM32_PCD_OTG_ISR_Handler();
    return 0;
}

void rt_usbd_process (void *parameter)
{
    //adc_buffer_t *send_buf;
    uint8_t eof_pkt[64] = {0xFF, 0xFF};
    uint8_t i = 0;
    static uint32_t timeOutCnt = 0;

    USB_OTG_BSP_TimeInit(0);
    //USB_OTG_BSP_mDelay(1000);
    USB_Interrupts_Config();
    Set_USBClock();
    USB_Init();

    while (1) 
    {  
        if(bDeviceState != CONFIGURED)
        {
            continue;
        }
        if(PrevXferComplete != USB_TRANSFER_NEXT_PKT &&
                PrevXferComplete != USB_TRANSFER_SEND_EOF)
        {
            //rt_kprintf("Status = %d\n", PrevXferComplete);
            //rt_thread_delay(10);
            continue;
        }
        if (PrevXferComplete == USB_TRANSFER_NEXT_PKT) 
        {
            PrevXferComplete = USB_TRANSFER_IN_PROGRESS;
            for(i = 0; i < 64; i++)
            {
                eof_pkt[i] = i;
            }
            USB_SIL_Write(EP1_IN, &eof_pkt[0], 64);
            pc_send_count++;
            //rt_kprintf("data sent\n");
        } else 
        {
            PrevXferComplete = USB_TRANSFER_EOF_CMPLT;
            for(i = 0; i < 64; i++)
            {
                eof_pkt[i] = 0;
            }
            eof_pkt[0] = 0xff;
            eof_pkt[1] = 0xff;
            USB_SIL_Write(EP1_IN, eof_pkt, 64);
            rt_kprintf("EOF sent\n");
        }
    }
}
#endif

void rt_vcp_process(void *parameter)
{
    uint8_t buf[32] = {0};;
    buf[0]=0;
    buf[31] = 0xff;
    while(1)
    {
        if(u8VCPSendEnable == 1)
        {
            //rt_thread_delay(10);
            rt_kprintf("Send one packet\n");
            APP_FOPS.pIf_DataTx(buf,32);
            buf[0] ++;
            u8VCPSendEnable = 0;
        }
    }
}

void rt_init_thread_entry(void* parameter)
{										  		    
    char ch;
    int sample_idx;
    int tmp_hz;		  
#ifdef CONNECT_TO_ANDROID
    rt_thread_t usbh_process_thread;
#endif
#ifdef CONNECT_TO_PC
    rt_thread_t usbd_process_thread;
#endif
    rt_thread_t vcp_process_thread;

#ifdef CONNECT_TO_ANDROID
    /* Initialize USB ADB process */
    usbh_process_thread = rt_thread_create("usbh_process",	   		  
            rt_usbh_process, RT_NULL,
            2048, 20, 10);

    if (usbh_process_thread != RT_NULL)
        rt_thread_startup(usbh_process_thread);
#endif

#ifdef CONNECT_TO_PC
    usbd_process_thread = rt_thread_create("usbd_process",
            rt_usbd_process, RT_NULL,
            2048, 20, 10);
    if (usbd_process_thread != RT_NULL)
        rt_thread_startup(usbd_process_thread);
#endif

    /* Initialize EXG IO control and AD7988 */
    //DBInit();
    USBD_Init(&USB_OTG_dev,   USB_OTG_FS_CORE_ID,  &USR_desc,  &USBD_CDC_cb,  &USR_cb);
    exg_io_init();
    rt_hw_led_init();

    AdInter_init();
    //exg_set_nstart(); 
    //exg_trigger_start();
    //exg_set_channel(2);
    rt_hw_led_on(0);
    rt_hw_led_on(1);

    vcp_process_thread = rt_thread_create("vcp_process", rt_vcp_process, RT_NULL, 2048, 20, 10);
    if (vcp_process_thread != RT_NULL)
    {
        rt_thread_startup(vcp_process_thread);
    }
    /* Read charactors from UART */
#if 1
    while (1)
    {
        /* wait receive */		 
        if (rt_sem_take(&rx_sem, RT_WAITING_FOREVER) != RT_EOK) continue;

        /* read one character from device */
        while (rt_device_read(app_dev, 0, &ch, 1) == 1)
        {
            /* handle CR key */
            if (ch == '\r')	  						  
            {
                char next;

                if (rt_device_read(app_dev, 0, &next, 1) == 1)
                    ch = next;
                else ch = '\r';
            }
            /* handle tab key */
            else if (ch == '\t')
            {
                /* change tab to whitespace */
                ch = ' ';
            }
            /* handle backspace key */			   
            else if (ch == 0x7f || ch == 0x08)
            {
                if (lines_idx != 0)
                {
                    rt_kprintf("%c %c", ch, ch);  
                }
                if (lines_idx <= 0)    
                    lines_idx = 0;
                else 
                    lines_idx --;

                continue;					 
            }

            /* handle end of line, break */
            if (ch == '\r' || ch == '\n')		  
            {
                if (lines[0] == 's') {
                    //rt_kprintf("    Stop 32K\n");
                    //exg_stop_32k();
                    u8VCPSendEnable = 0;
                    DBDebugStatistics();
                } else if (lines[0] == 'S') {
                    rt_kprintf("    Start 32K\n");
                    u8VCPSendEnable = 1;
                    exg_start_32k();
                } else if (lines[0] == 't') {
                    rt_kprintf("    Trigger start signal\n");
                    exg_trigger_start();
                } else if (lines[0] >= '0' && lines[0] <= '3') {
                    rt_kprintf("    Set gain to %d\n", (lines[0] - '0'));
                    exg_set_gain((lines[0] - '0'));
                } else if (lines[0] == 'r') {
                    tmp_hz = 0;
                    sample_idx = 1;
                    while (lines[sample_idx] != 'x') {
                        tmp_hz *= 10;
                        tmp_hz += (lines[sample_idx] - '0');
                        sample_idx++;
                    }
                    hz_config = tmp_hz;
                    rt_kprintf("    Set hz to %d\n", hz_config);
                } else {
                    rt_kprintf("Unrecognized command\n's' for stop 32K\n'S' for start 32K\n"
                            "'t' for trigger start signal\n'0'-'3' for gain 0~3\n");
                }
                lines_idx = 0;	  
                break;
            }

            /* it's a large line, discard it */
            if (lines_idx >= MAX_CHARS) 
                lines_idx = 0;

            /* normal character */
            lines[lines_idx] = ch; 
            ch = 0;
            rt_kprintf("%c", lines[lines_idx]);
            lines_idx++;
        } /* end of device read */
    }
#endif
}								 

static rt_err_t uart_rx_ind(rt_device_t dev, rt_size_t size)
{
    /* release semaphore to let finsh thread rx data */
    rt_sem_release(&rx_sem);

    return RT_EOK;
}

int rt_application_init()
{
    rt_thread_t init_thread;

    app_dev = rt_device_find(CONSOLE_DEVICE);
    if (app_dev != RT_NULL && rt_device_open(app_dev, RT_DEVICE_OFLAG_RDONLY) == RT_EOK)
    {
        rt_device_set_rx_indicate(app_dev, uart_rx_ind);
    }
    else
    {
        rt_kprintf("application: can not find device:%s\n", CONSOLE_DEVICE);
    }

    rt_sem_init(&(rx_sem), "uart_rx_sem", 0, 0);
    rt_sem_init(&(usb_tx_sem), "usb_tx_sem", 0, 0);

#if (RT_THREAD_PRIORITY_MAX == 32)
    init_thread = rt_thread_create("init",	  
            rt_init_thread_entry, RT_NULL,
            2048, 8, 20);
#else
    init_thread = rt_thread_create("init",
            rt_init_thread_entry, RT_NULL,
            2048, 80, 20);
#endif

    if (init_thread != RT_NULL)
        rt_thread_startup(init_thread);

    return 0; 
}

/*@}*/
