/******************************************************************************
 *
 * Abstraction layer for Marvell 88w8686, this is the layer between
 * net_dev_drv_t layer and the bottom gspi/sdio layer.
 *
 * This layer abstracts the common operations for firmware download,
 * packet tx/rx
 ******************************************************************************/

#include "wifi_drv.h"

static w8686_drv_t wifi_driver;
static w8686_drv_t *wifi_driver_p = &wifi_driver;

static void delay_n (rt_uint32_t n)						    
{
	rt_uint32_t	i;
	for (i = 0; i < n; i++);
}

static rt_err_t w8686_firmware_cmd (rt_uint8_t *cmd, 
									rt_uint16_t len,
									rt_uint8_t **cmd_resp_pp)
{								   
	rt_uint8_t *resp;							  
	rt_uint16_t resp_len;
	w_fm_api_msg_t *msg_req, *msg_resp;
	rt_err_t ret = RT_EOK;

    if (wifi_driver_p->send_cmd(cmd, len) != RT_EOK) {
		return -RT_ERROR;
	}    
	resp_len = wifi_driver_p->get_cmd_resp_len();

	if (resp_len == 0) {	    
		return -RT_ERROR;
	}									   

	resp = (rt_uint8_t *)rt_malloc(resp_len);
	if (resp == NULL) {
		rt_kprintf("Failed to malloc memory!\n");
		return -RT_ERROR;
	}

   	delay_n(1000);
	wifi_driver_p->recv_cmd_resp(resp, resp_len);
	
	/* Check the message response */
	msg_req = (w_fm_api_msg_t *)cmd;
	msg_resp = (w_fm_api_msg_t *)resp;
	
	if (msg_req->cmd_type != msg_resp->cmd_type ||
		msg_resp->resp_flag != 0x80 ||
		msg_req->cmd_seq != msg_resp->cmd_seq ||
		msg_resp->result != 0) {
		ret = -RT_ERROR;
	}
					 					 
	/* If passed in cmd_resp_pp, then hand the result out */	
	if (cmd_resp_pp != NULL) {
		*cmd_resp_pp = resp;
	} else {
		rt_free(resp);
	}

	return ret;
}

static rt_err_t w8686_mac_control (void)
{
	rt_uint8_t cmd[] = 
	{	
        0x28, 0x00,
        0x0c, 0x00,
        0x02, 0x00,
        0x00, 0x00,
        0x03, 0x00,
        0x00, 0x00				    
    };				 	  

    return w8686_firmware_cmd(cmd, sizeof(cmd), NULL);  
}

static rt_err_t w8686_enable_rsn (void)
{
    rt_uint8_t cmd[] = 
	{	
        0x2f, 0x00,
        0x0c, 0x00,
        0x03, 0x00,
        0x00, 0x00,
        0x01, 0x00,
        0x00, 0x00,
    };

    return w8686_firmware_cmd(cmd, sizeof(cmd), NULL);
}

static rt_err_t w8686_authenticate()
{
    rt_uint8_t cmd[] = 
	{	
        0x11, 0x00,
        0x19, 0x00,
        0x04, 0x00,
        0x00, 0x00,
        0x00, 0x22, 0xb0, 0xaa, 0xc2, 0x66,
        0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
        
    return w8686_firmware_cmd(cmd, sizeof(cmd), NULL);
}

static rt_err_t w8686_radio_control()
{
    rt_uint8_t cmd[] = 
	{	
        0x1c, 0x00,
        0x0c, 0x00,
        0x05, 0x00,
        0x00, 0x00,
        0x01, 0x00,
        0x01, 0x00,
    };
        
    return w8686_firmware_cmd(cmd, sizeof(cmd), NULL);
}

static rt_err_t w8686_associate()
{
	rt_uint8_t *cmd_resp = NULL;
	rt_err_t ret = -RT_ERROR;

    rt_uint8_t cmd[] = 
	{	
        0x12, 0x00,
        0x3a, 0x00,
        0x06, 0x00,
        0x00, 0x00,
        0x00, 0x22, 0xb0, 0xaa, 0xc2, 0x66,
        0x21, 0x04,
        0x0a, 0x00,		  			 
        0x00, 0x00,
        0x00,
        0x00, 0x00,
        0x05, 0x00,
        0x66, 0x65, 0x6c, 0x69, 0x78,
        0x03, 0x00,
        0x01, 0x00,
        0x04,
        0x04, 0x00,
        0x06, 0x00,
        0x00,
        0x00,
        0x00, 0x00,
        0x00, 0x00,
        0x01, 0x00,
        0x04, 0x00,
        0x82, 0x84, 0x8b, 0x96,
        0x1f, 0x01,
        0x01, 0x00,
        0x00,
    };
        
    ret = w8686_firmware_cmd(cmd, sizeof(cmd), &cmd_resp);

	if (ret == RT_EOK && cmd_resp != NULL) {
		if ((cmd_resp[4] | (cmd_resp[5] << 8)) == 0xffff) {
			ret = -RT_ERROR;
		} else {
			rt_kprintf("Associate success!\n");
		}
	}

	if (cmd_resp != NULL) {
		rt_free(cmd_resp);
	}
	return ret;
}

static rt_err_t w8686_associate_with_ap (void)
{
	rt_uint8_t retry_count = 10;
	rt_err_t ret;

	if (w8686_mac_control() != RT_EOK) {
		return -RT_ERROR;
	}
	if (w8686_enable_rsn() != RT_EOK) {
		return -RT_ERROR;
	}
	if (w8686_authenticate() != RT_EOK) {
	 	return -RT_ERROR;
	}
	if (w8686_radio_control() != RT_EOK) {
		return -RT_ERROR;
	}

	/* now try to associate with AP */
	do {
		ret = w8686_associate();
		retry_count--;
	} while (ret != RT_EOK && retry_count > 0);

	return ret;
}

/*
 * Initialize 88w8686 firmware
 * Associate with known AP
 */
rt_err_t w8686_init (rt_device_t dev)
{
    if (wifi_driver_p->bringup_firmware() != RT_EOK) {
        rt_kprintf("88w8686 firmware init failed\n");
        return -RT_ERROR;
    }									   

    if (w8686_associate_with_ap() != RT_EOK) {
        rt_kprintf("88w8686 associate with AP failed\n");
        return -RT_ERROR;
    }
	
	return RT_EOK;
}

void *w8686_pkt_rsrc_alloc (rt_uint8_t **data_pp, rt_uint16_t len)
{
	struct pbuf *p = NULL;
	*data_pp = NULL;

	p = pbuf_alloc(PBUF_RAW, len, PBUF_RAM);
	if (p != NULL) {
        *data_pp = p->payload;
    }

	return (void *)p;
}

struct pbuf *w8686_rx (rt_device_t dev)
{
    struct pbuf *p = NULL; 

    /* take lock */
    if (net_dev_drv_lock() != RT_EOK) {
        rt_kprintf("Failed to get net_dev_drv lock\n");
        return NULL;
    }

	p = (struct pbuf *)wifi_driver_p->recv_data(w8686_pkt_rsrc_alloc);

    /* release lock */
    net_dev_drv_unlock();

    return p;		    
}				   

/*
 * Packet xmit routine, assume per pkt per pbuf
 */
rt_err_t w8686_tx (rt_device_t dev, struct pbuf *p)
{
    rt_err_t ret;

    /* get lock */
    if (net_dev_drv_lock() != RT_EOK) {
        rt_kprintf("Failed to get net_dev_drv lock\n");
        return NULL;
    }

    if (p->tot_len > ETHER_FRAME_LEN) {
        rt_kprintf("Pkt length exceeding 1514\n");
        net_dev_drv_unlock();
        return NULL;
    }
				    
    ret = wifi_driver_p->send_data(p);

    /* release lock */
    net_dev_drv_unlock();   

    return ret;
}

/*
 * Initialize host interface related operations
 */
rt_err_t w8686_register_interface_handler (bringup_firmware_func bringup, 
                                     send_cmd_func send_cmd,
                                     get_cmd_resp_len_func get_cmd_resp_len, 
                                     recv_cmd_resp_func recv_cmd_resp,
                                     send_data_func send_data, 
                                     recv_data_func recv_data)
{
    wifi_driver_p->bringup_firmware = bringup;	 
    wifi_driver_p->send_cmd = send_cmd;
    wifi_driver_p->get_cmd_resp_len = get_cmd_resp_len;
    wifi_driver_p->recv_cmd_resp = recv_cmd_resp;
    wifi_driver_p->send_data = send_data;			    
    wifi_driver_p->recv_data = recv_data;

    return net_dev_drv_register_handler(w8686_init, NULL, NULL, NULL, 
                                        NULL, NULL,
                                        w8686_rx, w8686_tx);
}

