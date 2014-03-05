/**
  ******************************************************************************
  * @file    usbd_cdc_vcp.c
  * @author  MCD Application Team
  * @version V1.1.0
  * @date    19-March-2012
  * @brief   Generic media access Layer.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2012 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */ 

#ifdef USB_OTG_HS_INTERNAL_DMA_ENABLED 
#pragma     data_alignment = 4 
#endif /* USB_OTG_HS_INTERNAL_DMA_ENABLED */

/* Includes ------------------------------------------------------------------*/
#include "usbd_cdc_vcp.h"
#include "usb_conf.h"
#include "SoftSPIAdc.h"
#include "Sensor701.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
LINE_CODING linecoding =
  {
    115200, /* baud rate*/
    0x00,   /* stop bits-1*/
    0x00,   /* parity - none*/
    0x08    /* nb. of bits 8*/
  };

uint8_t paraSetLock = 0;

//USART_InitTypeDef USART_InitStructure;

/* These are external variables imported from CDC core to be used for IN 
   transfer management. */
extern uint8_t  APP_Rx_Buffer []; /* Write CDC received data in this buffer.
                                     These data will be sent over USB IN endpoint
                                     in the CDC core functions. */
extern uint32_t APP_Rx_ptr_in;    /* Increment this pointer or roll it back to
                                     start address when writing received data
                                     in the buffer APP_Rx_Buffer. */

/* Private function prototypes -----------------------------------------------*/
static uint16_t VCP_Init     (void);
static uint16_t VCP_DeInit   (void);
static uint16_t VCP_Ctrl     (uint32_t Cmd, uint8_t* Buf, uint32_t Len);
static uint16_t VCP_DataTx   (uint8_t* Buf, uint32_t Len);
static uint16_t VCP_DataRx   (uint8_t* Buf, uint32_t Len);

static uint16_t VCP_COMConfig(uint8_t Conf);

CDC_IF_Prop_TypeDef VCP_fops = 
{
  VCP_Init,
  VCP_DeInit,
  VCP_Ctrl,
  VCP_DataTx,
  VCP_DataRx
};

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  VCP_Init
  *         Initializes the Media on the STM32
  * @param  None
  * @retval Result of the opeartion (USBD_OK in all cases)
  */
static uint16_t VCP_Init(void)
{
//  NVIC_InitTypeDef NVIC_InitStructure;
//  
//  /* EVAL_COM1 default configuration */
//  /* EVAL_COM1 configured as follow:
//        - BaudRate = 115200 baud  
//        - Word Length = 8 Bits
//        - One Stop Bit
//        - Parity Odd
//        - Hardware flow control disabled
//        - Receive and transmit enabled
//  */
//  USART_InitStructure.USART_BaudRate = 115200;
//  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
//  USART_InitStructure.USART_StopBits = USART_StopBits_1;
//  USART_InitStructure.USART_Parity = USART_Parity_Odd;
//  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
//  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

//  /* Configure and enable the USART */
//  STM_EVAL_COMInit(COM1, &USART_InitStructure);

//  /* Enable the USART Receive interrupt */
//  USART_ITConfig(EVAL_COM1, USART_IT_RXNE, ENABLE);

//  /* Enable USART Interrupt */
//  NVIC_InitStructure.NVIC_IRQChannel = EVAL_COM1_IRQn;
//  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
//  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
//  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
//  NVIC_Init(&NVIC_InitStructure);
  
  return USBD_OK;
}

/**
  * @brief  VCP_DeInit
  *         DeInitializes the Media on the STM32
  * @param  None
  * @retval Result of the opeartion (USBD_OK in all cases)
  */
static uint16_t VCP_DeInit(void)
{

  return USBD_OK;
}


/**
  * @brief  VCP_Ctrl
  *         Manage the CDC class requests
  * @param  Cmd: Command code            
  * @param  Buf: Buffer containing command data (request parameters)
  * @param  Len: Number of data to be sent (in bytes)
  * @retval Result of the opeartion (USBD_OK in all cases)
  */
static uint16_t VCP_Ctrl (uint32_t Cmd, uint8_t* Buf, uint32_t Len)
{ 
  switch (Cmd)
  {
  case SEND_ENCAPSULATED_COMMAND:
    /* Not  needed for this driver */
    break;

  case GET_ENCAPSULATED_RESPONSE:
    /* Not  needed for this driver */
    break;

  case SET_COMM_FEATURE:
    /* Not  needed for this driver */
    break;

  case GET_COMM_FEATURE:
    /* Not  needed for this driver */
    break;

  case CLEAR_COMM_FEATURE:
    /* Not  needed for this driver */
    break;

  case SET_LINE_CODING:
    linecoding.bitrate = (uint32_t)(Buf[0] | (Buf[1] << 8) | (Buf[2] << 16) | (Buf[3] << 24));
    linecoding.format = Buf[4];
    linecoding.paritytype = Buf[5];
    linecoding.datatype = Buf[6];
    /* Set the new configuration */
    VCP_COMConfig(OTHER_CONFIG);
    break;

  case GET_LINE_CODING:
    Buf[0] = (uint8_t)(linecoding.bitrate);
    Buf[1] = (uint8_t)(linecoding.bitrate >> 8);
    Buf[2] = (uint8_t)(linecoding.bitrate >> 16);
    Buf[3] = (uint8_t)(linecoding.bitrate >> 24);
    Buf[4] = linecoding.format;
    Buf[5] = linecoding.paritytype;
    Buf[6] = linecoding.datatype; 
    break;

  case SET_CONTROL_LINE_STATE:
    /* Not  needed for this driver */
    break;

  case SEND_BREAK:
    /* Not  needed for this driver */
    break;    
    
  default:
    break;
  }

  return USBD_OK;
}

/**
  * @brief  VCP_DataTx
  *         CDC received data to be send over USB IN endpoint are managed in 
  *         this function.
  * @param  Buf: Buffer of data to be sent
  * @param  Len: Number of data to be sent (in bytes)
  * @retval Result of the opeartion: USBD_OK if all operations are OK else VCP_FAIL
  */
static uint16_t VCP_DataTx (uint8_t* Buf, uint32_t Len)
{
//  if (linecoding.datatype == 7)
//  {
//    APP_Rx_Buffer[APP_Rx_ptr_in] = USART_ReceiveData(EVAL_COM1) & 0x7F;
//  }
//  else if (linecoding.datatype == 8)
//  {
//    APP_Rx_Buffer[APP_Rx_ptr_in] = USART_ReceiveData(EVAL_COM1);
//  }
  uint8_t iIdx = 0;

    //Rotate buffer
    while(iIdx < Len)
    {
        APP_Rx_Buffer[APP_Rx_ptr_in] = Buf[iIdx];
        iIdx++;
        APP_Rx_ptr_in = (APP_Rx_ptr_in+1) % APP_RX_DATA_SIZE;
    }
  
  return USBD_OK;
}

/**
  * @brief  VCP_DataRx
  *         Data received over USB OUT endpoint are sent over CDC interface 
  *         through this function.
  *           
  *         @note
  *         This function will block any OUT packet reception on USB endpoint 
  *         untill exiting this function. If you exit this function before transfer
  *         is complete on CDC interface (ie. using DMA controller) it will result 
  *         in receiving more data while previous ones are still not sent.
  *                 
  * @param  Buf: Buffer of data to be received
  * @param  Len: Number of data received (in bytes)
  * @retval Result of the opeartion: USBD_OK if all operations are OK else VCP_FAIL
  */
static uint16_t VCP_DataRx (uint8_t* Buf, uint32_t Len)
{
    paraSetLock = 1;
  //uint8_t tempreture[5];
  //uint8_t readConfig = 0;
  //uint8_t index = 0;
    //Buf[2] gain
    //Buf[3] channel
    //Buf[4] Rate
    //Set Gain
  //Buf[0],Buf[1] = 'FC'  Command
  //Buf[0],Buf[1] = 'TE'  Tempreture Read

  //All Reg mode defines data retrive protocol
  //Buffer    AllRegMode     Function
  //'FC'      0              Setting working mode;using Timer to read
  // 3F 02    1              Read all reg
  // 3F 03    1              Write single reg
  // 3F 10    1              Only Setting all reg mode
  // 3F 11    2              Setting AllRegMode 2; using Int1 to read
  if(Buf[0] == 'F' && Buf[1] == 'C')
  {
    allRegMode = 0;
    SetChannel(Buf[3] - '0');
    SetGainUdr(Buf[2], Buf[4]);
  }  
  else if (0x3F==Buf[0] && 0x02==Buf[1])
  {
    //Read all register in sensor; using timer4;
    allRegMode = 1;
    restParaNum = 0;
  }
  else if(0x3F==Buf[0] && 0x05==Buf[1])
  {
    allRegMode = 3;
    restParaNum = Buf[2];
  }
  else if (0x3F==Buf[0] && 0x03==Buf[1])
  {
    //Write all register in sensor
    //Buf[2]; adr
    //Buf[3]; value
    allRegMode = 1;
    SetRegData(Buf[3], Buf[4]);
  }
  else if(0x3F==Buf[0] && 0x10==Buf[1])
  {
    allRegMode = 1;
  }
  else if(0x3F==Buf[0] && 0x11==Buf[1])
  {
    allRegMode = 2;
    SetChannel(Buf[3] - '0');
     SetGainUdr(Buf[2], Buf[4]);
  }
  paraSetLock = 0;
  return USBD_OK;
}

/**
  * @brief  VCP_COMConfig
  *         Configure the COM Port with default values or values received from host.
  * @param  Conf: can be DEFAULT_CONFIG to set the default configuration or OTHER_CONFIG
  *         to set a configuration received from the host.
  * @retval None.
  */
static uint16_t VCP_COMConfig(uint8_t Conf)
{
//  if (Conf == DEFAULT_CONFIG)  
//  {
//    /* EVAL_COM1 default configuration */
//    /* EVAL_COM1 configured as follow:
//    - BaudRate = 115200 baud  
//    - Word Length = 8 Bits
//    - One Stop Bit
//    - Parity Odd
//    - Hardware flow control disabled
//    - Receive and transmit enabled
//    */
//    USART_InitStructure.USART_BaudRate = 115200;
//    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
//    USART_InitStructure.USART_StopBits = USART_StopBits_1;
//    USART_InitStructure.USART_Parity = USART_Parity_Odd;
//    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
//    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
//    
//    /* Configure and enable the USART */
//    STM_EVAL_COMInit(COM1, &USART_InitStructure);
//    
//    /* Enable the USART Receive interrupt */
//    USART_ITConfig(EVAL_COM1, USART_IT_RXNE, ENABLE);
//  }
//  else
//  {
//    /* set the Stop bit*/
//    switch (linecoding.format)
//    {
//    case 0:
//      USART_InitStructure.USART_StopBits = USART_StopBits_1;
//      break;
//    case 1:
//      USART_InitStructure.USART_StopBits = USART_StopBits_1_5;
//      break;
//    case 2:
//      USART_InitStructure.USART_StopBits = USART_StopBits_2;
//      break;
//    default :
//      VCP_COMConfig(DEFAULT_CONFIG);
//      return (USBD_FAIL);
//    }
//    
//    /* set the parity bit*/
//    switch (linecoding.paritytype)
//    {
//    case 0:
//      USART_InitStructure.USART_Parity = USART_Parity_No;
//      break;
//    case 1:
//      USART_InitStructure.USART_Parity = USART_Parity_Even;
//      break;
//    case 2:
//      USART_InitStructure.USART_Parity = USART_Parity_Odd;
//      break;
//    default :
//      VCP_COMConfig(DEFAULT_CONFIG);
//      return (USBD_FAIL);
//    }
//    
//    /*set the data type : only 8bits and 9bits is supported */
//    switch (linecoding.datatype)
//    {
//    case 0x07:
//      /* With this configuration a parity (Even or Odd) should be set */
//      USART_InitStructure.USART_WordLength = USART_WordLength_8b;
//      break;
//    case 0x08:
//      if (USART_InitStructure.USART_Parity == USART_Parity_No)
//      {
//        USART_InitStructure.USART_WordLength = USART_WordLength_8b;
//      }
//      else 
//      {
//        USART_InitStructure.USART_WordLength = USART_WordLength_9b;
//      }
//      
//      break;
//    default :
//      VCP_COMConfig(DEFAULT_CONFIG);
//      return (USBD_FAIL);
//    }
//    
//    USART_InitStructure.USART_BaudRate = linecoding.bitrate;
//    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
//    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
//    
//    /* Configure and enable the USART */
//    STM_EVAL_COMInit(COM1, &USART_InitStructure);
//  }
  return USBD_OK;
}

///**
//  * @brief  EVAL_COM_IRQHandler
//  *         
//  * @param  None.
//  * @retval None.
//  */
//void EVAL_COM_IRQHandler(void)
//{
//  if (USART_GetITStatus(EVAL_COM1, USART_IT_RXNE) != RESET)
//  {
//    /* Send the received data to the PC Host*/
//    VCP_DataTx (0,0);
//  }
//
//  /* If overrun condition occurs, clear the ORE flag and recover communication */
//  if (USART_GetFlagStatus(EVAL_COM1, USART_FLAG_ORE) != RESET)
//  {
//    (void)USART_ReceiveData(EVAL_COM1);
//  }
//}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
