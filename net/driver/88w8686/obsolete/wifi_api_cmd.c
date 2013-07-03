/******************************************
 * Marvell 88W8686 firmware API source file
 *
 ******************************************/

#include "wifi_api_cmd.h"

//#define DEBUG_WIFI_CMD
#undef DEBUG_WIFI_CMD

struct rt_semaphore sd_read_lock, sd_write_lock;
rt_bool_t sd_waiting_write = RT_FALSE;
rt_bool_t sd_waiting_cmd_resp = RT_FALSE;
rt_wifi_cmd_cb_t cmd_cb;
struct rt_semaphore eth_rx_lock;
rt_rx_fifo_t *head = RT_NULL;
rt_rx_fifo_t *tail = RT_NULL;

static void rt_print_ascii_char(u8 x)
{
 	rt_kprintf("%x%x ", ((x & 0xF0) >> 4), (x & 0x0F));
}

void rt_wifi_resp_thread_entry(void* parameter)
{
	SD_Error errorstatus = SD_OK;
	u32 resp_len, idx;
	u8 *result_buffer;
	rt_uint32_t level;
	u16 type;
	u16 size;

 	/* 
	 * SDIO wifi will interrupt us when there 
	 * is data available, so we just need wait
	 * on the semaphore...
	 */
	while(1) {
        /* wait receive */
        if (rt_sem_take(&sd_read_lock, RT_WAITING_FOREVER) != RT_EOK) {
			continue;
		}
		
		level = rt_hw_interrupt_disable();

		errorstatus = SDIO_ReadScratch(1, &resp_len);
    	if (errorstatus != SD_OK) {
			rt_hw_interrupt_enable(level);
        	rt_kprintf("wifi_resp_thread read scratch register error!\n");
			continue;
    	}

#ifdef DEBUG_WIFI_CMD
		rt_kprintf("Got %d bytes response from wifi\n", resp_len);
#endif
		result_buffer = (u8 *)rt_malloc(resp_len);
		if (result_buffer == RT_NULL) {
			rt_hw_interrupt_enable(level);
			rt_kprintf("Error rt_malloc returned NULL, out of memory!\n");
			continue;   
		}	  

		/* Now read out the true data */
    	errorstatus = SDIO_IORWExtendReadHelper(result_buffer, resp_len);
		rt_hw_interrupt_enable(level);

    	if (errorstatus != SD_OK) {
        	rt_kprintf("Error reading result from SDIO, abort....\n");
			continue;
    	}
		
		/* Now we shall print out the result through UART */
		/*
		for (idx = 0; idx < resp_len; idx++) {
			rt_print_ascii_char(result_buffer[idx]);
		}
		rt_kprintf("\n");
		*/

		size = result_buffer[0] | (result_buffer[1] << 8);
		type = result_buffer[2] | (result_buffer[3] << 8);
		/* 
		 * Be careful, each handler should take care of	
		 * rt_free(result_buffer), otherwise there will
		 * be memory leak!!!
		 */
		switch (type) {
		case MVMS_DAT:
			rt_wifi_handle_data(result_buffer, size);
			break;
		case MVMS_CMD:
			rt_wifi_handle_cmd(result_buffer, size);
			break;
		case MVMS_EVENT:
			rt_wifi_handle_event(result_buffer, size);
			break;
		default:
			rt_kprintf("invalid type from wifi module:\n");
			for (idx = 0; idx < resp_len; idx++) {
				rt_print_ascii_char(result_buffer[idx]);
			}
			rt_kprintf("\n");
			rt_free(result_buffer);
		}
	}
}

u8* rt_ascii_to_char(u8 *x, u32 ti, u32 *to)  
{
	u32 cur_v = 0;
	rt_bool_t adv_flag = RT_FALSE;
	u8 *ret_p;

	*to = 0;
	ret_p = x;
	while (ti > 0) {
	 	if (x[0] >= '0' && x[0] <= '9') {
			cur_v *= 16;
			cur_v += x[0] - '0';
			adv_flag = RT_TRUE;
		} else if (x[0] >= 'a' && x[0] <= 'f') {
			cur_v *= 16;
			cur_v += x[0] - 'a' + 10;
			adv_flag = RT_TRUE;
		} else if (x[0] >= 'A' && x[0] <= 'F') {
			cur_v *= 16;
			cur_v += x[0] - 'A' + 10;
			adv_flag = RT_TRUE;
		} else if (x[0] == ' ') {
			if (adv_flag == RT_TRUE) {
				adv_flag = RT_FALSE;
				if (cur_v > 0xff) {
					return RT_NULL;
				}
				ret_p[*to] = cur_v & 0xff;
				*to += 1;
				cur_v = 0;
			}
		} else {
			return RT_NULL;
		}
		ti--;
		x++; 
	}
	if (adv_flag == RT_TRUE) {
		adv_flag = RT_FALSE;
		if (cur_v > 0xff) {
			return RT_NULL;
		}
		ret_p[*to] = cur_v & 0xff;
		*to += 1;
		cur_v = 0;
	}
	return ret_p;
}

extern u8 pkt3[];

/* Wifi cmd request wrapper for UART */
void rt_wifi_uart_req_send_sync(u8 *x, int tl)
{
	u8 *xc = RT_NULL;
	u32 to = 0;
	u16 result;
	u8 *result_p = RT_NULL;
	u16 size, idx;

	/* 
	 * for now we force send arp response
	 */
	struct pbuf p3;

	if (tl <= 0) {
		rt_kprintf("Ignore null command...\n");
		return;
	}

	xc = rt_ascii_to_char(x, tl, &to);
	if (xc == RT_NULL) {
		rt_kprintf("Error converting ascii string to char...\n");
		
		p3.next = NULL;										    
		p3.tot_len = 0x44;
		p3.len = 0x44;
		p3.payload = &pkt3[0];
		rt_kprintf("Send ARP RESPONSE...\n");
		rt_wifi_tx(NULL, &p3);
		return;
	}

	result = rt_wifi_req_send_cmd_sync(xc, to, &result_p);
	if (result == RT_FALSE) {
	 	rt_kprintf("UART command 0x%x error response: %d\n", 
			cmd_cb.type, result);
	}

	if (result_p != RT_NULL) {
		rt_kprintf("Dumping UART cmd response:\n");
		size = result_p[0] | (result_p[1] << 8);
		for (idx = 0; idx < size; idx++) {
			rt_print_ascii_char(result_p[idx]);
		}
		rt_kprintf("\n");

		rt_free(result_p);
	}
}

/* 
 * Please notice that we asume there are additional
 * 4 bytes before the header of the input buffer
 * pointer (we are doing this to reduce memory copy) 
 */
rt_bool_t rt_wifi_req_send_cmd_sync(u8 *x, int tl, u8 **result_buf)
{
	rt_bool_t result = RT_FALSE;

	if (tl < 8) {
		rt_kprintf("insufficient cmd data size %d\n", tl);
		return result;
	}
	if (SDIO_WaitStatus(SDIO_IO_RDY & SDIO_DL_RDY) == SD_CMD_RSP_TIMEOUT) {
		/* Oops, sdio is not ready, waiting on interrupt */
		sd_waiting_write = RT_TRUE;
		if (rt_sem_take(&sd_write_lock, RT_WAITING_FOREVER) != RT_EOK) {
			sd_waiting_write = RT_FALSE;
			rt_kprintf("Error waiting sd_write_lock semaphore!\n");
			return result;
		}
		sd_waiting_write = RT_FALSE;
	}

	x[-4] = (tl+4) & 0xff;
	x[-3] = ((tl+4) >> 8) & 0xff;
	x[-2] = 1;
	x[-1] = 0;

#ifdef DEBUG_WIFI_CMD									    
	/* debug message */
	rt_kprintf("Now preparing to send cmd: %x %x %x %x %x %x %x %x\n",
		x[-4], x[-3], x[-2], x[-1], x[0], x[1], x[2], x[3]);	
#endif

	/* FIXME
	 * shall we disable interrupt during send cmd and 
	 * flag update?
	 */
    if (SDIO_IORWExtendHelper((u8 *)(x-4), tl+4) != SD_OK) {
		rt_kprintf("Error download command!\n");
        return result;
    }

	/* Next we are waiting on sd_cmd_resp_lock */
	cmd_cb.type = x[0] | (x[1] << 8) | (0x80 << 8);
	cmd_cb.seq = x[4] | (x[5] << 8);
	cmd_cb.result_buf = result_buf;
	//rt_kprintf("Waiting on type 0x%x seq 0x%x\n", cmd_cb.type, cmd_cb.seq);
	sd_waiting_cmd_resp = RT_TRUE;
	if (rt_sem_take(&(cmd_cb.cmd_resp_lock), RT_WAITING_FOREVER) != RT_EOK) {
		sd_waiting_cmd_resp = RT_FALSE;
		rt_kprintf("Error waiting sd_cmd_resp_lock semaphore!\n");
		return result;
	}
	sd_waiting_cmd_resp = RT_FALSE;

	return cmd_cb.result;
}

/* 
 * Please notice that we asume there are additional
 * 4 bytes before the header of the input buffer
 * pointer (we are doing this to reduce memory copy) 
 */
rt_bool_t rt_wifi_send_data_no_wait(txpd_t *txpd_p, struct pbuf *p)
{
	rt_bool_t result = RT_FALSE;

	if (SDIO_WaitStatus(SDIO_IO_RDY & SDIO_DL_RDY) == SD_CMD_RSP_TIMEOUT) {
		/* Oops, sdio is not ready, waiting on interrupt */
		sd_waiting_write = RT_TRUE;
		if (rt_sem_take(&sd_write_lock, RT_WAITING_FOREVER) != RT_EOK) {
			sd_waiting_write = RT_FALSE;
			rt_kprintf("Error waiting sd_write_lock semaphore!\n");
			return result;
		}
		sd_waiting_write = RT_FALSE;
	}

	txpd_p->sdio_tl = sizeof(txpd_t) + p->tot_len;		  
	txpd_p->trans_type = 0x0000;

	/* FIXME
	 * shall we disable interrupt during send cmd and 
	 * flag update?
	 */
    if (SDIO_IORWExtendSendPkt(txpd_p, p) != SD_OK) {
		rt_kprintf("Error download command!\n");
        return result;
    }

	return RT_TRUE;
}

/* Cmd response handler */
static void rt_wifi_handle_cmd(u8 *cmd_buf, u16 size)
{
	u16 type, seq, result;
	u16 idx;

	if (size < 12) {
		rt_kprintf("Error, received insufficient size cmd response\n");
		goto out;
	}
	type = cmd_buf[4] | (cmd_buf[5] << 8);
	seq = cmd_buf[8] | (cmd_buf[9] << 8);
	result = cmd_buf[10] | (cmd_buf[11] << 8);
	//rt_kprintf("Got cmd type 0x%x seq 0x%x result 0x%x\n", type, seq, result);
 	/*
	 * First check if we have cmd request pending on
	 * this response
	 */
	if (sd_waiting_cmd_resp == RT_TRUE) {
		/*
		 * This response should be the reply for
		 * some well known request cmd, double
		 * check the finger print
		 */
		if (type != cmd_cb.type ||
				seq != cmd_cb.seq ||
				result != 0) {
			cmd_cb.result = RT_FALSE;
		} else {
			cmd_cb.result = RT_TRUE;
			if (cmd_cb.result_buf != RT_NULL) {
				*(cmd_cb.result_buf) = cmd_buf;
				cmd_buf = RT_NULL;	
			}
		}
		rt_sem_release(&(cmd_cb.cmd_resp_lock));
	} else {
		/*
		 * received gratuitous command, now
		 * we just print that on UART
		 */
		rt_kprintf("Received unexpected command:\n");
		for (idx = 0; idx < size; idx++) {
			rt_print_ascii_char(cmd_buf[idx]);
		}
		rt_kprintf("\n");
	}
out:
	if (cmd_buf != RT_NULL) {
		rt_free(cmd_buf);
	}
}

/* Data reception handler */
static void rt_wifi_handle_data(u8 *data_buf, u16 size)
{
	u16 idx;
	rxpd_t *p_rx_pd;

	/*
	 * we are receiving data, directly place the
	 * received data on receive fifo
	 */
	/*
	rt_kprintf("rt_wifi_handle_data received data:\n");

	for (idx = 0; idx < size; idx++) {
		rt_print_ascii_char(data_buf[idx]);
	}
	rt_kprintf("\n");
	rt_free(data_buf); 
	*/
	

	/* 
	 * Here we do not need to grab lock,
	 * since there is at least one element
	 * in the fifo, there is no chance that
	 * head and tail can ever see each other
	 */
	p_rx_pd = (rxpd_t *)(data_buf + 4);
	if (p_rx_pd->pkt_len != (size - 4 - p_rx_pd->pkt_ptr)) {
		rt_kprintf("Error: rt_wifi_handle_data received invalid data frame!\n");
	}

	tail->p = pbuf_alloc(PBUF_RAW, p_rx_pd->pkt_len, PBUF_RAM);
	memcpy(tail->p->payload, (u8 *)(((u8 *)p_rx_pd) + p_rx_pd->pkt_ptr), p_rx_pd->pkt_len);
	tail->next = (rt_rx_fifo_t *)rt_malloc(sizeof(rt_rx_fifo_t));
	tail = tail->next;
	tail->next = RT_NULL;
	tail->p = RT_NULL;
	rt_sem_release(&eth_rx_lock);
	rt_free(data_buf); 
}

/* Event handler */
static void rt_wifi_handle_event(u8 *event_buf, u16 size)
{
	u16 idx;
	/* FIXME
	 * Now we just print the message on screen, 
	 * will add notification handler later
	 */
	rt_kprintf("rt_wifi_handle_event received event:\n");
	for (idx = 0; idx < size; idx++) {
		rt_print_ascii_char(event_buf[idx]);
	}
	rt_kprintf("\n");
	rt_free(event_buf);
}

struct pbuf *rt_wifi_rx(rt_device_t dev)
{
	struct pbuf *p;
	rt_rx_fifo_t *rx_p;

	rt_sem_take(&eth_rx_lock, RT_WAITING_FOREVER);

	p = head->p;	 
	rx_p = head;
	head = head->next;
	rx_p->next = RT_NULL;
	rx_p->p = RT_NULL;
	rt_free(rx_p);

	return p;
}
