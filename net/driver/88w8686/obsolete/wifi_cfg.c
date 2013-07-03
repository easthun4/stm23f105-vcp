/*********************************************
 * Define SDIO WIFI configure operations
 *
 *
 *********************************************/

#include "wifi_cfg.h"
#include "wifi_8686.h"
#include "sdcard.h"

extern net_device_8686_t mrvl_8686_dev_entry;

/*
static rt_bool_t rt_wifi_set_wep()
{
	u8 cmd[] = 
	{	
		0x00, 0x00, 0x00, 0x00,	// space holder for sdio ctrl part
		0x13, 0x00,
		0x50, 0x00,
		0x01, 0x00, 
		0x00, 0x00,
		0x02, 0x00,
		0x00, 0x00,
		0x02,
		0x00,
		0x00,
		0x00,
        0x77, 0x61, 0x6e, 0x67, 0x74, 0x69, 0x6e, 0x67, 0x31, 0x39, 0x33, 0x31, 0x32, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    
    return rt_wifi_req_send_cmd_sync((cmd+4), (sizeof(cmd)-4), RT_NULL);
}
*/

static rt_bool_t rt_wifi_mac_control()
{
	u8 cmd[] = 
	{	
		0x00, 0x00, 0x00, 0x00,	/* space holder for sdio ctrl part */
        0x28, 0x00,
        0x0c, 0x00,
        0x02, 0x00,
        0x00, 0x00,
        0x03, 0x00,
        0x00, 0x00
    };

    return rt_wifi_req_send_cmd_sync((cmd+4), (sizeof(cmd)-4), RT_NULL);
}

static rt_bool_t rt_wifi_enable_rsn()
{
    u8 cmd[] = 
	{	
		0x00, 0x00, 0x00, 0x00,	/* space holder for sdio ctrl part */
        0x2f, 0x00,
        0x0c, 0x00,
        0x03, 0x00,
        0x00, 0x00,
        0x01, 0x00,
        0x00, 0x00,
    };

    return rt_wifi_req_send_cmd_sync((cmd+4), (sizeof(cmd)-4), RT_NULL);
}

static rt_bool_t rt_wifi_authenticate()
{
    u8 cmd[] = 
	{	
		0x00, 0x00, 0x00, 0x00,	/* space holder for sdio ctrl part */
        0x11, 0x00,
        0x19, 0x00,
        0x04, 0x00,
        0x00, 0x00,
        0x00, 0x22, 0xb0, 0xaa, 0xc2, 0x66,
        0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
        
    return rt_wifi_req_send_cmd_sync((cmd+4), (sizeof(cmd)-4), RT_NULL);
}

static rt_bool_t rt_wifi_radio_control()
{
    u8 cmd[] = 
	{	
		0x00, 0x00, 0x00, 0x00,	/* space holder for sdio ctrl part */
        0x1c, 0x00,
        0x0c, 0x00,
        0x05, 0x00,
        0x00, 0x00,
        0x01, 0x00,
        0x01, 0x00,
    };
        
    return rt_wifi_req_send_cmd_sync((cmd+4), (sizeof(cmd)-4), RT_NULL);
}

static rt_bool_t rt_wifi_associate()
{
	u8 *cmd_resp = RT_NULL;
	rt_bool_t ret = RT_FALSE;

    u8 cmd[] = 
	{	
		0x00, 0x00, 0x00, 0x00,	/* space holder for sdio ctrl part */
        0x12, 0x00,
        0x3a, 0x00,
        0x06, 0x00,
        0x00, 0x00,
        0x00, 0x22, 0xb0, 0xaa, 0xc2, 0x66,
        0x21, 0x04,
        0x0a, 0x00,		  			 
        0x00, 0x00,
        0x00,
        0x00, 0x00,
        0x05, 0x00,
        0x66, 0x65, 0x6c, 0x69, 0x78,
        0x03, 0x00,
        0x01, 0x00,
        0x04,
        0x04, 0x00,
        0x06, 0x00,
        0x00,
        0x00,
        0x00, 0x00,
        0x00, 0x00,
        0x01, 0x00,
        0x04, 0x00,
        0x82, 0x84, 0x8b, 0x96,
        0x1f, 0x01,
        0x01, 0x00,
        0x00,
    };
        
    ret = rt_wifi_req_send_cmd_sync((cmd+4), (sizeof(cmd)-4), &cmd_resp);

	if (ret == RT_TRUE && cmd_resp != RT_NULL) {
		if ((cmd_resp[16] | (cmd_resp[17] << 8)) == 0xffff) {
			ret = RT_FALSE;
		} else {
			rt_kprintf("Associate success!\n");
		}
	}

	if (cmd_resp != RT_NULL) {
		rt_free(cmd_resp);
	}
	return ret;
}

static void delay(void)
{
  vu32 i = 0;

  for(i = 0x1000; i != 0; i--)
  {
  }
}

rt_bool_t rt_wifi_associate_with_felix()
{
	u8 retry_count;
	rt_bool_t ret = RT_FALSE;
	
	/*
	ret = rt_wifi_set_wep();
	if (ret == RT_FALSE) {
		return ret;
	}
	*/
	
	ret = rt_wifi_mac_control();
	if (ret == RT_FALSE) {
		return ret;
	}
	
	ret = rt_wifi_enable_rsn();
	if (ret == RT_FALSE) {
		return ret;
	}
	
	ret = rt_wifi_authenticate();
	if (ret == RT_FALSE) {
		return ret;
	}
	
	ret = rt_wifi_radio_control();
	if (ret == RT_FALSE) {
		return ret;
	}
	
	retry_count = 20;
	do {
		retry_count--;
		delay();
		rt_kprintf("Try assiciate...\n"); 
		ret = rt_wifi_associate();
	} while (ret == RT_FALSE && retry_count > 0);

	if (ret == RT_TRUE) {
		eth_device_ready((struct eth_device*)&(mrvl_8686_dev_entry.parent));
	}
	return ret;	 	
}
