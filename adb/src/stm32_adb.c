/*
 * STM32 USB routine source file
 * Implement bulk transfer
 */

#include "stm32_adb.h"

rt_inline void rt_list_init(rt_list_t *l)
{
	l->next = l->prev = l;
}

USB_OTG_CORE_HANDLE usb_host_reg; 

void stm32_init (void) 
{
	USB_OTG_GCCFG_TypeDef	 cfgctl;

	/* hardware clock init */
	USB_OTG_BSP_Init();

	/* timer init for delay functions */
	USB_OTG_BSP_TimeInit(1);

	/*
	 * Initialize and setup a timer to check 
	 * the trans_wait_list timeout 
	 */
	rt_list_init(&(usb_host_reg.host.trans_wait_list));
	usb_host_reg.host.ConnSts = 0;
  	usb_trans_timer_init();

	/* Initialize the channel structure */
	USBH_Init_Channels();

	/* initialize USB host registers */
	USB_OTG_InitReg(&usb_host_reg);

	/* reset USB host core */
	USB_OTG_CoreReset(&usb_host_reg);

	/* Force host mode */
	USB_OTG_SetHostMode(&usb_host_reg);

	USB_OTG_CoreInitHost(&usb_host_reg);

	USB_OTG_EnableGlobalInt(&usb_host_reg);

  	/* Enable Interrupts */
  	USB_OTG_BSP_EnableInterrupt();

  	/* Deactivate the power down and enable the sensing*/
  	cfgctl.d32 = 0;
  	cfgctl.b.pwdn = 1; 
	
  	USB_OTG_WRITE_REG32 (&(usb_host_reg.regs.GREGS->GCCFG), cfgctl.d32);	
}
