/*********************************************
 * Marvell 88W8686 RT-Thread driver model
 *
 *
 *
 *********************************************/

#ifndef __8686_WIFI_DRIVER__
#define __8686_WIFI_DRIVER__

#include <netif/ethernetif.h>
#include "wifi_tx.h"

typedef struct net_device_8686
{
    /* inherit from ethernet device */
    struct eth_device parent;

	/* interface address info. */
    rt_uint8_t  dev_addr[ETH_ALEN];         /* hw address   */
} net_device_8686_t;

void rt_hw_88w8686_init(void);

#endif
