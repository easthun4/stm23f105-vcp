/*
 * ECG QRS detection && auto trigger logic
 */
#include "auto_trigger.h"
#include "exg_data_buffer.h"
#include "exg_io.h"

//status flags
extern uint32_t adc_sample_counter;
static uint32_t __ad_last_qrs[8] = {0};
static uint32_t __ad_last_virtual_trigger[8] = {0};
static uint32_t __ad_last_trigger = 0;
static uint32_t __ad_stat_qrs_count[8] = {0};
static uint16_t __ad_dbg_sample[8] = {0};
static uint8_t  __ad_qrs_on[8] = {0};
static uint8_t  __ad_qrs_count[8] = {0};

static int filter[AT_MAX_CHANNEL], time[AT_MAX_CHANNEL] = {0}, slopecrit[AT_MAX_CHANNEL], 
    sign[AT_MAX_CHANNEL], maxslope[AT_MAX_CHANNEL] = {0},  
    nslope[AT_MAX_CHANNEL] = {0}, qtime[AT_MAX_CHANNEL], maxtime[AT_MAX_CHANNEL], 
    t0[AT_MAX_CHANNEL], t1[AT_MAX_CHANNEL], t2[AT_MAX_CHANNEL], t3[AT_MAX_CHANNEL], 
    t4[AT_MAX_CHANNEL], t5[AT_MAX_CHANNEL], t6[AT_MAX_CHANNEL], t7[AT_MAX_CHANNEL], 
    t8[AT_MAX_CHANNEL], t9[AT_MAX_CHANNEL], ms160[AT_MAX_CHANNEL], ms200[AT_MAX_CHANNEL], 
    s2[AT_MAX_CHANNEL], scmax[AT_MAX_CHANNEL], scmin[AT_MAX_CHANNEL] = {0}, at_started[AT_MAX_CHANNEL] = {0};

uint8_t __ATOneSample (uint8_t idx, uint16_t uSample)
{
    uint8_t isQRS = 0;

    __ad_dbg_sample[idx] = uSample;
    if (unlikely(at_started[idx] == 0)) {
        t9[idx] = t8[idx] = t7[idx] = t6[idx] = t5[idx] = t4[idx] = t3[idx] = t2[idx] = t1[idx] = uSample;
    }
    at_started[idx] = 1;
    filter[idx] = (t0[idx] = uSample) + 4*t1[idx] + 6*t2[idx] + 4*t3[idx] + t4[idx]
        - t5[idx] - 4*t6[idx] - 6*t7[idx] - 4*t8[idx] - t9[idx];
    if (time[idx] % s2[idx] == 0) {
        if (nslope[idx] == 0) {
            slopecrit[idx] -= slopecrit[idx] >> 4;
            if (slopecrit[idx] < scmin[idx]) slopecrit[idx] = scmin[idx];
        }
        else if (nslope[idx] >= 5) {
            slopecrit[idx] += slopecrit[idx] >> 4;
            if (slopecrit[idx] > scmax[idx]) slopecrit[idx] = scmax[idx];
        }
    }
    if (nslope[idx] == 0 && abs(filter[idx]) > slopecrit[idx]) {
        nslope[idx] = 1; maxtime[idx] = ms160[idx];
        sign[idx] = (filter[idx] > 0) ? 1 : -1;
        qtime[idx] = time[idx];
    }
    if (nslope[idx] != 0) {
        if (filter[idx] * sign[idx] < -slopecrit[idx]) {
            sign[idx] = -sign[idx];
            maxtime[idx] = (++(nslope[idx]) > 4) ? ms200[idx] : ms160[idx];
        }
        else if (filter[idx] * sign[idx] > slopecrit[idx] &&
                abs(filter[idx]) > maxslope[idx])
            maxslope[idx] = abs(filter[idx]);
        if ((maxtime[idx])-- < 0) {
            if (2 <= nslope[idx] && nslope[idx] <= 4) {
                slopecrit[idx] += (((maxslope[idx])>>2) - slopecrit[idx]) >> 3;
                if (slopecrit[idx] < scmin[idx]) slopecrit[idx] = scmin[idx];
                else if (slopecrit[idx] > scmax[idx]) slopecrit[idx] = scmax[idx];
                //annot.time = strtim("i") - (time[idx] - qtime[idx]) - 4;
                //annot.anntyp = NORMAL; (void)putann(0, &annot);
                time[idx] = 0;
                isQRS = 1; 
            }
            else if (nslope[idx] >= 5) {
                //annot.time = strtim("i") - (time - qtime) - 4;
                //annot.anntyp = ARFCT; (void)putann(0, &annot);
            }
            nslope[idx] = 0;
        }
    }
    t9[idx] = t8[idx]; t8[idx] = t7[idx]; t7[idx] = t6[idx]; t6[idx] = t5[idx]; t5[idx] = t4[idx];
    t4[idx] = t3[idx]; t3[idx] = t2[idx]; t2[idx] = t1[idx]; t1[idx] = t0[idx]; (time[idx])++;

    return isQRS;
}

void __ATOffline (void)
{
    int i;

    for (i = 0; i < 8; i++) {
        if (__ad_qrs_on[i] > 0) {
            return;
        }
    }
    if ((adc_sample_counter - __ad_last_trigger) > AT_TRIGGER_INTERVAL) {
        rt_kprintf("All channel offline, trigger start\n");
        exg_trigger_start();
        __ad_last_trigger = adc_sample_counter;
    }
}

void __ATAnalyzeOneChannel (uint8_t channel, uint16_t sample)
{
    uint8_t ret = 0;

    ret = __ATOneSample(channel, sample);
    if (ret == 1) {
        //rt_kprintf("Found QRS on channel %d: %d\n", channel, (adc_sample_counter - __ad_last_qrs[channel]));
        __ad_stat_qrs_count[channel]++;
        if ((adc_sample_counter - __ad_last_qrs[channel]) > AT_MIN_SAMPLE_COUNTER_INTERVAL &&
            (adc_sample_counter - __ad_last_qrs[channel]) < AT_MAX_SAMPLE_COUNTER_INTERVAL) {
            if (__ad_qrs_on[channel] == 0) {
                __ad_qrs_count[channel]++;
                if (__ad_qrs_count[channel] >= AT_ONLINE_THRESHOLD) {
                    rt_kprintf("channel %d online\n", channel);
                    __ad_qrs_on[channel] = 1;
                    __ad_qrs_count[channel] = 0;
                }
            }
        } else {
            __ad_qrs_count[channel] = 0;
        }
        __ad_last_qrs[channel] = adc_sample_counter;
    } else {
        if ((adc_sample_counter - __ad_last_qrs[channel]) > AT_QRS_TIMEOUT &&
            (adc_sample_counter - __ad_last_virtual_trigger[channel]) > AT_TRIGGER_INTERVAL) {
            rt_kprintf("channel %d didn't see QRS, timeout\n", channel);
            __ad_qrs_on[channel] = 0;
            __ad_qrs_count[channel] = 0;
            __ad_last_virtual_trigger[channel] = adc_sample_counter;
            __ATOffline();
        }
    }
}

void ATCheckOneBuffer (void)
{
    static uint8_t in_sync = 0;
    static uint16_t count = 0;
    struct exg_db *pDB = RT_NULL;
    static uint8_t high_val = 0;
    static uint8_t low_val = 0;
    uint16_t idx = 0;
    uint8_t *pBuf = RT_NULL;
    uint8_t ret = 0;

    // First get next buffer
    do {
        pDB = DBGetNextBufferForTD();
    } while (pDB == RT_NULL);

    // Second 
    if (unlikely(in_sync == 0)) {
        pBuf = pDB->buffer;
        idx = 0;
        while (1) {
            if ((pBuf[0] == 0x00) && (pBuf[1] == 0x00) && (pBuf[2] & 0x80)) {
                break;
            }
            pBuf++;
            idx++;
            if (idx > DB_BYTES_PER_BUFFER - 3) {
                rt_kprintf("BUG at %s line %d\n", __FUNCTION__, __LINE__);
                return;
            }
        }
        in_sync = 1;
        count = 0;    
    }

    // Third get the data
    // idx points to the start position
    // count is the continuous count
    pBuf = pDB->buffer;
    while (idx < DB_BYTES_PER_BUFFER) {        
        if (count == 2) {
            if (!(pBuf[idx] & 0x80)) {
                rt_kprintf("start resync\n");
                in_sync = 0;
                goto exit;
            }
            high_val = pBuf[idx] & 0x7F;
        } else if (count == 3) {
            low_val = pBuf[idx];
        } else if (count == 4) {
            __ATAnalyzeOneChannel(0, ((high_val) << 8 | low_val));
            high_val = pBuf[idx];
        } else if (count == 5) {
            low_val = pBuf[idx];
        } else if (count == 6) {
            __ATAnalyzeOneChannel(1, ((high_val) << 8 | low_val));
            high_val = pBuf[idx];
        } else if (count == 7) {
            low_val = pBuf[idx];
        } else if (count == 8) {
            __ATAnalyzeOneChannel(2, ((high_val) << 8 | low_val));
            high_val = pBuf[idx];
        } else if (count == 9) {
            low_val = pBuf[idx];
        } else if (count == 10) {
            __ATAnalyzeOneChannel(3, ((high_val) << 8 | low_val));
            high_val = pBuf[idx];
        }
        idx++;
        count = (count + 1) % 36;
    }
exit:
    DBPutBuffer(pDB);
}

void ATInit (void)
{
    uint32_t idx = 0;

    for (idx = 0; idx < AT_MAX_CHANNEL; idx++) {
        scmin[idx] = 200;
        scmax[idx] = 10 * scmin[idx];
        slopecrit[idx] = scmax[idx];
        ms160[idx] = 40;
        ms200[idx] = 50;
        s2[idx] = 500;
        at_started[idx] = 0;
    }
}

void ATDebugStatistics (void)
{
    rt_kprintf("__ad_stat_qrs_count[0]: %d\n", __ad_stat_qrs_count[0]);
    rt_kprintf("__ad_stat_qrs_count[1]: %d\n", __ad_stat_qrs_count[1]);
    rt_kprintf("__ad_stat_qrs_count[2]: %d\n", __ad_stat_qrs_count[2]);
    rt_kprintf("__ad_stat_qrs_count[3]: %d\n", __ad_stat_qrs_count[3]);
    rt_kprintf("__ad_stat_qrs_count[4]: %d\n", __ad_stat_qrs_count[4]);
    rt_kprintf("__ad_stat_qrs_count[5]: %d\n", __ad_stat_qrs_count[5]);
    rt_kprintf("__ad_stat_qrs_count[6]: %d\n", __ad_stat_qrs_count[6]);
    rt_kprintf("__ad_stat_qrs_count[7]: %d\n", __ad_stat_qrs_count[7]);
}

uint8_t ATIsOnline (void)
{
    int i = 0;
    for (i = 0; i < 8; i++) {
        if (__ad_qrs_on[i] > 0) {
            return 1;
        }
    }
    return 0;
}  