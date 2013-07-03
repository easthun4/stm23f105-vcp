/*********************************************************************
 *
 * Abstracts generic network device operation, and provide dummy
 * stub here if required. Each driver implementation hooks to
 * this interface
 *
 *********************************************************************/

#ifndef __NET__DRIVER__NET_DEV__
#define __NET__DRIVER__NET_DEV__

#include <netif/ethernetif.h>
#include <rtdef.h>

typedef rt_err_t (*net_dev_drv_init_func)(rt_device_t dev);
typedef rt_err_t (*net_dev_drv_open_func)(rt_device_t dev, rt_uint16_t oflag);
typedef rt_err_t (*net_dev_drv_close_func)(rt_device_t dev);
typedef rt_size_t (*net_dev_drv_read_func)(rt_device_t dev, rt_off_t pos, 
                    void* buffer, rt_size_t size);
typedef rt_size_t (*net_dev_drv_write_func)(rt_device_t dev, rt_off_t pos, 
                    const void* buffer, rt_size_t size);
typedef rt_err_t (*net_dev_drv_control_func)(rt_device_t dev, rt_uint8_t cmd, 
                    void *args);
typedef struct pbuf *(*net_dev_drv_rx_func)(rt_device_t dev);
typedef rt_err_t (*net_dev_drv_tx_func)(rt_device_t dev, struct pbuf *p);

#define ETH_ALEN    6
#define ETHER_FRAME_LEN 1514

/*
 * This structure hooks specific device driver
 * with lwIP ethernet layer, this is singleton
 * in the system
 */
typedef struct net_dev_drv
{
    /* inherit from ethernet device */
    struct eth_device parent;

    /* interface address info. */
    rt_uint8_t  dev_addr[ETH_ALEN];         /* hw address   */

    struct rt_semaphore net_dev_sem;    /* tx/rx semaphore */

    rt_uint8_t  rx_pkt_pending;     /* receive pkt pending counter, added by 
                                       rx interrupt, and subed by eth_rx */
} net_dev_drv_t;

/*
 * Initialize file operations by specific driver implementation
 * Parameters set to NULL means use the default defined in net_dev.c
 */
rt_err_t net_dev_drv_register_handler (net_dev_drv_init_func, 
									   net_dev_drv_open_func,
                                   	   net_dev_drv_close_func, 
									   net_dev_drv_read_func,
                                   	   net_dev_drv_write_func, 
									   net_dev_drv_control_func,
                                   	   net_dev_drv_rx_func, 
									   net_dev_drv_tx_func);

/*
 * Initialize device MAC address by device driver implementation
 */
void net_dev_drv_set_dev_addr (rt_uint8_t *device_address);

/*
 * Notify rx data to eth_rx
 */
rt_err_t net_dev_drv_notify_rx (void);

/*
 * Lock/unlock protecting against simultanous read and write
 */
rt_err_t net_dev_drv_lock (void);
void net_dev_drv_unlock (void);

#endif
