/*
 * EXG/monitor application API
 */

#ifndef __EXG_API__
#define __EXG_API__

// from EXG to monitor app
#define	EXG2MNT_RATE_PER_SEC 			0x00
#define	EXG2MNT_CHAN_NUM				0x01
#define	EXG2MNT_DATA					0x02

// from monitor app to EXG
#define	MNT2EXG_SET_RATE				0x80
#define	MNT2EXG_ENABLE_CHAN		   		0x81
#define	MNT2EXG_DISABLE_CHAN			0x82
#define	MNT2EXG_SAMPLE_NUM_PER_CHAN		0x83

#pragma pack(1)
typedef struct exg_msg {
	uint8_t type;
	uint16_t length;
	uint8_t value[0];
} exg_msg_t ;
#pragma pack()

#endif
