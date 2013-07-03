/***************************************************
 * This is the main routine of wifi application
 *
 *
 *
 ***************************************************/

#include "wifi_main.h"

u8 pkt1[] = 	    
	{	/* Failed ARP REQ */
		//0x00, 0x0d, 0x60, 0x7e, 0xe8, 0x14,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
        0x00, 0x0b, 0x6c, 0x89, 0xbd, 0x60,	    
		0x00, 0x24,
		0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00,
        0x08, 0x06, 0x00, 0x01, 0x08, 0x00, 
		0x06, 0x04, 0x00/*, 0x01, 
		0x00, 0x0b, 0x6c, 0x89, 0xbd, 0x60, 
		0xc0, 0xa8, 0x00, 0x6f, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0xc0, 0xa8, 0x00, 0x6d */
		
	};

u8 pkt2[] = 
{
		0x01, 
		0x00, 0x0b, 0x6c, 0x89, 0xbd, 0x60, 
		0xc0, 0xa8, 0x00, 0x6f, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0xc0, 0xa8, 0x00, 0x6d
};

u8 pkt3[] = 
{
//0x00, 0x0d, 0x60, 0x7e, 0xe8, 0x14, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0x00, 0x0b, 0x6c, 0x89, 0xbd, 0x60, 0x00, 0x36, 0xaa, 0xaa,
0x03, 0x00, 0x00, 0x00, 0x08, 0x06, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x02, 0x00, 0x0b,
0x6c, 0x89, 0xbd, 0x60, 0xc0, 0xa8, 0x00, 0x6d, 0x00, 0x0d, 0x60, 0x7e, 0xe8, 0x14, 0xc0, 0xa8,
0x00, 0x6b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00

};

void wifi_main_entry(void* parameter)
{
	//struct pbuf p1, p2;
	//struct pbuf p3;

	/* First start associate with AP felix */
	if (rt_wifi_associate_with_felix() == RT_FALSE) {
		rt_kprintf("Associate failed...\n");
	}
	/*
	p1.next = &p2;
	p1.tot_len = 50;
	p1.len = 29;
	p1.payload = &pkt1[0];

	p2.next = NULL;
	p2.tot_len = 0;
	p2.len = 21;
	p2.payload = &pkt2[0];

	rt_wifi_tx(NULL, &p1);			  
	*/
	/*
	p3.next = NULL;
	p3.tot_len = 0x44;
	p3.len = 0x44;
	p3.payload = &pkt3[0];			    
	rt_wifi_tx(NULL, &p3);
	*/ 
}
