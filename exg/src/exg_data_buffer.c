/*
 * EXG Data Buffer Management Routines
 */
#include "exg_data_buffer.h"

/*
 * DB related static states
 */
static struct exg_db __db[DB_CIRCULAR_BUFFER_DEPTH];
static int16_t __db_seq_num = 0;        // Sequence number for debugging
static int16_t __db_push_idx = 0;       // The next available buffer for PushBuffer
static int32_t __db_stat_in_use_bytes = 0;
static int32_t __db_stat_free_bytes = DB_CIRCULAR_BUFFER_DEPTH * DB_BYTES_PER_BUFFER;
static int32_t __db_stat_overflow_bytes = 0;    // Overflow means the count of bytes
                                                // that cannot insert into buffer

/* 
 * Feature related statistics 
 */
// USB
static int16_t __db_get_idx_usb = -1;   // The next available buffer for Get by USB
static int16_t __db_stat_new_for_usb = 0;
static int16_t __db_stat_empty_req_for_usb = 0;
static int16_t __db_stat_good_req_for_usb = 0;
static int32_t __db_stat_dump_bytes_for_usb = 0;
// TD
static int16_t __db_get_idx_td = -1;    // The next available buffer for Get by Trigger Detection
static int16_t __db_stat_new_for_td = 0;
static int16_t __db_stat_empty_req_for_td = 0;
static int16_t __db_stat_good_req_for_td = 0;
static int32_t __db_stat_dump_bytes_for_td = 0;

/*********************** Static internal utility functions **************************/
/*
 * Announce this buffer is NEW
 */
static void __DBNewBuffer(uint16_t idx)
{
    register rt_base_t 	temp;

    /* disable interrupt */
	temp = rt_hw_interrupt_disable();
    
    if (__db[idx].status != DB_STATUS_BIP || __db[idx].ref_cnt != 0) {
        rt_kprintf("BUG %s line %d\n", __FUNCTION__, __LINE__);
        __db[idx].ref_cnt = 0;
    }
    __db[idx].status = DB_STATUS_NEW;
    __db[idx].seq_num = __db_seq_num;
    __db_seq_num++;
    __db_stat_new_for_usb++;
    __db_stat_new_for_td++;

    /* enable interrupt */
	rt_hw_interrupt_enable(temp);
}

/*
 * Announce this buffer is Build In Progress
 */
static void __DBMarkBufferBIP(uint16_t idx)
{
    register rt_base_t 	temp;

    /* disable interrupt */
	temp = rt_hw_interrupt_disable();
    
    if ((__db[idx].status & DB_STATUS_BIP) || __db[idx].ref_cnt != 0) {
        rt_kprintf("BUG %s line %d\n", __FUNCTION__, __LINE__);
        __db[idx].ref_cnt = 0;
    }
    if (__db[idx].next_buffer_idx != 0) {
        rt_kprintf("BUG %s line %d\n", __FUNCTION__, __LINE__);
        __db[idx].next_buffer_idx = 0;
    }
    __db[idx].status = DB_STATUS_BIP;

    /* enable interrupt */
	rt_hw_interrupt_enable(temp);
}

/*
 * Mark this buffer is referenced by somebody
 */
static void __DBGetBuffer(uint16_t idx, uint8_t owner)
{
    register rt_base_t 	temp;

    /* disable interrupt */
	temp = rt_hw_interrupt_disable();
    
    //if (unlikely((__db[idx].status & owner) == owner)) {
    //    rt_kprintf("BUG %s line %d idx: %d\n", __FUNCTION__, __LINE__, idx);
    //}
    __db[idx].status |= owner;
    if (owner == DB_STATUS_USB) {
        __db_stat_new_for_usb--;
    } else if (owner == DB_STATUS_TD) {
        __db_stat_new_for_td--;
    }
    __db[idx].ref_cnt++;

    /* enable interrupt */
	rt_hw_interrupt_enable(temp);
}

/*
 * Reserve resource: 
 * Dump those resource not hold by features
 */
static uint8_t __DBReserveBuffer(uint8_t uLen)
{
    register rt_base_t 	temp;
    int16_t tmp_idx = 0;       // The next available buffer for PushBuffer
    int8_t uLenCopy = uLen;

    /* disable interrupt */
	temp = rt_hw_interrupt_disable();
    
    // First check all buffers in range are not referenced,
    // otherwise return failure, and the request is marked
    // as buffer overflow
    tmp_idx = __db_push_idx;
    while (uLen > 0) {
        if (__db[tmp_idx].ref_cnt == 0) {
            if (__db[tmp_idx].status & DB_STATUS_BIP) {
                if ((DB_BYTES_PER_BUFFER - __db[tmp_idx].next_buffer_idx) >= uLen) {
                    break;
                } else {
                    uLen -= (DB_BYTES_PER_BUFFER - __db[tmp_idx].next_buffer_idx);
                }
            } else {
                if (uLen >= DB_BYTES_PER_BUFFER) {
                   uLen -= DB_BYTES_PER_BUFFER;
                } else {
                   break;
                } 
            }
        } else {
            return DB_FAILURE;
        }
        tmp_idx = (tmp_idx + 1) % DB_CIRCULAR_BUFFER_DEPTH;
    }

    // Virtual allocation is successful, now do the real
    // reservation, and update statistics
    uLen = uLenCopy;
    tmp_idx = __db_push_idx;
    while (uLen > 0) {
        if (__db[tmp_idx].status & DB_STATUS_BIP) {
            if ((DB_BYTES_PER_BUFFER - __db[tmp_idx].next_buffer_idx) >= uLen) {
                break;
            } else {
                uLen -= (DB_BYTES_PER_BUFFER - __db[tmp_idx].next_buffer_idx);
            }
        } else {
            if (__db[tmp_idx].status & DB_STATUS_NEW) {
                if (!(__db[tmp_idx].status & DB_STATUS_USB)) {
                    __db_stat_dump_bytes_for_usb += DB_BYTES_PER_BUFFER;
                    __db_stat_new_for_usb--;
                    if (__db_get_idx_usb == tmp_idx) {
                        __db_get_idx_usb = (__db_get_idx_usb + 1) % DB_CIRCULAR_BUFFER_DEPTH;
                    }
                }
                if (!(__db[tmp_idx].status & DB_STATUS_TD)) {
                    __db_stat_dump_bytes_for_td += DB_BYTES_PER_BUFFER;
                    __db_stat_new_for_td--;
                    if (__db_get_idx_td == tmp_idx) {
                        __db_get_idx_td = (__db_get_idx_td + 1) % DB_CIRCULAR_BUFFER_DEPTH;
                    }
                }
            }

            // Cleanup state
            __db[tmp_idx].status = 0;
            __db[tmp_idx].next_buffer_idx = 0;
            __db[tmp_idx].seq_num = 0;
            __db_stat_in_use_bytes -= DB_BYTES_PER_BUFFER;
            __db_stat_free_bytes += DB_BYTES_PER_BUFFER;

            if (uLen >= DB_BYTES_PER_BUFFER) {
                uLen -= DB_BYTES_PER_BUFFER;
            } else {
                break;
            } 
        }
        tmp_idx = (tmp_idx + 1) % DB_CIRCULAR_BUFFER_DEPTH;
    }

    /* enable interrupt */
	rt_hw_interrupt_enable(temp);
}

/*
 * Invoked by ADC sampling routine to push
 * EXG generated data
 */
static void __DBPushBuffer(uint8_t *pBuf, uint8_t *ghost_ptr, uint8_t uLen)
{
    uint16_t usTmp = 0;

    if (unlikely(__db[__db_push_idx].ref_cnt != 0)) {
        rt_kprintf("BUG %s line %d\n", __FUNCTION__, __LINE__);
        return;
    }
    if (unlikely(!(__db[__db_push_idx].status & DB_STATUS_BIP))) {
        __DBMarkBufferBIP(__db_push_idx);
    }
    if ((DB_BYTES_PER_BUFFER - __db[__db_push_idx].next_buffer_idx) >= uLen) {
        // The remaining seats in the db enough
        memcpy(&(__db[__db_push_idx].buffer[__db[__db_push_idx].next_buffer_idx]), 
                    pBuf, uLen);
        memcpy(&(__db[__db_push_idx].ghost_buffer[__db[__db_push_idx].next_buffer_idx]), 
                    ghost_ptr, uLen);
        __db_stat_in_use_bytes += uLen;
        __db_stat_free_bytes -= uLen;
        __db[__db_push_idx].next_buffer_idx += uLen;
        if (unlikely(__db[__db_push_idx].next_buffer_idx == DB_BYTES_PER_BUFFER)) {
            // Invoke general GetBuffer
            __DBNewBuffer(__db_push_idx);
            __db_push_idx = (__db_push_idx + 1) % DB_CIRCULAR_BUFFER_DEPTH;
        }
    } else {
        // BUG
        if (unlikely(DB_BYTES_PER_BUFFER <= __db[__db_push_idx].next_buffer_idx)) {
            rt_kprintf("BUG %s line %d\n", __FUNCTION__, __LINE__);
            return;
        }
        usTmp = (DB_BYTES_PER_BUFFER - __db[__db_push_idx].next_buffer_idx);
        memcpy(&(__db[__db_push_idx].buffer[__db[__db_push_idx].next_buffer_idx]), 
                    pBuf, usTmp);
        memcpy(&(__db[__db_push_idx].ghost_buffer[__db[__db_push_idx].next_buffer_idx]), 
                    ghost_ptr, usTmp);
        __db_stat_in_use_bytes += usTmp;
        __db_stat_free_bytes -= usTmp;
        pBuf += usTmp;
        ghost_ptr += usTmp;
        uLen -= usTmp;
        __db[__db_push_idx].next_buffer_idx = DB_BYTES_PER_BUFFER;
        // Invoke general GetBuffer
        __DBNewBuffer(__db_push_idx);
        __db_push_idx = (__db_push_idx + 1) % DB_CIRCULAR_BUFFER_DEPTH;

        // Get next buffer
        if (unlikely(__db[__db_push_idx].ref_cnt != 0 || __db[__db_push_idx].next_buffer_idx != 0)) {
            rt_kprintf("BUG %s line %d\n", __FUNCTION__, __LINE__);
            return;
        }
        __DBPushBuffer(pBuf, ghost_ptr, uLen);
    }
}

/*
 * Executed with lock on
 */
struct exg_db *__DBGetNextBufferForUSB (void)
{
    register rt_base_t 	temp;
    struct exg_db *ret = RT_NULL;

    /* disable interrupt */
	temp = rt_hw_interrupt_disable();
    
    if ((__db[__db_get_idx_usb].status & DB_STATUS_NEW) &&
            !(__db[__db_get_idx_usb].status & DB_STATUS_USB)) {
        __DBGetBuffer(__db_get_idx_usb, DB_STATUS_USB);
        ret = &(__db[__db_get_idx_usb]);
        __db_get_idx_usb = (__db_get_idx_usb + 1) % DB_CIRCULAR_BUFFER_DEPTH;
        __db_stat_good_req_for_usb++;
    } else {
        __db_stat_empty_req_for_usb++;
    }

    /* enable interrupt */
	rt_hw_interrupt_enable(temp);

    return ret;
}

/*
 * Executed with lock on
 */
struct exg_db *__DBGetNextBufferForTD (void)
{
    register rt_base_t 	temp;
    struct exg_db *ret = RT_NULL;

    /* disable interrupt */
	temp = rt_hw_interrupt_disable();
    
    if ((__db[__db_get_idx_td].status & DB_STATUS_NEW) &&
           !(__db[__db_get_idx_td].status & DB_STATUS_TD)) {
        __DBGetBuffer(__db_get_idx_td, DB_STATUS_TD);
        ret = &(__db[__db_get_idx_td]);
        __db_get_idx_td = (__db_get_idx_td + 1) % DB_CIRCULAR_BUFFER_DEPTH;
        __db_stat_good_req_for_td++;
    } else {
        __db_stat_empty_req_for_td++;
    }

    /* enable interrupt */
	rt_hw_interrupt_enable(temp);

    return ret;
}
/***********************          External APIs            **************************/
/*
 * Initialize all EXG Data Buffer staff
 */
void DBInit (void)
{
    memset((void *)__db, 0, (DB_CIRCULAR_BUFFER_DEPTH * sizeof(struct exg_db)));
    //__DBMarkBufferBIP(__db_push_idx);
    __db_get_idx_usb = 0;
    __db_get_idx_td = 0;
}

/*
 * Invoked by USB TX to get the next FULL buffer
 * for USB transaction
 */
struct exg_db *DBGetNextBufferForUSB (void)
{
    if ((__db[__db_get_idx_usb].status & DB_STATUS_NEW) &&
            !(__db[__db_get_idx_usb].status & DB_STATUS_USB)) {
        return __DBGetNextBufferForUSB();
    } else {
        __db_stat_empty_req_for_usb++;
        return RT_NULL;
    }
}

/*
 * Invoked by Trigger Detection to pull more
 * data for analysis
 */
struct exg_db *DBGetNextBufferForTD (void)
{
    if ((__db[__db_get_idx_td].status & DB_STATUS_NEW) &&
           !(__db[__db_get_idx_td].status & DB_STATUS_TD)) {
        return __DBGetNextBufferForTD();
    } else {
        __db_stat_empty_req_for_td++;
        return RT_NULL;
    }
}

/*
 * Manage ref_cnt
 */
void DBPutBuffer (struct exg_db *pDb)
{
    register rt_base_t 	temp;

    /* disable interrupt */
	temp = rt_hw_interrupt_disable();
    
    if (unlikely(pDb->ref_cnt == 0)) {
        rt_kprintf("BUG %s line %d\n", __FUNCTION__, __LINE__);
    } else {
        pDb->ref_cnt--;
        if (pDb->ref_cnt == 0 && (pDb->status & DB_STATUS_USB) &&
                (pDb->status & DB_STATUS_TD)) {
            // Cleanup buffer control states && update statistics
            __db_stat_in_use_bytes -= DB_BYTES_PER_BUFFER;
            __db_stat_free_bytes += DB_BYTES_PER_BUFFER;
            pDb->status = 0;
            pDb->next_buffer_idx = 0;
            pDb->seq_num = 0;
        }
    }

    /* enable interrupt */
	rt_hw_interrupt_enable(temp);
}

/*
 * Invoked by ADC sampling routine to push
 * EXG generated data
 */
void DBPushBuffer (uint8_t *pBuf, uint8_t uLen)
{
    // First reserve resource
    uint8_t ghost_info[18] = {0x84, 0xB0,
                              0x04, 0xB0,
                              0x04, 0xB0,
                              0x04, 0xB0,
                              0x04, 0xB0,
                              0x04, 0xB0,
                              0x04, 0xB0,
                              0x04, 0xB0,
                              0x00, 0x00};

    if (unlikely(__db_stat_free_bytes < uLen)) {
        if (unlikely(uLen > DB_MAX_SIZE)) {
            rt_kprintf("ERROR: request size %d exceed max buffer capacity %d\n",
                    uLen, DB_MAX_SIZE);
            __db_stat_overflow_bytes += uLen;
            return;
        }
        if (__DBReserveBuffer(uLen) != DB_SUCCESS) {
            __db_stat_overflow_bytes += uLen;
            return;
        } 
    }
    __DBPushBuffer(pBuf, ghost_info, uLen);
}

/*
 * Dump internal statistics
 */
void DBDebugStatistics (void)
{
    rt_kprintf("__db_seq_num: %d\n", __db_seq_num);
    rt_kprintf("__db_push_idx: %d\n", __db_push_idx);
    rt_kprintf("__db_get_idx_usb: %d\n", __db_get_idx_usb);
    rt_kprintf("__db_get_idx_td: %d\n", __db_get_idx_td);
    rt_kprintf("__db_stat_in_use_bytes: %d\n", __db_stat_in_use_bytes);
    rt_kprintf("__db_stat_free_bytes: %d\n", __db_stat_free_bytes);
    rt_kprintf("__db_stat_overflow_bytes: %d\n", __db_stat_overflow_bytes);
    rt_kprintf("__db_stat_dump_bytes_for_usb: %d\n", __db_stat_dump_bytes_for_usb);
    rt_kprintf("__db_stat_dump_bytes_for_td: %d\n", __db_stat_dump_bytes_for_td);
    rt_kprintf("__db_stat_new_for_usb: %d\n", __db_stat_new_for_usb);
    rt_kprintf("__db_stat_new_for_td: %d\n", __db_stat_new_for_td);
    rt_kprintf("__db_stat_empty_req_for_usb: %d\n", __db_stat_empty_req_for_usb);
    rt_kprintf("__db_stat_empty_req_for_td: %d\n", __db_stat_empty_req_for_td);
    rt_kprintf("__db_stat_good_req_for_usb: %d\n", __db_stat_good_req_for_usb);
    rt_kprintf("__db_stat_good_req_for_td: %d\n", __db_stat_good_req_for_td);
}

/*
 * Invoked by ADC sampling routine for debug
 * purpose
 */
void DBPushBufferDbg (uint8_t *pBuf, uint8_t uLen)
{

}

