/*
 * Software SPI interface to work with pressure sensor
 */
#ifndef __SOFTWARE_SPI__
#define __SOFTWARE_SPI__

#include <rtthread.h>
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"


#define CS_PORT     GPIOB
#define CS_PIN       GPIO_Pin_12
#define SCK_PORT   GPIOB
#define SCK_PIN      GPIO_Pin_13
#define SDA_PORT   GPIOB
#define SDA_PIN      GPIO_Pin_15
#define NOTUSED_PORT GPIOB
#define NOTUSED_PIN  GPIO_Pin_14

#define SD_PORT     GPIOA
#define SD_PIN        GPIO_Pin_1
#define S701INT_PORT    GPIOA
#define S701INT_PIN       GPIO_Pin_0

#define CS_HIGH()   GPIO_SetBits(CS_PORT, CS_PIN)
#define CS_LOW()    GPIO_ResetBits(CS_PORT, CS_PIN)
#define SCK_HIGH()  GPIO_SetBits(SCK_PORT, SCK_PIN)
#define SCK_LOW()   GPIO_ResetBits(SCK_PORT, SCK_PIN)
#define SDA_HIGH()  GPIO_SetBits(SDA_PORT, SDA_PIN)
#define SDA_LOW()   GPIO_ResetBits(SDA_PORT, SDA_PIN)
#define R_SDA()       GPIO_ReadInputDataBit(SDA_PORT,SDA_PIN)

void softwareSPIInit (void);
void delay_L(uint32_t n);
void SDA_R(void);
void SDA_W(void);

void SPI_int(void);
uint8_t SPI_rcvb(void);
uint8_t SPI_rcvb_test (void);
void SPI_sendb(uint8_t outData);
void SPI_sendb_test (uint8_t outData);

void SPI_write(uint8_t addr,uint8_t len,uint8_t *P);
void SPI_read(uint8_t addr,uint8_t len,uint8_t *P);
uint8_t SPI_read_b(uint8_t addr);
uint8_t SPI_read_b_test(uint8_t addr);
void SPI_write_b(uint8_t addr,uint8_t dat);
void SPI_write_b_test (uint8_t addr,uint8_t dat);

void Get_bri_spi_701(uint8_t *P);
void Get_pre_spi_701(uint8_t *P);
void Get_tem_spi_701(uint8_t* P);

void testIOHighLow(void);


#endif
