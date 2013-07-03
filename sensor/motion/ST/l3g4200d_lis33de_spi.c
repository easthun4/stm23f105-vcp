/***************************************************
 *
 * SPI interface for motion sensor L3G4200D/LIS33DE
 *
 ***************************************************/

#include "l3g4200d_lis33de_spi.h"

#define ENABLE_L3G4200D_INTERRUPT

///////////////////////////////////////////////////////////////////////////////
//                     I n i t i a l i z a t i o n                           //
///////////////////////////////////////////////////////////////////////////////

static void RCC_Configuration (void)
{
    /* enable spi1 clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

    /* enable gpiob port clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
}

static void NVIC_Configuration (void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

#ifdef ENABLE_L3G4200D_INTERRUPT 
    /* Enable the EXTI0 Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
#endif

	NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

}

static void GPIO_Configuration (void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;

    /* L3G4200D configure PC9 as external interrupt */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* LIS33DE configure PB12 as external interrupt */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* Configure SPI1 pins:  SCK, MISO and MOSI ----------------------------*/
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	  
    GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* L3G4200D CS signal */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* LIS33DE CS signal */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

#ifdef ENABLE_L3G4200D_INTERRUPT
    /* Connect L3G4200D INT Line to GPIOC Pin 9 */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource9);

    /* Configure L3G4200D INT Line to generate an interrupt on falling edge */
    EXTI_InitStructure.EXTI_Line = EXTI_Line9;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* Clear the Key Button EXTI line pending bit */
    EXTI_ClearITPendingBit(EXTI_Line9);
#endif

	/* Connect LIS33DE INT Line to GPIOB Pin 12 */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource12);

    /* Configure LIS33DE INT Line to generate an interrupt on falling edge */
    EXTI_InitStructure.EXTI_Line = EXTI_Line12;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* Clear the Key Button EXTI line pending bit */
    EXTI_ClearITPendingBit(EXTI_Line12);
}

static void SPI_Configuration (void)
{
    SPI_InitTypeDef SPI_InitStructure;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);
    SPI_Cmd(SPI1, ENABLE);
}

///////////////////////////////////////////////////////////////////////////////
//                  B a s i c   I / O   O p e r a t i o n s                  //
///////////////////////////////////////////////////////////////////////////////

static void gyro_cs_disable (void)
{
	GPIOA->BSRR = GPIO_Pin_8;
}

static void gyro_cs_enable (void)
{
	GPIOA->BRR = GPIO_Pin_8;
}

static void acle_cs_disable (void)
{
	GPIOC->BSRR = GPIO_Pin_8;
}

static void acle_cs_enable (void)
{
	GPIOC->BRR = GPIO_Pin_8;
}

/*
 * For multiple read please provide depot
 * Also enable increase address when doing
 * multiple read
 */
static rt_uint8_t read_reg_internal (rt_uint8_t addr,
									 rt_uint8_t len,
									 rt_uint8_t *data)
{
	rt_uint8_t ret_val = 0;
	rt_uint8_t i;

   	if (addr > 0x3f) {
		rt_kprintf("Sensor read register error: address 0x%x exceeding range 0x3F\n", addr);
		return ret_val;
	}

	if (len > 1 && data == RT_NULL) {
		rt_kprintf("Multiple read please provide depot!\n");
		return ret_val;
	}

	SPI_I2S_SendData(SPI1, (addr | 0x80 | ((len > 1) ? 0x40 : 0x00)));
	while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY)==SET);
	while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE)!=SET);
	SPI_I2S_ReceiveData(SPI1);

	if (len == 1) {
		SPI_I2S_SendData(SPI1, addr);
		while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE)!=SET);
		ret_val = SPI_I2S_ReceiveData(SPI1);
	} else {
		for (i = 0; i < len; i++) {
			SPI_I2S_SendData(SPI1, addr);
			while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE)!=SET);
			data[i] = SPI_I2S_ReceiveData(SPI1);
		}
	}

	return ret_val;
}

static void write_reg_core (rt_uint8_t addr, rt_uint8_t val)
{
	SPI_I2S_SendData(SPI1, addr);    
	while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY)==SET);

	SPI_I2S_SendData(SPI1, val);
	while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY)==SET);
}						 			  				 

/*
 * According to datasheet, write to reserved register could
 * cause permanent damage to the device, so be careful when
 * we are doing the write
 */
static rt_err_t gyro_addr_bl_check (rt_uint8_t addr)
{
	if (addr < 0x20 || (addr >= 0x26 && addr < 0x30) ||
		addr == 0x31 || addr > 0x38) {
		return -RT_ERROR;
	}

	return RT_EOK;
}

static void write_gyro_reg_internal (rt_uint8_t addr,
									rt_uint8_t val)
{
	if (gyro_addr_bl_check(addr) != RT_EOK) {
		rt_kprintf("address 0x%x write access invalid\n", addr);
		return;
	}
	write_reg_core(addr, val);
}

static rt_err_t acle_addr_bl_check (rt_uint8_t addr)
{
	if (ACLE_WRITABLE(addr)) {
		return RT_EOK;
	} else {
		return -RT_ERROR;
	} 
}

static void write_acle_reg_internal (rt_uint8_t addr,
									rt_uint8_t val)
{
	if (acle_addr_bl_check(addr) != RT_EOK) {
		rt_kprintf("address 0x%x write access invalid\n", addr);
		return;
	}
	write_reg_core(addr, val);
}

static void dev_io_init (void)
{
	RCC_Configuration();
	NVIC_Configuration();
	GPIO_Configuration();		    

   	/* disable cs by default */
	gyro_cs_disable();
	acle_cs_disable();			   

	SPI_Configuration();		 
}

void motion_sensor_init (void)
{
 	dev_io_init();
	write_gyro_reg(GYRO_CTRL_REG1, 0x0F);
}

///////////////////////////////////////////////////////////////////////////////
//					  I n t e r r u p t    H a n d l e r					 //
///////////////////////////////////////////////////////////////////////////////

void l3g4200d_isr (void)
{
#ifdef ENABLE_L3G4200D_INTERRUPT
	rt_kprintf("L3G4200D interrupt!!\n");
#endif
}

void lis33de_isr (void)
{
	rt_kprintf("LIS33DE interrupt!!\n");
}

///////////////////////////////////////////////////////////////////////////////
//                E x t e r n a l   I / O   H a n d l e r                    //
///////////////////////////////////////////////////////////////////////////////
/*
 * Write single u8 into L3G4200D register
 */
void write_gyro_reg (rt_uint8_t addr, rt_uint8_t val)
{
	gyro_cs_enable();
	write_gyro_reg_internal(addr, val);
	gyro_cs_disable();
}

/*
 * Write single u8 into LIS33DE register
 */
void write_acle_reg (rt_uint8_t addr, rt_uint8_t val)
{
	acle_cs_enable();
	write_acle_reg_internal(addr, val);
	acle_cs_disable();
}

/*
 * Read single or multiple continuous register from
 * L3G4200D.
 * For single reg read, data should be RT_NULL, and
 *		read result sits in the return value.
 * For multiple reg read, data should point to valid
 *		memory location, and final result will be 
 *      written there.
 */
rt_uint8_t read_gyro_reg (rt_uint8_t addr, rt_uint8_t len,
							rt_uint8_t *data)
{
	rt_uint8_t ret;

	gyro_cs_enable();
	ret = read_reg_internal(addr, len, data);
	gyro_cs_disable();

	return ret;
}

/*
 * Read single or multiple continuous register from
 * LIS33DE.
 * For single reg read, data should be RT_NULL, and
 *		read result sits in the return value.
 * For multiple reg read, data should point to valid
 *		memory location, and final result will be 	     
 *      written there.
 */
rt_uint8_t read_acle_reg (rt_uint8_t addr, rt_uint8_t len,
							rt_uint8_t *data)
{
	rt_uint8_t ret;

	acle_cs_enable();
	ret = read_reg_internal(addr, len, data);
	acle_cs_disable();

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//                         D e b u g    C m d s                              //
///////////////////////////////////////////////////////////////////////////////
rt_uint16_t gyro_read_data (rt_uint8_t dir)
{
	rt_uint8_t data[2];
	rt_uint16_t result = 0;

	switch (dir) {
		case 1:
			/* X */
			read_gyro_reg(GYRO_OUT_X_L, 2, data);
			result = (data[1] << 8) | data[0];
			break;
		case 2:
			/* Y */
			read_gyro_reg(GYRO_OUT_Y_L, 2, data);
			result = (data[1] << 8) | data[0];
			break;
		case 3:
			/* Z */
			read_gyro_reg(GYRO_OUT_Z_L, 2, data);
			result = (data[1] << 8) | data[0];
			break;
		default:
			rt_kprintf("Unknown direction\n");
	}
	return result;
}