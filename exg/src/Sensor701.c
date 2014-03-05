/*
 * The external interface to App
 */
//#include "SoftSPIAdc.h"
#include "SoftwareSPI.h"
#include "Sensor701.h"
#include "stm32f10x_exti.h"
#include "exg_data_buffer.h"

static uint8_t spiMode = 0;   // 0 means softwareSPI, 1 means hardwareSPI

uint8_t tempreBuf[21] = {0};
uint8_t pressureBuf[21] = {0};
uint8_t paraBuf[21] = {0};

uint8_t tempreIndex= 0;
uint8_t pressureIndex= 0;
uint8_t paraBufIndex = 0;



uint16_t tempUpdateRate = 25;
uint16_t pressureUpdateRate = 20;
uint8_t restParaNum = 100;
uint8_t allRegMode = 0;
uint8_t gCurChannel = 3;



volatile uint16_t hz = 600;
uint16_t delay_count = 0;

#define TIM_MSEC_DELAY       0x01
#define TIM_USEC_DELAY       0x02

#define TIM_BASE_MODE 200000

void BSP_SetTime(TIM_TypeDef* TIMx, uint8_t unit, uint16_t custom_period);


void RCC_Configuration(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE); 
}


void sensor701Init(void)
{
    //Init RCC
    RCC_Configuration();
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);  

    	/* Enable the TIM2 gloabal Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&NVIC_InitStructure);

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE); 

    delay_count = TIM_BASE_MODE/(2*hz);	/* 100us */
    BSP_SetTime(TIM4, TIM_USEC_DELAY, 55); 

    /* EXTI Interrupt Configuration ----------------------------------------*/
    /* Enable the TIM2 gloabal Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&NVIC_InitStructure);

    //Sensor701's interrupt pin PA0;
    EXTI_InitTypeDef EXTI_InitStructure;
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);
  
    EXTI_InitStructure.EXTI_Line = EXTI_Line0;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    //Some variable
    tempreBuf[0] = 0x85;
    tempreBuf[1] = 0x75;
    tempreBuf[2] = 0x10;
    tempreIndex = 3;
    pressureBuf[0] = 0x85;
    pressureBuf[1] = 0x75;
    pressureBuf[2] = 0x20;
    pressureIndex = 3;
    paraBuf[0] = 0x85;
    paraBuf[1] = 0x75;
    paraBuf[2] = 0x30;
    paraBufIndex = 3;
    
}
/*
 * Read register from sensor
 */

void BSP_SetTime(TIM_TypeDef* TIMx, uint8_t unit, uint16_t custom_period)
{
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
  
  TIM_Cmd(TIMx,DISABLE);
  TIM_ITConfig(TIMx, TIM_IT_Update, DISABLE); 
  
  
  if(unit == TIM_USEC_DELAY)
  {  
    TIM_TimeBaseStructure.TIM_Period = (custom_period != 0) ? custom_period : 11;
  }
  else if(unit == TIM_MSEC_DELAY)
  {
    TIM_TimeBaseStructure.TIM_Period = (custom_period != 0) ? custom_period : 11999;
  }
  
  TIM_TimeBaseStructure.TIM_Prescaler = 5;
  TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  
  TIM_TimeBaseInit(TIMx, &TIM_TimeBaseStructure);
  TIM_ClearITPendingBit(TIMx, TIM_IT_Update);
  
  TIM_ARRPreloadConfig(TIMx, ENABLE);
  
  /* TIM IT enable */
  TIM_ITConfig(TIMx, TIM_IT_Update, ENABLE);
  
  /* TIM2 enable counter */ 
  TIM_Cmd(TIMx, ENABLE);  
  
}

uint8_t sensorReadByte (uint8_t addr)
{
    if (spiMode == 0) {
        return 0; //SPI_read_b(addr);
    } else {
        //return hardwareSPI_read_b(addr);
        return 0;
    }
}

void sensorReadBytes (uint8_t addr, uint8_t len, uint8_t *pBuf)
{
    if (spiMode == 0) {
        //SPI_read(addr, len, pBuf);
    } else {
        return;
    }
}

/*
 * Write register to sensor
 */
void sensorWriteByte (uint8_t addr, uint8_t dat)
{
    if (spiMode == 0) {
        //SPI_write_b(addr, dat);
        if (addr == 0 && dat == 0x81) {
            //spiMode = 1;
            //initSensorSPI();
        }
    }
}

//Init Timer 4
void Senso701_timer_handler (void) 
{
    uint8_t tempre[2] = {0,0};
    uint8_t press[3] = {0,0,0};
    static uint8_t tempReadTick = 0;
    static uint8_t pressReadTick = 0;
    static uint8_t paraReadTick = 0;

    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET) { 
    	        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    	delay_count--; 
    	if (delay_count == 0) {			 
        delay_count = TIM_BASE_MODE/(2*hz);
            tempReadTick++;
            pressReadTick++;
            paraReadTick++;
            if(tempReadTick>tempUpdateRate && gCurChannel==3 && allRegMode==0)
            {
                tempReadTick  = 0;
                Get_tem_spi_701(tempre) ;
                tempreBuf[tempreIndex]= tempre[0];
                tempreIndex++;
                tempreBuf[tempreIndex] = tempre[1];
                tempreIndex++;
                if(tempreIndex >= 21)
                {
                    tempreIndex = 3;
                    rt_kprintf(" 0x%x 0x%x \n",tempre[0], tempre[1]);
                    DBPushBuffer(tempreBuf, 21);
                }
            }

            if(pressReadTick>pressureUpdateRate && gCurChannel<3 && allRegMode==0)
            {
                pressReadTick = 0;
                //Read pressure
                Get_pre_spi_701(press) ;
                //push pressure
                pressureBuf[pressureIndex]= press[0];
                pressureIndex++;
                pressureBuf[pressureIndex] = press[1];
                pressureIndex++;
                pressureBuf[pressureIndex] = press[2];
                pressureIndex++;
                if(pressureIndex >= 21)
                {
                    rt_kprintf(" 0x%x 0x%x 0x%x\n", press[0], press[1], press[2]);
                    pressureIndex = 3;
                    DBPushBuffer(pressureBuf, 21);
                }
            }
            if(paraReadTick > 21 && allRegMode==1)
            {
                paraReadTick = 0;
                if(restParaNum < 64)
                {
                     //Get one parameter;
                     paraBuf[3] = restParaNum;
                    //GetRegData(&(paraBuf[4]), restParaNum);
                    //Test only get one register
                    GetRegData(&(paraBuf[4]), restParaNum);
                    restParaNum++; 
                    DBPushBuffer(paraBuf, 21); 
                    paraBuf[4] = 0;
                }
            }
            if(paraReadTick > 21 && allRegMode == 3)
            {
                paraReadTick= 0;
                paraBuf[3] = restParaNum;
                GetRegData(&(paraBuf[4]), restParaNum);
                DBPushBuffer(paraBuf, 21);
                paraBuf[4] = 0;              
            }
           //rt_hw_led_toggle(0);

    	}				
    }		
}

void Sensor701_Pressure_INT(void)
{
    uint8_t press[3] = {0,0,0};
    uint8_t tempre[2] = {0,0};
    if(EXTI_GetITStatus(EXTI_Line0) != RESET)
    {
        EXTI_ClearITPendingBit(EXTI_Line0);
        if( 1 == paraSetLock )
        {
            return;
        }
        //EXTI_ClearFlag(EXTI_Line0);
        if( allRegMode == 2 )
        {
            rt_hw_led_toggle(0);
            //Read pressure
            if( gCurChannel > 2 )
            {
                Get_tem_spi_701(tempre) ;
                tempreBuf[tempreIndex]= tempre[0];
                tempreIndex++;
                tempreBuf[tempreIndex] = tempre[1];
                tempreIndex++;
                if(tempreIndex >= 21)
                {
                    tempreIndex = 3;
                    DBPushBuffer(tempreBuf, 21);
                }
            }
            else
            {
                Get_pre_spi_701(press) ;
                //push pressure
                pressureBuf[pressureIndex]= press[0];
                pressureIndex++;
                pressureBuf[pressureIndex] = press[1];
                pressureIndex++;
                pressureBuf[pressureIndex] = press[2];
                pressureIndex++;
                if(pressureIndex >= 21)
                {
                    //rt_kprintf(" 0x%x 0x%x 0x%x\n", press[0], press[1], press[2]);
                    pressureIndex = 3;
                    DBPushBuffer(pressureBuf, 21);
                }
            }
        }
    }
}

void SetChannel(uint8_t Ch)
{
    uint8_t readConfig;
        //Setting Channel
    readConfig = 0; //SPI_read_b_test(0x0B);
    readConfig &= ~(0x73);
    gCurChannel = Ch;
    switch(gCurChannel)
    {
        case 0:
            readConfig |=(0x00);
            break;
        case 1:
            readConfig |=(0x01);
            break;
        case 2:
            readConfig |=(0x02);
            break;
        case 3:
            readConfig |=(0x20);
            break;
        default:
            readConfig |=(0x20);
            break;  
    }
    SPI_write_b_test(0x0B, readConfig);    
    //rt_kprintf("Setting Channel 0x%x\n", 0); //SPI_read_b_test(0x0B));
}

void SetGainUdr(uint8_t gain, uint8_t udr)
{
    uint8_t readConfig;
    uint8_t address;
    //Setting gain
    readConfig = 0x80; //SPI_read_b_test(0x0E);
    readConfig &= (~0x7F);
    switch(gain)
    {
        case '0':
            readConfig |=(0x00);
            break;
        case '1':
            readConfig |=(0x10);
            break;
        case '2':
            readConfig |=(0x20);
            break;
        case '3':
            readConfig |=(0x30);
            break;
        case '4':
            readConfig |=(0x40);
            break;
        case '5':
            readConfig |=(0x50);
            break;
        case '6':
            readConfig |=(0x60);
            break;
        case '7':
            readConfig |=(0x70);
            break;
        default:
            break;  
    }
    //Setting Rate
    switch(udr)
    {
        case '0':
            readConfig |=(0x00);
            (gCurChannel>2)?(tempUpdateRate=200):(pressureUpdateRate=100);
            break;
        case '1':
             (gCurChannel>2)?(tempUpdateRate=100):(pressureUpdateRate=50);
            readConfig |=(0x01);
            break;
        case '2':
             (gCurChannel>2)?(tempUpdateRate=50):(pressureUpdateRate=40);
            readConfig |=(0x02);
            break;
        case '3':
             (gCurChannel>2)?(tempUpdateRate=12):(pressureUpdateRate=30);
            readConfig |=(0x03);
            break;
        case '4':
             (gCurChannel>2)?(tempUpdateRate=12):(pressureUpdateRate=25);
            readConfig |=(0x04);
            break;
        case '5':
             (gCurChannel>2)?(tempUpdateRate=6):(pressureUpdateRate=12);
            readConfig |=(0x05);
            break;
        case '6':
             (gCurChannel>2)?(tempUpdateRate=6):(pressureUpdateRate=6);
            readConfig |=(0x06);
            break;
        case '7':
             (gCurChannel>2)?(tempUpdateRate=6):(pressureUpdateRate=3);
            readConfig |=(0x07);
            break;
        case '8':
             (gCurChannel>2)?(tempUpdateRate=6):(pressureUpdateRate=1);
            readConfig |=(0x08);
            break;
        default:
            break;  
    }

    switch(gCurChannel)
    {
        case 1:
            address = 0x0F;
            break;
        case 2:
            address = 0x10;
            break;
        case 0:
        default:
            address = 0x0E;
            break;
    }
    SPI_write_b_test(address, readConfig);
    //rt_kprintf("Setting Gain,Rate 0x%x\n", 0); //SPI_read_b_test(0x0E));
}
