/*
 * Internal ADC converter logic implementation
 */

#include "AdInternal.h"
#include "Misc.h"
#include "exg_api.h"
#include "exg_io.h"
#include "Led.h"
#include "exg_data_buffer.h"

extern volatile int hz;
extern volatile int hz_config;
uint32_t ADC_DualConvertedValueTab[16]={0};

#define ADC1_DR_Address    ((uint32_t)0x4001244C)
static uint8_t gChannelId=0;
uint32_t adc_sample_counter = 0;


///////////////////////////////////////////////////////////////////////////////
//                     I n i t i a l i z a t i o n                           //
///////////////////////////////////////////////////////////////////////////////
static void RCC_Configuration (void)
{
    /* Enable ADC1 clock, configure IN12 as regular channel */
    RCC_ADCCLKConfig(RCC_PCLK2_Div4);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1 ,ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE); 
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE); 
    /* Enable ADC1, ADC2 and GPIOC clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_ADC2 | RCC_APB2Periph_GPIOC, ENABLE);
}


static void GPIO_Configuration (void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    /* Configure PC.01, PC.02 and PC.04 (ADC Channel11, Channel12 and Channel14)
      as analog input ----------------------------------------------------------*/
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_4 |GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOC, &GPIO_InitStructure)  ;
}

#define USINGTIM3
void AdInter_SmplTimer_Configuration()
{

 #ifdef USINGTIM4
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_OCInitTypeDef  TIM_OCInitStructure;

    /* TIM4 Peripheral Configuration ----------------------------------------*/
    /* TIM4 Slave Configuration: PWM1 Mode */
    TIM_TimeBaseStructure.TIM_Period = 3600;
    TIM_TimeBaseStructure.TIM_Prescaler = 9;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Timing;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_Pulse = 0x1;
    TIM_OC4Init(TIM4, &TIM_OCInitStructure);
    
    TIM_ITConfig(TIM4, TIM_IT_CC4, ENABLE);
    //TIM_ITConfig(TIM4, TIM_IT_Trigger, ENABLE);

    NVIC_InitTypeDef NVIC_InitStructure;

    /* Enable the TIM2 gloabal Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&NVIC_InitStructure);

    /* TIM1 counter enable */
    TIM_Cmd(TIM4, ENABLE);
#elif defined(USINGTIM3)
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;
    
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE);
    
    TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStruct.TIM_Period = 3600;
    TIM_TimeBaseInitStruct.TIM_Prescaler = 9;
    TIM_TimeBaseInit(TIM3,&TIM_TimeBaseInitStruct);
    
    TIM_SelectOutputTrigger(TIM3,TIM_TRGOSource_Update); //选择TRGO触发源为计时器更新时间
    
//    TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE);
//    NVIC_InitStruct.NVIC_IRQChannel = TIM3_IRQn;
//    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0;
//    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 3;
//    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
//    NVIC_Init(&NVIC_InitStruct);
    TIM_Cmd(TIM3,ENABLE);



#endif
}

void AdInter_DMA_Configuration()
{
    DMA_InitTypeDef DMA_InitStructure;
    
    /* DMA1 channel1 configuration ----------------------------------------------*/
    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)ADC1_DR_Address;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)ADC_DualConvertedValueTab;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = 4;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
    /* Enable DMA1 Channel1 */
    DMA_Cmd(DMA1_Channel1, ENABLE);
}


void  AdInter_ADC_Configuration()
{
    NVIC_InitTypeDef NVIC_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;
    /* ADC1 configuration ------------------------------------------------------*/
    ADC_InitStructure.ADC_Mode = ADC_Mode_RegSimult;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T3_TRGO;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);
    /* ADC1 regular channels configuration */ 
    ADC_RegularChannelConfig(ADC1, ADC_Channel_14, 1, ADC_SampleTime_239Cycles5);    
    //ADC_RegularChannelConfig(ADC1, ADC_Channel_17, 2, ADC_SampleTime_239Cycles5);
    /* Enable ADC1 DMA */
    ADC_DMACmd(ADC1, ENABLE);
    ADC_ExternalTrigConvCmd(ADC1, ENABLE);
    
    /* ADC2 configuration ------------------------------------------------------*/
    ADC_InitStructure.ADC_Mode = ADC_Mode_RegSimult;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC2, &ADC_InitStructure);
    /* ADC2 regular channels configuration */ 
    ADC_RegularChannelConfig(ADC2, ADC_Channel_15, 1, ADC_SampleTime_239Cycles5);
    //ADC_RegularChannelConfig(ADC2, ADC_Channel_12, 2, ADC_SampleTime_239Cycles5);
    /* Enable ADC2 external trigger conversion */
    ADC_ExternalTrigConvCmd(ADC2, ENABLE);
    
    /* Enable ADC1 */
    ADC_Cmd(ADC1, ENABLE);
    /* Enable Vrefint channel17 */
    ADC_TempSensorVrefintCmd(ENABLE);
    
    /* Enable ADC1 reset calibration register */   
    ADC_ResetCalibration(ADC1);
    /* Check the end of ADC1 reset calibration register */
    while(ADC_GetResetCalibrationStatus(ADC1));
    
    /* Start ADC1 calibration */
    ADC_StartCalibration(ADC1);
    /* Check the end of ADC1 calibration */
    while(ADC_GetCalibrationStatus(ADC1));
    
    /* Enable ADC2 */
    ADC_Cmd(ADC2, ENABLE);
    
    /* Enable ADC2 reset calibration register */   
    ADC_ResetCalibration(ADC2);
    /* Check the end of ADC2 reset calibration register */
    while(ADC_GetResetCalibrationStatus(ADC2));
    
    /* Start ADC2 calibration */
    ADC_StartCalibration(ADC2);
    /* Check the end of ADC2 calibration */
    while(ADC_GetCalibrationStatus(ADC2));

    
    
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
      /* Enable Transfer complete interrupt */
    DMA_ITConfig(DMA1_Channel1 , DMA1_IT_TC1, ENABLE);
    /* Enable DMA1 Channel1 */
    DMA_Cmd(DMA1_Channel1, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = ADC1_2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
      /* Enable Transfer complete interrupt */
    ADC_ITConfig(ADC2, ADC_IT_EOC, ENABLE);
}

void AdInter_init (void)
{
    RCC_Configuration();
    GPIO_Configuration();
    AdInter_SmplTimer_Configuration();
    AdInter_DMA_Configuration();
    AdInter_ADC_Configuration();
      
	//AdInter_sample_timer_init(ad_handler);
}

void AD_timer_handler (void) 
{
#ifdef USINGTIM4
    if (TIM_GetITStatus(TIM4, TIM_IT_CC4) != RESET) 
    { 
    	 TIM_ClearITPendingBit(TIM4, TIM_IT_CC4);
         rt_hw_led_toggle(0);
          /* Start ADC1 Software Conversion */
          ADC_SoftwareStartConvCmd(ADC1, ENABLE);
          //TIM_GenerateEvent(TIM4, TIM_EventSource_CC4);
    }
#elif defined(USINGTIM3)
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) 
    { 
    	 TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
         //rt_hw_led_toggle(0);
          /* Start ADC1 Software Conversion */
          //ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    }
#endif
   
    return;
}

void AD_SmplFinish_handler (void)
{
    ADC_ClearITPendingBit(ADC2, ADC_IT_EOC);
    rt_hw_led_toggle(1);
    gChannelId++; 
    exg_set_channel(gChannelId);
    return;
}

void AD_DMA_Handler(void)
{   
    uint8_t *pBuf = RT_NULL;
    uint16_t *sample = 0;

    DMA_ClearFlag(DMA1_FLAG_TC1);
    adc_sample_counter++;
    rt_hw_led_toggle(0);
     gChannelId = 0;
     exg_set_channel(gChannelId);
    ADC_DualConvertedValueTab[4] = 0;
    sample = (uint16_t *)ADC_DualConvertedValueTab;
    *sample = ((*sample & 0xFF00) >> 8) | ((*sample & 0x00FF) << 8);
    sample++;
    *sample = ((*sample & 0xFF00) >> 8) | ((*sample & 0x00FF) << 8);
    sample++;
    *sample = ((*sample & 0xFF00) >> 8) | ((*sample & 0x00FF) << 8);
    sample++;
    *sample = ((*sample & 0xFF00) >> 8) | ((*sample & 0x00FF) << 8);
    sample++;
    *sample = ((*sample & 0xFF00) >> 8) | ((*sample & 0x00FF) << 8);
    sample++;
    *sample = ((*sample & 0xFF00) >> 8) | ((*sample & 0x00FF) << 8);
    sample++;
    *sample = ((*sample & 0xFF00) >> 8) | ((*sample & 0x00FF) << 8);
    sample++;
    *sample = ((*sample & 0xFF00) >> 8) | ((*sample & 0x00FF) << 8);
    
    pBuf = (uint8_t *)ADC_DualConvertedValueTab;
    *pBuf = *pBuf | 0x80;
    DBPushBuffer((uint8_t *)ADC_DualConvertedValueTab, 18);
    return;
}

