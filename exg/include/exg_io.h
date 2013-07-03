#ifndef __EXG_IO
#define __EXG_IO

#include "stm32f10x_tim.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

void exg_io_init (void);

void exg_set_channel (uint8_t chan_num);

void exg_set_bandwidth (uint8_t bw_num);

void exg_set_gain (uint8_t gain_idx);

void exg_set_start (void);

void exg_set_nstart (void);

void exg_set_mode (uint8_t mode);


void exg_start_32k (void);

void exg_stop_32k (void);

void exg_trigger_start (void);

void exg_start_signal_handler (void);

#endif
