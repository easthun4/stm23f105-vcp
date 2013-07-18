/*
 * Software SPI interface to work with pressure sensor
 */
#include "SoftwareSPI.h"
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
    GPIO_InitStructure.GPIO_Pin = (uint16_t)(CS_PIN | SCK_PIN | SDA_PIN);
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
     GPIO_InitStructure.GPIO_Pin = (uint16_t)(NOTUSED_PIN);
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(NOTUSED_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = (uint16_t)(SD_PIN);
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(SD_PORT, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = (uint16_t)(S701INT_PIN);
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(S701INT_PORT, &GPIO_InitStructure);


    GPIO_ResetBits(NOTUSED_PORT, NOTUSED_PIN);
//    GPIO_SetBits(SD_PORT,SD_PIN);    
//    delay_L(1000);
    GPIO_SetBits(CS_PORT, CS_PIN);
    delay_L(1000);
    GPIO_ResetBits(SDA_PORT, SDA_PIN);
    GPIO_ResetBits(SCK_PORT, SCK_PIN);
    GPIO_ResetBits(SD_PORT,SD_PIN);

}

void softwareSPIInit()
{
    RCC_Configuration();
    SoftwareSPI_Configuration();
}

void SDA_R()
{
    /*
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStructure.GPIO_Pin = (uint16_t)(SDA_PIN);
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    */
    uint32_t tmpreg = SDA_PORT->CRH;
    tmpreg &= ~(0xF0000000);
    tmpreg |= 0x40000000;
    SDA_PORT->CRH = tmpreg;
}

void SDA_W()
{
    /*
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = (uint16_t)(SDA_PIN);
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(SDA_PORT, &GPIO_InitStructure);
    */
        uint32_t tmpreg = SDA_PORT->CRH;
    tmpreg &= ~(0xF0000000);
    tmpreg |=    0x30000000;
    SDA_PORT->CRH = tmpreg;
}

uint8_t SPI_rcvb (void)
{
    uint8_t i = 0, inData = 0;
    SDA_R();
    delay_L(20);
    for(i=0; i<8; i++)
    {
        inData <<= 1;
        inData |= R_SDA();
        delay_L(20);
        SCK_HIGH();
        delay_L(20);
        SCK_LOW();
        delay_L(20);
    }
    SDA_W();
    delay_L(40);
    //SDA_LOW();
    return(inData);
}

void SPI_sendb (uint8_t outData)
{
    uint8_t i = 0;
    SDA_W();
    delay_L(20);
    for (i=0; i<8; i++)
    {
        if((outData<<i)&0x80)
        {
            SDA_HIGH();
        }
        else
        {
            SDA_LOW();
        }
        delay_L(20);
        SCK_HIGH();
        delay_L(20);
        SCK_LOW();
        delay_L(20);
    }
    //SDA_LOW();
    //SDA_R();
    delay_L(40);
}

uint8_t SPI_rcvb_test (void)
{
    uint8_t i = 0, inData = 0;
    SDA_R();
    delay_L(20);
    for(i=0; i<8; i++)
    {
        inData <<= 1;
        inData |= R_SDA();
        delay_L(40);
        SCK_HIGH();
        delay_L(10);
        SCK_LOW();
        delay_L(40);
    }
    delay_L(40);
    return(inData);
}


void SPI_sendb_test (uint8_t outData)
{
    uint8_t i = 0;
    SDA_W();
    delay_L(20);
    for (i=0; i<8; i++)
    {
        if((outData<<i)&0x80)
        {
            SDA_HIGH();
        }
        else
        {
            SDA_LOW();
        }
        delay_L(40);
        SCK_HIGH();
        delay_L(10);
        if(i==7)
        {
            SDA_R(); 
        }
        SCK_LOW();
        delay_L(40);
    }
    //SDA_LOW();
    //SDA_R();
    delay_L(40);
}

void SPI_write (uint8_t addr,uint8_t len,uint8_t *P)
{
    uint8_t i;
    CS_LOW();
    delay_L(20);
    for(i=0; i<len; i++)
    {
        SPI_sendb(0x00);
        SPI_sendb(addr+i);
        SPI_sendb(*P);
        P++;
    }
    CS_HIGH();
    delay_L(20);
}

void SPI_read (uint8_t addr,uint8_t len,uint8_t *P)
{
    uint8_t i;
    CS_LOW();
    delay_L(20);
    for(i=0; i<len; i++)
    {
        SPI_sendb(0x80);
        SPI_sendb(addr+i);
        *P=SPI_rcvb();
        //*P= SPI_rcvb_test();
        P++;
    }
    CS_HIGH();
    delay_L(20);
}
uint8_t SPI_read_b (uint8_t addr)
{
    uint8_t dat;
    CS_LOW();
    delay_L(20);
    SPI_sendb(0x80);
    SPI_sendb(addr);
    dat=SPI_rcvb();
    CS_HIGH();
    delay_L(20);
    return (dat);
}

void SPI_write_b (uint8_t addr,uint8_t dat)
{
    CS_LOW();
    delay_L(20);
    SPI_sendb(0x00);
    SPI_sendb(addr);
    SPI_sendb(dat);
    CS_HIGH();
    delay_L(20);
}

uint8_t SPI_read_b_test(uint8_t addr)
{
    uint8_t dat;
    CS_LOW();
    delay_L(20);
    SPI_sendb_test(0x80);
    SPI_sendb_test(addr);
    dat=SPI_rcvb();
    delay_L(20);
    CS_HIGH();   
    return (dat);
}

void SPI_write_b_test (uint8_t addr,uint8_t dat)
{
    CS_LOW();
    delay_L(20);
    SPI_sendb_test(0x00);
    SPI_sendb_test(addr);
    SPI_sendb_test(dat);
    delay_L(20);
    CS_HIGH();   
}

void Get_bri_spi_701 (uint8_t *P)
{
    uint8_t m;
    m=SPI_read_b(0x0B);
    m=m&0x1F;
    m=m|0x10;
    SPI_write_b(0x0B,m);
	/*
    while(((IOA&0x02)==0)&&(i>10))
       i--;
    if((IOA&0x02)!=0)
    {
       Read_spi(P);
    }
    else {
		ret = 1;
	}
	*/
	return;
}

void Get_pre_spi_701 (uint8_t *P)
{
    *P = SPI_read_b_test(0x11);
    *(P+1) = SPI_read_b_test(0x12);
    *(P+2) = SPI_read_b_test(0x13);
}


//Received buffer should at least 2 bytes
void Get_tem_spi_701 (uint8_t *P)
{
    uint8_t m;
    m=SPI_read_b_test(0x0B);
    m=m&0x2F;
    m=m|0x20;
    SPI_write_b_test(0x0B,m);
    //delay_L(50);

    *P = SPI_read_b_test(0x14);  //Temp High
    //delay_L(50);
    *(P+1) = SPI_read_b_test(0x15);  //Temp Low
    

//    while(((IOA&0x02)==0)&&(i>10))
//       i--;
//    if((IOA&0x02)!=0)
//    {
//       
//    }
//    else {
//       ret = 1;
//	}
//	return;
}

void testIOHighLow(void)
{
    SDA_R();
    while(1)
    {
        if(GPIO_ReadInputDataBit(S701INT_PORT,S701INT_PIN))
        {
            rt_hw_led_on(0); 
        }
        else
        {
            rt_hw_led_off(0);
        }
    }
    SDA_W();
}

