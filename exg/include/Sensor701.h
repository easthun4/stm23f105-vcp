/*
 * The external interface to App
 */
#ifndef __SENSOR_SPI_H__
#define __SNESOR_SPI_H__


#include <rtthread.h>
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "usbd_cdc_vcp.h"
#include "Misc.h"
#include "led.h"


/*
 * Read register from sensor
 */
void sensor701Init(void);

uint8_t sensorReadByte (uint8_t addr);
void sensorReadBytes (uint8_t addr, uint8_t len, uint8_t *pBuf);

/*
 * Write register to sensor
 */
void sensorWriteByte (uint8_t addr, uint8_t dat);

/*
 * Interrupt service
 */
 void Senso701_timer_handler (void) ;
void Sensor701_Pressure_INT(void);

void SetChannel(uint8_t Ch);
void SetGainUdr(uint8_t gain, uint8_t udr);


//Exported variable
extern uint16_t tempUpdateRate;
extern uint16_t pressureUpdateRate;
extern uint8_t restParaNum;
extern uint8_t allRegMode;

#endif
