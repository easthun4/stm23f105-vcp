/******************************************************************************
 *
 * Define the bottom layer GSPI I/O operations. During initialization
 * we register these operations with the layer above us, then all the
 * high level operations should be able to map to this bottom layer
 * transparently
 ******************************************************************************/

#ifndef __NET__DRIVER__88W8686__GSPI__IF_GSPI__
#define __NET__DRIVER__88W8686__GSPI__IF_GSPI__

#include "wifi_drv.h"
#include <rtdef.h>
#include "misc.h"
#include <stm32f10x.h>
#include <stm32f10x_spi.h>
#include <stm32f10x_gpio.h>
#include <stm32f10x_rcc.h>
#include <stm32f10x_exti.h>
#include <stm32f10x_dma.h>

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
#define SPI_SPU_BUS_MODE            0x70
#define     SSBM_DELAY_METHOD               (1 << 2)

#define 	FIRMWARE_DNLD_PCKCNT	32

#define		SPI2_RX_DMA		DMA1_Channel4
#define		SPI2_TX_DMA		DMA1_Channel5

#define 	WLAN_UPLD_SIZE	2312

typedef struct rx_pkt_desc {
	rt_uint16_t reserved1;
	rt_uint8_t 	snr;
	rt_uint8_t	reserved2;

	rt_uint16_t	rx_pkt_len;
	rt_uint8_t	noise_floor;
	rt_uint8_t	rx_rate;

	rt_uint32_t	rx_pkt_offset;
	rt_uint32_t	reserved3;

	rt_uint8_t	priority;
	rt_uint8_t	pad[3];
} rx_pkt_desc_t;

typedef struct tx_pkt_desc {
	rt_uint32_t tx_status;
	rt_uint32_t tx_control;
	rt_uint32_t tx_packet_location;
 
    rt_uint16_t tx_packet_length;
    rt_uint8_t  tx_dest_addr_high[2];
    
    rt_uint8_t  tx_dest_addr_low[4];

    rt_uint8_t  priority;
    rt_uint8_t  powermgmt;
    rt_uint8_t  pktdelay_2ms;
    rt_uint8_t  reserved1;
} tx_pkt_desc_t;


rt_err_t spi_wifi_init (void);
void spi2_dma_send_irq (void);
void spi2_dma_recv_irq (void);
void spi2_isr (void);

/* 
 * command handlers 
 */
rt_err_t send_cmd (rt_uint8_t *data, rt_uint16_t len);
rt_uint16_t get_cmd_resp_len (void);
void recv_cmd_resp (rt_uint8_t *data, rt_uint16_t len);
rt_err_t dma_send_pbuf (struct pbuf *p);
void *dma_recv_pbuf (pkt_rsrc_alloc_func pkt_rsrc_alloc);

/*
 * Initialization entry
 */
void w8686_spi_register_handler (void);

/* debug cmd */
rt_uint32_t debug_read_reg (rt_uint16_t addr);
void w8686_spi_debug_cmd (rt_uint8_t *data, rt_uint16_t len);
void w8686_send_manual_pkt (void);

#endif
