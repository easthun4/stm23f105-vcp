/*
 * Software SPI interface to work with pressure sensor
 */
#ifndef __SOFTSPI_ADC__
#define __SOFTSPI_ADC__

#include <rtthread.h>
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"


#define CS_PORT     GPIOB
#define CS_PIN       GPIO_Pin_12
#define SCK_PORT   GPIOB
#define SCK_PIN      GPIO_Pin_13
#define MOSI_PORT   GPIOB
#define MOSI_PIN      GPIO_Pin_15
#define MISO_PORT GPIOB
#define MISO_PIN  GPIO_Pin_14

#define SD_PORT     GPIOA
#define SD_PIN        GPIO_Pin_1
#define S701INT_PORT    GPIOA
#define S701INT_PIN       GPIO_Pin_0

#define CS_HIGH()   GPIO_SetBits(CS_PORT, CS_PIN)
#define CS_LOW()    GPIO_ResetBits(CS_PORT, CS_PIN)
#define SCK_HIGH()  GPIO_SetBits(SCK_PORT, SCK_PIN)
#define SCK_LOW()   GPIO_ResetBits(SCK_PORT, SCK_PIN)
#define MOSI_HIGH()  GPIO_SetBits(MOSI_PORT, MOSI_PIN)
#define MOSI_LOW()   GPIO_ResetBits(MOSI_PORT, MOSI_PIN)
#define READ_MISO()       GPIO_ReadInputDataBit(MISO_PORT,MISO_PIN)
#define RESET_HIGH()  GPIO_SetBits(S701INT_PORT, S701INT_PIN)
#define RESET_LOW()   GPIO_ResetBits(S701INT_PORT, S701INT_PIN)

void softwareSPIInit (void);
void delay_L(uint32_t n);

void SPI_sendAdr(uint8_t adr, uint8_t r_w);
uint8_t SPI_recvByte (void);
void SPI_sendByte (uint8_t outData);
void SPI_write (uint8_t adr,uint8_t dat);
uint8_t SPI_read(uint8_t addr);

void Get_bri_spi_701(uint8_t *P);
void Get_pre_spi_701(uint8_t *P);
void Get_tem_spi_701(uint8_t* P);
void GetRegData(uint8_t *P, uint8_t adr);
void SetRegData(uint8_t adr, uint8_t val);



#endif
