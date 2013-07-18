/*
 * ECG QRS detection && auto trigger logic
 */
#ifndef __AUTO_TRIGGER__
#define __AUTO_TRIGGER__

#include "stm32f10x.h"

#define AT_MAX_CHANNEL 8
#define abs(A)  ((A) >= 0 ? (A) : -(A))
// sample rate is 500Hz
#define AT_MIN_SAMPLE_COUNTER_INTERVAL  100
#define AT_MAX_SAMPLE_COUNTER_INTERVAL  1500
#define AT_QRS_TIMEOUT 2000
#define AT_TRIGGER_INTERVAL 3000
//#define AT_ACCUMULATE_MIN_THRESHOLD 5
//#define AT_ACCUMULATE_MAX_THRESHOLD 5
#define AT_ONLINE_THRESHOLD 2

/*
 * Invoked by application thread
 */
void ATCheckOneBuffer (void);

/*
 * Dump internal statistics
 */
void ATDebugStatistics (void);

/*
 * Initialize static variables
 */
void ATInit (void);

/*
 * Notify USB about online status
 */
uint8_t ATIsOnline (void);

#endif
