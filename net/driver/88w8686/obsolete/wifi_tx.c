/*********************************************
 * wifi firmware send packet handler
 *
 *
 *
 *********************************************/

#include "wifi_tx.h"
					    
struct rt_semaphore rt_wifi_tx_lock;  
			    
/* FIXME
 * pbuf transmit function, now we assume
 * there is only one buf on the pbuf list   
 */
rt_err_t rt_wifi_tx(rt_device_t dev, struct pbuf* p)
{
	rt_err_t ret = RT_EOK;
	txpd_t *txpd_p = RT_NULL;
	rt_uint32_t level;

	/* check frame length */  		  
	if (p->len > ETHER_FRAME_LEN) {
		rt_kprintf("rt_wifi_tx packet length exceeding %d not supported\n",
			ETHER_FRAME_LEN);
		return RT_ERROR;
	}

	txpd_p = rt_malloc(sizeof(txpd_t));
	if (txpd_p == RT_NULL) {
		rt_kprintf("No memory\n");
		return RT_ERROR;
	}

	/* wait receive */
    if (rt_sem_take(&rt_wifi_tx_lock, RT_WAITING_FOREVER) != RT_EOK) {
		ret = RT_EBUSY;
		goto out_free_txpd;
	}
	
	memcpy(txpd_p->tx_dest_addr_high, p->payload, ETH_ALEN);
	txpd_p->tx_packet_length = p->tot_len;
	txpd_p->tx_packet_location = sizeof(txpd_t) - 4;

   	level = rt_hw_interrupt_disable();
	/* Send data */
	if (rt_wifi_send_data_no_wait(txpd_p, p) != RT_TRUE) {
		ret = RT_ERROR;
		rt_kprintf("rt_wifi_tx failed to send data...\n");
	} else {
		//rt_kprintf("rt_wifi_tx successfully sent data!\n");
	}
	rt_hw_interrupt_enable(level);

	rt_sem_release(&rt_wifi_tx_lock);

out_free_txpd:
	rt_free(txpd_p);

	return ret;
}
