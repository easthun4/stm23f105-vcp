/**
  ******************************************************************************
  * @file    Project/Template/stm32f10x_it.c 
  * @author  MCD Application Team
  * @version V3.1.0
  * @date    06/19/2009
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @copy
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2009 STMicroelectronics</center></h2>
  */ 

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"
#include "stm32f10x_exti.h"
#include <board.h>
#include <rtthread.h>
#include <stm32f10x_gpio.h>
#include "usb_hcd_int.h"
#include "usb_dcd_int.h"

//#include "usbh_core.h"
#include "usbd_core.h"
#include "stm32_adb.h"
#include "ad7947.h"
#include "adInternal.h"
#include "exg_io.h"
#include "Sensor701.h"

/** @addtogroup Template_Project
  * @{
  */
extern USB_OTG_CORE_HANDLE           USB_OTG_dev;

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief   This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f10x_xx.s).                                            */
/******************************************************************************/

/*******************************************************************************
* Function Name  : DMA1_Channel2_IRQHandler
* Description    : This function handles DMA1 Channel 2 interrupt request.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void DMA1_Channel2_IRQHandler(void)
{
#ifdef RT_USING_UART3
    extern struct rt_device uart3_device;
	extern void rt_hw_serial_dma_tx_isr(struct rt_device *device);

    /* enter interrupt */
    rt_interrupt_enter();

    if (DMA_GetITStatus(DMA1_IT_TC2))
    {
        /* transmission complete, invoke serial dma tx isr */
        rt_hw_serial_dma_tx_isr(&uart3_device);
    }

    /* clear DMA flag */
    DMA_ClearFlag(DMA1_FLAG_TC2 | DMA1_FLAG_TE2);

    /* leave interrupt */
    rt_interrupt_leave();
#endif
}

/*******************************************************************************
* Function Name  : USART1_IRQHandler
* Description    : This function handles USART1 global interrupt request.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void USART1_IRQHandler(void)
{
#ifdef RT_USING_UART1
    extern struct rt_device uart1_device;
	extern void rt_hw_serial_isr(struct rt_device *device);
	
    /* enter interrupt */
    rt_interrupt_enter();

    rt_hw_serial_isr(&uart1_device);

    /* leave interrupt */
    rt_interrupt_leave();
#endif
}

/*******************************************************************************
* Function Name  : USART2_IRQHandler
* Description    : This function handles USART2 global interrupt request.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void USART2_IRQHandler(void)
{
#ifdef RT_USING_UART2
    extern struct rt_device uart2_device;
	extern void rt_hw_serial_isr(struct rt_device *device);

    /* enter interrupt */
    rt_interrupt_enter();

    rt_hw_serial_isr(&uart2_device);

    /* leave interrupt */
    rt_interrupt_leave();
#endif
}

/*******************************************************************************
* Function Name  : USART3_IRQHandler
* Description    : This function handles USART3 global interrupt request.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void USART3_IRQHandler(void)
{
#ifdef RT_USING_UART3
    extern struct rt_device uart3_device;
	extern void rt_hw_serial_isr(struct rt_device *device);

    /* enter interrupt */
    rt_interrupt_enter();

    rt_hw_serial_isr(&uart3_device);

    /* leave interrupt */
    rt_interrupt_leave();
#endif
}

/*******************************************************************************
* Function Name  : UART4_IRQHandler
* Description    : This function handles USART1 global interrupt request.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void UART4_IRQHandler(void)
{
#ifdef RT_USING_UART4
    extern struct rt_device uart4_device;
	extern void rt_hw_serial_isr(struct rt_device *device);
	
    /* enter interrupt */
    rt_interrupt_enter();

    rt_hw_serial_isr(&uart4_device);

    /* leave interrupt */
    rt_interrupt_leave();
#endif
}

/*******************************************************************************
* Function Name  : SDIO_IRQHandler
* Description    : This function handles SDIO global interrupt request.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void SDIO_IRQHandler(void)
{
    extern int SD_ProcessIRQSrc(void);

    /* enter interrupt */
    rt_interrupt_enter();

    /* Process All SDIO Interrupt Sources */
    //SD_ProcessIRQSrc();

    /* leave interrupt */
    rt_interrupt_leave();
}

#ifdef RT_USING_LWIP
#ifdef STM32F10X_CL
/*******************************************************************************
* Function Name  : ETH_IRQHandler
* Description    : This function handles ETH interrupt request.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void ETH_IRQHandler(void)
{
	extern void rt_hw_stm32_eth_isr(void);
	
    /* enter interrupt */
    rt_interrupt_enter();
	
	//rt_hw_stm32_eth_isr();

    /* leave interrupt */
    rt_interrupt_leave();
}
#else
#if (STM32_ETH_IF == 0)
/*******************************************************************************
* Function Name  : EXTI0_IRQHandler
* Description    : This function handles External interrupt Line 0 request.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void EXTI0_IRQHandler(void)
{
    extern void enc28j60_isr(void);

    /* enter interrupt */
    rt_interrupt_enter();

    //enc28j60_isr();
    
    /* Clear the Key Button EXTI line pending bit */
    EXTI_ClearITPendingBit(EXTI_Line0);

    /* leave interrupt */
    rt_interrupt_leave();
}

#endif

#if (STM32_ETH_IF == 1)
/*******************************************************************************
* Function Name  : EXTI9_5_IRQHandler
* Description    : This function handles External lines 9 to 5 interrupt request.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void EXTI9_5_IRQHandler(void)
{
	extern void rt_dm9000_isr(void);

	/* enter interrupt */
	rt_interrupt_enter();

	rt_dm9000_isr();

	/* Clear the Key Button EXTI line pending bit */
	EXTI_ClearITPendingBit(EXTI_Line7);

	/* leave interrupt */
	rt_interrupt_leave();
}
#endif
#endif
#endif /* end of RT_USING_LWIP */

/* This is for SPI WIFI module */
void EXTI9_5_IRQHandler(void)
{
}

void EXTI15_10_IRQHandler (void)
{
}

void DMA1_Channel1_IRQHandler(void)
{
    //AD_DMA_Handler();
}

void DMA1_Channel4_IRQHandler(void)
{
}

void DMA1_Channel5_IRQHandler(void)
{
}

/**
  * @brief  OTG_FS_IRQHandler
  *          This function handles USB-On-The-Go FS global interrupt request.
  *          requests.
  * @param  None
  * @retval None
  */
void OTG_FS_IRQHandler(void)
{
    USBD_OTG_ISR_Handler (&USB_OTG_dev);
}

/**
  * @brief  TIM2_IRQHandler
  *         This function handles Timer2 Handler.
  * @param  None
  * @retval None
  */
void TIM2_IRQHandler(void)
{
	//USB_OTG_BSP_TimerIRQ();
}

/**
  * @brief  TIM3_IRQHandler
  *         This function handles Timer3 Handler.
  * @param  None
  * @retval None
  */
void TIM3_IRQHandler(void)
{
#ifdef CONNECT_TO_ANDROID
	usb_trans_timeout_handler(&usb_host_reg);
#endif
    //AD_timer_handler();	
}

/*
 * TIM4_IRQHandler for ADC sample timer
 */
void TIM4_IRQHandler (void)
{
	//AD_timer_handler();						
	Senso701_timer_handler();
}

void TIM6_IRQHandler (void)
{
	//exg_start_signal_handler();
}

void ADC1_2_IRQHandler (void)
{
	//AD_SmplFinish_handler();
}

/*******************************************************************************
* Function Name  : EXTI0_IRQHandler
* Description    : This function handles External interrupt Line 0 request.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void EXTI0_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    Sensor701_Pressure_INT();

    /* leave interrupt */
    rt_interrupt_leave();
}

/**
  * @}
  */ 


/******************* (C) COPYRIGHT 2009 STMicroelectronics *****END OF FILE****/
