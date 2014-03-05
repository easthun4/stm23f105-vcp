/*
 * Software SPI interface to work with pressure sensor
 */
#include "SoftSPIAdc.h"
#include "Misc.h"
#include "led.h"



void delay_L(uint32_t n) 
{
    uint32_t i;
    for (i = 0; i < n; i++) {
	}
}

/*
 * Initialize IO port
 */
 
static void RCC_Configuration (void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
}

void SoftwareSPI_Configuration (void)
{ 
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = (uint16_t)(CS_PIN | SCK_PIN | MOSI_PIN);
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = (uint16_t)(MISO_PIN);
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(MISO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = (uint16_t)(SD_PIN);
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(SD_PORT, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = (uint16_t)(S701INT_PIN);
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(S701INT_PORT, &GPIO_InitStructure);


// GPIO_ResetBits(MISO_PORT, MISO_PIN);
//    GPIO_SetBits(SD_PORT,SD_PIN);    
//    delay_L(1000);
    CS_LOW();
    delay_L(1000);
    MOSI_LOW();
    SCK_LOW();
    GPIO_ResetBits(SD_PORT,SD_PIN);

}

void softwareSPIInit()
{
    RCC_Configuration();
    SoftwareSPI_Configuration();
}

void SPI_sendAdr(uint8_t adr, uint8_t r_w)
{
    uint8_t i = 0;
    uint8_t outData = 0;
    if(1 == r_w)
    {
        outData =  ((adr&0x07)<<1) | 0x01;
    }
    else
    {
        outData = (adr&0x07)<<1;
    }
    
    for (i=0; i<4; i++)
    {
        if((outData>>i)&0x01)
        {
            MOSI_HIGH();
        }
        else
        {
            MOSI_LOW();
        }
        delay_L(10);
        SCK_HIGH();
        delay_L(5);
        SCK_LOW();
        delay_L(10);
    }
    MOSI_LOW();
    delay_L(10);
}

uint8_t SPI_recvByte (void)
{
    uint8_t i = 0, inData = 0;
    
    delay_L(10);
    for(i=0; i<8; i++)
    {
        SCK_HIGH();
        delay_L(10);
        inData >>= 1;
        if(READ_MISO())
        {
            inData |= 0x80;
        }
        else
        {
            inData &= (~0x80);
        }
        SCK_LOW();
        delay_L(10);
    }
    SCK_HIGH();
    delay_L(10);
    SCK_LOW();
    return(inData);
}


void SPI_sendByte (uint8_t outData)
{
    uint8_t i = 0;
    
    delay_L(10);
    for (i=0; i<8; i++)
    {
        if((outData>>i)&0x01)
        {
            MOSI_HIGH();
        }
        else
        {
            MOSI_LOW();
        }
        delay_L(10);
        SCK_HIGH();
        delay_L(5);
        SCK_LOW();
        delay_L(10);
    }
    MOSI_LOW();
    delay_L(10);
}

void SPI_write (uint8_t adr,uint8_t dat)
{
    CS_HIGH();
    delay_L(10);
    // -----------
    SPI_sendAdr(adr, 0);    //write
    SPI_sendByte(dat);
    // -----------
    CS_LOW();  
    SCK_HIGH();
    delay_L(5);
    SCK_LOW();
}

uint8_t SPI_read(uint8_t addr)
{
    uint8_t dat;
    CS_HIGH();
    delay_L(10);
    // -----------
    SPI_sendAdr(addr, 1);     //read
    dat = SPI_recvByte();
    // -----------
    CS_LOW();   
    SCK_HIGH();
    delay_L(10);
    SCK_LOW();
    return (dat);
}

void Get_bri_spi_701 (uint8_t *P)
{
    *P = 0;
}

void Get_pre_spi_701 (uint8_t *P)
{
    *P = 0;
}


//Received buffer should at least 2 bytes
void Get_tem_spi_701 (uint8_t *P)
{
    *P = 0;
}

void GetRegData(uint8_t *P, uint8_t adr)
{
    *P = SPI_read(adr);
}

void SetRegData(uint8_t adr, uint8_t val)
{
    SPI_write(adr, val);
}





