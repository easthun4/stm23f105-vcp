/*
 * STM32 USB routine header file
 */

#ifndef __STM32_ADB__
#define __STM32_ADB__

#include "usbd_core.h"
#include "usb_core.h"
#include "stm32_adb.h"
//#include "usbh_hcs.h"

extern USB_OTG_CORE_HANDLE usb_host_reg;

void stm32_init (void);

#endif
