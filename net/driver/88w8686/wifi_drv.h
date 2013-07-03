/******************************************************************************
 *
 * Abstraction layer for Marvell 88w8686, this is the layer between
 * net_dev_drv_t layer and the bottom gspi/sdio layer.
 *
 * This layer abstracts the common operations for firmware download,
 * packet tx/rx
 ******************************************************************************/

#ifndef __NET__DRIVER__88W8686__WIFI_DRV__
#define __NET__DRIVER__88W8686__WIFI_DRV__

#include "net_dev.h"

/* 
 * callback function used by interface driver 
 * to allocate pbuf resource
 */
typedef void *(*pkt_rsrc_alloc_func) (rt_uint8_t **data_pp, rt_uint16_t len);

typedef rt_err_t (*bringup_firmware_func) (void);
typedef rt_err_t (*send_cmd_func) (rt_uint8_t *data, rt_uint16_t len);
typedef rt_uint16_t (*get_cmd_resp_len_func) (void);
typedef void (*recv_cmd_resp_func) (rt_uint8_t *data, rt_uint16_t len);
typedef rt_err_t (*send_data_func) (struct pbuf *p);
typedef void *(*recv_data_func) (pkt_rsrc_alloc_func pkt_rsrc_alloc);

typedef struct w8686_drv {
    rt_err_t (*bringup_firmware) (void);
    /* cmd */
    rt_err_t (*send_cmd) (rt_uint8_t *data, rt_uint16_t len);
    rt_uint16_t (*get_cmd_resp_len) (void);
    void (*recv_cmd_resp) (rt_uint8_t *data, rt_uint16_t len);
    /* data */
    rt_err_t (*send_data) (struct pbuf *p);
    void *(*recv_data) (pkt_rsrc_alloc_func pkt_rsrc_alloc);
} w8686_drv_t;

typedef struct w_fm_api_msg {
	rt_uint8_t 	cmd_type;
	rt_uint8_t 	resp_flag;
	rt_uint16_t len;
	rt_uint16_t cmd_seq;
	rt_uint16_t result;
	rt_uint16_t *data;	
} w_fm_api_msg_t;

/*
 * Initialize host interface related operations
 */
rt_err_t w8686_register_interface_handler (bringup_firmware_func, 
										   send_cmd_func,
                                           get_cmd_resp_len_func, 
										   recv_cmd_resp_func,
                                           send_data_func, 
                                           recv_data_func);

void *w8686_pkt_rsrc_alloc (rt_uint8_t **data_pp, rt_uint16_t len);

#endif
