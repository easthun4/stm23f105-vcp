/*
 * AD7988 converter logic implementation
 */

#include "ad7947.h"
#include "stm32_hcd.h"
#include "exg_api.h"

ad7947_callback *adc_handler = RT_NULL;
static uint16_t delay_count;
extern volatile int hz;
extern volatile int hz_config;

adc_buffer_t *adc_buffer_filling = RT_NULL;
adc_buffer_t *adc_buffer_sending = RT_NULL;
int adc_buffer_skip_send = 0;
int send_count = 0;
int fill_count = 0;
extern int usb_connection_open;

///////////////////////////////////////////////////////////////////////////////
//                     I n i t i a l i z a t i o n                           //
///////////////////////////////////////////////////////////////////////////////

static void enable_cs (void)	  
{
	GPIOC->BSRR = GPIO_BSRR_BR6;
}

static void disable_cs (void)
{
	GPIOC->BSRR = GPIO_BSRR_BS6;
}

static void RCC_Configuration (void)
{
    /* enable SPI2 clock */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    /* enable gpiob port clock */
    RCC_APB2PeriphClockCmd((RCC_APB2Periph_GPIOC | 
							RCC_APB2Periph_GPIOB | 
							RCC_APB2Periph_AFIO), ENABLE);
}

static void GPIO_Configuration (void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

	/* PC6 as ADC_CNV */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* Configure SPI2 pins:  SCK, MISO */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	  
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

static void SPI_Configuration (void)
{
    SPI_InitTypeDef SPI_InitStructure;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_16b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &SPI_InitStructure);

	disable_cs();

    SPI_Cmd(SPI2, ENABLE);
}
					   
void ad7947_timer_handler (void) 
{
	if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET) { 
    	TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
		delay_count--; 
		if (delay_count == 0) {			 
			delay_count = 1000000/(2*hz);
        	/* Trigger sample if handler is defined */        
			if (adc_handler != RT_NULL) {
				adc_handler(0);
			}
		}				
	}												  
}

static void ad7947_sample_timer_init (ad7947_callback *ad_handler)
{
  	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);  
	
	adc_handler = ad_handler;

  	/* Enable the TIM2 gloabal Interrupt */
  	NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  
  	NVIC_Init(&NVIC_InitStructure);
  
  	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE); 
	
	delay_count = 1000000/(2*hz);	/* 100us */
	BSP_SetTime(TIM4, TIM_USEC_DELAY, 0);  	
}


void ad7947_init (ad7947_callback *ad_handler)
{
	RCC_Configuration();
	GPIO_Configuration();
	SPI_Configuration();
	ad7947_sample_timer_init(ad_handler);
}

unsigned short ad7947_sample (void)	   
{
	unsigned short ret = 0;

	enable_cs();

	SPI_I2S_SendData(SPI2, 0x55AA);
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE)!=SET);
	ret = SPI_I2S_ReceiveData(SPI2);

	disable_cs();

	return ret;
}		  

