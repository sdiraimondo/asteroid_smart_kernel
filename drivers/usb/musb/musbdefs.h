#ifndef __MUSB_MUSBDEFS_H__
#define __MUSB_MUSBDEFS_H__
/* for high speed test mode; see USB 2.0 spec */
static const u8 musb_test_packet[53] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
	0xee, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xbf, 0xdf,
	0xef, 0xf7, 0xfb, 0xfd, 0xfc, 0x7e, 0xbf, 0xdf,
	0xef, 0xf7, 0xfb, 0xfd, 0x7e
};


#endif	/* __MUSB_MUSBDEFS_H__ */