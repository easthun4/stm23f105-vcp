/**********************************************
 * Holding function prototypes for spi.c
 *
 **********************************************/
#include <stm32f10x.h>
#include <stm32f10x_spi.h>
#include <stm32f10x_gpio.h>
#include <stm32f10x_rcc.h>
#include <stm32f10x_exti.h>
#include <stm32f10x_dma.h>
#include <misc.h>
#include <rtthread.h>

#define SPI_DEV_ID_CTRL				0x00
#define SPI_IO_RD_BASE_ADDR			0x04
#define	SPI_IO_WR_BASE_ADDR			0x08
#define SPI_IO_PORT					0x0c
#define SPI_CMD_RD_BASE_ADDR		0x10
#define SPI_CMD_WR_BASE_ADDR		0x14
#define	SPI_CMD_PORT				0x18
#define SPI_DATA_RD_BASE_ADDR		0x1c
#define SPI_DATA_WR_BASE_ADDR		0x20
#define SPI_DATA_PORT				0x24
#define SPI_SCRATCH_1				0x28
#define SPI_SCRATCH_2				0x2c
#define SPI_SCRATCH_3				0x30
#define SPI_SCRATCH_4				0x34
#define SPI_TRANS_FRM_SEQ_NUM		0x38
#define SPI_TRANS_FRM_STUS			0x3c
#define SPI_HOST_INTR_CTRL			0x40
#define 	SHIC_CMD_DNLD_OVR_AUTO		(1 << 7)
#define SPI_CARD_INTR_CAUSE			0x44
#define		SCIC_CMD_UPLD_OVR			(1 << 4)
#define		SCIC_HOST_EVENT				(1 << 3)
#define		SCIC_CMD_DNLD_OVR			(1 << 2)
#define		SCIC_CMD_RX_UPLD_OVR		(1 << 1)
#define		SCIC_CMD_TX_DNLD_OVR		(1 << 0)
#define SPI_CARD_INTR_STUS			0x48
#define SPI_CARD_INTR_EVT_MSK		0x4c
#define SPI_CARD_INTR_STUS_MSK		0x50
#define SPI_CARD_INTR_RST_SEL		0x54
#define SPI_HOST_INTR_CAUSE			0x58
#define SPI_HOST_INT_STATUS_REG		0x5c
#define 	SHISR_CMD_RD_FIFO_UNDRFLOW	(1 << 10)
#define		SHISR_CMD_WR_FIFO_OVRFLOW	(1 << 9)
#define		SHISR_DATA_RD_FIFO_UNDRFLOW	(1 << 8)
#define 	SHISR_DATA_WR_FIFO_OVRFLOW	(1 << 7)
#define		SHISR_IO_RD_FIFO_UNDRFLOW	(1 << 6)
#define		SHISR_IO_WR_FIFO_OVRFLOW	(1 << 5)
#define		SHISR_CMD_UPLD_RDY				(1 << 4)
#define 	SHISR_CARD_EVENT				(1 << 3)
#define		SHISR_CMD_DNLD_RDY				(1 << 2)
#define		SHISR_RX_UPLD_RDY				(1 << 1)
#define		SHISR_TX_DNLD_RDY				(1 << 0)

#define 	FIRMWARE_DNLD_PCKCNT	64

#define		SPI2_RX_DMA		DMA1_Channel4
#define		SPI2_TX_DMA		DMA1_Channel5

#define 	WLAN_UPLD_SIZE	2312

void rt_hw_spi_init(void);
void rt_hw_spi_88w8686_isr(void);
void rt_hw_spi_cs_enable(void);
void rt_hw_spi_cs_disable(void);
void test_gpio(void);
uint32_t rt_hw_spi_read_reg(uint16_t addr);
uint32_t rt_hw_spi_read_reg_fast(uint16_t addr);
void rt_hw_spi_write_reg(uint16_t addr, uint32_t val);
void rt_hw_spi_dma_tx (u16 reg, u8 *data, u16 len);
void rt_hw_spi_dma_rx (u16 reg, u8 *data, u16 len);
int rt_hw_spi_program_firmware(void);
int rt_hw_spi_boot_wifi(void);
void rt_hw_spi_uart_req_send(u8 *x, u32 tl);
extern u8* rt_ascii_to_char(u8 *x, u32 ti, u32 *to);
void rt_hw_spi_cmd_upld_thread_entry(void *parameter);
void rt_hw_spi_rx_upld_thread_entry(void *parameter);

