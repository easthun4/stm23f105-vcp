/******************************************
 * Marvell 88W8686 firmware API header file
 *
 ******************************************/

#ifndef __8686_FW_API__
#define __8686_FW_API__

#include <rtthread.h>
#include <rthw.h>
#include "sdcard.h"

enum mv_ms_type {
    MVMS_DAT = 0, 
    MVMS_CMD = 1,
    MVMS_TXDONE = 2,
    MVMS_EVENT
};         

/* 
 * WIFI command control block, used to
 * keep track of the command type, and
 * synchronize between sender/receiver 
 * threads
 */
typedef struct rt_wifi_cmd_cb {
	struct rt_semaphore cmd_resp_lock;	/* sync between sender/receiver threads */
	u16 type;
	u16 seq;
	rt_bool_t result;
	u8** result_buf;
} rt_wifi_cmd_cb_t;

typedef struct rt_rx_fifo {
	struct pbuf *p;
	struct rt_rx_fifo *next;
} rt_rx_fifo_t;

/* RxPD Descriptor */
typedef struct rxpd {
    /* union to cope up with later FW revisions */
    union {
        /* Current Rx packet status */
        u16 status;
        struct {
            /* BSS type: client, AP, etc. */
            u8 bss_type;
            /* BSS number */
            u8 bss_num;
        } bss;
    } u;

    /* SNR */
    u8 snr;

    /* Tx control */
    u8 rx_control;

    /* Pkt length */
    u16 pkt_len;

    /* Noise Floor */
    u8 nf;

    /* Rx Packet Rate */
    u8 rx_rate;

    /* Pkt addr */
    u32 pkt_ptr;

    /* Next Rx RxPD addr */
    u32 next_rxpd_ptr;

    /* Pkt Priority */
    u8 priority;
    u8 reserved[3];
} rxpd_t;

void rt_wifi_resp_thread_entry(void* parameter);
void rt_wifi_uart_req_send_sync(u8 *x, int tl);
rt_bool_t rt_wifi_req_send_cmd_sync(u8 *x, int tl, u8 **result_buf);
rt_bool_t rt_wifi_send_data_no_wait(txpd_t *txpd_p, struct pbuf *p);
static void rt_wifi_handle_cmd(u8 *cmd_buf, u16 size);
static void rt_wifi_handle_data(u8 *data_buf, u16 size);
static void rt_wifi_handle_event(u8 *event_buf, u16 size);
struct pbuf *rt_wifi_rx(rt_device_t dev);

#endif
