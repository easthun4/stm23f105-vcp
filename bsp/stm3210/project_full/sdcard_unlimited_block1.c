/******************** (C) COPYRIGHT 2008 STMicroelectronics ********************
 * File Name          : sdcard.c
 * Author             : MCD Application Team
 * Version            : V2.0.1
 * Date               : 06/13/2008
 * Description        : This file provides all the SD Card driver firmware
 *                      functions.
 ********************************************************************************
 * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
 * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE TIME.
 * AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
 * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
 * CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
 * INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "sdcard.h"
#include "rtthread.h"
#include "stm32f10x_sdio.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define NULL 0
#define SDIO_STATIC_FLAGS               ((u32)0x000005FF)
#define SDIO_CMD0TIMEOUT                ((u32)0x00002710)
#define SDIO_FIFO_Address               ((u32)0x40018080)

/* Mask for errors Card Status R1 */
#define SDIO_CS_ADDR_OUT_OF_RANGE        ((u32)0x80000000)
#define SDIO_CS_COM_CRC_FAILED           ((u32)0x00800000)
#define SDIO_CS_ILLEGAL_CMD              ((u32)0x00400000)
#define SDIO_CS_GENERAL_UNKNOWN_ERROR    ((u32)0x00080000)
#define SDIO_CS_ERRORBITS                ((u32)0x80C80000)

/* Masks for R4 Response */
#define SDIO_R4_CARD_RDY                ((u32)0x80000000)
#define SDIO_R4_OCR                     ((u32)0x00FFFFFF)

/* Masks for R6 Response */
#define SD_R6_GENERAL_UNKNOWN_ERROR     ((u32)0x00002000)
#define SD_R6_ILLEGAL_CMD               ((u32)0x00004000)
#define SD_R6_COM_CRC_FAILED            ((u32)0x00008000)

#define SDIO_VOLTAGE_WINDOW             ((u32)0x00180000)

#define SD_MAX_VOLT_TRIAL               ((u32)0x0000FFFF)
#define SD_ALLZERO                      ((u32)0x00000000)

#define SD_WIDE_BUS_SUPPORT             ((u32)0x00040000)
#define SD_SINGLE_BUS_SUPPORT           ((u32)0x00010000)
#define SD_CARD_LOCKED                  ((u32)0x02000000)
#define SD_CARD_PROGRAMMING             ((u32)0x00000007)
#define SD_CARD_RECEIVING               ((u32)0x00000006)
#define SD_DATATIMEOUT                  ((u32)0x000FFFFF)
#define SD_0TO7BITS                     ((u32)0x000000FF)
#define SD_8TO15BITS                    ((u32)0x0000FF00)
#define SD_16TO23BITS                   ((u32)0x00FF0000)
#define SD_24TO31BITS                   ((u32)0xFF000000)
#define SD_MAX_DATA_LENGTH              ((u32)0x01FFFFFF)

#define SD_HALFFIFO                     ((u32)0x00000008)
#define SD_HALFFIFOBYTES                ((u32)0x00000020)

/* Command Class Supported */
#define SD_CCCC_LOCK_UNLOCK             ((u32)0x00000080)
#define SD_CCCC_WRITE_PROT              ((u32)0x00000040)
#define SD_CCCC_ERASE                   ((u32)0x00000020)

#define SDIO_INIT_CLK_DIV                  ((u8)0xB2)
#define SDIO_TRANSFER_CLK_DIV              ((u8)0x1) 

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
//static u32 DeviceMode = SD_POLLING_MODE;
static u32 TotalNumberOfBytes = 0;
u32 *SrcBuffer, *DestBuffer;
volatile SD_Error TransferError = SD_OK;
vu32 TransferEnd = 0;
vu32 NumberOfBytes = 0;
SDIO_InitTypeDef SDIO_InitStructure;
SDIO_CmdInitTypeDef SDIO_CmdInitStructure;
SDIO_DataInitTypeDef SDIO_DataInitStructure;
SDIO_CardInfo_t sd8686;
SDIO_CCCR_t cccr;
extern struct rt_semaphore sd_read_lock, sd_write_lock;
extern rt_bool_t sd_waiting_write;

/* Private function prototypes -----------------------------------------------*/
static void GPIO_Configuration(void);
static SD_Error SDIO_EnableWide(void);
static SD_Error SDIO_EnableProbe(void);
static SD_Error SDIO_EnableInterrupt(u8 fn);
static SD_Error SDIO_ProgFirmwareHelper(void);
static SD_Error SDIO_ProgFirmwareReal(void);
static SD_Error SDIO_IORWDirect(u8 r0w1, u8 fn, u32 regaddr, u8 in_data, u8 *out_data);
static SD_Error SDIO_WriteMultiBlock(u8 fn, u32 *writebuff, u16 BlockSize, u16 NumOfBlocks);
static SD_Error SDIO_WriteMultiByte(u8 fn, u32 *writebuff, u16 NumOfBytes);
static SD_Error SDIO_WaitShortResp(u32 cmd, u32 *resp);
//static SD_Error SDIO_DoScan(void);
//static SD_Error SDIO_DoSetMac(void);
static SD_Error SDIO_ReadMultiBlock(u8 fn, u32 *readbuff, u16 BlockSize, u16 NumOfBlocks);

/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
 * Function Name  : SD_Init
 * Description    : Initializes the SDIO Card, download firmware and make 88w8686
 *                  ready for data transfer).
 * Input          : None
 * Output         : None
 * Return         : SD_Error: SD Card Error code.
 *******************************************************************************/
SD_Error SD_Init(void)
{
    SD_Error errorstatus = SD_OK;

    /*
       {
       GPIO_InitTypeDef GPIO_InitStructure;

       RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

       GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_5;
       GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
       GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
       GPIO_Init(GPIOB, &GPIO_InitStructure);

       GPIO_ResetBits(GPIOB, GPIO_Pin_5) ;
       }
       */

    /* Configure SDIO interface GPIO */
    GPIO_Configuration();

    /* Enable the SDIO AHB Clock */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_SDIO, ENABLE);

    /* Enable the DMA2 Clock */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2, ENABLE);

    SDIO_DeInit();

    errorstatus = SD_PowerON();

    if (errorstatus != SD_OK)
    {
        /* CMD Response TimeOut (wait for CMDSENT flag) */
        return(errorstatus);
    }

    errorstatus = SD_InitializeCards();

    if (errorstatus != SD_OK)
    {
        /* CMD Response TimeOut (wait for CMDSENT flag) */
        return(errorstatus);
    }

    /* Configure the SDIO peripheral */
    /* HCLK = 72 MHz, SDIOCLK = 72 MHz, SDIO_CK = HCLK/(2 + 1) = 24 MHz */  
    SDIO_InitStructure.SDIO_ClockDiv = SDIO_TRANSFER_CLK_DIV; 
    SDIO_InitStructure.SDIO_ClockEdge = SDIO_ClockEdge_Rising;
    SDIO_InitStructure.SDIO_ClockBypass = SDIO_ClockBypass_Disable;
    SDIO_InitStructure.SDIO_ClockPowerSave = SDIO_ClockPowerSave_Disable;
    SDIO_InitStructure.SDIO_BusWide = SDIO_BusWide_1b;
    SDIO_InitStructure.SDIO_HardwareFlowControl = SDIO_HardwareFlowControl_Disable;
    //SDIO_InitStructure.SDIO_HardwareFlowControl = SDIO_HardwareFlowControl_Enable;
    SDIO_Init(&SDIO_InitStructure);

    /* put SDIO into 4-bit mode */
    errorstatus = SDIO_EnableWide();
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    /* enable interrupt, get ioport */
    errorstatus = SDIO_EnableProbe();
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }		    

	errorstatus = SDIO_EnableInterrupt(1);
	if (errorstatus != SD_OK) {
		return(errorstatus);
	}

    errorstatus = SDIO_ProgFirmwareHelper();
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }						    
		    
    errorstatus = SDIO_ProgFirmwareReal();
    if (errorstatus != SD_OK) {	   
        return(errorstatus);
    }				 		 	  

	//errorstatus = SDIO_DoScan();					   
    //errorstatus = SDIO_DoSetMac();

    return(errorstatus);		    
}

/* SDIO enable function fn */
SD_Error SDIO_EnableFunc(u8 fn)
{
    SD_Error errorstatus = SD_OK;
    u32 timeout = SDIO_CMD0TIMEOUT;
    u8 x;

    errorstatus = SDIO_IORWDirect(0, 0, SDIO_CCCR_IOEx, 0, &x);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    x |= 1 << fn;

    errorstatus = SDIO_IORWDirect(1, 0, SDIO_CCCR_IOEx, x, NULL);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    /* 
     * Loop through check IO_RDY status, until we are
     * seeing IO ready
     */
    while (1) {
        errorstatus = SDIO_IORWDirect(0, 0, SDIO_CCCR_IORx, 0, &x);
        if (errorstatus != SD_OK) {
            return(errorstatus);
        }
        /* check to see if fn is ready or not */
        if (x & (1 << fn)) {
            break;
        }
        if (timeout-- == 0) {
            errorstatus = SD_CMD_RSP_TIMEOUT;
            break;
        }
    }

    return(errorstatus);
}

/* SDIO enable interrupt on function fn */
SD_Error SDIO_EnableInterrupt(u8 fn)
{
    SD_Error errorstatus = SD_OK;
    u8 x;

    errorstatus = SDIO_IORWDirect(0, 0, SDIO_CCCR_IENx, 0, &x);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    /* enable function interrupt */
    x |= 1 << fn;

    /* enable common interrupt */
    x |= 1;

    errorstatus = SDIO_IORWDirect(1, 0, SDIO_CCCR_IENx, x, NULL);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    //errorstatus = SDIO_IORWDirect(0, 0, SDIO_CCCR_IF, 0, &x);
    //if (errorstatus != SD_OK) {
    //    return(errorstatus);
    //}

	/* enable ECSI */
	//x |= 0x20;
	//errorstatus = SDIO_IORWDirect(1, 0, SDIO_CCCR_IF, x, NULL);

    //if (errorstatus != SD_OK) {
    //    return(errorstatus);
    //}

    return(errorstatus);
}

SD_Error SDIO_GetIOPort(u8 fn, u32 *ioport)
{
    SD_Error errorstatus = SD_OK;
    u8 x;

    errorstatus = SDIO_IORWDirect(0, fn, SDIO_IOPORT, 0, &x);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    *ioport = x;

    errorstatus = SDIO_IORWDirect(0, fn, SDIO_IOPORT+1, 0, &x);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    } 

    *ioport |= (x << 8);

    errorstatus = SDIO_IORWDirect(0, fn, SDIO_IOPORT+2, 0, &x);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    } 

    *ioport |= (x << 16);

    return(errorstatus);
}

SD_Error SDIO_ReadScratch(u8 fn, u32 *scratch)
{
    SD_Error errorstatus = SD_OK;
    u8 x;

    errorstatus = SDIO_IORWDirect(0, fn, SDIO_SCRATCH, 0, &x);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }  

    *scratch = x;

    errorstatus = SDIO_IORWDirect(0, fn, SDIO_SCRATCH+1, 0, &x);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    } 

    *scratch |= (x << 8);

    return(errorstatus);
}

SD_Error SDIO_SetBlockSize(u8 fn, u32 block_size)
{
    SD_Error errorstatus = SD_OK;

    if (block_size > sd8686.max_block_size) {
        return(SD_UNSUPPORTED_FEATURE);
    }

    /* create some default value if missing input param */
    if (block_size == 0) {
        block_size = MIN(sd8686.max_block_size, sd8686.f1.max_block_size);
        block_size = MIN(block_size, 512);
    }

    errorstatus = SDIO_IORWDirect(1, 0, 
            (SDIO_FBR_BASE(fn) + SDIO_FBR_BLKSIZE), (block_size & 0xff), NULL);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    errorstatus = SDIO_IORWDirect(1, 0, 
            (SDIO_FBR_BASE(fn) + SDIO_FBR_BLKSIZE + 1), 
            ((block_size >> 8) & 0xff), NULL);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    sd8686.f1.cur_block_size = block_size;

    return(errorstatus);
}

static void delay(void)
{
  vu32 i = 0;

  for(i = 0x1000; i != 0; i--)
  {
  }
}

SD_Error SDIO_WaitStatus(u8 condition)
{
    SD_Error errorstatus = SD_OK;
    u32 timeout = SDIO_CMD0TIMEOUT;
    u8 x;

    while (1) {
        /* check SDIO_STATUS */
        errorstatus = SDIO_IORWDirect(0, 1, SDIO_STATUS, 0, &x);
        if (errorstatus != SD_OK) {
            return(errorstatus);
        }
        if ((x & condition) == condition) {
            break;
        }
        if (timeout-- == 0) {
            errorstatus = SD_CMD_RSP_TIMEOUT;
            break;
        }
        delay();
    }				 

    return(errorstatus);
}

/*******************************************************************************
* Function Name  : convert_from_bytes_to_power_of_two
* Description    : Converts the number of bytes in power of two and returns the
*                  power.
* Input          : NumberOfBytes: number of bytes.
* Output         : None
* Return         : None
*******************************************************************************/
static u8 convert_from_bytes_to_power_of_two(u16 NumberOfBytes)
{
    u8 count = 0;

    while (NumberOfBytes != 1)
    {
        NumberOfBytes >>= 1;
        count++;
    }
    return(count);
}

/* 
 * Caller is responsible for rounding 
 * count to 4 byte boundry
 */
SD_Error SDIO_IORWExtendReadHelper(u8 *src, u32 count)
{
    SD_Error errorstatus = SD_OK;
    u8 block_num;

    /*
    if (((count >> 2) << 2) != count) {
        return(SD_INVALID_PARAMETER);
    }
    */

    while (count >= sd8686.f1.cur_block_size) {
        block_num = count/sd8686.f1.cur_block_size;
        errorstatus = SDIO_ReadMultiBlock(1, (u32 *)src, 
                sd8686.f1.cur_block_size, block_num);
        if (errorstatus != SD_OK) {
            return(errorstatus);
        }					  
        count -= sd8686.f1.cur_block_size*block_num;
        src += sd8686.f1.cur_block_size*block_num;
    }
						 
    return(errorstatus);
}

SD_Error SDIO_IORWExtendHelper(u8 *src, u32 count)
{
    SD_Error errorstatus = SD_OK;
    u16 block_num;

    /* First try to use multi-block copy */
    if (cccr.support_multi_block && (count >= sd8686.f1.cur_block_size)) {
        while (count >= sd8686.f1.cur_block_size) {
            block_num = count/sd8686.f1.cur_block_size;
            errorstatus = SDIO_WriteMultiBlock(1, (u32 *)src, 
                                sd8686.f1.cur_block_size, block_num);
            if (errorstatus != SD_OK) {
                return(errorstatus);
            }					  
            count -= sd8686.f1.cur_block_size*block_num;
            src += sd8686.f1.cur_block_size*block_num;
        }
    }
						 
    /* Then try to use byte copy */
    if (count > 0) {
        errorstatus = SDIO_WriteMultiByte(1, (u32 *)src, count);
    }

    return(errorstatus);
}

/*******************************************************************************
* Function Name  : SDIO_WriteMultiByte
* Description    : Allows to write one block starting from a specified address 
*                  in a card.
* Input          : - addr: Address from where data are to be read.
*                  - writebuff: pointer to the buffer that contain the data to be
*                    transferred.
*                  - BlockSize: the SD card Data block size.
* Output         : None
* Return         : SD_Error: SD Card Error code.
*******************************************************************************/
SD_Error SDIO_WriteMultiByte(u8 fn, u32 *writebuff, u16 NumOfBytes)
{
    SD_Error errorstatus = SD_OK;
    //u8  power = 0;
    u32 count = 0, restwords = 0;
    u32 *tempbuff = writebuff;
    u32 response_r1;

    if (writebuff == NULL)
    {
        errorstatus = SD_INVALID_PARAMETER;
        return(errorstatus);
    }

    /* Send CMD53 IO_RW_EXTENDED */
    SDIO_CmdInitStructure.SDIO_Argument = 0x80000000 |            /* Write */
                                            ((fn & 0x7) << 28) |  /* Function number */
                                            (0x0 << 27) |         /* Byte mode */
                                            (0x0 << 26) |         /* Write to fixed addr */
                                            (sd8686.f1.ioport & 0x1FFFF) << 9 |   /* Starting addr */
                                            (NumOfBytes & 0x1FF);
    SDIO_CmdInitStructure.SDIO_CmdIndex = SDIO_SDIO_RW_EXTENDED;
    SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
    SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
    SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
    SDIO_SendCommand(&SDIO_CmdInitStructure);

    /*
     * Waiting for response
     */
    errorstatus = SDIO_WaitShortResp(SDIO_SDIO_RW_EXTENDED, &response_r1);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }
    if ((response_r1 & 0xFF) != 0x00) {
        return(SD_ERROR);
    }

    //SrcBuffer = writebuff;
    //power = convert_from_bytes_to_power_of_two(NumOfBytes);

    SDIO_DataInitStructure.SDIO_DataTimeOut = SD_DATATIMEOUT;
    SDIO_DataInitStructure.SDIO_DataLength = NumOfBytes;
    //SDIO_DataInitStructure.SDIO_DataBlockSize = (u32) power << 4;
    SDIO_DataInitStructure.SDIO_DataBlockSize = (u32) 0 << 4;
    SDIO_DataInitStructure.SDIO_TransferDir = SDIO_TransferDir_ToCard;
    SDIO_DataInitStructure.SDIO_TransferMode = SDIO_TransferMode_Stream;
    SDIO_DataInitStructure.SDIO_DPSM = SDIO_DPSM_Enable;
    SDIO_DataConfig(&SDIO_DataInitStructure);

    /* IO_RW_EXTENDED does not need stop cmd */
    /* 
     * If total size is no more than total TX FIFO
     * size, send them all at once
     */
    restwords = NumOfBytes >> 2;
    if ((restwords << 2) != NumOfBytes) {
        restwords += 1;
    }
    for (count = 0; count < restwords; count++) {
        SDIO_WriteData(*(tempbuff + count));
    }
    while (!(SDIO->STA & (SDIO_FLAG_TXUNDERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DATAEND | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_STBITERR))) {
        ;
    }
    if (SDIO_GetFlagStatus(SDIO_FLAG_DTIMEOUT) != RESET)
    {
        SDIO_ClearFlag(SDIO_FLAG_DTIMEOUT);
        errorstatus = SD_DATA_TIMEOUT;
        return(errorstatus);
    }
    else if (SDIO_GetFlagStatus(SDIO_FLAG_DCRCFAIL) != RESET)
    {
        SDIO_ClearFlag(SDIO_FLAG_DCRCFAIL);
        errorstatus = SD_DATA_CRC_FAIL;
        return(errorstatus);
    }
    else if (SDIO_GetFlagStatus(SDIO_FLAG_TXUNDERR) != RESET)
    {
        SDIO_ClearFlag(SDIO_FLAG_TXUNDERR);
        errorstatus = SD_TX_UNDERRUN;
        return(errorstatus);
    }
    else if (SDIO_GetFlagStatus(SDIO_FLAG_STBITERR) != RESET)
    {
        SDIO_ClearFlag(SDIO_FLAG_STBITERR);
        errorstatus = SD_START_BIT_ERR;
        return(errorstatus);
    }

    /* Clear all the static flags */
    SDIO_ClearFlag(SDIO_STATIC_FLAGS);

    /* Wait till the card is in programming state */
    errorstatus = SDIO_WaitStatus(SDIO_IO_RDY & SDIO_DL_RDY);

    return(errorstatus);
}

/*******************************************************************************
* Function Name  : SDIO_WriteMultiBlock
* Description    : Allows to write one block starting from a specified address 
*                  in a card.
* Input          : - addr: Address from where data are to be read.
*                  - writebuff: pointer to the buffer that contain the data to be
*                    transferred.
*                  - BlockSize: the SD card Data block size.
* Output         : None
* Return         : SD_Error: SD Card Error code.
*******************************************************************************/
SD_Error SDIO_WriteMultiBlock(u8 fn, u32 *writebuff, u16 BlockSize, u16 NumOfBlocks)
{
    SD_Error errorstatus = SD_OK;
    u8  power = 0;
    u32 count = 0, restwords = 0;
    u32 *tempbuff = writebuff;
    u32 response_r1;
    u32 bytes_remain;
    u8 x;

    if (writebuff == NULL)
    {
        errorstatus = SD_INVALID_PARAMETER;
        return(errorstatus);
    }

    /* Send CMD53 IO_RW_EXTENDED */
    SDIO_CmdInitStructure.SDIO_Argument = 0x80000000 |            /* Write */
                                            ((fn & 0x7) << 28) |  /* Function number */
                                            (0x1 << 27) |         /* Block mode */
                                            (0x0 << 26) |         /* Write to fixed addr */
                                            (sd8686.f1.ioport & 0x1FFFF) << 9 |   /* Starting addr */
                                            NumOfBlocks <= 0x1FF ? (NumOfBlocks & 0x1FF):0;
    SDIO_CmdInitStructure.SDIO_CmdIndex = SDIO_SDIO_RW_EXTENDED;
    SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
    SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
    SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
    SDIO_SendCommand(&SDIO_CmdInitStructure);

    /*
     * Waiting for response
     */
    errorstatus = SDIO_WaitShortResp(SDIO_SDIO_RW_EXTENDED, &response_r1);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }
    if ((response_r1 & 0xFF) != 0x00) {
        return(SD_ERROR);
    }

    TotalNumberOfBytes = NumOfBlocks * BlockSize;
    bytes_remain = TotalNumberOfBytes;
    //SrcBuffer = writebuff;
    power = convert_from_bytes_to_power_of_two(BlockSize);

    SDIO_DataInitStructure.SDIO_DataTimeOut = SD_DATATIMEOUT;
    SDIO_DataInitStructure.SDIO_DataLength = TotalNumberOfBytes;
    SDIO_DataInitStructure.SDIO_DataBlockSize = (u32) power << 4;
    SDIO_DataInitStructure.SDIO_TransferDir = SDIO_TransferDir_ToCard;
    SDIO_DataInitStructure.SDIO_TransferMode = SDIO_TransferMode_Block;
    SDIO_DataInitStructure.SDIO_DPSM = SDIO_DPSM_Enable;
    SDIO_DataConfig(&SDIO_DataInitStructure);

    /* IO_RW_EXTENDED does not need stop cmd */
    if (TotalNumberOfBytes <= 32*4) {
        /* 
         * If total size is no more than total TX FIFO
         * size, send them all at once
         */
        restwords = TotalNumberOfBytes >> 2;
        if ((restwords << 2) != TotalNumberOfBytes) {
            restwords += 1;
        }
        for (count = 0; count < restwords; count++) {
            SDIO_WriteData(*(tempbuff + count));
        }
        while (!(SDIO->STA & (SDIO_FLAG_TXUNDERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DATAEND | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_STBITERR))) {
            ;
        }
    } else {
        /* 
         * The total size is large, we had to consider
         * flow control by checking SDIO_FLAG_TXFIFOHE flag
         */
        while (!(SDIO->STA & (SDIO_FLAG_TXUNDERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DATAEND | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_STBITERR)))
        {
            if (SDIO_GetFlagStatus(SDIO_FLAG_TXFIFOHE) != RESET)
            {
                if (bytes_remain >= SD_HALFFIFOBYTES)
                {
                    for (count = 0; count < SD_HALFFIFO; count++)
                    {
                        SDIO_WriteData(*(tempbuff + count));
                    }
                    tempbuff += SD_HALFFIFO;
                    bytes_remain -= SD_HALFFIFOBYTES;
                }
                else
                {
                    restwords = bytes_remain >> 2;
                    if ((restwords << 2) != bytes_remain) {
                        restwords += 1;
                    }
                    for (count = 0; count < restwords; count++)
                    {
                        SDIO_WriteData(*(tempbuff + count));
                    }
                    bytes_remain = 0;
                }
            }
        }
        if (NumOfBlocks > 0x1FF) {
            x = 0x01;
            errorstatus = SDIO_IORWDirect(1, 0, SDIO_CCCR_ABORT, x, NULL);
            if (errorstatus != SD_OK) {
                return(errorstatus);
            }
        }
    }
    if (SDIO_GetFlagStatus(SDIO_FLAG_DTIMEOUT) != RESET)
    {
        SDIO_ClearFlag(SDIO_FLAG_DTIMEOUT);
        errorstatus = SD_DATA_TIMEOUT;
        return(errorstatus);
    }
    else if (SDIO_GetFlagStatus(SDIO_FLAG_DCRCFAIL) != RESET)
    {
        SDIO_ClearFlag(SDIO_FLAG_DCRCFAIL);
        errorstatus = SD_DATA_CRC_FAIL;
        return(errorstatus);
    }
    else if (SDIO_GetFlagStatus(SDIO_FLAG_TXUNDERR) != RESET)
    {
        SDIO_ClearFlag(SDIO_FLAG_TXUNDERR);
        errorstatus = SD_TX_UNDERRUN;
        return(errorstatus);
    }
    else if (SDIO_GetFlagStatus(SDIO_FLAG_STBITERR) != RESET)
    {
        SDIO_ClearFlag(SDIO_FLAG_STBITERR);
        errorstatus = SD_START_BIT_ERR;
        return(errorstatus);
    }

    /* Clear all the static flags */
    SDIO_ClearFlag(SDIO_STATIC_FLAGS);

    /* Wait till the card is in programming state */
    errorstatus = SDIO_WaitStatus(SDIO_IO_RDY & SDIO_DL_RDY);

    return(errorstatus);
}


SD_Error SDIO_ReadMultiBlock(u8 fn, u32 *readbuff, u16 BlockSize, u16 NumOfBlocks)
{
    SD_Error errorstatus = SD_OK;
    u8  power = 0;
    u32 count = 0;
    u32 *tempbuff = readbuff;
    //u32 response_r1;
    u16 read_count = 0;

    if (readbuff == NULL)
    {
        errorstatus = SD_INVALID_PARAMETER;
        return(errorstatus);
    }


    /*
     * Waiting for response
     */
    /*
    errorstatus = SDIO_WaitShortResp(SDIO_SDIO_RW_EXTENDED, &response_r1);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }
    if ((response_r1 & 0xFF) != 0x00) {
        return(SD_ERROR);
    }
    */

    TotalNumberOfBytes = NumOfBlocks * BlockSize;
    power = convert_from_bytes_to_power_of_two(BlockSize);

    SDIO_DataInitStructure.SDIO_DataTimeOut = SD_DATATIMEOUT;
    SDIO_DataInitStructure.SDIO_DataLength = TotalNumberOfBytes;
    SDIO_DataInitStructure.SDIO_DataBlockSize = (u32) power << 4;
    SDIO_DataInitStructure.SDIO_TransferDir = SDIO_TransferDir_ToSDIO;
    SDIO_DataInitStructure.SDIO_TransferMode = SDIO_TransferMode_Block;
    SDIO_DataInitStructure.SDIO_DPSM = SDIO_DPSM_Enable;
    SDIO_DataConfig(&SDIO_DataInitStructure);
					  
    /* Send CMD53 IO_RW_EXTENDED */
    SDIO_CmdInitStructure.SDIO_Argument = 0x00000000 |            /* Read */
                                            ((fn & 0x7) << 28) |  /* Function number */
                                            (0x1 << 27) |         /* Block mode */
                                            (0x0 << 26) |         /* Read from fixed addr */
                                            (sd8686.f1.ioport & 0x1FFFF) << 9 |   /* Starting addr */
                                            (NumOfBlocks & 0x1FF);
    SDIO_CmdInitStructure.SDIO_CmdIndex = SDIO_SDIO_RW_EXTENDED;
    SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
    SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
    SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
    SDIO_SendCommand(&SDIO_CmdInitStructure);

    /* IO_RW_EXTENDED does not need stop cmd */
    while (!(SDIO->STA &(SDIO_FLAG_RXOVERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DATAEND | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_STBITERR)))
    {
        if (SDIO_GetFlagStatus(SDIO_FLAG_RXFIFOHF) != RESET) {
            for (count = 0; count < SD_HALFFIFO; count++)
            {
                *(tempbuff + count) = SDIO_ReadData();
                read_count++;
            }
            tempbuff += SD_HALFFIFO;
        } else while (SDIO_GetFlagStatus(SDIO_FLAG_RXDAVL) != RESET) {
            *tempbuff = SDIO_ReadData();
            tempbuff++;
            read_count++;
        }
    } 				    
    if (SDIO_GetFlagStatus(SDIO_FLAG_DTIMEOUT) != RESET)
    {				  
        SDIO_ClearFlag(SDIO_FLAG_DTIMEOUT);
    }				 	     
    else if (SDIO_GetFlagStatus(SDIO_FLAG_DCRCFAIL) != RESET)
    {
        SDIO_ClearFlag(SDIO_FLAG_DCRCFAIL);
        errorstatus = SD_DATA_CRC_FAIL;
        return(errorstatus);
    }
    else if (SDIO_GetFlagStatus(SDIO_FLAG_RXOVERR) != RESET)
    {
        SDIO_ClearFlag(SDIO_FLAG_RXOVERR);
        errorstatus = SD_RX_OVERRUN;
        return(errorstatus);
    }
    else if (SDIO_GetFlagStatus(SDIO_FLAG_STBITERR) != RESET)
    {
        SDIO_ClearFlag(SDIO_FLAG_STBITERR);
        errorstatus = SD_START_BIT_ERR;
        return(errorstatus);
    }

    while (SDIO_GetFlagStatus(SDIO_FLAG_RXDAVL) != RESET)
    {
        *tempbuff = SDIO_ReadData();
        tempbuff++;
        read_count++;
    }

    /* Clear all the static flags */
    SDIO_ClearFlag(SDIO_STATIC_FLAGS);

    return(errorstatus);
}

/* Query the size indication */
SD_Error SDIO_GetNextBlockSize(u16 *block_size)
{
    SD_Error errorstatus = SD_OK;
    u32 timeout = SDIO_CMD0TIMEOUT;
    u8 x;
    u16 req_size;

    do {
        timeout--;

        errorstatus = SDIO_IORWDirect(0, 1, SDIO_RD_BASE, 0, &x);
        if (errorstatus != SD_OK) {
            return(errorstatus);
        }
        req_size = x;
        errorstatus = SDIO_IORWDirect(0, 1, SDIO_RD_BASE+1, 0, &x);
        if (errorstatus != SD_OK) {
            return(errorstatus);
        }
        req_size |= (x << 8);
        /* break out if req_size is not 0 */
        if (req_size != 0) {
            break;
        }
    } while (timeout > 0);

    if ((timeout == 0) && (req_size == 0)) {
        errorstatus = SD_CMD_RSP_TIMEOUT;
        return(errorstatus);
    }

    *block_size = req_size;

    return(errorstatus);
}

/* first download sd8686_helper.bin */
SD_Error SDIO_ProgFirmwareHelper()
{
    SD_Error errorstatus = SD_OK;
    u32 scratch;
    u8  *firmware;
    u32 size;
#define BATCH_BLKSIZE 64
    u8  chunk_buffer[BATCH_BLKSIZE];
    u32 *chunk_header;
    u32 chunk_size; 
    u8 fn = 1;
    u16 next_block_size;
    u32 i;

    errorstatus = SDIO_ReadScratch(fn, &scratch);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    /* If firmware status is already running, exiting... */
    if (scratch == SDIO_FIRMWARE_OK) {
        return(errorstatus);
    }

    /* First we provision the block size */
    errorstatus = SDIO_SetBlockSize(1, 1);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    /* 
     * sd8686_helper.bin is programmed into flash,
     * starting from address 0x08000000, size 0x9D4 (2516 bytes)
     */
    firmware = (u8 *)0x08000000;
    size = 2516;
    while (size) {
        /* make sure the status is ready before dnld image */
        errorstatus = SDIO_WaitStatus(SDIO_IO_RDY & SDIO_DL_RDY);
        if (errorstatus != SD_OK) {
            return(errorstatus);
        }

        chunk_size = MIN(size, (BATCH_BLKSIZE-4));
        chunk_header = (u32 *)chunk_buffer;
        *chunk_header = (BATCH_BLKSIZE-4);
        //*chunk_header = chunk_size;

        for (i = 0; i < chunk_size; i++) {
            chunk_buffer[4+i] = *firmware;
            firmware++;
        }
        for (i = chunk_size; i < (BATCH_BLKSIZE-4); i++) {
            chunk_buffer[4+i] = 0;
        }

        errorstatus = SDIO_IORWExtendHelper(chunk_buffer, BATCH_BLKSIZE);
        if (errorstatus != SD_OK) {
            return(errorstatus);
        }

        size -= chunk_size;
    }

    /* after writing all data, write 0x00000000 as end indication */
    for (i = 0; i < BATCH_BLKSIZE; i++) {
        chunk_buffer[i] = 0x00;
    }

    errorstatus = SDIO_IORWExtendHelper(chunk_buffer, 4);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    /* check the helper status by looking at size indication */
    errorstatus = SDIO_GetNextBlockSize(&next_block_size);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    return(errorstatus);
}

/* Now download sd8686.bin */
SD_Error SDIO_ProgFirmwareReal()
{
    SD_Error errorstatus = SD_OK;
    u32 scratch;
    u8  *firmware;
    u32 size;
    //u8  chunk_buffer[64];
    //u32 *chunk_header;
    //u32 chunk_size; 
    u8 fn = 1;
    u16 next_block_size;
    u16 prev_next_block_size;
    u32 retry_count;


    /* 
     * sd8686.bin is programmed into flash,
     * starting from address 0x08001000, size 0x1E024 (122916 bytes)
     */
    firmware = (u8 *)0x08001000;
    size = 122916;
    while (size > 0) {
        /* make sure the status is ready before dnld image */
        errorstatus = SDIO_WaitStatus(SDIO_IO_RDY & SDIO_DL_RDY);
        if (errorstatus != SD_OK) {
            return(errorstatus);
        }

        delay();

        if (size <= 2000) {
            size = size;
        }			   

        /* check the helper status by looking at size indication */
        errorstatus = SDIO_GetNextBlockSize(&next_block_size);
        if (errorstatus != SD_OK) {
            return(errorstatus);
        }
        if (next_block_size & 0x01) {
            retry_count = 0;
            firmware -= prev_next_block_size;
            do {			  
                next_block_size = prev_next_block_size;
                errorstatus = SDIO_IORWExtendHelper(firmware, next_block_size);
                if (errorstatus != SD_OK) {
                    return(errorstatus);
                }
                /* check the helper status by looking at size indication */
                errorstatus = SDIO_GetNextBlockSize(&next_block_size);
                if (errorstatus != SD_OK) {
                    return(errorstatus);
                }			    
                retry_count++;
            } while ((next_block_size & 0x01) && retry_count < 10);

            if (retry_count >= 10 || (next_block_size & 0x01)) {
                return(SD_ERROR);
            } else {
                firmware += prev_next_block_size;
            }
        }
        next_block_size = MIN(next_block_size, size);

        errorstatus = SDIO_IORWExtendHelper(firmware, next_block_size);
        if (errorstatus != SD_OK) {
            return(errorstatus);
        }
   			  	    
        size -= next_block_size;
        prev_next_block_size = next_block_size;
        firmware += next_block_size;
    }

    /* 
     * Now check scratch register to wait until
     * firmware is up and running
     */
    retry_count = 0;
    do {
        retry_count++;
        errorstatus = SDIO_ReadScratch(fn, &scratch);
        if (errorstatus != SD_OK) {
            return(errorstatus);
        }
        delay();
    } while (scratch != SDIO_FIRMWARE_OK && retry_count < 2000);

    if (scratch != SDIO_FIRMWARE_OK) {
        return(SD_ERROR);
    }

    return(errorstatus);
}


/* Enable interrupt, get IO port info */
SD_Error SDIO_EnableProbe()
{
    SD_Error errorstatus = SD_OK;

    errorstatus = SDIO_EnableFunc(1);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    errorstatus = SDIO_GetIOPort(1, &sd8686.f1.ioport);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    return(errorstatus);
}

/*******************************************************************************
 * Function Name  : SD_PowerON
 * Description    : Enquires cards about their operating voltage and configures 
 *                  clock controls.
 * Input          : None
 * Output         : None
 * Return         : SD_Error: SD Card Error code.
 *******************************************************************************/
SD_Error SD_PowerON(void)
{
    SD_Error errorstatus = SD_OK;

    /* Power ON Sequence -------------------------------------------------------*/
    /* Configure the SDIO peripheral */
    SDIO_InitStructure.SDIO_ClockDiv = SDIO_INIT_CLK_DIV; /* HCLK = 72MHz, SDIOCLK = 72MHz, SDIO_CK = HCLK/(178 + 2) = 400 KHz */
    SDIO_InitStructure.SDIO_ClockEdge = SDIO_ClockEdge_Rising;
    SDIO_InitStructure.SDIO_ClockBypass = SDIO_ClockBypass_Disable;
    SDIO_InitStructure.SDIO_ClockPowerSave = SDIO_ClockPowerSave_Disable;
    SDIO_InitStructure.SDIO_BusWide = SDIO_BusWide_1b;
    SDIO_InitStructure.SDIO_HardwareFlowControl = SDIO_HardwareFlowControl_Disable;
    SDIO_Init(&SDIO_InitStructure);

    /* Set Power State to ON */
    SDIO_SetPowerState(SDIO_PowerState_ON);

    /* Enable SDIO Clock */
    SDIO_ClockCmd(ENABLE);

    return(errorstatus);
}

/*******************************************************************************
 * Function Name  : SD_PowerOFF
 * Description    : Turns the SDIO output signals off.
 * Input          : None
 * Output         : None
 * Return         : SD_Error: SD Card Error code.
 *******************************************************************************/
SD_Error SD_PowerOFF(void)
{
    SD_Error errorstatus = SD_OK;

    /* Set Power State to OFF */
    SDIO_SetPowerState(SDIO_PowerState_OFF);

    return(errorstatus);
}

SD_Error SDIO_WaitShortResp(u32 cmd, u32 *resp)
{
    SD_Error errorstatus = SD_OK;
    u32 timeout = SDIO_CMD0TIMEOUT;
    u32 status;

    *resp = 0;

    do {
        timeout--;
        status = SDIO->STA;
    } while (!(status & (SDIO_FLAG_CCRCFAIL | SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT)) && (timeout > 0));

    if ((timeout == 0) || (status & SDIO_FLAG_CTIMEOUT)) {
        errorstatus = SD_CMD_RSP_TIMEOUT;
        SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
        return(errorstatus);
    }

    if (status & SDIO_FLAG_CCRCFAIL) {
        errorstatus = SD_CMD_CRC_FAIL;
        SDIO_ClearFlag(SDIO_FLAG_CCRCFAIL);
        return(errorstatus);
    }

    /* Clear all the static flags */
    SDIO_ClearFlag(SDIO_STATIC_FLAGS);

    /* For SEND_OP_COND command, RESPCMD should be 0x3F */
    if (cmd == SDIO_SDIO_SEN_OP_COND) {
        cmd = 0x3F;
    }

    /* Check response received is of desired command */
    if (SDIO_GetCommandResponse() != cmd)
    {
        errorstatus = SD_ILLEGAL_CMD;
        return(errorstatus);
    }

    /* We have received response, retrieve it for analysis  */
    *resp = SDIO_GetResponse(SDIO_RESP1);

    return(errorstatus);
}

SD_Error SDIO_SendIOOpCond(u32 ocr, u32 *rocr)
{
    SD_Error errorstatus = SD_OK;
    u32 response;
    u32 i;

    for (i = 3; i > 0; i--) {
        *rocr = 0;
        /* 
         * CMD5: IO_SEND_OP_COND 
         * SDIO host asks for and sets OP voltage
         * Response: R4
         *      R4_CARD_RDY
         *      R4_IO_OCR
         */
        SDIO_CmdInitStructure.SDIO_Argument = ocr;
        SDIO_CmdInitStructure.SDIO_CmdIndex = SDIO_SDIO_SEN_OP_COND;
        SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
        SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
        SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
        SDIO_SendCommand(&SDIO_CmdInitStructure);

        /*
         * Waiting for response
         */
        errorstatus = SDIO_WaitShortResp(SDIO_SDIO_SEN_OP_COND, &response);
        if (errorstatus != SD_OK) {
            return(errorstatus);
        }
        
        if (response & SDIO_R4_CARD_RDY) {
            errorstatus = SD_OK;
            *rocr = response & SDIO_R4_OCR;
            break;
        } else {
            errorstatus = SD_CMD_RSP_TIMEOUT;
        }
    }

    return(errorstatus);
}

SD_Error SDIO_SendRelativeAddr(u16 *rca)
{
    SD_Error errorstatus = SD_OK;
    u32 response_r1;

    SDIO_CmdInitStructure.SDIO_Argument = 0x00;
    SDIO_CmdInitStructure.SDIO_CmdIndex = SDIO_SET_REL_ADDR;
    SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
    SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
    SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
    SDIO_SendCommand(&SDIO_CmdInitStructure);

    errorstatus = SDIO_WaitShortResp(SDIO_SET_REL_ADDR, &response_r1);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    if (SD_ALLZERO == (response_r1 & (SD_R6_GENERAL_UNKNOWN_ERROR | SD_R6_ILLEGAL_CMD | SD_R6_COM_CRC_FAILED)))
    {
        *rca = (u16) (response_r1 >> 16);
        return(errorstatus);
    }

    if (response_r1 & SD_R6_GENERAL_UNKNOWN_ERROR)
    {
        errorstatus = SD_GENERAL_UNKNOWN_ERROR;
    }

    if (response_r1 & SD_R6_ILLEGAL_CMD)
    {
        errorstatus = SD_ILLEGAL_CMD;
    }

    if (response_r1 & SD_R6_COM_CRC_FAILED)
    {
        errorstatus = SD_COM_CRC_FAILED;
    }

    *rca = 0;
    return(SD_ERROR);
}

SD_Error SDIO_SelectCard(u16 rca)
{
    SD_Error errorstatus = SD_OK;
    u32 response_r1;

    /* Send CMD7 SDIO_SEL_DESEL_CARD */
    SDIO_CmdInitStructure.SDIO_Argument = rca << 16;
    SDIO_CmdInitStructure.SDIO_CmdIndex = SDIO_SEL_DESEL_CARD;
    SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
    SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
    SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
    SDIO_SendCommand(&SDIO_CmdInitStructure);

    errorstatus = SDIO_WaitShortResp(SDIO_SEL_DESEL_CARD, &response_r1);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    if ((response_r1 & SDIO_CS_ERRORBITS) == SD_ALLZERO)
    {
        return(errorstatus);
    }

    if (response_r1 & SDIO_CS_ADDR_OUT_OF_RANGE)
    {
        return(SD_ADDR_OUT_OF_RANGE);
    }

    if (response_r1 & SDIO_CS_COM_CRC_FAILED)
    {
        return(SD_COM_CRC_FAILED);
    }

    if (response_r1 & SDIO_CS_ILLEGAL_CMD)
    {
        return(SD_ILLEGAL_CMD);
    }

    if (response_r1 & SDIO_CS_GENERAL_UNKNOWN_ERROR)
    {
        return(SD_GENERAL_UNKNOWN_ERROR);
    }

    return(SD_ERROR);
}

SD_Error SDIO_IORWDirect(u8 r0w1, u8 fn, u32 regaddr, u8 in_data, u8 *out_data)
{
    SD_Error errorstatus = SD_OK;
    u32 response_r1;

    /* Send CMD52 IO_RW_DIRECT */
    SDIO_CmdInitStructure.SDIO_Argument = (r0w1? 0x80000000 : 0x00000000) |
                                          ((fn & 0x7) << 28) |
                                          ((r0w1 && out_data)?  0x08000000 : 0x00000000) |
                                          ((regaddr & 0x1FFFF) << 9) |
                                          in_data;
    SDIO_CmdInitStructure.SDIO_CmdIndex = SDIO_SDIO_RW_DIRECT;
    SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
    SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
    SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
    SDIO_SendCommand(&SDIO_CmdInitStructure);

    errorstatus = SDIO_WaitShortResp(SDIO_SDIO_RW_DIRECT, &response_r1);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    if ((response_r1 & SDIO_CS_ERRORBITS) == SD_ALLZERO)
    {
        if (out_data != NULL) {
            *out_data = response_r1 & 0xFF;
        }
        return(errorstatus);
    }

    if (response_r1 & SDIO_CS_ADDR_OUT_OF_RANGE)
    {
        return(SD_ADDR_OUT_OF_RANGE);
    }

    if (response_r1 & SDIO_CS_COM_CRC_FAILED)
    {
        return(SD_COM_CRC_FAILED);
    }

    if (response_r1 & SDIO_CS_ILLEGAL_CMD)
    {
        return(SD_ILLEGAL_CMD);
    }

    if (response_r1 & SDIO_CS_GENERAL_UNKNOWN_ERROR)
    {
        return(SD_GENERAL_UNKNOWN_ERROR);
    }

    return(SD_ERROR);
}

SD_Error SDIO_ReadCCCR(SDIO_CCCR_t *cccr)
{
    SD_Error errorstatus = SD_OK;
    u8 data;

    errorstatus = SDIO_IORWDirect(0, 0, SDIO_CCCR_CCCR, 0, &data);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    cccr->sdio_ver = (data & 0xF0) >> 4;
    cccr->cccr_ver = (data & 0x0F);

    errorstatus = SDIO_IORWDirect(0, 0, SDIO_CCCR_SD, 0, &data);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    cccr->sd_ver = data & 0x0F;

    errorstatus = SDIO_IORWDirect(0, 0, SDIO_CCCR_IORx, 0, &data);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    cccr->io_rdy = (data & 0x02) >> 1;

    errorstatus = SDIO_IORWDirect(0, 0, SDIO_CCCR_CAPS, 0, &data);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    cccr->support_direct_cmd = data & 0x01;
    cccr->support_multi_block = (data & 0x02) >> 1;
    cccr->support_4bit_int = (data & 0x10) >> 4;
    cccr->enable_4bit_int = (data & 0x20) >> 5;

    return(errorstatus);
}

SD_Error SDIO_ReadCIS(SDIO_CISTuples_t **head, u8 func_num)
{
    SD_Error errorstatus = SD_OK;
    u32 i, ptr = 0;
    u8 x;
    u8 tpl_code, tpl_link;
    SDIO_CISTuples_t *tmp_cis;

    /* get CIS base pointer */
    for (i = 0; i < 3; i++) {
        errorstatus = SDIO_IORWDirect(0, 0, 
                (SDIO_FBR_BASE(func_num) + SDIO_FBR_CIS + i), 0, &x);
        if (errorstatus != SD_OK) {
            return(errorstatus);
        }
        ptr |= x << (i * 8);
    }

    /* Now read through CIS tuples */
    do {
        /* get tuple code */
        errorstatus = SDIO_IORWDirect(0, 0, ptr++, 0, &tpl_code);
        if (errorstatus != SD_OK) {
            return(errorstatus);
        }
        /* End of Tuple list */
        if (tpl_code == 0xFF) {
            break;
        }
        
        /* allocate space for current tuple */
        if (*head == NULL) {
            *head = (SDIO_CISTuples_t *)rt_malloc(sizeof(SDIO_CISTuples_t));
            tmp_cis = *head;
            tmp_cis->next = NULL;
            tmp_cis->code = tpl_code;
            tmp_cis->size = 0;
            tmp_cis->data = NULL;
            tmp_cis->fn = func_num;
        } else {
            tmp_cis->next = (SDIO_CISTuples_t *)rt_malloc(sizeof(SDIO_CISTuples_t));
            tmp_cis = tmp_cis->next;
            tmp_cis->next = NULL;
            tmp_cis->code = tpl_code;
            tmp_cis->size = 0;
            tmp_cis->data = NULL;
            tmp_cis->fn = func_num;
        }

        /* get tuple size */
        errorstatus = SDIO_IORWDirect(0, 0, ptr++, 0, &tpl_link);
        if (errorstatus != SD_OK) {
            return(errorstatus);
        }
        tmp_cis->data = (u8 *)rt_malloc(tpl_link);
        tmp_cis->size = tpl_link;
        
        for (i = 0; i < tpl_link; i++) {
            errorstatus = SDIO_IORWDirect(0, 0, ptr++, 0, tmp_cis->data+i);
            if (errorstatus != SD_OK) {
                return(errorstatus);
            }           
        }
    } while(1);

    return(errorstatus);
}

static const unsigned char speed_val[16] =
    { 0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80 };
static const unsigned int speed_unit[8] =
    { 10000, 100000, 1000000, 10000000, 0, 0, 0, 0 };

SD_Error SDIO_ProcessCommonCISTuples(SDIO_CISTuples_t *head, SDIO_CardInfo_t *card_info)
{
    SD_Error errorstatus = SD_OK;

    card_info->str_info = NULL;

    /* This is common CIS */
    while (head != NULL) {
        switch (head->code) {
            case 0x15:
                /* Add some code to copy string */
                break;
            case 0x20:
                card_info->vendor = head->data[0] | (head->data[1] << 8);
                card_info->device = head->data[2] | (head->data[3] << 8);
                break;
            case 0x22:
                /* sanity check the first byte */
                if (head->data[0] != 0x00) {
                    errorstatus = SD_ERROR;
                    return(errorstatus);
                }
                card_info->max_block_size = head->data[1] | (head->data[2] << 8);
                card_info->max_speed = speed_val[(head->data[3] >> 3) & 15] *
                    speed_unit[head->data[3] & 7];
                break;
            default:
                break;
        }
        head = head->next;
    }

    return(errorstatus);
}

SD_Error SDIO_ProcessFuncCISTuples(SDIO_CISTuples_t *head, SDIO_Function_t *func)
{
    SD_Error errorstatus = SD_OK;

    /* This is per function's CIS */
    while (head != NULL) {
        switch (head->code) {
            case 0x22:
                /* sanity check the first byte should be 0x01 */
                if (head->data[0] != 0x01) {
                    errorstatus = SD_ERROR;
                    return(errorstatus);
                }
                func->wake_up_support = head->data[1] & 0x01;
                func->sdio_ver = head->data[2];
                func->card_psn = head->data[3] | (head->data[4] << 8) |
                                (head->data[5] << 16) | (head->data[6] << 24);
                func->csa_size = head->data[7] | (head->data[8] << 8) |
                                (head->data[9] << 16) | (head->data[10] << 24);
                func->csa_no_reformat = (head->data[11] & 0x02) >> 1;
                func->csa_write_prot = (head->data[11] & 0x01);
                func->max_block_size = head->data[12] | (head->data[13] << 8);
                /* FIXME 
                 * Check if there is remaining data to parse!
                 */
                
                break;
            default:
                break;
        }
        head = head->next;
    }

    return(errorstatus);
}

SD_Error SDIO_EnableWide()
{
    SD_Error errorstatus = SD_OK;
    u8 x;

    errorstatus = SDIO_IORWDirect(0, 0, SDIO_CCCR_IF, 0, &x);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    x |= SDIO_BUS_WIDTH_4BIT;

    errorstatus = SDIO_IORWDirect(1, 0, SDIO_CCCR_IF, x, NULL);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    /* set STM32 SDIO to 4bit mode */
    SDIO_InitStructure.SDIO_ClockDiv = SDIO_TRANSFER_CLK_DIV; 
    SDIO_InitStructure.SDIO_ClockEdge = SDIO_ClockEdge_Rising;
    SDIO_InitStructure.SDIO_ClockBypass = SDIO_ClockBypass_Disable;
    SDIO_InitStructure.SDIO_ClockPowerSave = SDIO_ClockPowerSave_Disable;
    SDIO_InitStructure.SDIO_BusWide = SDIO_BusWide_4b;
    SDIO_InitStructure.SDIO_HardwareFlowControl = SDIO_HardwareFlowControl_Disable;
    SDIO_Init(&SDIO_InitStructure);

    return(errorstatus);
}

/*******************************************************************************
 * Function Name  : SD_InitializeCards
 * Description    : Intialises all cards or single card as the case may be. 
 *                  Card(s) come into standby state.
 * Input          : None
 * Output         : None
 * Return         : SD_Error: SD Card Error code.
 *******************************************************************************/
SD_Error SD_InitializeCards(void)
{
    SD_Error errorstatus = SD_OK;
    u16 rca = 0x01;
    u32 ocr;
    SDIO_CISTuples_t *cis_head = NULL;
    SDIO_CISTuples_t *cis_tmp;

    /* get the op range */
    errorstatus = SDIO_SendIOOpCond(0, &ocr);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    /* set the op range */
    errorstatus = SDIO_SendIOOpCond(SDIO_VOLTAGE_WINDOW, &ocr);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    /* Send CMD3 SET_REL_ADDR with argument 0 */
    errorstatus = SDIO_SendRelativeAddr(&rca);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    errorstatus = SDIO_SelectCard(rca);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }
    
    errorstatus = SDIO_ReadCCCR(&cccr);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    /* read in common CIS and then parse common CIS */
    errorstatus = SDIO_ReadCIS(&cis_head, 0);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }
    
    errorstatus = SDIO_ProcessCommonCISTuples(cis_head, &sd8686);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    /* free cis_head */
    while (cis_head != NULL) {
        cis_tmp = cis_head->next;
        cis_head->next = NULL;
        rt_free(cis_head);
        cis_head = cis_tmp;
    }

    /* read function 1 CIS, and then parse that */
    errorstatus = SDIO_ReadCIS(&cis_head, 1);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }
    
    errorstatus = SDIO_ProcessFuncCISTuples(cis_head, &sd8686.f1);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    /* free cis_head */
    while (cis_head != NULL) {
        cis_tmp = cis_head->next;
        cis_head->next = NULL;
        rt_free(cis_head);
        cis_head = cis_tmp;
    }

    return(errorstatus);
}

/*******************************************************************************
 * Function Name  : SD_ProcessIRQSrc
 * Description    : Allows to process all the interrupts that are high.
 * Input          : None
 * Output         : None
 * Return         : SD_Error: SD Card Error code.
 *******************************************************************************/
SD_Error SD_ProcessIRQSrc(void)
{
	SD_Error errorstatus = SD_OK;
	u8 x;

    if (SDIO_GetITStatus(SDIO_IT_SDIOIT) != RESET)
    {
	    errorstatus = SDIO_IORWDirect(0, 1, SDIO_CCCR_INTx, 0, &x);
    	if (errorstatus != SD_OK) {
			rt_kprintf("SD_ProcessIRQSrc read interrupt status failed\n");
        	goto out;
    	}
		errorstatus = SDIO_IORWDirect(1, 1, SDIO_CCCR_INTx, ~x, NULL);
    	if (errorstatus != SD_OK) {
			rt_kprintf("SD_ProcessIRQSrc clear interrupt status failed\n");
        	goto out;
    	}
		if (x & 0x8) {
			rt_kprintf("SD_ProcessIRQSrc got overflow error\n");
			goto out;
		}
		if (x & 0x4) {
			rt_kprintf("SD_ProcessIRQSrc got underflow error\n");
			goto out;
		}
		if (x & 0x2) {
			if (sd_waiting_write == RT_TRUE) {
				rt_sem_release(&sd_write_lock);
				rt_kprintf("SD_ProcessIRQSrc ready to download\n");
			}
		}
		if (x & 0x1) {
			rt_sem_release(&sd_read_lock);
			//rt_kprintf("SD_ProcessIRQSrc ready to upload\n");
		}
out:
		SDIO_ClearITPendingBit(SDIO_IT_SDIOIT);
    }

    return(errorstatus);
}

/*******************************************************************************
 * Function Name  : GPIO_Configuration
 * Description    : Configures the SDIO Corresponding GPIO Ports
 * Input          : None
 * Output         : None
 * Return         : None
 *******************************************************************************/
static void GPIO_Configuration(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;

    /* GPIOC and GPIOD Periph clock enable */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD, ENABLE);

    /* Configure PC.08, PC.09, PC.10, PC.11, PC.12 pin: D0, D1, D2, D3, CLK pin */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* Configure PD.02 CMD line */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
}

/*
 * Try to send a single scan request to firmware
 */
/*
SD_Error SDIO_DoScan()
{
    SD_Error errorstatus = SD_OK;
    cmd_ds_802_11_scan_t *scan_cmd;
    u8 *rb = NULL;
    u32 tlen;
    u32 resp_len;
    u8 result[32];

    tlen = sizeof(*scan_cmd)+4;
    rb = (u8 *)rt_malloc(tlen);
    rb[0] = (sizeof(*scan_cmd)+4) & 0xff;
    rb[1] = (((sizeof(*scan_cmd)+4) & 0xff) >> 8) & 0xff;
    rb[2] = 1;
    rb[3] = 0;

    scan_cmd = (cmd_ds_802_11_scan_t *)(&rb[4]);
    scan_cmd->bsstype = 0x03;
    scan_cmd->bssid[0] = 0x00;
    scan_cmd->bssid[1] = 0x00;
    scan_cmd->bssid[2] = 0x00;
    scan_cmd->bssid[3] = 0x00;
    scan_cmd->bssid[4] = 0x00;
    scan_cmd->bssid[5] = 0x00;
    scan_cmd->hdr.size = 15;
    scan_cmd->hdr.command = CMD_802_11_SCAN;
    //scan_cmd->hdr.command = 0xFFFF;
    scan_cmd->hdr.seqnum = 0;
    scan_cmd->hdr.result = 0;

    errorstatus = SDIO_WaitStatus(SDIO_IO_RDY & SDIO_DL_RDY);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    errorstatus = SDIO_IORWExtendHelper(rb, tlen);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    delay();

    errorstatus = SDIO_WaitStatus(SDIO_UL_RDY);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    errorstatus = SDIO_ReadScratch(1, &resp_len);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    //resp_len += 4;

    errorstatus = SDIO_IORWExtendReadHelper(result, resp_len);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    return(errorstatus);
}
*/

SD_Error SDIO_DoSetMac()
{
    SD_Error errorstatus = SD_OK;
    cmd_ds_802_11_mac_t *mac_cmd;
    u8 *rb = NULL;
    u32 tlen;
    u32 resp_len;
    u8 result[32];

    tlen = sizeof(*mac_cmd)+4;
    rb = (u8 *)rt_malloc(tlen);
    rb[0] = (sizeof(*mac_cmd)+4) & 0xff;
    rb[1] = (((sizeof(*mac_cmd)+4) & 0xff) >> 8) & 0xff;
    rb[2] = 1;
    rb[3] = 0;

    mac_cmd = (cmd_ds_802_11_mac_t *)(&rb[4]);
    mac_cmd->mac_addr[0] = 0x01;
    mac_cmd->mac_addr[1] = 0x02;
    mac_cmd->mac_addr[2] = 0x03;
    mac_cmd->mac_addr[3] = 0x04;
    mac_cmd->mac_addr[4] = 0x05;
    mac_cmd->mac_addr[5] = 0x06;
    mac_cmd->action = 1;
    mac_cmd->hdr.size = sizeof(*mac_cmd);
    mac_cmd->hdr.command = CMD_802_11_MAC_ADDR;
    mac_cmd->hdr.seqnum = 1;
    mac_cmd->hdr.result = 0;

    errorstatus = SDIO_WaitStatus(SDIO_IO_RDY & SDIO_DL_RDY);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    errorstatus = SDIO_IORWExtendHelper(rb, tlen);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }
	delay();

    errorstatus = SDIO_WaitStatus(SDIO_UL_RDY);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    errorstatus = SDIO_ReadScratch(1, &resp_len);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    //resp_len += 4;

    errorstatus = SDIO_IORWExtendReadHelper(result, resp_len);
    if (errorstatus != SD_OK) {
        return(errorstatus);
    }

    return(errorstatus);
}

void rt_hw_sdcard_init()
{
    NVIC_InitTypeDef NVIC_InitStructure;
	SD_Error errorstatus = SD_OK;
	u8 x;

	if (SD_Init() == SD_OK) {
		rt_kprintf("Marvell 88W8686 initialize OK!\n");
	} else {
	 	rt_kprintf("Marvell 88W8686 init failed...\n");
	} 	

	/* initialize read/write semaphore */
	if (rt_sem_init(&sd_read_lock, "sd_read_lock", 0, 0) != RT_EOK)
    {
    	rt_kprintf("init sd read lock semaphore failed\n");
    }

	if (rt_sem_init(&sd_write_lock, "sd_write_lock", 0, 0) != RT_EOK)
    {
    	rt_kprintf("init sd write lock semaphore failed\n");
    }

	/* Now enable SDIO interrupt */
	/* First enable interrupt on host */
    NVIC_InitStructure.NVIC_IRQChannel = 49;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

	// STM32 is working in PIO mode, only allow
	// card to interrupt us.
	SDIO_ITConfig(SDIO_IT_SDIOIT, ENABLE);

	errorstatus = SDIO_IORWDirect(0, 0, SDIO_CCCR_IENx, 0, &x);
    if (errorstatus != SD_OK) {
        rt_kprintf("Error enable interrupt\n");
    }

	if (SDIO_IORWDirect(1, 1, SDIO_CCCR_IENx, 0x0F, NULL) != SD_OK) {
	 	rt_kprintf("Error enable interrupt\n");
	}

	//if (SDIO_IORWDirect(1, 1, SDIO_HOST_INTR_RSR, 0x0F, NULL) != SD_OK) {
	// 	rt_kprintf("Error enable interrupt\n");
	//}

	if (SDIO_IORWDirect(1, 1, SDIO_HOST_INTR_MSK, 0x0F, NULL) != SD_OK) {
	 	rt_kprintf("Error enable interrupt\n");
	}

}
/******************* (C) COPYRIGHT 2008 STMicroelectronics *****END OF FILE****/
