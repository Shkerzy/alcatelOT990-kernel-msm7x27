/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/platform_device.h>
#include <linux/gpio_event.h>

#include <asm/mach-types.h>

/* don't turn this on without updating the ffa support */
#define SCAN_FUNCTION_KEYS 0

/* FFA:
 36: KEYSENSE_N(0)
 37: KEYSENSE_N(1)
 38: KEYSENSE_N(2)
 39: KEYSENSE_N(3)
 40: KEYSENSE_N(4)

 31: KYPD_17
 32: KYPD_15
 33: KYPD_13
 34: KYPD_11
 35: KYPD_9
 41: KYPD_MEMO
*/
//by huyugui
/*****************************************
 * T&A opal test board keypad define(6*8)
 *36: KEYINPUT[0]	col
 *37: KEYINPUT[1]  	col
 *38: KEYINPUT[2]	col
 *39: KEYINPUT[3]	col
 *40: KEYINPUT[4]	col
 *41: KEYINPUT[5]	col
 *
 *
 *35: KEYOUTPUT[0]	row
 *34: KEYOUTPUT[1]	row
 *33: KEYOUTPUT[2]	row
 *32: KEYOUTPUT[3]	row
 *31: KEYOUTOUT[4]	row
 *29: KEYOUTPUT[5]	row
 *28: KEYOUTPUT[6]	row	
 *27: KEYOUTPUT[7]	row
 ****************************************/
 /*
static unsigned int keypad_row_opal[] = {35, 34, 33, 32, 31, 29, 28, 30};
static unsigned int keypad_col_opal[] = {36, 37, 38, 39, 40, 41};

#define KEY_INDEX_OPAL(row, col) ((row)*ARRAY_SIZE(keypad_col_opal) + (col))
static const unsigned short keypad_keymap_opal[ARRAY_SIZE(keypad_col_opal) *
					       ARRAY_SIZE(keypad_row_opal)] = {
	[KEY_INDEX_OPAL(0, 0)] = KEY_BACK,
	[KEY_INDEX_OPAL(0, 1)] = KEY_A,
	[KEY_INDEX_OPAL(0, 2)] = 3,
	[KEY_INDEX_OPAL(0, 3)] = KEY_P,
	[KEY_INDEX_OPAL(0, 4)] = KEY_DELETE,
	[KEY_INDEX_OPAL(0, 5)] = KEY_ENTER,

	[KEY_INDEX_OPAL(1, 0)] = KEY_RIGHT,
	[KEY_INDEX_OPAL(1, 1)] = KEY_W,
	[KEY_INDEX_OPAL(1, 2)] = 127,
	[KEY_INDEX_OPAL(1, 3)] = KEY_O,
	[KEY_INDEX_OPAL(1, 4)] = KEY_L,
	[KEY_INDEX_OPAL(1, 5)] = 353,

	[KEY_INDEX_OPAL(2, 0)] = KEY_UP,
	[KEY_INDEX_OPAL(2, 1)] = KEY_Q,
	[KEY_INDEX_OPAL(2, 2)] = KEY_SPACE,
	[KEY_INDEX_OPAL(2, 3)] = KEY_I,
	[KEY_INDEX_OPAL(2, 4)] = KEY_K,
	[KEY_INDEX_OPAL(2, 5)] = KEY_M,

	[KEY_INDEX_OPAL(3, 0)] = KEY_OK,
	[KEY_INDEX_OPAL(3, 1)] = KEY_SEND,
	[KEY_INDEX_OPAL(3, 2)] = KEY_0,
	[KEY_INDEX_OPAL(3, 3)] = KEY_U,
	[KEY_INDEX_OPAL(3, 4)] = KEY_J,
	[KEY_INDEX_OPAL(3, 5)] = KEY_N,
	
	[KEY_INDEX_OPAL(4, 0)] = 119,
	[KEY_INDEX_OPAL(4, 1)] = KEY_DOWN,
	[KEY_INDEX_OPAL(4, 2)] = KEY_CAPSLOCK,
	[KEY_INDEX_OPAL(4, 3)] = KEY_Y,
	[KEY_INDEX_OPAL(4, 4)] = KEY_H,
	[KEY_INDEX_OPAL(4, 5)] = KEY_B,

	[KEY_INDEX_OPAL(5, 0)] = KEY_CAMERA,
	[KEY_INDEX_OPAL(5, 1)] = KEY_LEFT,
	[KEY_INDEX_OPAL(5, 2)] = KEY_Z,
	[KEY_INDEX_OPAL(5, 3)] = KEY_T,
	[KEY_INDEX_OPAL(5, 4)] = KEY_G,
	[KEY_INDEX_OPAL(5, 5)] = KEY_V,
	
	[KEY_INDEX_OPAL(6, 0)] = KEY_VOLUMEUP,
	[KEY_INDEX_OPAL(6, 1)] = KEY_MENU,
	[KEY_INDEX_OPAL(6, 2)] = KEY_FN,
	[KEY_INDEX_OPAL(6, 3)] = KEY_R,
	[KEY_INDEX_OPAL(6, 4)] = KEY_F,
	[KEY_INDEX_OPAL(6, 5)] = KEY_C,

	[KEY_INDEX_OPAL(7, 0)] = KEY_VOLUMEDOWN,
	[KEY_INDEX_OPAL(7, 1)] = 354,
	[KEY_INDEX_OPAL(7, 2)] = KEY_S,
	[KEY_INDEX_OPAL(7, 3)] = KEY_E,
	[KEY_INDEX_OPAL(7, 4)] = KEY_D,
	[KEY_INDEX_OPAL(7, 5)] = KEY_X,	

};*/

static unsigned int keypad_row_opal[] = {35};
static unsigned int keypad_col_opal[] = {39, 40, 41};

#define KEY_INDEX_OPAL(row, col) ((row)*ARRAY_SIZE(keypad_col_opal) + (col))
static const unsigned short keypad_keymap_opal[ARRAY_SIZE(keypad_col_opal) *
					       ARRAY_SIZE(keypad_row_opal)] = {
	[KEY_INDEX_OPAL(0, 0)] = KEY_VOLUMEUP,
	[KEY_INDEX_OPAL(0, 1)] = KEY_VOLUMEDOWN,
	[KEY_INDEX_OPAL(0, 2)] = KEY_HOME,
};

static unsigned int keypad_row_gpios[] = {
	31, 32, 33, 34, 35, 41
#if SCAN_FUNCTION_KEYS
	, 42
#endif
};

static unsigned int keypad_col_gpios[] = { 36, 37, 38, 39, 40 };

static unsigned int keypad_row_gpios_8k_ffa[] = {31, 32, 33, 34, 35, 36};
static unsigned int keypad_col_gpios_8k_ffa[] = {38, 39, 40, 41, 42};

#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(keypad_col_gpios) + (col))
#define FFA_8K_KEYMAP_INDEX(row, col) ((row)* \
				ARRAY_SIZE(keypad_col_gpios_8k_ffa) + (col))

static const unsigned short keypad_keymap_surf[ARRAY_SIZE(keypad_col_gpios) *
					  ARRAY_SIZE(keypad_row_gpios)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_5,
	[KEYMAP_INDEX(0, 1)] = KEY_9,
	[KEYMAP_INDEX(0, 2)] = 229,            /* SOFT1 */
	[KEYMAP_INDEX(0, 3)] = KEY_6,
	[KEYMAP_INDEX(0, 4)] = KEY_LEFT,

	[KEYMAP_INDEX(1, 0)] = KEY_0,
	[KEYMAP_INDEX(1, 1)] = KEY_RIGHT,
	[KEYMAP_INDEX(1, 2)] = KEY_1,
	[KEYMAP_INDEX(1, 3)] = 228,           /* KEY_SHARP */
	[KEYMAP_INDEX(1, 4)] = KEY_SEND,

	[KEYMAP_INDEX(2, 0)] = KEY_VOLUMEUP,
	[KEYMAP_INDEX(2, 1)] = KEY_HOME,      /* FA   */
	[KEYMAP_INDEX(2, 2)] = KEY_F8,        /* QCHT */
	[KEYMAP_INDEX(2, 3)] = KEY_F6,        /* R+   */
	[KEYMAP_INDEX(2, 4)] = KEY_F7,        /* R-   */

	[KEYMAP_INDEX(3, 0)] = KEY_UP,
	[KEYMAP_INDEX(3, 1)] = KEY_CLEAR,
	[KEYMAP_INDEX(3, 2)] = KEY_4,
	[KEYMAP_INDEX(3, 3)] = KEY_MUTE,      /* SPKR */
	[KEYMAP_INDEX(3, 4)] = KEY_2,

	[KEYMAP_INDEX(4, 0)] = 230,           /* SOFT2 */
	[KEYMAP_INDEX(4, 1)] = 232,           /* KEY_CENTER */
	[KEYMAP_INDEX(4, 2)] = KEY_DOWN,
	[KEYMAP_INDEX(4, 3)] = KEY_BACK,      /* FB */
	[KEYMAP_INDEX(4, 4)] = KEY_8,

	[KEYMAP_INDEX(5, 0)] = KEY_VOLUMEDOWN,
	[KEYMAP_INDEX(5, 1)] = 227,           /* KEY_STAR */
	[KEYMAP_INDEX(5, 2)] = KEY_MAIL,      /* MESG */
	[KEYMAP_INDEX(5, 3)] = KEY_3,
	[KEYMAP_INDEX(5, 4)] = KEY_7,

#if SCAN_FUNCTION_KEYS
	[KEYMAP_INDEX(6, 0)] = KEY_F5,
	[KEYMAP_INDEX(6, 1)] = KEY_F4,
	[KEYMAP_INDEX(6, 2)] = KEY_F3,
	[KEYMAP_INDEX(6, 3)] = KEY_F2,
	[KEYMAP_INDEX(6, 4)] = KEY_F1
#endif
};

static const unsigned short keypad_keymap_ffa[ARRAY_SIZE(keypad_col_gpios) *
					      ARRAY_SIZE(keypad_row_gpios)] = {
	/*[KEYMAP_INDEX(0, 0)] = ,*/
	/*[KEYMAP_INDEX(0, 1)] = ,*/
	[KEYMAP_INDEX(0, 2)] = KEY_1,
	[KEYMAP_INDEX(0, 3)] = KEY_SEND,
	[KEYMAP_INDEX(0, 4)] = KEY_LEFT,

	[KEYMAP_INDEX(1, 0)] = KEY_3,
	[KEYMAP_INDEX(1, 1)] = KEY_RIGHT,
	[KEYMAP_INDEX(1, 2)] = KEY_VOLUMEUP,
	/*[KEYMAP_INDEX(1, 3)] = ,*/
	[KEYMAP_INDEX(1, 4)] = KEY_6,

	[KEYMAP_INDEX(2, 0)] = KEY_HOME,      /* A */
	[KEYMAP_INDEX(2, 1)] = KEY_BACK,      /* B */
	[KEYMAP_INDEX(2, 2)] = KEY_0,
	[KEYMAP_INDEX(2, 3)] = 228,           /* KEY_SHARP */
	[KEYMAP_INDEX(2, 4)] = KEY_9,

	[KEYMAP_INDEX(3, 0)] = KEY_UP,
	[KEYMAP_INDEX(3, 1)] = 232, /* KEY_CENTER */ /* i */
	[KEYMAP_INDEX(3, 2)] = KEY_4,
	/*[KEYMAP_INDEX(3, 3)] = ,*/
	[KEYMAP_INDEX(3, 4)] = KEY_2,

	[KEYMAP_INDEX(4, 0)] = KEY_VOLUMEDOWN,
	[KEYMAP_INDEX(4, 1)] = KEY_SOUND,
	[KEYMAP_INDEX(4, 2)] = KEY_DOWN,
	[KEYMAP_INDEX(4, 3)] = KEY_8,
	[KEYMAP_INDEX(4, 4)] = KEY_5,

	/*[KEYMAP_INDEX(5, 0)] = ,*/
	[KEYMAP_INDEX(5, 1)] = 227,           /* KEY_STAR */
	[KEYMAP_INDEX(5, 2)] = 230, /*SOFT2*/ /* 2 */
	[KEYMAP_INDEX(5, 3)] = KEY_MENU,      /* 1 */
	[KEYMAP_INDEX(5, 4)] = KEY_7,
};

#define QSD8x50_FFA_KEYMAP_SIZE (ARRAY_SIZE(keypad_col_gpios_8k_ffa) * \
			ARRAY_SIZE(keypad_row_gpios_8k_ffa))

static const unsigned short keypad_keymap_8k_ffa[QSD8x50_FFA_KEYMAP_SIZE] = {

	[FFA_8K_KEYMAP_INDEX(0, 0)] = KEY_VOLUMEDOWN,
	/*[KEYMAP_INDEX(0, 1)] = ,*/
	[FFA_8K_KEYMAP_INDEX(0, 2)] = KEY_DOWN,
	[FFA_8K_KEYMAP_INDEX(0, 3)] = KEY_8,
	[FFA_8K_KEYMAP_INDEX(0, 4)] = KEY_5,

	[FFA_8K_KEYMAP_INDEX(1, 0)] = KEY_UP,
	[FFA_8K_KEYMAP_INDEX(1, 1)] = KEY_CLEAR,
	[FFA_8K_KEYMAP_INDEX(1, 2)] = KEY_4,
	/*[KEYMAP_INDEX(1, 3)] = ,*/
	[FFA_8K_KEYMAP_INDEX(1, 4)] = KEY_2,

	[FFA_8K_KEYMAP_INDEX(2, 0)] = KEY_HOME,      /* A */
	[FFA_8K_KEYMAP_INDEX(2, 1)] = KEY_BACK,      /* B */
	[FFA_8K_KEYMAP_INDEX(2, 2)] = KEY_0,
	[FFA_8K_KEYMAP_INDEX(2, 3)] = 228,           /* KEY_SHARP */
	[FFA_8K_KEYMAP_INDEX(2, 4)] = KEY_9,

	[FFA_8K_KEYMAP_INDEX(3, 0)] = KEY_3,
	[FFA_8K_KEYMAP_INDEX(3, 1)] = KEY_RIGHT,
	[FFA_8K_KEYMAP_INDEX(3, 2)] = KEY_VOLUMEUP,
	/*[KEYMAP_INDEX(3, 3)] = ,*/
	[FFA_8K_KEYMAP_INDEX(3, 4)] = KEY_6,

	[FFA_8K_KEYMAP_INDEX(4, 0)] = 232,		/* OK */
	[FFA_8K_KEYMAP_INDEX(4, 1)] = KEY_SOUND,
	[FFA_8K_KEYMAP_INDEX(4, 2)] = KEY_1,
	[FFA_8K_KEYMAP_INDEX(4, 3)] = KEY_SEND,
	[FFA_8K_KEYMAP_INDEX(4, 4)] = KEY_LEFT,

	/*[KEYMAP_INDEX(5, 0)] = ,*/
	[FFA_8K_KEYMAP_INDEX(5, 1)] = 227,           /* KEY_STAR */
	[FFA_8K_KEYMAP_INDEX(5, 2)] = 230, /*SOFT2*/ /* 2 */
	[FFA_8K_KEYMAP_INDEX(5, 3)] = 229,      /* 1 */
	[FFA_8K_KEYMAP_INDEX(5, 4)] = KEY_7,
};

/* SURF keypad platform device information */
static struct gpio_event_matrix_info surf_keypad_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_surf,
	.output_gpios	= keypad_row_gpios,
	.input_gpios	= keypad_col_gpios,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_DRIVE_INACTIVE |
			  GPIOKPF_PRINT_UNMAPPED_KEYS
};

static struct gpio_event_info *surf_keypad_info[] = {
	&surf_keypad_matrix_info.info
};

static struct gpio_event_platform_data surf_keypad_data = {
	.name		= "surf_keypad",
	.info		= surf_keypad_info,
	.info_count	= ARRAY_SIZE(surf_keypad_info)
};

struct platform_device keypad_device_surf = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &surf_keypad_data,
	},
};

/* 8k FFA keypad platform device information */
static struct gpio_event_matrix_info keypad_matrix_info_8k_ffa = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_8k_ffa,
	.output_gpios	= keypad_row_gpios_8k_ffa,
	.input_gpios	= keypad_col_gpios_8k_ffa,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios_8k_ffa),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios_8k_ffa),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_DRIVE_INACTIVE |
			  GPIOKPF_PRINT_UNMAPPED_KEYS
};

static struct gpio_event_info *keypad_info_8k_ffa[] = {
	&keypad_matrix_info_8k_ffa.info
};

static struct gpio_event_platform_data keypad_data_8k_ffa = {
	.name		= "8k_ffa_keypad",
	.info		= keypad_info_8k_ffa,
	.info_count	= ARRAY_SIZE(keypad_info_8k_ffa)
};

struct platform_device keypad_device_8k_ffa = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &keypad_data_8k_ffa,
	},
};

#if 0
/* 7k FFA keypad platform device information */
static struct gpio_event_matrix_info keypad_matrix_info_7k_ffa = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_ffa,
	.output_gpios	= keypad_row_gpios,
	.input_gpios	= keypad_col_gpios,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_DRIVE_INACTIVE |
			  GPIOKPF_PRINT_UNMAPPED_KEYS
};
static struct gpio_event_info *keypad_info_7k_ffa[] = {
	&keypad_matrix_info_7k_ffa.info
};

#endif

#if 1
/* opal keypad platform device information */
static struct gpio_event_matrix_info keypad_matrix_info_7k_opal = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_opal,
	.output_gpios	= keypad_row_opal,
	.input_gpios	= keypad_col_opal,
	.noutputs	= ARRAY_SIZE(keypad_row_opal),
	.ninputs	= ARRAY_SIZE(keypad_col_opal),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ |
			  GPIOKPF_PRINT_UNMAPPED_KEYS
};

static struct gpio_event_info *keypad_info_7k_ffa[] = {
	&keypad_matrix_info_7k_opal.info
};
#endif
static struct gpio_event_platform_data keypad_data_7k_ffa = {
	.name		= "7k_ffa_keypad",
	.info		= keypad_info_7k_ffa,
	.info_count	= ARRAY_SIZE(keypad_info_7k_ffa)
};

struct platform_device keypad_device_7k_ffa = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &keypad_data_7k_ffa,
	},
};

