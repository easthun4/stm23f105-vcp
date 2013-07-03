/*
 * AD7988 converter control logic header file
 * ===========================================
 * IO connections:
 * ADC_CNV:			PA4
 * ADC_SCK:			PA5
 * ADC_SDO:			PA6
 */

#ifndef __AD7988
#define __AD7988

#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_spi.h"
#include <rtthread.h>
#include "exg_api.h"

#define SAMPLES_PER_BUFFER 27
#define CIRCULAR_BUFFER_DEPTH 100
/* skip send threshold */
#define CIRCULAR_BUFFER_THRESHOLD 5

#pragma pack(1)
typedef struct adc_buffer {		 
	struct adc_buffer *next;
	short cur_idx;
	exg_msg_t msg_hdr;
	short seq_num;	
	short channel_num;
	short sample_per_sec;	// number of samples per second
	short samples[SAMPLES_PER_BUFFER];
} adc_buffer_t;
#pragma pack()

typedef void (ad7988_callback) (uint16_t input);

typedef void (AdInter_callback) (uint16_t input);

void AD_timer_handler (void);

void AdInter_init (AdInter_callback *ad_handler);

unsigned short AdInter_sample (void);

adc_buffer_t *AdInter_get_free_buf (void);

adc_buffer_t *AdInter_get_send_buf (void);

#endif
