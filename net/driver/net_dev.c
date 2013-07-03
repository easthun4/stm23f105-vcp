/*********************************************************************
 *
 * Abstracts generic network device operation, and provide dummy
 * stub here if required. Each driver implementation hooks to
 * this interface
 *
 *********************************************************************/

#include "net_dev.h"

static net_dev_drv_t net_device_driver;
static net_dev_drv_t *net_device_driver_p = &net_device_driver;

static rt_err_t default_init (rt_device_t dev)
{
    return RT_EOK;
}

static rt_err_t default_open (rt_device_t dev, rt_uint16_t oflag)
{
    return RT_EOK;
}
 
static rt_err_t default_close (rt_device_t dev)
{
    return RT_EOK;
}

static rt_size_t default_read (rt_device_t dev, rt_off_t pos, void* buffer, 
                                rt_size_t size)
{
    rt_set_errno(-RT_ENOSYS);
    return RT_EOK;
}

static rt_size_t default_write (rt_device_t dev, rt_off_t pos, const void* buffer, rt_size_t size)
{
    rt_set_errno(-RT_ENOSYS);
    return RT_EOK;
}

static rt_err_t default_control(rt_device_t dev, rt_uint8_t cmd, void *args)
{
    switch (cmd)
    {
    case NIOCTL_GADDR:
        /* get mac address */
        if (args) rt_memcpy(args, net_device_driver_p->dev_addr, ETH_ALEN);
        else return -RT_ERROR;
        break;

    default :
        break;
    }

    return RT_EOK;
}

/*
 * Initialize file operations by specific driver implementation
 * Parameters set to NULL means use the default defined in net_dev.c
 */
rt_err_t net_dev_drv_register_handler (net_dev_drv_init_func init, 
                                       net_dev_drv_open_func open,
                                       net_dev_drv_close_func close, 
                                       net_dev_drv_read_func read,
                                       net_dev_drv_write_func write, 
                                       net_dev_drv_control_func control,
                                       net_dev_drv_rx_func rx, 
                                       net_dev_drv_tx_func tx)
{														   
    /* do not allow multiple registry */
    if (net_device_driver_p->parent.eth_rx || 
            net_device_driver_p->parent.eth_tx ||
            rx == NULL || tx == NULL) {
        return -RT_ERROR;
    }

    /* dummy initialization */
    net_device_driver_p->parent.parent.init = init ? init : default_init;
    net_device_driver_p->parent.parent.open = open ? open : default_open;
    net_device_driver_p->parent.parent.close = close ? close : default_close;
    net_device_driver_p->parent.parent.read = read ? read : default_read;
    net_device_driver_p->parent.parent.write = write ? write : default_write;
    net_device_driver_p->parent.parent.control = control ? control : default_control;
    net_device_driver_p->parent.eth_rx = rx;
    net_device_driver_p->parent.eth_tx = tx;

    return RT_EOK;
}

/*
 * Initialize device MAC address by device driver implementation
 */
void net_dev_drv_set_dev_addr (rt_uint8_t *device_address)
{
    rt_memcpy(net_device_driver_p->dev_addr, device_address, ETH_ALEN);
}

/*
 * Hook this driver into lwIP
 */
rt_err_t net_dev_drv_init (void) 
{
    if (net_device_driver_p->parent.eth_rx == NULL ||
            net_device_driver_p->parent.eth_tx == NULL) {
        return -RT_ERROR;
    }

    /* Update MAC address */
	if (1) {
    net_device_driver_p->dev_addr[0] = 0x00;
    net_device_driver_p->dev_addr[1] = 0x0B;
    net_device_driver_p->dev_addr[2] = 0x6C;
    net_device_driver_p->dev_addr[3] = 0x89;
    net_device_driver_p->dev_addr[4] = 0xBD;
    net_device_driver_p->dev_addr[5] = 0x60;	  
	} else {
	net_device_driver_p->dev_addr[0] = 0x00;
    net_device_driver_p->dev_addr[1] = 0x0B;
    net_device_driver_p->dev_addr[2] = 0x6C;
    net_device_driver_p->dev_addr[3] = 0x89;   
    net_device_driver_p->dev_addr[4] = 0x5B;
    net_device_driver_p->dev_addr[5] = 0x1E;	  
	}

    rt_sem_init(&net_device_driver_p->net_dev_sem, "net_dev_sem", 1, 
                    RT_IPC_FLAG_FIFO);

    net_device_driver_p->rx_pkt_pending = 0;   

    /* add device to rt_device list */		    
    return eth_device_init(&(net_device_driver_p->parent), "e0");
}

rt_err_t net_dev_drv_notify_rx (void)
{											  
    return eth_device_ready(&net_device_driver_p->parent);
}

/*
 * Lock/unlock protecting against simultanous read and write
 */
rt_err_t net_dev_drv_lock (void)
{
    return rt_sem_take(&net_device_driver_p->net_dev_sem, RT_WAITING_FOREVER);
}

void net_dev_drv_unlock (void)
{
    rt_sem_release(&net_device_driver_p->net_dev_sem);
}
   