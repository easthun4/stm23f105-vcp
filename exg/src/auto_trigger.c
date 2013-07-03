/*
 * ECG QRS detection && auto trigger logic
 */
#include "auto_trigger.h"

uint8_t __ATOneSample (uint8_t idx, uint16_t uSample)
{
    static int filter[AT_MAX_CHANNEL], time[AT_MAX_CHANNEL] = {0}, slopecrit[AT_MAX_CHANNEL], 
               sign[AT_MAX_CHANNEL], maxslope[AT_MAX_CHANNEL] = {0},  
               nslope[AT_MAX_CHANNEL] = {0}, qtime[AT_MAX_CHANNEL], maxtime[AT_MAX_CHANNEL], 
               t0[AT_MAX_CHANNEL], t1[AT_MAX_CHANNEL], t2[AT_MAX_CHANNEL], t3[AT_MAX_CHANNEL], 
               t4[AT_MAX_CHANNEL], t5[AT_MAX_CHANNEL], t6[AT_MAX_CHANNEL], t7[AT_MAX_CHANNEL], 
               t8[AT_MAX_CHANNEL], t9[AT_MAX_CHANNEL], ms160[AT_MAX_CHANNEL], ms200[AT_MAX_CHANNEL], 
               s2[AT_MAX_CHANNEL], scmax[AT_MAX_CHANNEL], scmin[AT_MAX_CHANNEL] = {0};
    uint8_t isQRS = 0;

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
