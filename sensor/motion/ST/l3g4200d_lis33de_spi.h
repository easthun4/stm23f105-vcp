/***************************************************
 *
 * SPI interface for motion sensor L3G4200D/LIS33DE
 *
 ***************************************************/

#ifndef __SENSOR_MOTION_L3G4200D_SPI__
#define __SENSOR_MOTION_L3G4200D_SPI__

#include <rtdef.h>
#include <rtthread.h>
#include "misc.h"
#include <stm32f10x.h>
#include <stm32f10x_spi.h>
#include <stm32f10x_gpio.h>
#include <stm32f10x_rcc.h>
#include <stm32f10x_exti.h>
#include <stm32f10x_dma.h>

/*
 * L3G4200D registers
 */
#define GYRO_CTRL_REG1	0x20
#define GYRO_CTRL_REG2	0x21
#define GYRO_CTRL_REG3	0x22
#define GYRO_CTRL_REG4	0x23
#define	GYRO_CTRL_REG5	0x24
#define GYRO_OUT_X_L	0x28
#define GYRO_OUT_X_H	0x29
#define	GYRO_OUT_Y_L	0x2A
#define	GYRO_OUT_Y_H	0x2B
#define	GYRO_OUT_Z_L	0x2C
#define	GYRO_OUT_Z_H	0x2D


/*
 * LIS33DE write accessible registers
 */
#define ACLE_CTRL_REG1	0x20
#define ACLE_CTRL_REG2	0x21
#define ACLE_CTRL_REG3	0x22
#define ACLE_FF_WU_CFG	0x30
#define	ACLE_FF_WU_THS	0x32
#define ACLE_FF_WU_DURATION	0x33

#define ACLE_WRITABLE(addr) (addr == ACLE_CTRL_REG1 ||	  \
								addr == ACLE_CTRL_REG2 || \
								addr == ACLE_CTRL_REG3 || \
								addr == ACLE_FF_WU_CFG || \
								addr == ACLE_FF_WU_THS || \
								addr == ACLE_FF_WU_DURATION)

void motion_sensor_init (void);
void l3g4200d_isr (void);
void lis33de_isr (void);
void write_gyro_reg (rt_uint8_t addr, rt_uint8_t val);
void write_acle_reg (rt_uint8_t addr, rt_uint8_t val);
rt_uint8_t read_gyro_reg (rt_uint8_t addr, rt_uint8_t len,
							rt_uint8_t *data);
rt_uint8_t read_acle_reg (rt_uint8_t addr, rt_uint8_t len,
							rt_uint8_t *data);
rt_uint16_t gyro_read_data (rt_uint8_t dir);

#endif