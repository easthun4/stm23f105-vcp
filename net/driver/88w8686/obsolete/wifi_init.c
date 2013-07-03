/*******************************************************
 * WIFI initialization routine
 *
 *
 *******************************************************/

#include "wifi_init.h"

extern struct rt_semaphore sd_read_lock, sd_write_lock;
extern rt_wifi_cmd_cb_t cmd_cb;
extern struct rt_semaphore rt_wifi_tx_lock;

void rt_wifi_init()
{
	rt_thread_t wifi_resp_thread;
	rt_thread_t wifi_main_thread;

	// sd_read_lock/sd_write_lock should have been 
	// initialized in rt_hw_sdcard_init, and please
	// make sure rt_wifi_init is placed after 
	// rt_hw_sdcard_init.

	/*
	 * initialize cmd response lock, work in the 
	 * similar way as previous lock
	 */
	rt_sem_init(&(cmd_cb.cmd_resp_lock), "sd_cmd_resp_lock", 0, 0);

	/*
	 * initialize packet send lock, allow concurrently
	 * no more than one thread to invoke wifi_tx
	 */
	rt_sem_init(&(rt_wifi_tx_lock), "rt_wifi_tx_lock", 1, 0);
	 
	/* initialize wifi response thread */
	wifi_resp_thread = rt_thread_create("wifi_resp_thread",
								rt_wifi_resp_thread_entry, RT_NULL,
								2048, 8, 20);

	if (wifi_resp_thread != RT_NULL) {
		rt_thread_startup(wifi_resp_thread);
	}

	/* initialize wifi main thread */
	wifi_main_thread = rt_thread_create("wifi_main_thread",
								wifi_main_entry, RT_NULL,
								2048, 8, 20);

	if (wifi_main_thread != RT_NULL) {
		rt_thread_startup(wifi_main_thread);
	}
}
