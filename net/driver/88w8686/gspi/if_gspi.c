/******************************************************************************
 *
 * Define the bottom layer GSPI I/O operations. During initialization
 * we register these operations with the layer above us, then all the
 * high level operations should be able to map to this bottom layer
 * transparently
 ******************************************************************************/

#include "if_gspi.h"

#define DEBUG_PKT 1		     

static rt_uint8_t use_fast_read = 0;
static rt_uint8_t cmd_dnld_rdy = 0;
static rt_uint8_t dummy_dma_tx = 0;
static struct rt_semaphore dma_send_sem;
static struct rt_semaphore dma_recv_sem;
static struct rt_semaphore cmd_upld_sem;
static struct rt_semaphore card_evt_sem;
static struct rt_semaphore data_dnld_sem;
static struct pbuf pbuf_tx_hdr;
static tx_pkt_desc_t *tx_desc_p;	   

///////////////////////////////////////////////////////////////////////////////
//                     I n i t i a l i z a t i o n                           //
///////////////////////////////////////////////////////////////////////////////

static void RCC_Configuration (void)
{
    /* enable spi2 clock */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    /* enable gpiob port clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);

	/* DMA clock enable */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
}

static void NVIC_Configuration (void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    /* Enable the EXTI0 Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

	/* Enable DMA interrupts */
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

static void DMA_Configuration (void)
{
	DMA_InitTypeDef DMA_InitStructure;

	/* cleanup old state */
	DMA_Cmd(SPI2_RX_DMA, DISABLE);
	DMA_DeInit(SPI2_RX_DMA);
	DMA_InitStructure.DMA_PeripheralBaseAddr = (rt_uint32_t)&(SPI2->DR);
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	/* memory address is initialized to ZERO */
    DMA_InitStructure.DMA_MemoryBaseAddr = (rt_uint32_t)0;
    DMA_InitStructure.DMA_BufferSize = 0;
    DMA_Init(SPI2_RX_DMA, &DMA_InitStructure);
    DMA_ITConfig(SPI2_RX_DMA, DMA_IT_TC | DMA_IT_TE, ENABLE);
    DMA_ClearFlag(DMA1_FLAG_TC4);

	DMA_Cmd(SPI2_RX_DMA, ENABLE);

	/* cleanup old state */
	DMA_Cmd(SPI2_TX_DMA, DISABLE);
	DMA_DeInit(SPI2_TX_DMA);
	DMA_InitStructure.DMA_PeripheralBaseAddr = (rt_uint32_t)&(SPI2->DR);
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
	/* memory address is initialized to ZERO */
    DMA_InitStructure.DMA_MemoryBaseAddr = (rt_uint32_t)0;
    DMA_InitStructure.DMA_BufferSize = 0;
    DMA_Init(SPI2_TX_DMA, &DMA_InitStructure);
    DMA_ITConfig(SPI2_TX_DMA, DMA_IT_TC | DMA_IT_TE, ENABLE);
    DMA_ClearFlag(DMA1_FLAG_TC5);

	DMA_Cmd(SPI2_TX_DMA, ENABLE);
} 

static void GPIO_Configuration (void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;

    /* configure PC7 as external interrupt */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* Configure SPI2 pins:  SCK, MISO and MOSI ----------------------------*/
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	  
    GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* 88W8686 CS signal */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* Connect 88W8686 INT Line to GPIOB Pin 0 */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource7);

    /* Configure 88W8686 INT Line to generate an interrupt on falling edge */
    EXTI_InitStructure.EXTI_Line = EXTI_Line7;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* Clear the Key Button EXTI line pending bit */
    EXTI_ClearITPendingBit(EXTI_Line7);
}

static void SPI_Configuration (void)
{
    SPI_InitTypeDef SPI_InitStructure;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_16b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &SPI_InitStructure);
    SPI_Cmd(SPI2, ENABLE);
}

///////////////////////////////////////////////////////////////////////////////
//                  B a s i c   I / O   O p e r a t i o n s                  //
///////////////////////////////////////////////////////////////////////////////

static void cs_disable (void)
{
	GPIOC->BSRR = GPIO_Pin_6;
}

static void cs_enable (void)
{
	GPIOC->BRR = GPIO_Pin_6;
}

static rt_uint32_t read_reg (rt_uint16_t addr)
{
	rt_uint32_t ret_val = 0;
	rt_uint32_t counter;
	rt_uint16_t spi_rx = 0;

	cs_enable();

    if (use_fast_read == 0) {
        for (counter = 0; counter < 5; counter++) {
            SPI_I2S_SendData(SPI2, addr);
            while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);
        }
    }

	SPI_I2S_SendData(SPI2, addr);
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE)!=SET);
	SPI_I2S_ReceiveData(SPI2);

    if (use_fast_read != 0) {
        /* short delay */
        for (counter = 0; counter < 10; counter++);
    }

	SPI_I2S_SendData(SPI2, addr);
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE)!=SET);
	spi_rx = SPI_I2S_ReceiveData(SPI2);

	ret_val = spi_rx;

	SPI_I2S_SendData(SPI2, addr);		   
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE)!=SET);
	spi_rx = SPI_I2S_ReceiveData(SPI2);

	ret_val |= (spi_rx << 16);

	cs_disable();

	return ret_val;
}

rt_uint32_t debug_read_reg (rt_uint16_t addr)   
{
	return read_reg(addr);
}

static void write_reg (rt_uint16_t addr, rt_uint32_t val)
{
	cs_enable();

	SPI_I2S_SendData(SPI2, (0x8000 | addr));    
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);

	SPI_I2S_SendData(SPI2, (val & 0xffff));
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);

	SPI_I2S_SendData(SPI2, (val >> 16));
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);

	cs_disable();
}						 			  				 

static void dma_send (rt_uint16_t reg, rt_uint8_t *data, rt_uint16_t len)
{
	cs_enable();
	SPI_I2S_SendData(SPI2, (0x8000 | reg));     
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);

	DMA_Cmd(SPI2_TX_DMA, DISABLE);
	SPI2_TX_DMA->CMAR = (rt_uint32_t)data;
	SPI2_TX_DMA->CNDTR = (len + 1)/2;
	DMA_Cmd(SPI2_TX_DMA, ENABLE);

	SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);

	/* Waiting on DMA transfer done signal */
	if (rt_sem_take(&dma_send_sem, RT_WAITING_FOREVER) != RT_EOK) {
		rt_kprintf("dma_send failed\n");
	}										  

	cs_disable();

	/* signal card interrupt */
	if (reg == SPI_CMD_PORT) {
		write_reg(SPI_CARD_INTR_CAUSE, SCIC_CMD_DNLD_OVR);
	} else if (reg == SPI_DATA_PORT) {
		write_reg(SPI_CARD_INTR_CAUSE, SCIC_CMD_TX_DNLD_OVR);
	}
}		 

static void dma_recv (rt_uint16_t reg, rt_uint8_t *data, rt_uint16_t len)
{
	rt_uint8_t *tmp_buffer;

	tmp_buffer = (rt_uint8_t *)rt_malloc(len+1);
	if (tmp_buffer == NULL) {
		rt_kprintf("Failed to rt_malloc!\n");
		return;
	}
	rt_memset(tmp_buffer, 0, (len+1));

	cs_enable();
	SPI_I2S_SendData(SPI2, reg);     
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);
	SPI_I2S_ReceiveData(SPI2);

	DMA_Cmd(SPI2_RX_DMA, DISABLE);
	SPI2_RX_DMA->CMAR = (rt_uint32_t)data;
	SPI2_RX_DMA->CNDTR = (len + 1)/2;						   
	DMA_Cmd(SPI2_RX_DMA, ENABLE);

	SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, ENABLE);

	/* need to setup dummy dma tx */
	dummy_dma_tx = 1;	
	DMA_Cmd(SPI2_TX_DMA, DISABLE);
	SPI2_TX_DMA->CMAR = (rt_uint32_t)tmp_buffer;  
	SPI2_TX_DMA->CNDTR = (len + 1)/2;
	DMA_Cmd(SPI2_TX_DMA, ENABLE);

	SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);

	/* Waiting on DMA transfer done signal */					  
	if (rt_sem_take(&dma_recv_sem, RT_WAITING_FOREVER) != RT_EOK) {
		rt_kprintf("dma_recv failed\n");
	}										  

	cs_disable();

	/* signal card interrupt */
	if (reg == SPI_CMD_PORT) {
		write_reg(SPI_CARD_INTR_CAUSE, SCIC_CMD_UPLD_OVR);
	} else if (reg == SPI_DATA_PORT) {
		write_reg(SPI_CARD_INTR_CAUSE, SCIC_CMD_RX_UPLD_OVR);
	}
}

static void read_data_direct(rt_uint16_t reg, rt_uint8_t *data, 
							rt_uint16_t len)
{
	rt_uint16_t *cur_ptr_16;
	rt_uint32_t counter;

	cs_enable();

	SPI_I2S_SendData(SPI2, reg);    
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);

	cur_ptr_16 = (rt_uint16_t *)data;
	for (counter = 0; counter < (len+1)/2; counter++) {
		SPI_I2S_SendData(SPI2, reg);
		while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE)!=SET);
		cur_ptr_16[counter] = SPI_I2S_ReceiveData(SPI2);
	}

	cs_disable();
}				

static void delay_n (rt_uint32_t n)
{
	rt_uint32_t	i;
	for (i = 0; i < n; i++);
}

static rt_err_t wait_cmd_dnld_rdy (void)
{
#define HOST_INTR_STUS_RETRY	10
	rt_uint32_t cnt = 0;

    do {
        if (cmd_dnld_rdy == 1 || 
                (read_reg(SPI_HOST_INT_STATUS_REG) & SHISR_CMD_DNLD_RDY)) {
            cmd_dnld_rdy = 0;
            return RT_EOK;
        }
        delay_n(100);
        cnt++;
    } while (cnt < HOST_INTR_STUS_RETRY);

    return -RT_ERROR;
}

///////////////////////////////////////////////////////////////////////////////
//                                 I n i t                                   //
///////////////////////////////////////////////////////////////////////////////

static void dev_sw_init (void)
{
    rt_sem_init(&(dma_send_sem), "dma_send_sem", 0, 0);
    rt_sem_init(&(dma_recv_sem), "dma_recv_sem", 0, 0);
    rt_sem_init(&(cmd_upld_sem), "cmd_upld_sem", 0, 0);
    rt_sem_init(&(card_evt_sem), "card_evt_sem", 0, 0);	  
    rt_sem_init(&(data_dnld_sem), "data_dnld_sem", 0, 0);
	pbuf_tx_hdr.next = NULL;
	pbuf_tx_hdr.payload = rt_malloc(sizeof(tx_pkt_desc_t));
	if (pbuf_tx_hdr.payload == NULL) {
		rt_kprintf("dev_sw_init: rt_malloc failed!\n");
		return;							   
	}
	rt_memset(pbuf_tx_hdr.payload, 0, sizeof(tx_pkt_desc_t));
	tx_desc_p = (tx_pkt_desc_t *)pbuf_tx_hdr.payload;
	tx_desc_p->tx_packet_location = sizeof(tx_pkt_desc_t);
	pbuf_tx_hdr.len = sizeof(tx_pkt_desc_t);
	pbuf_tx_hdr.tot_len = sizeof(tx_pkt_desc_t);
}

static void dev_io_init (void)
{
	RCC_Configuration();
	NVIC_Configuration();
	GPIO_Configuration();
	DMA_Configuration();		    

   	/* disable cs by default */
	cs_disable();			   

	SPI_Configuration();		 
}

static void dev_init (void)
{
    /* configure delay mode to delay by time, and update flag */   
    write_reg(SPI_SPU_BUS_MODE, 
                (read_reg(SPI_SPU_BUS_MODE) & (~SSBM_DELAY_METHOD)));
    use_fast_read = 1;
}

static void pio_send(rt_uint16_t reg, rt_uint8_t *data, 
						rt_uint16_t len)
{
	rt_uint16_t *cur_ptr_16;
	rt_uint32_t counter;

	cs_enable();

	SPI_I2S_SendData(SPI2, (0x8000 | reg));    
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);

	cur_ptr_16 = (rt_uint16_t *)data;
	for (counter = 0; counter < (len+1)/2; counter++) {
		SPI_I2S_SendData(SPI2, *cur_ptr_16);
		cur_ptr_16++;
		while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);
	}

	cs_disable();
}						

static rt_err_t download_helper (void)			    
{
#define SPI_HELPER_SIZE			2176
#define SPI_HELPER_START_ADDR	0x08000000		
	rt_uint8_t *helper_ptr = (rt_uint8_t *)SPI_HELPER_START_ADDR;
	rt_uint32_t	helper_cnt;
	rt_uint32_t delta_len;

	for (helper_cnt = 0; helper_cnt < SPI_HELPER_SIZE;
		helper_cnt += FIRMWARE_DNLD_PCKCNT) {
		delta_len = (SPI_HELPER_SIZE - helper_cnt) < FIRMWARE_DNLD_PCKCNT ?
					(SPI_HELPER_SIZE - helper_cnt) : FIRMWARE_DNLD_PCKCNT;
        /* write length */
		write_reg(SPI_SCRATCH_1, delta_len);
		if (wait_cmd_dnld_rdy() != RT_EOK) {
			return -RT_ERROR;
		}
        /* output data */
		pio_send(SPI_CMD_PORT, helper_ptr, delta_len);
		helper_ptr += delta_len;
		write_reg(SPI_CARD_INTR_CAUSE, SCIC_CMD_DNLD_OVR);
		delay_n(1000);
	}

    /* signal End-Of-Helper */
	write_reg(SPI_SCRATCH_1, 0x0000);	
	if (wait_cmd_dnld_rdy() != RT_EOK) {
		return -RT_ERROR;
	}
	write_reg(SPI_CARD_INTR_CAUSE, SCIC_CMD_DNLD_OVR);
	
	return RT_EOK;				 
}

static rt_err_t download_firmware (void)
{
#define SPI_FIRMWARE_SIZE			119696
#define SPI_FIRMWARE_START_ADDR	0x08001000
	rt_uint8_t *firmware_ptr = (rt_uint8_t *)SPI_FIRMWARE_START_ADDR;
	rt_uint8_t *prev_firmware_ptr = NULL;
	rt_uint8_t  retry_loop = 0;
	rt_uint32_t delta_len;
	rt_uint32_t prev_delta_len = 0;
	rt_uint32_t len_left = SPI_FIRMWARE_SIZE;
	
	delay_n(10000);

	for (;;) {	  
		if (len_left == 0) {
			break;
		}
							 	   
		if (wait_cmd_dnld_rdy() != RT_EOK) {
			return -RT_ERROR;
		}
		
		retry_loop = 0;					 
		do {
			delay_n(10);
			delta_len = read_reg(SPI_SCRATCH_1);
			retry_loop++;
			if (retry_loop == 255) {
				return -RT_ERROR;
			}
		} while (!delta_len);

		if (delta_len & 0x1) {
			if (prev_delta_len == 0) {
				delta_len &= ~1;
			} else {
			 	rt_kprintf("Re-download previous block: 0x%x len %d\n", 
								prev_firmware_ptr, prev_delta_len);
				firmware_ptr = prev_firmware_ptr;
				delta_len = prev_delta_len;
				len_left += prev_delta_len;
			}			
		}									 	   

		dma_send(SPI_CMD_PORT, firmware_ptr, delta_len);
		delay_n(1000);
		
		prev_firmware_ptr = firmware_ptr;
		prev_delta_len = delta_len;
							    
		len_left -= delta_len;
		firmware_ptr += delta_len;
	}										 
	return 0;			   
}

/* 
 * Download firmware and wait until cmd_dnld_rdy
 */
static rt_err_t firmware_init (void)
{
	int count = 0;

	if (download_helper() != RT_EOK) {
		return -RT_ERROR;
	}

	if (download_firmware() != RT_EOK) {
		return -RT_ERROR;
	}

	while ((read_reg(SPI_SCRATCH_4) != 0x88888888) &&
			count < 10000) {		 
		delay_n(100);
		count++;
	}

	if (count >= 10000) {
		rt_kprintf("didn't see 88888888 flag\n");
		return -RT_ERROR;
	}

    count = 0;
    do {
        if (cmd_dnld_rdy == 1) {
			rt_kprintf("wifi firmware status OK!\n");
            return RT_EOK;
        } else {
            delay_n(100);
        }
        count++;
    } while (count < 1000);
    
	return -RT_ERROR;
}

///////////////////////////////////////////////////////////////////////////////
//                         E x p o r t e d   A P I s                         //
///////////////////////////////////////////////////////////////////////////////

rt_err_t spi_wifi_init (void)
{
    dev_sw_init();
    dev_io_init();
    dev_init();
    return firmware_init();
}

/*
 * This handler is hard coded in DMA1_Channel5_IRQHandler
 */						 
void spi2_dma_send_irq (void)		   
{
	if (DMA_GetITStatus(DMA1_IT_TC5)) {
		if (dummy_dma_tx == 0) {
			rt_sem_release(&dma_send_sem);
		} else {
			dummy_dma_tx = 0;
		}
	}
	if (DMA_GetITStatus(DMA1_IT_TE5)) {
		rt_kprintf("SPI2 DMA TX Error\n");
	}
	DMA_ClearFlag(DMA1_FLAG_TC5 | DMA1_FLAG_TE5);
}

/*
 * This handler is hard coded in DMA1_Channel4_IRQHandler
 */
void spi2_dma_recv_irq (void)
{
	if (DMA_GetITStatus(DMA1_IT_TC4)) {
		rt_sem_release(&dma_recv_sem);
	}
	if (DMA_GetITStatus(DMA1_IT_TE4)) {
		rt_kprintf("SPI2 DMA RX Error\n");
	}
	DMA_ClearFlag(DMA1_FLAG_TC4 | DMA1_FLAG_TE4);
}

void spi2_isr (void)
{
    rt_uint32_t int_status, enable_mask;
    int_status = read_reg(SPI_HOST_INT_STATUS_REG);

    if (int_status == 0xffffffff) {
	    write_reg(SPI_HOST_INT_STATUS_REG, 0);
		return;
	}

    enable_mask = ~int_status;

    /* Error reporting */
 	if (int_status & SHISR_CMD_RD_FIFO_UNDRFLOW) {
		rt_kprintf("SHISR_CMD_RD_FIFO_UNDRFLOW\n");
	}
	if (int_status & SHISR_CMD_WR_FIFO_OVRFLOW) {
		rt_kprintf("SHISR_CMD_WR_FIFO_OVRFLOW\n");
	}
	if (int_status & SHISR_DATA_RD_FIFO_UNDRFLOW) {
		rt_kprintf("SHISR_DATA_RD_FIFO_UNDRFLOW\n");
	}
	if (int_status & SHISR_DATA_WR_FIFO_OVRFLOW) {
		rt_kprintf("SHISR_DATA_WR_FIFO_OVRFLOW\n");
	}
	if (int_status & SHISR_IO_RD_FIFO_UNDRFLOW) {
		rt_kprintf("SHISR_IO_RD_FIFO_UNDRFLOW\n");
	}
	if (int_status & SHISR_IO_WR_FIFO_OVRFLOW) {
		rt_kprintf("SHISR_IO_WR_FIFO_OVRFLOW\n");
	}

	if (int_status & SHISR_CMD_UPLD_RDY) {
		//rt_kprintf("SHISR_CMD_UPLD_RDY\n");
		rt_sem_release(&cmd_upld_sem);
	}
	if (int_status & SHISR_CARD_EVENT) {
        rt_sem_release(&card_evt_sem);
	}
	if (int_status & SHISR_CMD_DNLD_RDY) {
        cmd_dnld_rdy = 1;
	}
	if (int_status & SHISR_RX_UPLD_RDY) {
#ifdef DEBUG_PKT
		rt_kprintf("SHISR_RX_UPLD_RDY\n");
#endif
        /*
         * eth_system_device_init is invoked before
         * rt_device_init_all, so eth_rx_thread_mb should
         * have already been initialized
         */
        if (net_dev_drv_notify_rx() != RT_EOK) {
            rt_kprintf("Notify ethernet rx ready failed...\n");
        }

        /* 
         * disable data receive interrupt until rx thread 
         * retrieve that data 
         */
        enable_mask |= SHISR_RX_UPLD_RDY;
	}
	if (int_status & SHISR_TX_DNLD_RDY) {
#ifdef DEBUG_PKT
		rt_kprintf("SHISR_TX_DNLD_RDY\n");
#endif
        rt_sem_release(&data_dnld_sem);
	}

	write_reg(SPI_HOST_INT_STATUS_REG, enable_mask);
}

/*
 * CMD request
 */
rt_err_t send_cmd (rt_uint8_t *data, rt_uint16_t len)
{
    if (wait_cmd_dnld_rdy() != RT_EOK) {
        return -RT_ERROR;
    }

    dma_send(SPI_CMD_PORT, data, len);
    return RT_EOK;
}
				   
/*		    
 * CMD response
 */
rt_uint16_t get_cmd_resp_len (void)
{
    rt_uint16_t len;					 					   

    if (rt_sem_take(&cmd_upld_sem, RT_WAITING_FOREVER) != RT_EOK) {
        return 0;
    }

	len = read_reg(SPI_SCRATCH_2);
   	if (len > WLAN_UPLD_SIZE) {
    	len = 0;
    }
    return len;
}

void recv_cmd_resp (rt_uint8_t *data, rt_uint16_t len)
{
    dma_recv(SPI_CMD_PORT, data, len);
	//read_data_direct(SPI_CMD_PORT, data, len);
}

/*
 * Send out pbuf using DMA, we handle
 * segmented pbuf accordingly
 */
rt_err_t dma_send_pbuf (struct pbuf *p)
{
    rt_uint8_t hw_aligned = 1;
    struct pbuf *q = p;
    rt_uint8_t *buf = NULL;
    rt_uint8_t *tmp_buf = NULL;
	rt_err_t ret = RT_EOK;
	tx_pkt_desc_t *tx_desc_p = pbuf_tx_hdr.payload;
#ifdef DEBUG_PKT
	struct pbuf *p_dbg = p;
	rt_uint16_t dbg_count;
	rt_kprintf("Sending pkt tot_len: %d\n", p_dbg->tot_len);
	while (p_dbg != NULL) {
		for (dbg_count = 0; dbg_count < p_dbg->len; dbg_count++) {
			rt_kprintf("%2x ", 
			((rt_uint8_t *)(p_dbg->payload))[dbg_count]);
		}
		p_dbg = p_dbg->next;
	}
	rt_kprintf("\n\n");
#endif

	if (p == NULL) {
		return -RT_ERROR;
	}
	/*
	 * Append dummy header in front of the packet, this
	 * is required by 88W8686 firmware
	 */
	rt_memcpy(tx_desc_p->tx_dest_addr_high, p->payload, ETH_ALEN);
	tx_desc_p->tx_packet_length = p->tot_len;
	
	pbuf_tx_hdr.next = p;
	pbuf_tx_hdr.tot_len = p->tot_len + pbuf_tx_hdr.len;
	p = &pbuf_tx_hdr;

    /*
     * we need to handle two bytes alignment carefully:
     * If the pbuf list elem num is 1, no need for check
     * If the pbuf list contains multiple elems, check 
     * each len:
     *      if all len is even, then ok
     *      else
     *          allocate tot_len space, and copy data
     *          into this buffer
     */
	while (q) {
        /* we do allow the odd last segment */
        if (((q->len%2) == 1) && (q->next != NULL)) {
            hw_aligned = 0;
            break;
        }
        q = q->next;
    }
    /* if not halfword aligned, prepare a big buffer */
    if (hw_aligned == 0) {
        buf = (rt_uint8_t *)rt_malloc(p->tot_len);
        if (buf == NULL) {
			pbuf_tx_hdr.next = NULL;
            return -RT_ERROR;
        }
        q = p;
        tmp_buf = buf;
        while (q) {
            rt_memcpy(tmp_buf, q->payload, q->len);
            tmp_buf += q->len;
            q = q->next;
        }
        /* add tail blank for the last two bytes */
        dma_send(SPI_DATA_PORT, buf, (p->tot_len + 2));
        rt_free(buf);
		pbuf_tx_hdr.next = NULL;
        return RT_EOK;
    }
    /* else, then fall through segmented DMA send */

    q = p;
	cs_enable();
	SPI_I2S_SendData(SPI2, (0x8000 | SPI_DATA_PORT));     
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);

    while (q) {
        DMA_Cmd(SPI2_TX_DMA, DISABLE);
        SPI2_TX_DMA->CMAR = (rt_uint32_t)q->payload;
		if (q->next == NULL) {
			SPI2_TX_DMA->CNDTR = (q->len + 1)/2 + 1;
		} else {
        	SPI2_TX_DMA->CNDTR = (q->len + 1)/2;
		}
        DMA_Cmd(SPI2_TX_DMA, ENABLE);

        SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE); 

        /* Waiting on DMA transfer done signal */
        if (rt_sem_take(&dma_send_sem, RT_WAITING_FOREVER) != RT_EOK) {
            rt_kprintf("dma_send failed\n");
			ret = -RT_ERROR;
        }										  
        q = q->next;
    }
	cs_disable();

	/* signal card interrupt */
    write_reg(SPI_CARD_INTR_CAUSE, SCIC_CMD_TX_DNLD_OVR);
	pbuf_tx_hdr.next = NULL;

	return ret;
}

/*
 * Get rid of pkt descriptor in front of the pkt
 */
void *dma_recv_pbuf (pkt_rsrc_alloc_func pkt_rsrc_alloc)
{
    rt_uint32_t enable_mask = ~SHISR_RX_UPLD_RDY;
	rt_uint8_t *raw_data;
	void *ret = NULL;
	rx_pkt_desc_t *rx_desc;
	rt_uint8_t *payload_p, *src_payload;
	rt_uint16_t payload_len;
	rt_uint16_t tot_len = read_reg(SPI_SCRATCH_1);

#ifdef DEBUG_PKT
	rt_uint16_t dbg_count;
#endif

	/* 
	 * eth_rx may continuously poll this handle
	 * to see if there is any more data to receive
	 */
	if (tot_len == 0) {
		return NULL;
	}	

	raw_data = (rt_uint8_t *)rt_malloc(tot_len);
	if (raw_data == NULL) {
		rt_kprintf("dma_recv_pbuf: failed to malloc %d\n", tot_len);
		return NULL;
	}

    dma_recv(SPI_DATA_PORT, raw_data, tot_len);

#ifdef DEBUG_PKT
	rt_kprintf("Receiving pkt, tot_len: %d\n", tot_len);
	for (dbg_count = 0; dbg_count < tot_len; dbg_count++) {
		rt_kprintf("%2x ", raw_data[dbg_count]);   
	}
	rt_kprintf("\n\n");
	rt_kprintf("Now re-enable RX_UPLD_RDY interrupt\n");
#endif
	/* check with the pkt length etc. */
	rx_desc = (rx_pkt_desc_t *)raw_data;
	payload_len = rx_desc->rx_pkt_len;
	src_payload = ((rt_uint8_t *)rx_desc) + rx_desc->rx_pkt_offset;

	ret = pkt_rsrc_alloc(&payload_p, payload_len);
	if (payload_p != NULL) {
		rt_memcpy(payload_p, src_payload, payload_len);
	}

	rt_free(raw_data);

    /* enable data rx_upld_rdy interrupt */
	write_reg(SPI_HOST_INT_STATUS_REG, enable_mask);

	return ret;
}

void w8686_spi_register_handler (void)
{
    w8686_register_interface_handler (spi_wifi_init,
                                        send_cmd,
                                        get_cmd_resp_len,
                                        recv_cmd_resp,
                                        dma_send_pbuf,
                                        dma_recv_pbuf);
}									  

///////////////////////////////////////////////////////////////////////////////
//                         D e b u g    C m d s                              //
///////////////////////////////////////////////////////////////////////////////

void w8686_spi_debug_cmd (rt_uint8_t *data, rt_uint16_t len)
{
	rt_uint16_t resp_len, idx;
	rt_uint8_t *resp_data;

	send_cmd(data, len);

	rt_kprintf("cmd sent\n");
	resp_len = get_cmd_resp_len();

	if (resp_len == 0) {
		rt_kprintf("Error: read response size 0\n");
		return;
	}

	resp_data = (rt_uint8_t *)rt_malloc(resp_len);
	if (resp_data == NULL) {
		rt_kprintf("Error: failed to malloc\n");
		return;
	}

	recv_cmd_resp(resp_data, resp_len);
	rt_kprintf("Got response:\n");
	for (idx = 0; idx < resp_len; idx++) {
		rt_kprintf("%x ", resp_data[idx]);
	}
	rt_kprintf("\n");
	rt_free(resp_data);
}	

void w8686_send_manual_pkt (void)
{
	rt_uint8_t pkt[] = {
		//0x00, 0x0d, 0x60, 0x7e, 0xe8, 0x14,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0x00, 0x0b, 0x6c, 0x89, 0xbd, 0x60, 0x00, 0x36, 0xaa, 0xaa,
		0x03, 0x00, 0x00, 0x00, 0x08, 0x06, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x02, 0x00, 0x0b,
		0x6c, 0x89, 0xbd, 0x60, 0xc0, 0xa8, 0x00, 0x6d, 0x00, 0x0d, 0x60, 0x7e, 0xe8, 0x14, 0xc0, 0xa8,
		0x00, 0x6b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00};

	struct pbuf pkt_pbuf;

	pkt_pbuf.tot_len = sizeof(pkt);
	pkt_pbuf.len = sizeof(pkt);
	pkt_pbuf.next = NULL;
	pkt_pbuf.payload = (void *)pkt;

	dma_send_pbuf(&pkt_pbuf);
} 					 
