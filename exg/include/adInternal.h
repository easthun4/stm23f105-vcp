/*
 * AD7988 converter control logic header file
 * ===========================================
 * IO connections:
 * ADC_CNV:			PA4
 * ADC_SCK:			PA5
 * ADC_SDO:			PA6
 */

#ifndef __AD_INTERNAL__
#define __AD_INTERNAL__

#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_dma.h"
#include <rtthread.h>
#include "exg_api.h"

#define SAMPLES_PER_BUFFER 27
#define CIRCULAR_BUFFER_DEPTH 100
/* skip send threshold */
#define CIRCULAR_BUFFER_THRESHOLD 5

void AdInter_init (void);

void AD_timer_handler (void);

void AD_SmplFinish_handler (void);

void AD_DMA_Handler(void);




#endif
