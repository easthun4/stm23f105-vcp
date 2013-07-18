/*
 * EXG Data Buffer related routines
 */
#ifndef __EXG_DATA_BUFFER__
#define __EXG_DATA_BUFFER__

#include "stm32f10x.h"
#include "rtthread.h"

#define DB_CIRCULAR_BUFFER_DEPTH 32
#define DB_BYTES_PER_BUFFER 21
#define DB_MAX_SIZE (DB_BYTES_PER_BUFFER * DB_CIRCULAR_BUFFER_DEPTH)
#define DB_SUCCESS 0
#define DB_FAILURE 1
// Reference related flags
#define DB_STATUS_NEW       0x01
#define DB_STATUS_USB       0x02
#define DB_STATUS_TD        0x04
#define DB_STATUS_BIP       0x80    // Build In Progress 

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#pragma pack(1)
struct exg_db {		 
    uint8_t ref_cnt;            // 0 for free buffer, call Get/PutBuffer to manage
    uint8_t status;             // Keep track of feature related information (Trigger Detection)
	uint16_t next_buffer_idx;   // The index of next available seat in buffer
	uint16_t seq_num;           // Debug
	uint8_t buffer[DB_BYTES_PER_BUFFER];
	uint8_t ghost_buffer[DB_BYTES_PER_BUFFER];
};
#pragma pack()

/*
 * Initialize all EXG Data Buffer staff
 */
void DBInit (void);

/*
 * Invoked by USB TX to get the next FULL buffer
 * for USB transaction
 */
struct exg_db *DBGetNextBufferForUSB (void);

/*
 * Invoked by Trigger Detection to pull more
 * data for analysis
 */
struct exg_db *DBGetNextBufferForTD (void);

/*
 * Manage ref_cnt
 */
void DBPutBuffer (struct exg_db *pDb);

/*
 * Invoked by ADC sampling routine to push
 * EXG generated data
 */
void DBPushBuffer (uint8_t *pBuf, uint8_t uLen);

/*
 * Dump internal statistics
 */
void DBDebugStatistics (void);

/*
 * Invoked by ADC sampling routine for debug
 * purpose
 */
void DBPushBufferDbg (uint8_t *pBuf, uint8_t uLen);

#endif
