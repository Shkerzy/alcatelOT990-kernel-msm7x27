/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include <mach/vreg.h>
#include <linux/proc_fs.h>

#undef CDBG
//#define CDBG(fmt, args...) printk("ov7690: %s %d() | " fmt, __func__, __LINE__,  ##args)
#define CDBG(fmt, args...)
#define CDB printk(KERN_INFO "litao debug - %s %5d %s\n", __FILE__, __LINE__, __func__);

/* OV7690 Registers and their values */
/* Sensor Core Registers */
#define  REG_OV7690_MODEL_ID 0x0a
#define  OV7690_MODEL_ID     0x9176

/*  SOC Registers Page 1  */
#define  REG_OV7690_SENSOR_RESET     0x12

struct ov7690_work {
	struct work_struct work;
};

static struct ov7690_work *ov7690_sensorw;
static struct i2c_client *ov7690_client;

struct ov7690_ctrl_t {
	const struct msm_camera_sensor_info *sensordata;
};

static struct ov7690_ctrl_t *ov7690_ctrl;

static DECLARE_WAIT_QUEUE_HEAD(ov7690_wait_queue);
DECLARE_MUTEX(ov7690_sem);

/*litao add for ov7690 proc file*/
struct ov7690_proc_t {
	unsigned int i2c_addr;
	unsigned int i2c_data;
};

#define OV7690_PROC_NAME "ov7690"
#define SINGLE_OP_NAME "sbdata"
#define DOUBLE_READ_NAME "double_byte_read"
#define I2C_ADDR_NAME "i2c_addr"
#define OV7690_PROC_LEN 16
static struct proc_dir_entry *ov7690_dir, *s_file, *w_file, *dr_file;
static char ov7690_proc_buffer[OV7690_PROC_LEN];
static struct ov7690_proc_t ov7690_proc_dt = {
	.i2c_addr = 0,
	.i2c_data = 0,
};

/*litao add end*/
/*=============================================================*/

static int32_t ov7690_i2c_txdata(unsigned short saddr, unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
		 .addr = saddr,
		 .flags = 0,
		 .len = length,
		 .buf = txdata,
		 },
	};

	if (i2c_transfer(ov7690_client->adapter, msg, 1) < 0) {
		CDBG("ov7690_i2c_txdata faild\n");
		return -EIO;
	}

	return 0;
}

static int32_t write_cmos_sensor(unsigned char waddr, unsigned char wdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[4];
	//printk("w %4x %2x\n", waddr, wdata);

	memset(buf, 0, sizeof(buf));
	buf[0] = waddr;
	buf[1] = wdata;
	rc = ov7690_i2c_txdata(ov7690_client->addr, buf, 2);

	if (rc < 0)
		CDBG("i2c_write failed, addr = 0x%x, val = 0x%x!\n", waddr, wdata);

	return rc;
}

static int ov7690_i2c_rxdata(unsigned short saddr, unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
		{
		 .addr = saddr,
		 .flags = 0,
		 .len = 1,
		 .buf = rxdata,
		 },
		{
		 .addr = saddr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = rxdata,
		 },
	};

	if (i2c_transfer(ov7690_client->adapter, msgs, 2) < 0) {
		CDBG("ov7690_i2c_rxdata failed!\n");
		return -EIO;
	}

	return 0;
}

static int32_t read_cmos_sensor_2(unsigned char raddr, void *rdata)
{
	int32_t rc = 0;
	unsigned char buf[4];
	int len = 2;

	//printk("raddr=%4x, len=%4x\n", raddr, len);
	memset(buf, 0, sizeof(buf));

	buf[0] = raddr;

	//printk("7690 client addr is %x\n", ov7690_client->addr);
	rc = ov7690_i2c_rxdata(ov7690_client->addr, buf, len);
	memcpy(rdata, buf, len);
	//*(char*)rdata = buf[1];
	//*(char*)(rdata+1) = buf[0];

	if (rc < 0)
		CDBG("ov7690_i2c_read failed!\n");

	return rc;
}

/*
static void reg_dump(void)
{
	unsigned short a, d;
	int rc = 0;

	printk("reg dump===\n");
	a = 0x00;
	while (a < 0xf0) {
		if (!(a & 0x0f))
			printk("\n%4x: ", a);
		rc = read_cmos_sensor_2(a, &d);
		if (rc < 0) {
			printk("error\n");
			return;
		}
		printk("%4x ", d);
		a += 2;
	}
	printk("\nreg dump over===\n");
}
*/

/*
static int32_t read_cmos_sensor(unsigned char raddr, void *rdata, int len)
{
	int32_t rc = 0;
	unsigned char buf[4];
	printk("raddr=%x, len=%x\n", raddr, len);

	if (!rdata || len > 4)
		return -EIO;

	memset(buf, 0, sizeof(buf));

	buf[0] = raddr;

	rc = ov7690_i2c_rxdata(ov7690_client->addr, buf, len);

	memcpy(rdata, buf, len);

	if (rc < 0)
		CDBG("ov7690_i2c_read failed!\n");

	return rc;
}
*/

static unsigned char read_cmos_sensor_1(unsigned char raddr)
{
	unsigned short tmp16;
	unsigned char tmp;

	read_cmos_sensor_2(raddr, &tmp16);
	tmp = tmp16 & 0xff;
	return tmp;
}

typedef struct {
	unsigned char addr;
	unsigned char value;
} OV7690_sensor_reg;

OV7690_sensor_reg OV7690_Init_Reg[] = {
//{0x12, 0x80},
	{0x0c, 0x16},
	{0x48, 0x42},
	{0x27, 0x80},
	{0x42, 0x0d},
	{0x64, 0x10},
	{0x68, 0xb0},		//b4
	{0x69, 0x12},
	{0x2f, 0x60},
	{0x41, 0x43},
	{0x44, 0x24},		//Updated per v5
	{0x4b, 0x0e},		//Updated per V13
	{0x4c, 0x7b},		//Updated per V16;  set black sun reference voltage.
	{0x4d, 0x0a},		//Updated per V6
	{0x29, 0x50},		//
	{0x1b, 0x19},		//updated per V8
	{0x39, 0x00},		//updated per V16
	{0x80, 0x7e},		//
	{0x81, 0xef},		//ff
	{0x91, 0x20},		//YAVG after Gamma
	{0x21, 0x33},		//updated per V16
//;;===Format===;;
	{0x11, 0x01},
	{0x12, 0x00},
	{0x82, 0x03},
	{0xd0, 0x26},
	{0x2B, 0x38},
	{0x15, 0x38},		//B8
//;;===Resolution===;;
	{0x16, 0x00},
	{0x17, 0x69},
	{0x18, 0xa4},
	{0x19, 0x0c},
	{0x1a, 0xf6},
	{0x3e, 0x30},
	{0xc8, 0x02},
	{0xc9, 0x80},		//;ISP input hsize (640)
	{0xca, 0x01},		// 
	{0xcb, 0xe0},		//;ISP input vsize (480)
	{0xcc, 0x02},		// 
	{0xcd, 0x80},		//;ISP output hsize (640)
	{0xce, 0x01},		// 
	{0xcf, 0xe0},		//;ISP output vsize (480)
//;;===Lens Correction==;;
	{0x80, 0x7F},
	{0x85, 0x10},
	{0x86, 0x20},
	{0x87, 0x09},
	{0x88, 0xAF},
	{0x89, 0x25},
	{0x8a, 0x20},
	{0x8b, 0x20},
//;;====Color Matrix====;;
	{0xbb, 0xac},		//;D7
	{0xbc, 0xae},		//;DA
	{0xbd, 0x02},		//;03
	{0xbe, 0x1f},		//;27
	{0xbf, 0x93},		//;B8
	{0xc0, 0xb1},		//;DE             
	{0xc1, 0x1A},
//;;===Edge + Denoise====;;
	{0xb4, 0x16},
	{0xb5, 0x05},
	{0xb8, 0x06},
	{0xb9, 0x02},
	{0xba, 0x08},
//;;====AEC/AGC target====;;
	{0x24, 0x94},
	{0x25, 0x80},
	{0x26, 0xB4},
//;;=====UV adjust======;;
	{0x81, 0xef},		//ff
	{0x5a, 0x00},		//KJ 04-->00
	{0x5b, 0x9f},		//KJ a5-->9f
	{0x5c, 0x3f},		//KJ 30-->3f
	{0x5d, 0x10},		//KJ 20-->10
//;;====Gamma====;;
	{0xa3, 0x05},
	{0xa4, 0x10},
	{0xa5, 0x25},
	{0xa6, 0x46},
	{0xa7, 0x57},
	{0xa8, 0x64},
	{0xa9, 0x70},
	{0xaa, 0x7c},
	{0xab, 0x87},
	{0xac, 0x90},
	{0xad, 0x9f},
	{0xae, 0xac},
	{0xaf, 0xc1},
	{0xb0, 0xd5},
	{0xb1, 0xe7},
	{0xb2, 0x21},
//;;==Advance==;;
	{0x8c, 0x52},
	{0x8d, 0x11},
	{0x8e, 0x12},
	{0x8f, 0x19},
	{0x90, 0x50},
	{0x91, 0x20},
	{0x92, 0xb1},
	{0x93, 0x9a},
	{0x94, 0xc},
	{0x95, 0xc},
	{0x96, 0xf0},
	{0x97, 0x10},
	{0x98, 0x61},
	{0x99, 0x63},
	{0x9a, 0x71},
	{0x9b, 0x78},
	{0x9c, 0xf0},
	{0x9d, 0xf0},
	{0x9e, 0xf0},
	{0x9f, 0xff},
	{0xa0, 0xa7},
	{0xa1, 0xb0},
	{0xa2, 0xf},
	{0x14, 0x11},		//21-->11;16x gain ceiling, PPChrg off
	{0x13, 0xf7},
	{0x50, 0x49},		//;banding
//[sensor.YUV.640x480]
	{0xcc, 0x02},
	{0xcd, 0x80},
	{0xce, 0x01},
	{0xcf, 0xe0},
	{0x50, 0x94},		//;banding
//[sensor.YUV.640x480.15]
	{0x29, 0x50},
	{0x11, 0x01},
	{0x2c, 0x00},
	{0x21, 0x33},

//For test camera to hardware. jshuai
	{0xd2, 0x02},
	{0xd8, 0x42},
	{0xd9, 0x42},
//{0x5a, 0x14},
	{0x49, 0x0d},

	{0xFF, 0xFF}
};

/*****************************************************************************
 * FUNCTION
 *  init_cmos_sensor
 * DESCRIPTION
 *  
 * PARAMETERS
 *  void
 * RETURNS
 *  void
 *****************************************************************************/
void OV7690_init_cmos_sensor(void)
{
    /*----------------------------------------------------------------*/
	/* Local Variables                                                */
    /*----------------------------------------------------------------*/
	int i = 0;

    /*----------------------------------------------------------------*/
	/* Code Body                                                      */
    /*----------------------------------------------------------------*/
	write_cmos_sensor(0x12, 0x80);
	mdelay(5);

	while (0xFF != OV7690_Init_Reg[i].addr) {
		write_cmos_sensor(OV7690_Init_Reg[i].addr, OV7690_Init_Reg[i].value);
		i++;
	}
}

static long ov7690_reg_init(void)
{
	CDBG("ov7690_reg_init\n");
	OV7690_init_cmos_sensor();
	return 0;
}

static long ov7690_priview(void)
{
	long rc = 0;

	CDBG("ov7690 preview:\n");

	return rc;
}				/* OV2650_Preview */

static long ov7690_snapshot(void)
{
	long rc = 0;

	CDBG("ov7690 snapshot:\n");

	return rc;

}				/* OV2650_Capture */

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static long ov7690_set_sensor_mode(int mode)
{
	long rc = 0;

	CDBG("ov7690 set sensor mode %d:\n", mode);
	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		rc = ov7690_priview();
		break;
	case SENSOR_SNAPSHOT_MODE:
		rc = ov7690_snapshot();
		break;
	default:
		return -EFAULT;
	}

	return rc;
}

static long ov7690_set_antibanding(int value)
{
	long rc = 0;
	unsigned char tmp;

	CDBG("ov7690 set antibanding %d:\n", value);
	//rc = read_cmos_sensor(0x3014, &banding, 1); 
	tmp = read_cmos_sensor_1(0x13);
	switch (value) {
	case CAMERA_ANTIBANDING_OFF:
		write_cmos_sensor(0x13, tmp & ~0x20);
		printk("%d:%s-CAMERA_ANTIBANDING_OFF\n", __LINE__, __func__);
		break;
	case CAMERA_ANTIBANDING_50HZ:
		write_cmos_sensor(0x13, tmp | ~0x20);
		tmp = read_cmos_sensor_1(0x14);
		write_cmos_sensor(0x14, tmp | 0x01);
		printk("%d:%s-CAMERA_ANTIBANDING_50hz\n", __LINE__, __func__);
		break;
	case CAMERA_ANTIBANDING_60HZ:
		write_cmos_sensor(0x13, tmp | ~0x20);
		tmp = read_cmos_sensor_1(0x14);
		write_cmos_sensor(0x14, tmp & ~0x01);
		printk("%d:%s-CAMERA_ANTIBANDING_60hz\n", __LINE__, __func__);
		break;
	case CAMERA_ANTIBANDING_AUTO:
		printk("not support\n");
		printk("%d:%s-CAMERA_ANTIBANDING_auto\n", __LINE__, __func__);
		break;
	default:
		return -EFAULT;
	}
	return rc;
}

static long ov7690_set_wb(int value)
{
	long rc = 0;
	unsigned char temp_reg = 0;

	CDBG("ov7690 set wb %d:\n", value);

	temp_reg = read_cmos_sensor_1(0x13);
	switch (value) {
	case CAMERA_WB_AUTO:
		temp_reg |= 0x2;
		write_cmos_sensor(0x13, temp_reg);
		printk("%d:%s-whitebalance auto\n", __LINE__, __func__);
		break;
	case CAMERA_WB_INCANDESCENT:
		temp_reg &= ~0x2;
		write_cmos_sensor(0x13, temp_reg);
		write_cmos_sensor(0x01, 0xb0);
		write_cmos_sensor(0x02, 0x40);
		write_cmos_sensor(0x03, 0x4a);
		printk("%d:%s-CAMERA_WB_INCANDESCENT\n", __LINE__, __func__);
		break;
	case CAMERA_WB_FLUORESCENT:
		temp_reg &= ~0x2;
		write_cmos_sensor(0x13, temp_reg);
		write_cmos_sensor(0x01, 0x93);
		write_cmos_sensor(0x02, 0x50);
		write_cmos_sensor(0x03, 0x40);
		printk("%d:%s-CAMERA_WB_FLUORESCENT\n", __LINE__, __func__);
		break;
	case CAMERA_WB_DAYLIGHT:
		temp_reg &= ~0x2;
		write_cmos_sensor(0x13, temp_reg);
		write_cmos_sensor(0x01, 0x55);
		write_cmos_sensor(0x02, 0x64);
		write_cmos_sensor(0x03, 0x40);
		printk("%d:%s-CAMERA_WB_DAYLIGHT\n", __LINE__, __func__);
		break;
	case CAMERA_WB_CLOUDY_DAYLIGHT:
		temp_reg &= 0xfd;
		write_cmos_sensor(0x13, temp_reg);
		write_cmos_sensor(0x01, 0x54);
		write_cmos_sensor(0x02, 0x6f);
		write_cmos_sensor(0x03, 0x40);
		printk("%d:%s-CAMERA_WB_CLOUDY_\n", __LINE__, __func__);
		break;
	default:
		return -EFAULT;
	}
	return rc;
}

static long ov7690_set_brightness(int value)
{
	long rc = 0;
	unsigned char tmp, tmp1, tmp2;

	tmp = read_cmos_sensor_1(0x81);
	tmp1 = read_cmos_sensor_1(0xd2);
	tmp2 = read_cmos_sensor_1(0xdc);
	printk("%d:%s brightness is %d\n", __LINE__, __func__, value);
	switch (value) {
	case 8:
		write_cmos_sensor(0x81, tmp | 0x20);
		write_cmos_sensor(0xd2, tmp1 | 0x04);
		write_cmos_sensor(0xdc, tmp2 & ~0x08);
		write_cmos_sensor(0xd3, 0x40);
		printk("%d:%s-CAMERA_WB_INCANDESCENT 8\n", __LINE__, __func__);
		break;
	case 7:
		write_cmos_sensor(0x81, tmp | 0x20);
		write_cmos_sensor(0xd2, tmp1 | 0x04);
		write_cmos_sensor(0xdc, tmp2 & ~0x08);
		write_cmos_sensor(0xd3, 0x30);
		printk("%d:%s-CAMERA_WB_INCANDESCENT 7\n", __LINE__, __func__);
		break;
	case 6:
		write_cmos_sensor(0x81, tmp | 0x20);
		write_cmos_sensor(0xd2, tmp1 | 0x04);
		write_cmos_sensor(0xdc, tmp2 & ~0x08);
		write_cmos_sensor(0xd3, 0x20);
		printk("%d:%s-CAMERA_WB_INCANDESCENT 6\n", __LINE__, __func__);
		break;
	case 5:
		write_cmos_sensor(0x81, tmp | 0x20);
		write_cmos_sensor(0xd2, tmp1 | 0x04);
		write_cmos_sensor(0xdc, tmp2 & ~0x08);
		write_cmos_sensor(0xd3, 0x10);
		printk("%d:%s-CAMERA_WB_INCANDESCENT 5\n", __LINE__, __func__);
		break;
	case 4:
		write_cmos_sensor(0x81, tmp | 0x20);
		write_cmos_sensor(0xd2, tmp1 | 0x04);
		write_cmos_sensor(0xdc, tmp2 & ~0x08);
		write_cmos_sensor(0xd3, 0x00);
		printk("%d:%s-CAMERA_WB_INCANDESCENT 4\n", __LINE__, __func__);
		break;
	case 3:
		write_cmos_sensor(0x81, tmp | 0x20);
		write_cmos_sensor(0xd2, tmp1 | 0x04);
		write_cmos_sensor(0xdc, tmp2 | 0x08);
		write_cmos_sensor(0xd3, 0x10);
		printk("%d:%s-CAMERA_WB_INCANDESCENT 3\n", __LINE__, __func__);
		break;
	case 2:
		write_cmos_sensor(0x81, tmp | 0x20);
		write_cmos_sensor(0xd2, tmp1 | 0x04);
		write_cmos_sensor(0xdc, tmp2 | 0x08);
		write_cmos_sensor(0xd3, 0x20);
		printk("%d:%s-CAMERA_WB_INCANDESCENT 2\n", __LINE__, __func__);
		break;
	case 1:
		write_cmos_sensor(0x81, tmp | 0x20);
		write_cmos_sensor(0xd2, tmp1 | 0x04);
		write_cmos_sensor(0xdc, tmp2 | 0x08);
		write_cmos_sensor(0xd3, 0x30);
		printk("%d:%s-CAMERA_WB_INCANDESCENT\n", __LINE__, __func__);
		break;
	case 0:
		write_cmos_sensor(0x81, tmp | 0x20);
		write_cmos_sensor(0xd2, tmp1 | 0x04);
		write_cmos_sensor(0xdc, tmp2 | 0x08);
		write_cmos_sensor(0xd3, 0x40);
		printk("%d:%s-CAMERA_WB_INCANDESCENT 0\n", __LINE__, __func__);
		break;
	default:
		return -EFAULT;
	}
	return rc;
}

/*
static long ov7690_set_nightmode(int mode)
{
	long rc = 0;

	printk("%s:%d:%s-set nightmode %d\n", __FILE__, __LINE__, __func__, mode);
	if (mode == CAMERA_NIGHTSHOT_MODE_ON) {
		printk("%s:%d:%s-CAMERA_NIGHTSHOT_MODE_ON\n", __FILE__, __LINE__, __func__);
		//write_cmos_sensor(0x302D, extra_exposure_line_h);
		//write_cmos_sensor(0x302E, extra_exposure_line_l);

	} else {
		printk("%s:%d:%s-CAMERA_NIGHTSHOT_MODE_OFF\n", __FILE__, __LINE__, __func__);
		//rc = write_cmos_sensor(0x302d, 0x00);
		//rc = write_cmos_sensor(0x302e, 0x00);

	}
	return rc;
}
*/

static long ov7690_set_effect(int mode, int8_t effect)
{
	long rc = 0;
	unsigned char temp = 0, tmp, tmp1;

	tmp = read_cmos_sensor_1(0x28);
	tmp1 = read_cmos_sensor_1(0xd2);
	printk("%d:%s-set effect %d\n", __LINE__, __func__, effect);
	switch (effect) {
	case CAMERA_EFFECT_OFF:
		temp = read_cmos_sensor_1(0x81);
		temp |= 0x20;
		write_cmos_sensor(0x81, temp);
	//	write_cmos_sensor(0x28, tmp & ~0x80);
		write_cmos_sensor(0xd2, tmp1 & ~0x58);
		break;

	case CAMERA_EFFECT_SEPIA:
		temp = read_cmos_sensor_1(0x81);
		temp |= 0x20;
		write_cmos_sensor(0x81, temp);
	//	write_cmos_sensor(0x28, tmp & ~0x80);
		tmp1 &= ~0x40;
		tmp1 |= 0x18;
		write_cmos_sensor(0xd2, tmp1);
		write_cmos_sensor(0xda, 0x40);
		write_cmos_sensor(0xdb, 0xa0);
		break;

	case CAMERA_EFFECT_NEGATIVE:
		temp = read_cmos_sensor_1(0x81);
		temp |= 0x20;
		write_cmos_sensor(0x81, temp);
	//	write_cmos_sensor(0x28, tmp | 0x80);
		write_cmos_sensor(0xd2, tmp1 | 0x40);
		break;

	case CAMERA_EFFECT_MONO:	//B&W
		temp = read_cmos_sensor_1(0x81);
		temp |= 0x20;
		write_cmos_sensor(0x81, temp);
	//	write_cmos_sensor(0x28, tmp & ~0x80);
		tmp1 &= ~0x40;
		tmp1 |= 0x18;
		write_cmos_sensor(0xd2, tmp1);
		write_cmos_sensor(0xda, 0x80);
		write_cmos_sensor(0xdb, 0x80);
		break;

	default:
		break;
	}
	return rc;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static int ov7690_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
	uint16_t model_id = 0;
	int rc = 0, probe_retry = 3;

	//printk("set gpio 0 to 1\n");
	CDBG("init entry \n");
	//shut down ov5647
	rc = gpio_request(0, "ov5647");

	if (!rc) {
		//printk("gpio 0 set to 1\n");
		rc = gpio_direction_output(0, 1);
	}

	gpio_free(0);

	rc = gpio_request(1, "ov5647");

	if (!rc) {
		rc = gpio_direction_output(1, 0);
	}

	gpio_free(1);

	////////////////////////

	//printk("sensorpwd %d\n", data->sensor_pwd);
	rc = gpio_request(data->sensor_pwd, "ov7690");

	if (!rc) {
		//printk("gpio sensor set to 0\n");
		rc = gpio_direction_output(data->sensor_pwd, 0);
	}

	gpio_free(data->sensor_pwd);

	mdelay(5);
	//<4>set polarity (to BB)
	//<5>set data format to ISP (to BB)
	//<6>use 48MHz source (to BB)
	//<7>clk / 2 = 24MHz (to BB)
	//<8>set ISP driving current (to BB)

	mdelay(5);

	/* Read the Model ID of the sensor */
detect_retry:
	probe_retry--;
	printk("retry left: %d\nabout to read ID of 7690\n", probe_retry);
	rc = read_cmos_sensor_2(0x0a, &model_id);

	if (rc < 0){
		if(probe_retry)
			goto detect_retry;
		goto init_probe_fail;
	}

	CDBG("ov7690 model_id = 0x%x\n", model_id);

	/* Check if it matches it with the value in Datasheet */
	if (model_id != OV7690_MODEL_ID) {
		rc = -EFAULT;
		goto init_probe_fail;
	}

	rc = ov7690_reg_init();
	if (rc < 0)
		goto init_probe_fail;

	return rc;

      init_probe_fail:
	return rc;
}

//=============================//

static int proc_ov7690_dread(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len, rc;
	unsigned short tmp16;
	CDB
	/************************/
	/************************/
	    rc = read_cmos_sensor_2(ov7690_proc_dt.i2c_addr, &tmp16);
	if (rc < 0) {
		len = sprintf(page, "double 0x%x@ov7690_i2c read error\n", ov7690_proc_dt.i2c_addr);
	} else {
		len = sprintf(page, "double 0x%x@ov7690_i2c = %4x\n", ov7690_proc_dt.i2c_addr, tmp16);
	}
	CDB 
	return len;
}

static int proc_ov7690_sread(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len, rc;
	unsigned char tmp8;

	//rc = read_cmos_sensor(ov7690_proc_dt.i2c_addr, &tmp8, 1);
	tmp8 = read_cmos_sensor_1(ov7690_proc_dt.i2c_addr);
	rc = 0;
	if (rc < 0) {
		len = sprintf(page, "single byte 0x%x@ov7690_i2c read error\n", ov7690_proc_dt.i2c_addr);
	} else {
		len = sprintf(page, "single byte 0x%x@ov7690_i2c = %4x\n", ov7690_proc_dt.i2c_addr, tmp8);
	}

	return len;
#if 0
	int len, rc;
	unsigned char tmp[16];
	tmp[0] = 8;
	rc = 0;
	CDB
	    //rc = read_cmos_sensor(ov7690_proc_dt.i2c_addr, tmp, 3);
	    /*
	       if(rc < 0){
	       len = sprintf(page, "single byte 0x%x@ov7690_i2c read error\n",
	       ov7690_proc_dt.i2c_addr);
	       }
	       else{
	       len = sprintf(page, "single byte 0x%x@ov7690_i2c = %4x\n",
	       ov7690_proc_dt.i2c_addr, tmp[0]);
	       }
	     */
	    len = sprintf(page, "%x", tmp[0]);
	CDB 
	return len;
#endif
}

static int proc_ov7690_swrite(struct file *file, const char *buffer, unsigned long count, void *data)
{
	int len;

//CDB
	memset(ov7690_proc_buffer, 0, OV7690_PROC_LEN);
	if (count > OV7690_PROC_LEN)
		len = OV7690_PROC_LEN;
	else
		len = count;

	//printk("count = %d\n", len);
	if (copy_from_user(ov7690_proc_buffer, buffer, len))
		return -EFAULT;
	//printk("%s\n", ov7690_proc_buffer);
	sscanf(ov7690_proc_buffer, "%x", &ov7690_proc_dt.i2c_data);
	//printk("received %x\n", ov7690_proc_dt.i2c_addr);
	write_cmos_sensor(ov7690_proc_dt.i2c_addr, ov7690_proc_dt.i2c_data);
//CDB
	return len;

}

static int proc_ov7690_addr_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len;

	CDB 
	len = sprintf(page, "addr is 0x%x\n", ov7690_proc_dt.i2c_addr);
	CDB 
	return len;
}

static int proc_ov7690_addr_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
	int len;

	CDB 
	memset(ov7690_proc_buffer, 0, OV7690_PROC_LEN);
	if (count > OV7690_PROC_LEN)
		len = OV7690_PROC_LEN;
	else
		len = count;

	//printk("count = %d\n", len);
	if (copy_from_user(ov7690_proc_buffer, buffer, len))
		return -EFAULT;
	//printk("%s\n", ov7690_proc_buffer);
	sscanf(ov7690_proc_buffer, "%x", &ov7690_proc_dt.i2c_addr);
	//printk("received %x\n", ov7690_proc_dt.i2c_addr);
	CDB 
	return len;

}

int ov7690_add_proc(void)
{
	int rc;
/* add for proc*/
	/* create directory */
	ov7690_dir = proc_mkdir(OV7690_PROC_NAME, NULL);
	if (ov7690_dir == NULL) {
		rc = -ENOMEM;
		goto init_fail;
	}
	//ov7690_dir->owner = THIS_MODULE;

	/* create readfile */
	s_file = create_proc_entry(SINGLE_OP_NAME, 0644, ov7690_dir);
	if (s_file == NULL) {
		rc = -ENOMEM;
		goto no_s;
	}
	s_file->read_proc = proc_ov7690_sread;
	s_file->write_proc = proc_ov7690_swrite;
	//s_file->owner = THIS_MODULE;

	dr_file = create_proc_read_entry(DOUBLE_READ_NAME, 0444, ov7690_dir, proc_ov7690_dread, NULL);
	if (dr_file == NULL) {
		rc = -ENOMEM;
		goto no_dr;
	}
	//dr_file->owner = THIS_MODULE;

	/* create write file */
	w_file = create_proc_entry(I2C_ADDR_NAME, 0644, ov7690_dir);
	if (w_file == NULL) {
		rc = -ENOMEM;
		goto no_wr;
	}

	w_file->read_proc = proc_ov7690_addr_read;
	w_file->write_proc = proc_ov7690_addr_write;
	//w_file->owner = THIS_MODULE;

/*litao add end*/
	return 0;
/*litao add for proc*/
      no_wr:
	remove_proc_entry(DOUBLE_READ_NAME, ov7690_dir);
      no_dr:
	remove_proc_entry(SINGLE_OP_NAME, ov7690_dir);
      no_s:
	remove_proc_entry(OV7690_PROC_NAME, NULL);
/*litao add end*/
      init_fail:
	return 1;

}

int ov7690_del_proc(void)
{
	remove_proc_entry(I2C_ADDR_NAME, ov7690_dir);
	remove_proc_entry(DOUBLE_READ_NAME, ov7690_dir);
	remove_proc_entry(SINGLE_OP_NAME, ov7690_dir);
	remove_proc_entry(OV7690_PROC_NAME, NULL);
	printk(KERN_INFO "%s %s %s removed\n", SINGLE_OP_NAME, DOUBLE_READ_NAME, I2C_ADDR_NAME);
	return 0;
}

//=============================================//
int ov7690_sensor_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

	CDBG("ov7690_sensor_init\n");
	ov7690_ctrl = kzalloc(sizeof(struct ov7690_ctrl_t), GFP_KERNEL);
	if (!ov7690_ctrl) {
		CDBG("ov7690_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}

	if (data)
		ov7690_ctrl->sensordata = data;

	/* Input MCLK = 24MHz */
	msm_camio_clk_rate_set(24000000);
	mdelay(5);

	msm_camio_camif_pad_reg_reset();

	rc = ov7690_sensor_init_probe(data);
	if (rc < 0) {
		CDBG("ov7690_sensor_init failed!\n");
		goto init_fail;
	}
	ov7690_add_proc();

      init_done:
	return rc;

      init_fail:
	kfree(ov7690_ctrl);
	return rc;
}

static int ov7690_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	CDBG("ov7690_init_client\n");
	init_waitqueue_head(&ov7690_wait_queue);
	return 0;
}

int ov7690_sensor_config(void __user * argp)
{
	struct sensor_cfg_data cfg_data;
	long rc = 0;

	CDBG("ov7690_sensor_config\n");
	if (copy_from_user(&cfg_data, (void *)argp, sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	/* down(&ov7690_sem); */

	CDBG("ov7690_ioctl, cfgtype = %d, mode = %d\n", cfg_data.cfgtype, cfg_data.mode);

	switch (cfg_data.cfgtype) {
	case CFG_SET_MODE:
		rc = ov7690_set_sensor_mode(cfg_data.mode);
		break;

	case CFG_SET_EFFECT:
		rc = ov7690_set_effect(cfg_data.mode, cfg_data.cfg.effect);
		break;
/*
	case CFG_SET_NIGHTMODE:
		rc = ov7690_set_nightmode(cfg_data.cfg.value);
		break;
*/
	case CFG_SET_BRIGHTNESS:
		rc = ov7690_set_brightness(cfg_data.cfg.value);
		break;

	case CFG_SET_WB:
		rc = ov7690_set_wb(cfg_data.cfg.value);
		break;

	case CFG_SET_ANTIBANDING:
		rc = ov7690_set_antibanding(cfg_data.cfg.value);
		break;

	default:
		rc = -EFAULT;
		break;
	}

	/* up(&ov7690_sem); */

	return rc;
}

int ov7690_sensor_release(void)
{
	int rc = 0;

	CDBG("ov7690_sensor_release\n");
	/* down(&ov7690_sem); */

	/*shut down ov7690:pwn = gpio23*/
	/*
	gpio_request(23, "ov7690");
	gpio_direction_output(23, 1);
	gpio_free(23);
	*/

	kfree(ov7690_ctrl);
	/* up(&ov7690_sem); */

	ov7690_del_proc();
	return rc;
}

static int __exit ov7690_i2c_remove(struct i2c_client *client)
{
	struct ov7690_work_t *sensorw = i2c_get_clientdata(client);

	CDBG("ov7690_i2c_remove\n");
	free_irq(client->irq, sensorw);
	ov7690_client = NULL;
	ov7690_sensorw = NULL;
	kfree(sensorw);
	return 0;
}

static int ov7690_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc = 0;

	CDBG("ov7690_i2c_probe\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	ov7690_sensorw = kzalloc(sizeof(struct ov7690_work), GFP_KERNEL);

	if (!ov7690_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, ov7690_sensorw);
	ov7690_init_client(client);
	ov7690_client = client;

	CDBG("ov7690_probe succeeded!\n");

	return 0;

      probe_failure:
	//kfree(ov7690_sensorw);
	ov7690_sensorw = NULL;
	CDBG("ov7690_probe failed!\n");
	return rc;
}

static const struct i2c_device_id ov7690_i2c_id[] = {
	{"ov7690", 0},
	{},
};

static struct i2c_driver ov7690_i2c_driver = {
	.id_table = ov7690_i2c_id,
	.probe = ov7690_i2c_probe,
	.remove = __exit_p(ov7690_i2c_remove),
	.driver = {
		   .name = "ov7690",
		   },
};

static int ov7690_sensor_probe(const struct msm_camera_sensor_info *info, struct msm_sensor_ctrl *s)
{
	int rc;

	CDBG("ov7690_sensor_probe\n");
	rc = i2c_add_driver(&ov7690_i2c_driver);

	if (rc < 0 || ov7690_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_done;
	}

	/* Input MCLK = 24MHz */
	msm_camio_clk_rate_set(24000000);
	mdelay(5);

	rc = ov7690_sensor_init_probe(info);
	if (rc < 0)
		goto probe_done;

	s->s_init = ov7690_sensor_init;
	s->s_release = ov7690_sensor_release;
	s->s_config = ov7690_sensor_config;
	s->s_mount_angle  = 270;
	s->s_camera_type = FRONT_CAMERA_2D;

      probe_done:
	CDBG("%s:%d\n", __func__, __LINE__);
	return rc;
}

static int __ov7690_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, ov7690_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __ov7690_probe,
	.driver = {
		   .name = "msm_camera_ov7690",
		   .owner = THIS_MODULE,
		   },
};

static int __init ov7690_init(void)
{
	int rc;

	CDBG("ov7690 init\n");
	rc = platform_driver_register(&msm_camera_driver);
	return rc;

}

module_init(ov7690_init);
