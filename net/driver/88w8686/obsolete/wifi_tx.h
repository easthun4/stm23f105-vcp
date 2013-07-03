/*********************************************
 * wifi firmware send packet handler
 *
 *
 *
 *********************************************/

#ifndef __8686_WIFI_TX__
#define __8686_WIFI_TX__

#include <rtthread.h>
#include "stm32f10x_type.h"
#include "rtdef.h"
#include "lwip/pbuf.h"
#include "wifi_api_cmd.h"

#define ETHER_FRAME_LEN 1514
#define ETH_ALEN		6

rt_err_t rt_wifi_tx(rt_device_t dev, struct pbuf* p);

#endif
