
/*
 * EXG IO control implementation
 */

#include "exg_io.h"
//#include "stm32_hcd.h"
#include "misc.h"
#include "stm32f10x_adc.h"


#define TIM_MSEC_DELAY       0x01
#define TIM_USEC_DELAY       0x02

static uint16_t delay_count = 0;
extern uint16_t adc_current;
#define BW0Port GPIOA
#define BW1Port GPIOA
#define BW2Port GPIOA
#define STARTPort GPIOA
#define CLK32Port  GPIOA
#define MODEPort GPIOA
#define GAIN0Port GPIOA
#define GAIN1Port GPIOA
#define MUX0Port GPIOB
#define MUX1Port GPIOB
#define MUX2Port GPIOB
#define MUX3Port GPIOB

#define BW0Pin GPIO_Pin_2
#define BW1Pin GPIO_Pin_1
#define BW2Pin GPIO_Pin_0
#define STARTPin GPIO_Pin_3
#define CLK32Pin GPIO_Pin_4
#define MODEPin GPIO_Pin_5
#define GAIN0Pin GPIO_Pin_6
#define GAIN1Pin GPIO_Pin_7
#define MUX0Pin GPIO_Pin_8
#define MUX1Pin GPIO_Pin_9
#define MUX2Pin GPIO_Pin_6
#define MUX3Pin GPIO_Pin_7

///////////////////////////////////////////////////////////////////////////////
//                     I n i t i a l i z a t i o n                           //
///////////////////////////////////////////////////////////////////////////////

static void RCC_Configuration (void)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    RCC_APB2PeriphClockCmd((RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO), ENABLE);
    /* Enable timer6 start delay */
    /* Enable the TIM6 gloabal Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = TIM6_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&NVIC_InitStructure);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);	

    /* Enable timer5 for 32K signal */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);
}

static void GPIO_Configuration (void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* EXG_CLK32K maps to PA4, connects to TIM5CH4 */
    //BW2    PA0
    //BW1    PA1
    //BW0    PA2
    //START  PA3
    //CLK32  PA4
    //MODE   PA5
    //GAIN0  PA6
    //GAIN1  PA7
    GPIO_InitStructure.GPIO_Pin = (uint16_t)(BW0Pin|BW1Pin|BW2Pin|STARTPin|CLK32Pin|MODEPin|GAIN0Pin|GAIN1Pin);
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    //MUX0   PB8
    //MUX1   PB9
    //MUX2   PB6
    //MUX3   PB7
    GPIO_InitStructure.GPIO_Pin = (uint16_t)(MUX0Pin|MUX1Pin|MUX2Pin|MUX3Pin);
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

}

void exg_io_init (void)
{
    RCC_Configuration();
    GPIO_Configuration();
}

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

///////////////////////////////////////////////////////////////////////////////
//                        I / O    S e t t i n g s                           //
///////////////////////////////////////////////////////////////////////////////
/*
 * EXG_MUX(2,1,0) --> channel number
 *      000       -->       1
 *      001       -->       2
 *      010       -->       3
 *      011       -->       4
 *      100       -->       5
 *      101       -->       6
 *      110       -->       7
 *      111       -->       8
 */
void exg_set_channel (uint8_t chan_num)
{
    //Insure chan_num<4
    //set channel pair;
    //(0,4)(1,5)(2,6)(3,7)
    (chan_num&0x1)?GPIO_SetBits(MUX0Port, MUX0Pin):GPIO_ResetBits(MUX0Port, MUX0Pin);
    (chan_num&0x1)?GPIO_SetBits(MUX2Port, MUX2Pin):GPIO_ResetBits(MUX2Port, MUX2Pin);
    (chan_num&0x2)?GPIO_SetBits(MUX1Port, MUX1Pin):GPIO_ResetBits(MUX1Port, MUX1Pin);
    (chan_num&0x2)?GPIO_SetBits(MUX3Port, MUX3Pin):GPIO_ResetBits(MUX1Port, MUX1Pin);
}

void exg_set_bandwidth (uint8_t bw_num)
{
    (bw_num&0x1)?GPIO_SetBits(BW0Port, BW0Pin):GPIO_ResetBits(BW0Port, BW0Pin);
    (bw_num&0x2)?GPIO_SetBits(BW1Port, BW1Pin):GPIO_ResetBits(BW1Port, BW1Pin);
    (bw_num&0x4)?GPIO_SetBits(BW2Port, BW2Pin):GPIO_ResetBits(BW2Port, BW2Pin);
}

void exg_set_gain (uint8_t gain_idx)
{
    (gain_idx&0x1)?GPIO_SetBits(GAIN0Port, GAIN0Pin):GPIO_ResetBits(GAIN0Port, GAIN0Pin);
    (gain_idx&0x2)?GPIO_SetBits(GAIN1Port, GAIN1Pin):GPIO_ResetBits(GAIN1Port, GAIN1Pin);
}

void exg_set_mode (uint8_t mode)
{
    (mode&0x1)?GPIO_SetBits(MODEPort, MODEPin):GPIO_ResetBits(MODEPort, MODEPin);
}

void exg_set_start (void)
{
    GPIO_SetBits(STARTPort, STARTPin);
}

void exg_set_nstart (void)
{
    GPIO_ResetBits(STARTPort, STARTPin);
}

void exg_green_on (void)
{
    return;
}

int red_status = 0;
void exg_red_flip (void)
{
    return;
}

void exg_trigger_start (void)
{
    exg_set_start();
    delay_count = 50; 	// 50ms delay
    //Msec delay
    BSP_SetTime(TIM6, TIM_MSEC_DELAY, 0);
}

/* TIM5CLK: 72MHz */
void exg_start_32k (void)
{
    /* Does not support 32K */
    return;
    TIM_OCInitTypeDef TIM_OCInitStruct;
    /*
     * clock selection:
     * internal clock source
     */
    TIM_InternalClockConfig(TIM5);

    /* Auto Reload Register */
    TIM_SetAutoreload(TIM5, 0x0011);	 

    /* 
     * Capture/Compare Register 
     * this register value is the target
     * to compare against timer counter
     */
    TIM_SetCompare4(TIM5, 0x0008);

    /* setup prescaler */
    TIM_PrescalerConfig(TIM5, 63, 0);

    // TIM_SelectOCxM(TIM5, TIM_Channel_4, TIM_OCMode_Toggle);

    TIM_OCStructInit(&TIM_OCInitStruct);
    /* 
     * Capture/Compare mode: 
     * (1) channel configured as output
     * (2) output toggle on match
     */
    TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_Toggle;
    TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStruct.TIM_Pulse = 0x0008;

    TIM_OC4Init(TIM5, &TIM_OCInitStruct);

    /* Enable timer */
    TIM_Cmd(TIM5, ENABLE);
}

void exg_stop_32k (void)
{
    /* Does not support 32K */
    return;
    TIM_Cmd(TIM5, DISABLE);
}

///////////////////////////////////////////////////////////////////////////////
//                 I n t e r r u p t    H a n d l e r                        //
///////////////////////////////////////////////////////////////////////////////
void exg_start_signal_handler (void)
{
    if (TIM_GetITStatus(TIM6, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM6, TIM_IT_Update);
        delay_count--;
        if (delay_count ==0) {
            /* pull start signal low */
            exg_set_nstart();
            TIM_Cmd(TIM6, DISABLE);
        }	
    }		
}
