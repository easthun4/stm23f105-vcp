/*********************************************
 * Marvell 88W8686 RT-Thread driver model
 *
 *
 *
 *********************************************/

#include "wifi_8686.h"
#include "wifi_tx.h"

net_device_8686_t mrvl_8686_dev_entry;
extern struct rt_semaphore eth_rx_lock;
extern rt_rx_fifo_t *head;
extern rt_rx_fifo_t *tail;

rt_err_t mrvl_8686_init(rt_device_t dev)
{
	return RT_EOK;
}

rt_err_t mrvl_8686_open(rt_device_t dev, rt_uint16_t oflag)
{
    return RT_EOK;
}

rt_err_t mrvl_8686_close(rt_device_t dev)
{
    return RT_EOK;
}

rt_size_t mrvl_8686_read(rt_device_t dev, rt_off_t pos, void* buffer, rt_size_t size)
{
    rt_set_errno(-RT_ENOSYS);
    return 0;
}

rt_size_t mrvl_8686_write(rt_device_t dev, rt_off_t pos, const void* buffer, rt_size_t size)
{
    rt_set_errno(-RT_ENOSYS);
    return 0;
}

/* Currently we do not support IOCTL */
rt_err_t mrvl_8686_control(rt_device_t dev, rt_uint8_t cmd, void *args)
{
    switch(cmd)
    {

    default :
        break;
    }

    return -RT_ERROR;
}

/* Driver model initialize */
void rt_hw_88w8686_init()
{
    mrvl_8686_dev_entry.parent.parent.init       = mrvl_8686_init;
    mrvl_8686_dev_entry.parent.parent.open       = mrvl_8686_open;
    mrvl_8686_dev_entry.parent.parent.close      = mrvl_8686_close;
    mrvl_8686_dev_entry.parent.parent.read       = mrvl_8686_read;
    mrvl_8686_dev_entry.parent.parent.write      = mrvl_8686_write;
    mrvl_8686_dev_entry.parent.parent.control    = mrvl_8686_control;
	mrvl_8686_dev_entry.parent.eth_rx 			 = rt_wifi_rx;
    mrvl_8686_dev_entry.parent.eth_tx 			 = rt_wifi_tx;

    /* Update MAC address */
    mrvl_8686_dev_entry.dev_addr[0] = 0x00;
    mrvl_8686_dev_entry.dev_addr[1] = 0x0B;
    mrvl_8686_dev_entry.dev_addr[2] = 0x6C;
    mrvl_8686_dev_entry.dev_addr[3] = 0x89;
    mrvl_8686_dev_entry.dev_addr[4] = 0xBD;
    mrvl_8686_dev_entry.dev_addr[5] = 0x60;	  

	/* Initialize eth_rx lock */
	rt_sem_init(&eth_rx_lock, "eth_rx_lock", 0, 0);
	head = (rt_rx_fifo_t *)rt_malloc(sizeof(rt_rx_fifo_t));
	tail = head;
	head->next = RT_NULL;
	head->p = RT_NULL;

	eth_device_init(&(mrvl_8686_dev_entry.parent), "e0");
}
