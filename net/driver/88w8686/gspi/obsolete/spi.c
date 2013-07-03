/****************************************
 *	 Support SPI on STM32105xxx
 *
 ****************************************/

#include "spi.h"

int spi_read_mode 		= 0;
int spi_isr_status 		= 0;
struct rt_semaphore rt_hw_spi_dma_tx_done;
struct rt_semaphore rt_hw_spi_dma_rx_done;
struct rt_semaphore rt_hw_spi_cmd_upld_sem;
struct rt_semaphore rt_hw_spi_rx_upld_sem;
int dummy_dma_tx		= 0;

static void RCC_Configuration(void)
{
    /* enable spi2 clock */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    /* enable gpiob port clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);

	/* DMA clock enable */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
}

static void NVIC_Configuration(void)
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

static void DMA_Configuration()
{
	DMA_InitTypeDef DMA_InitStructure;

	/* cleanup old state */
	DMA_Cmd(SPI2_RX_DMA, DISABLE);
	DMA_DeInit(SPI2_RX_DMA);
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(SPI2->DR);
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	/* memory address is initialized to ZERO */
    DMA_InitStructure.DMA_MemoryBaseAddr = (u32)0;
    DMA_InitStructure.DMA_BufferSize = 0;
    DMA_Init(SPI2_RX_DMA, &DMA_InitStructure);
    DMA_ITConfig(SPI2_RX_DMA, DMA_IT_TC | DMA_IT_TE, ENABLE);
    DMA_ClearFlag(DMA1_FLAG_TC4);

	DMA_Cmd(SPI2_RX_DMA, ENABLE);

	/* cleanup old state */
	DMA_Cmd(SPI2_TX_DMA, DISABLE);
	DMA_DeInit(SPI2_TX_DMA);
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(SPI2->DR);
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
	/* memory address is initialized to ZERO */
    DMA_InitStructure.DMA_MemoryBaseAddr = (u32)0;
    DMA_InitStructure.DMA_BufferSize = 0;
    DMA_Init(SPI2_TX_DMA, &DMA_InitStructure);
    DMA_ITConfig(SPI2_TX_DMA, DMA_IT_TC | DMA_IT_TE, ENABLE);
    DMA_ClearFlag(DMA1_FLAG_TC5);

	DMA_Cmd(SPI2_TX_DMA, ENABLE);
}												    

static void GPIO_Configuration()
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

static void SetupSPI (void)
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

	//SPI_NSSInternalSoftwareConfig(SPI2, SPI_NSSInternalSoft_Reset);
}

static void spi_hw_init(void)
{
	RCC_Configuration();
	NVIC_Configuration();
	GPIO_Configuration();
	DMA_Configuration();		    
   	/* disable cs first */
	rt_hw_spi_cs_disable();			   

	SetupSPI();
	//test_gpio();
}

static void spi_sw_init(void)
{
	rt_thread_t wifi_cmd_upld_thread;
	rt_thread_t wifi_rx_upld_thread;
	
	/* initialize semaphore first */
	rt_sem_init(&(rt_hw_spi_dma_tx_done), "rt_hw_spi_dma_tx_done", 0, 0);
	rt_sem_init(&(rt_hw_spi_dma_rx_done), "rt_hw_spi_dma_rx_done", 0, 0);
	rt_sem_init(&(rt_hw_spi_cmd_upld_sem), "rt_hw_spi_cmd_upld_sem", 0, 0);
	rt_sem_init(&(rt_hw_spi_rx_upld_sem), "rt_hw_spi_rx_upld_sem", 0, 0);

	/* initialize wifi cmd response thread */
	wifi_cmd_upld_thread = rt_thread_create("wifi_cmd_upld_thread",
								rt_hw_spi_cmd_upld_thread_entry, RT_NULL,
								2048, 8, 20);

	if (wifi_cmd_upld_thread != RT_NULL) {
		rt_thread_startup(wifi_cmd_upld_thread);
	} else {
	 	rt_kprintf("spi_sw_init initialize wifi_cmd_upld_thread failed\n");
	}

	/* initialize wifi rx response thread */
	/*
	wifi_rx_upld_thread = rt_thread_create("wifi_rx_upld_thread",
								rt_hw_spi_rx_upld_thread_entry, RT_NULL,
								2048, 8, 20);

	if (wifi_rx_upld_thread != RT_NULL) {
		rt_thread_startup(wifi_rx_upld_thread);
	} else {
	 	rt_kprintf("spi_sw_init initialize wifi_rx_upld_thread failed\n");
	}
	*/
}

void rt_hw_spi_init(void)
{
	/* STM32 HW register initialization */
	spi_hw_init();

	/* RT-thread sw thread initialization */
	spi_sw_init();
}

static void spi_parse_isr_status(int isr_status)
{
	if (isr_status == 0xffffffff) {
		return;
	}
 	if (isr_status & SHISR_CMD_RD_FIFO_UNDRFLOW) {
		rt_kprintf("SHISR_CMD_RD_FIFO_UNDRFLOW\n");
	}
	if (isr_status & SHISR_CMD_WR_FIFO_OVRFLOW) {
		rt_kprintf("SHISR_CMD_WR_FIFO_OVRFLOW\n");
	}
	if (isr_status & SHISR_DATA_RD_FIFO_UNDRFLOW) {
		rt_kprintf("SHISR_DATA_RD_FIFO_UNDRFLOW\n");
	}
	if (isr_status & SHISR_DATA_WR_FIFO_OVRFLOW) {
		rt_kprintf("SHISR_DATA_WR_FIFO_OVRFLOW\n");
	}
	if (isr_status & SHISR_IO_RD_FIFO_UNDRFLOW) {
		rt_kprintf("SHISR_IO_RD_FIFO_UNDRFLOW\n");
	}
	if (isr_status & SHISR_IO_WR_FIFO_OVRFLOW) {
		rt_kprintf("SHISR_IO_WR_FIFO_OVRFLOW\n");
	}
	if (isr_status & SHISR_CMD_UPLD_RDY) {
		rt_kprintf("SHISR_CMD_UPLD_RDY\n");
		rt_sem_release(&rt_hw_spi_cmd_upld_sem);
	}
	if (isr_status & SHISR_CARD_EVENT) {
		rt_kprintf("SHISR_CARD_EVENT, cause: %d\n", 
						rt_hw_spi_read_reg_fast(SPI_SCRATCH_3));

		rt_hw_spi_write_reg(SPI_CARD_INTR_CAUSE, SCIC_HOST_EVENT);
	}
	if (isr_status & SHISR_CMD_DNLD_RDY) {
		//rt_kprintf("SHISR_CMD_DNLD_RDY\n");
	}
	if (isr_status & SHISR_RX_UPLD_RDY) {
		rt_kprintf("SHISR_RX_UPLD_RDY\n");
	}
	if (isr_status & SHISR_TX_DNLD_RDY) {
		rt_kprintf("SHISR_TX_DNLD_RDY\n");
	}
}

void rt_hw_spi_88w8686_isr(void)
{
	if (spi_read_mode == 0) {
		spi_isr_status = rt_hw_spi_read_reg(SPI_HOST_INT_STATUS_REG);
	} else {
		spi_isr_status = rt_hw_spi_read_reg_fast(SPI_HOST_INT_STATUS_REG);
	}
	//rt_kprintf("In rt_hw_spi_88w8686_isr, cause: 0x%x\n", spi_isr_status);	   
	spi_parse_isr_status(spi_isr_status);

	rt_hw_spi_write_reg(SPI_HOST_INT_STATUS_REG, ~spi_isr_status);
}

void rt_hw_spi_cs_enable(void)
{
	GPIOC->BRR = GPIO_Pin_6;
}

void rt_hw_spi_cs_disable(void)
{
	GPIOC->BSRR = GPIO_Pin_6;
}

void rt_hw_spi_test_enable(void)
{
	GPIOC->BRR = GPIO_Pin_6;
	GPIOB->BRR = GPIO_Pin_13;
}

void rt_hw_spi_test_disable(void)
{
	GPIOC->BSRR = GPIO_Pin_6;
	GPIOB->BSRR = GPIO_Pin_13;
}

static void delay_n(int n)
{
	int	i;
	for (i = 0; i < n; i++);
}

void test_gpio(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

uint32_t rt_hw_spi_read_reg(uint16_t addr)
{
	uint32_t ret_val = 0;
	uint32_t counter;
	uint16_t spi_rx = 0;

	rt_hw_spi_cs_enable();

	for (counter = 0; counter < 5; counter++) {
		SPI_I2S_SendData(SPI2, addr);
		while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);
	}

	SPI_I2S_SendData(SPI2, addr);
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE)!=SET);
	SPI_I2S_ReceiveData(SPI2);

	SPI_I2S_SendData(SPI2, addr);
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE)!=SET);
	spi_rx = SPI_I2S_ReceiveData(SPI2);

	ret_val = spi_rx;

	SPI_I2S_SendData(SPI2, addr);		   
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE)!=SET);
	spi_rx = SPI_I2S_ReceiveData(SPI2);

	ret_val |= (spi_rx << 16);

	rt_hw_spi_cs_disable();

	return ret_val;
}

void rt_hw_spi_write_reg(uint16_t addr, uint32_t val)
{
	rt_hw_spi_cs_enable();

	SPI_I2S_SendData(SPI2, (0x8000 | addr));    
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);

	SPI_I2S_SendData(SPI2, (val & 0xffff));
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);

	SPI_I2S_SendData(SPI2, (val >> 16));
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);

	rt_hw_spi_cs_disable();
}

uint32_t rt_hw_spi_read_reg_fast(uint16_t addr)
{
	uint32_t ret_val = 0;
	uint32_t counter;
	uint16_t spi_rx = 0;

	rt_hw_spi_cs_enable();

	SPI_I2S_SendData(SPI2, addr);
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE)!=SET);
	SPI_I2S_ReceiveData(SPI2);

	/* short delay */
	for (counter = 0; counter < 10; counter++);

	SPI_I2S_SendData(SPI2, addr);
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE)!=SET);
	spi_rx = SPI_I2S_ReceiveData(SPI2);

	ret_val = spi_rx;

	SPI_I2S_SendData(SPI2, addr);
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE)!=SET);
	spi_rx = SPI_I2S_ReceiveData(SPI2);

	ret_val |= (spi_rx << 16);

	rt_hw_spi_cs_disable();

	return ret_val;
}

void rt_hw_spi_write_data_direct(u16 reg, u8 *data, u16 len)
{
	u16 *cur_ptr_16;
	int counter;

	rt_hw_spi_cs_enable();

	SPI_I2S_SendData(SPI2, (0x8000 | reg));    
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);

	cur_ptr_16 = (u16 *)data;
	for (counter = 0; counter < (len+1)/2; counter++) {
		SPI_I2S_SendData(SPI2, *cur_ptr_16);
		cur_ptr_16++;
		while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);
	}

	rt_hw_spi_cs_disable();
}									  

void rt_hw_spi_read_data_direct(u16 reg, u8 *data, u16 len)
{
	u16 *cur_ptr_16;
	int counter;

	rt_hw_spi_cs_enable();

	SPI_I2S_SendData(SPI2, reg);    
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);

	cur_ptr_16 = (u16 *)data;
	for (counter = 0; counter < (len+1)/2; counter++) {
		SPI_I2S_SendData(SPI2, reg);
		while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE)!=SET);
		cur_ptr_16[counter] = SPI_I2S_ReceiveData(SPI2);
	}

	rt_hw_spi_cs_disable();
}					

void rt_hw_spi_dma_tx (u16 reg, u8 *data, u16 len)
{
	rt_hw_spi_cs_enable();
	SPI_I2S_SendData(SPI2, (0x8000 | reg));     
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);

	DMA_Cmd(SPI2_TX_DMA, DISABLE);
	SPI2_TX_DMA->CMAR = (uint32_t)data;
	SPI2_TX_DMA->CNDTR = (len + 1)/2;
	DMA_Cmd(SPI2_TX_DMA, ENABLE);

	SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);

	/* Waiting on DMA transfer done signal */
	if (rt_sem_take(&rt_hw_spi_dma_tx_done, RT_WAITING_FOREVER) != RT_EOK) {
		rt_kprintf("Waiting on dma_tx_done semaphore failed\n");
		return;
	}										  

	rt_hw_spi_cs_disable();

	/* signal card interrupt */
	if (reg == SPI_CMD_PORT) {
		rt_hw_spi_write_reg(SPI_CARD_INTR_CAUSE, SCIC_CMD_DNLD_OVR);
	} else if (reg == SPI_DATA_PORT) {
		rt_hw_spi_write_reg(SPI_CARD_INTR_CAUSE, SCIC_CMD_TX_DNLD_OVR);
	}
}

void rt_hw_spi_dma_rx (u16 reg, u8 *data, u16 len)
{
	rt_hw_spi_cs_enable();
	SPI_I2S_SendData(SPI2, reg);     
	while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY)==SET);
	SPI_I2S_ReceiveData(SPI2);

	DMA_Cmd(SPI2_RX_DMA, DISABLE);
	SPI2_RX_DMA->CMAR = (uint32_t)data;
	SPI2_RX_DMA->CNDTR = (len + 1)/2;						   
	DMA_Cmd(SPI2_RX_DMA, ENABLE);

	SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, ENABLE);

	/* need to setup dummy dma tx */
	dummy_dma_tx = 1;	
	DMA_Cmd(SPI2_TX_DMA, DISABLE);
	SPI2_TX_DMA->CMAR = (uint32_t)(data + ((len + 1)/2)*2);
	SPI2_TX_DMA->CNDTR = (len + 1)/2;
	DMA_Cmd(SPI2_TX_DMA, ENABLE);

	SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);

	/* Waiting on DMA transfer done signal */					  
	if (rt_sem_take(&rt_hw_spi_dma_rx_done, RT_WAITING_FOREVER) != RT_EOK) {
		rt_kprintf("Waiting on dma_rx_done semaphore failed\n");
		return;
	}										  

	rt_hw_spi_cs_disable();

	/* signal card interrupt */
	if (reg == SPI_CMD_PORT) {
		rt_hw_spi_write_reg(SPI_CARD_INTR_CAUSE, SCIC_CMD_UPLD_OVR);
	} else if (reg == SPI_DATA_PORT) {
		rt_hw_spi_write_reg(SPI_CARD_INTR_CAUSE, SCIC_CMD_RX_UPLD_OVR);
	}
}

void rt_hw_spi_dma_tx_irq (void)
{
	//rt_kprintf("rt_hw_spi_dma_tx_irq\n");
	if (DMA_GetITStatus(DMA1_IT_TC5)) {
		//rt_kprintf("DMA1_IT_TC5\n");
		if (dummy_dma_tx == 0) {
			rt_sem_release(&rt_hw_spi_dma_tx_done);
		} else {
			dummy_dma_tx = 0;
		}
	}
	if (DMA_GetITStatus(DMA1_IT_TE5)) {
		rt_kprintf("DMA reported SPI TX Error\n");
	}
	DMA_ClearFlag(DMA1_FLAG_TC5 | DMA1_FLAG_TE5);
}

void rt_hw_spi_dma_rx_irq (void)
{
	rt_kprintf("rt_hw_spi_dma_rx_irq\n");
	if (DMA_GetITStatus(DMA1_IT_TC4)) {
		//rt_kprintf("DMA1_IT_TC5\n");	   
		rt_sem_release(&rt_hw_spi_dma_rx_done);
	}
	if (DMA_GetITStatus(DMA1_IT_TE4)) {
		rt_kprintf("DMA reported SPI RX Error\n");
	}
	DMA_ClearFlag(DMA1_FLAG_TC4 | DMA1_FLAG_TE4);
}

static int spi_wait_for_hostintstatus(int bit_flag)
{
#define HOST_INTR_STUS_RETRY	10
	int isr_status;
	int cnt = 0;

	while (cnt < HOST_INTR_STUS_RETRY) {
		/* first check current host interrupt status reg */
		isr_status = rt_hw_spi_read_reg_fast(SPI_HOST_INT_STATUS_REG);
		rt_hw_spi_write_reg(SPI_HOST_INT_STATUS_REG, ~isr_status);

		if ((isr_status & bit_flag) || (spi_isr_status & bit_flag)) {
			spi_isr_status &= ~bit_flag;
			return 0;
		}
		delay_n(100);
		cnt++;
	}

	return 1;
}

static int spi_program_helper(void)
{
#define SPI_HELPER_SIZE			2176
#define SPI_HELPER_START_ADDR	0x08000000		
	u8 *helper_ptr = (u8 *)SPI_HELPER_START_ADDR;
	int	helper_cnt;
	int delta_len;

	for (helper_cnt = 0; helper_cnt < SPI_HELPER_SIZE;
		helper_cnt += FIRMWARE_DNLD_PCKCNT) {
		delta_len = (SPI_HELPER_SIZE - helper_cnt) < FIRMWARE_DNLD_PCKCNT ?
					(SPI_HELPER_SIZE - helper_cnt) : FIRMWARE_DNLD_PCKCNT;
		/* first program length */
		rt_hw_spi_write_reg(SPI_SCRATCH_1, delta_len);
		if (spi_wait_for_hostintstatus(SHISR_CMD_DNLD_RDY)) {
			return -1;
		}
		rt_hw_spi_write_data_direct(SPI_CMD_PORT, helper_ptr, delta_len);
		helper_ptr += delta_len;
		rt_hw_spi_write_reg(SPI_CARD_INTR_CAUSE, SCIC_CMD_DNLD_OVR);
		delay_n(1000);
	}

	rt_hw_spi_write_reg(SPI_SCRATCH_1, 0x0000);	
	if (spi_wait_for_hostintstatus(SHISR_CMD_DNLD_RDY)) {
		return -1;
	}
	rt_hw_spi_write_reg(SPI_CARD_INTR_CAUSE, SCIC_CMD_DNLD_OVR);
	//rt_hw_spi_write_data_direct(SPI_CMD_PORT, (u8 *)SPI_HELPER_START_ADDR, 1);
	
	return 0;
}

static int spi_program_firmware(void)
{
#define SPI_FIRMWARE_SIZE			119696
#define SPI_FIRMWARE_START_ADDR	0x08001000
	u8 *firmware_ptr = (u8 *)SPI_FIRMWARE_START_ADDR;
	u8 *prev_firmware_ptr;
	int delta_len;
	int prev_delta_len = 0;
	int len_left = SPI_FIRMWARE_SIZE;
	
	delay_n(10000);

	for (;;) {	  
		if (len_left == 0) {
			break;
		}
							 	   
		if (spi_wait_for_hostintstatus(SHISR_CMD_DNLD_RDY)) {
			return -1;
		}
							 
		do {
			delay_n(10);
			delta_len = rt_hw_spi_read_reg_fast(SPI_SCRATCH_1);
		} while (!delta_len);

		if (delta_len & 0x1) {
			//rt_kprintf("CRC Error\n");
			//delta_len &= ~1;
			if (prev_delta_len == 0) {
				delta_len &= ~1;
				//rt_kprintf("Initial CRC Error, download helper again...\n");
				//if (spi_program_helper()) {
				//	return -1;
				//}
				//goto again;			 
			} else {
			 	rt_kprintf("Re-download previous block: 0x%x len %d\n", 
								prev_firmware_ptr, prev_delta_len);
				firmware_ptr = prev_firmware_ptr;
				delta_len = prev_delta_len;
				len_left += prev_delta_len;
			}			
		}										   

		//rt_hw_spi_write_data_direct(SPI_CMD_PORT, firmware_ptr, delta_len);
		//rt_hw_spi_write_reg(SPI_CARD_INTR_CAUSE, SCIC_CMD_DNLD_OVR);
		rt_hw_spi_dma_tx(SPI_CMD_PORT, firmware_ptr, delta_len);
		delay_n(1000);
		
		prev_firmware_ptr = firmware_ptr;
		prev_delta_len = delta_len;
							    
		len_left -= delta_len;
		firmware_ptr += delta_len;
	}										 
	return 0;			   
}										  

int rt_hw_spi_boot_wifi(void)
{
	/* first configure auto generate of cmd_dnld_ovr interrupt */
	//rt_hw_spi_write_reg(SPI_HOST_INTR_CTRL, 
	//		(rt_hw_spi_read_reg_fast(SPI_HOST_INTR_CTRL) | SHIC_CMD_DNLD_OVR_AUTO));	
	int count = 0;

	if (spi_program_helper()) {
		return -1;
	}

	if (spi_program_firmware()) {
		return -1;
	}

	while ((rt_hw_spi_read_reg_fast(SPI_SCRATCH_4) != 0x88888888) &&
			count < 10000) {		 
		delay_n(100);
		count++;
	}

	if (count >= 10000) {
		rt_kprintf("didn't see 88888888 flag\n");
		return -1;
	}

	return 0;
}

/* Wifi cmd request wrapper for UART */
void rt_hw_spi_uart_req_send(u8 *x, u32 tl)
{
 	u8 *xc = RT_NULL;
	u32 to = 0;

	if (tl <= 0) {
		rt_kprintf("Ignore null command...\n");
		return;
	}

	xc = rt_ascii_to_char(x, tl, &to);
	if (xc == RT_NULL) {
		rt_kprintf("Error converting ascii string to char...\n");
		return;
	}

	rt_hw_spi_dma_tx(SPI_CMD_PORT, xc, to);
	rt_kprintf("spi_dma_tx done!\n");
}   

void rt_hw_spi_cmd_upld_thread_entry(void *parameter)
{
	u32 resp_len, idx;
	u8 *result_buffer;
	//rt_uint32_t level;
	u16 type;
	u16 size;

 	/* 
	 * SDIO wifi will interrupt us when there 
	 * is data available, so we just need wait
	 * on the semaphore...
	 */
	while(1) {
        /* wait receive */
        if (rt_sem_take(&rt_hw_spi_cmd_upld_sem, RT_WAITING_FOREVER) != RT_EOK) {
			continue;
		}
		
		//level = rt_hw_interrupt_disable();

		resp_len = rt_hw_spi_read_reg_fast(SPI_SCRATCH_2);
    	if (!resp_len || resp_len > WLAN_UPLD_SIZE) {
			//rt_hw_interrupt_enable(level);
			rt_kprintf("cmd_upld read scratch error, resp_len: %d\n", resp_len);
			continue;
		}
		resp_len = ((resp_len + 1)/2)*2;   
								    
		rt_kprintf("cmd_upld: got %d bytes response from wifi\n", resp_len);
											 
		result_buffer = (u8 *)rt_malloc(resp_len);	  
		if (result_buffer == RT_NULL) {
			//rt_hw_interrupt_enable(level);   
			rt_kprintf("Error rt_malloc returned NULL, out of memory!\n");
			continue;   
		}	  

		/* Now read out the true data */    
    	rt_hw_spi_dma_rx(SPI_CMD_PORT, result_buffer, resp_len);
		//rt_hw_interrupt_enable(level);	 		  
		//rt_hw_spi_read_data_direct(SPI_CMD_PORT, result_buffer, resp_len);		

		/* Now we shall print out the result through UART */
		/*
		for (idx = 0; idx < resp_len; idx++) {
			rt_kprintf("%x%x ", ((result_buffer[idx] & 0xF0) >> 4), 
					(result_buffer[idx] & 0x0F));
		}
		rt_kprintf("\n");
	   	
		continue;
		*/
		//size = result_buffer[0] | (result_buffer[1] << 8);
		//type = result_buffer[2] | (result_buffer[3] << 8);
		/* 
		 * Be careful, each handler should take care of	
		 * rt_free(result_buffer), otherwise there will
		 * be memory leak!!!
		 */
		/*
		switch (type) {
		case MVMS_DAT:
			rt_wifi_handle_data(result_buffer, size);
			break;
		case MVMS_CMD:
			rt_wifi_handle_cmd(result_buffer, size);
			break;
		case MVMS_EVENT:
			rt_wifi_handle_event(result_buffer, size);
			break;
		default:
			rt_kprintf("invalid type from wifi module:\n");
			for (idx = 0; idx < resp_len; idx++) {
				rt_print_ascii_char(result_buffer[idx]);
			}
			rt_kprintf("\n");
			rt_free(result_buffer);
		}
		*/
	}
}
