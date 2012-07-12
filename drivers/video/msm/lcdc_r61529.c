/* Copyright (c) 2010, JRD Communication Inc. All rights reserved.
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
 *-------------------------------------------------------
 */

#include <linux/delay.h>
#include <mach/gpio.h>
#include "msm_fb.h"
#include <mach/mpp.h>
#include <linux/leds.h>
#include <mach/pmic.h>
#include <linux/workqueue.h>
#include <linux/leds.h>

static int spi_cs;
static int spi_sclk;
static int spi_sdi;

struct r61529_state_type {
	boolean disp_initialized;
	boolean display_on;
	boolean disp_powered_up;
};
static struct r61529_state_type r61529_state = { 0 };

const int r61529_bk_intensity[6] = {0,14,15,4,3,1};
//unsigned int r61529_bk_intensity[7] = {0,1,2,3,4,5,6};

#if 0
unsigned int r61529_bk_intensity_debug[17] = {0,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1};
#endif

static struct msm_panel_common_pdata *lcdc_r61529_pdata;

#define GPIO_LCD_BK 90		//lcd backlight

static uint32_t set_gpio_bk_table[] = {
	GPIO_CFG(GPIO_LCD_BK, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA)
};

static uint32_t lcdc_gpio_table_sleep[] = {
	GPIO_CFG(132, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	//sclk
	GPIO_CFG(131, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	//cs
	GPIO_CFG(103, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	//sdi
	//GPIO_CFG(102, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),       //sdo
	GPIO_CFG(88, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),        //id

	GPIO_CFG(90, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	//bk
	//GPIO_CFG(96, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA) //reset
};

static void config_lcdc_gpio_table(uint32_t *table, int len, unsigned enable)
{
	int n, rc;
	for (n = 0; n < len; n++) {
		rc = gpio_tlmm_config(table[n],
			enable ? GPIO_CFG_ENABLE : GPIO_CFG_DISABLE);
		if (rc) {
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, table[n], rc);
			break;
		}
	}
}

/*
*It used index to indentify command or data tyep.1:data.0:cmd.
*/
static int spi_write(unsigned char index, unsigned char val)
{
	unsigned char sda;
	int i;
	gpio_set_value(spi_cs, 0);

	gpio_set_value(spi_sdi, index);
	gpio_set_value(spi_sclk, 0);
	udelay(0);
	gpio_set_value(spi_sclk, 1);
	udelay(0);

	for (i = 7; i >= 0; i--) {
		sda = ((val >> i) & 0x1);
		gpio_set_value(spi_sdi, sda);
		gpio_set_value(spi_sclk, 0);
		udelay(0);
		gpio_set_value(spi_sclk, 1);
		udelay(0);
	}
	gpio_set_value(spi_cs, 1);

	return 0;
}

static int r61529_reg_data(unsigned char data)
{
	spi_write(1, data);
	return 0;
}

static int r61529_reg_cmd(unsigned char cmd)
{
	spi_write(0, cmd);
	return 0;
}

static int spi_init(void)
{
	/* gpio setting */
	spi_sclk = *(lcdc_r61529_pdata->gpio_num);
	spi_cs = *(lcdc_r61529_pdata->gpio_num + 1);
	spi_sdi = *(lcdc_r61529_pdata->gpio_num + 2);
	//spi_sdo  = *(lcdc_r61529_pdata->gpio_num + 3);

	/* set the output so that we dont disturb the slave device */
	gpio_set_value(spi_sclk, 1);
	gpio_set_value(spi_sdi, 1);

	/* set the chip select de-asserted */
	gpio_set_value(spi_cs, 1);

	return 0;
}

static int r61529_reset(void)
{
	msleep(10);
	mpp_config_digital_out(13, MPP_CFG(MPP_DLOGIC_LVL_MSME, MPP_DLOGIC_OUT_CTRL_LOW));
	msleep(10);
	mpp_config_digital_out(13, MPP_CFG(MPP_DLOGIC_LVL_MSME, MPP_DLOGIC_OUT_CTRL_HIGH));
	msleep(100);

	return 0;
}

//update Nov 17,2010
static int r61529_init(void)
{
	r61529_reset();

	r61529_reg_cmd(0xB0);//
	r61529_reg_data(0x04);

	r61529_reg_cmd(0x36);
	r61529_reg_data(0x00);

	r61529_reg_cmd(0x3A);
	r61529_reg_data(0x66);

	r61529_reg_cmd(0xB4);
	r61529_reg_data(0x00);

	r61529_reg_cmd(0xC0);
	r61529_reg_data(0x03);//0013
	r61529_reg_data(0xDF);//480
	r61529_reg_data(0x40);
	r61529_reg_data(0x12);
	r61529_reg_data(0x00);
	r61529_reg_data(0x01);
	r61529_reg_data(0x00);
	r61529_reg_data(0x43);

	r61529_reg_cmd(0xC1);//frame frequency
	r61529_reg_data(0x05);//BCn,DIVn[1:0
	r61529_reg_data(0x28);//RTNn[4:0]
	r61529_reg_data(0x04);// BPn[7:0]
	r61529_reg_data(0x04);// FPn[7:0]
	r61529_reg_data(0x00);

	r61529_reg_cmd(0xC4);
	r61529_reg_data(0x64);//54
	r61529_reg_data(0x00);
	r61529_reg_data(0x08);
	r61529_reg_data(0x08);

	r61529_reg_cmd(0xC6);
	r61529_reg_data(0x0D);

 	r61529_reg_cmd(0xC8);//Gamma
	r61529_reg_data(0x06);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x16);
	r61529_reg_data(0x24);//26
	r61529_reg_data(0x30);//32
	r61529_reg_data(0x48);
	r61529_reg_data(0x3d);
	r61529_reg_data(0x28);
	r61529_reg_data(0x20);
	r61529_reg_data(0x14);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x04);
 	r61529_reg_data(0x06);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x16);
	r61529_reg_data(0x24);
	r61529_reg_data(0x30);
	r61529_reg_data(0x48);
	r61529_reg_data(0x3d);
	r61529_reg_data(0x28);
	r61529_reg_data(0x20);
	r61529_reg_data(0x14);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x04);

	r61529_reg_cmd(0xC9);//Gamma
	r61529_reg_data(0x06);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x16);
	r61529_reg_data(0x24);//26
	r61529_reg_data(0x30);//32
	r61529_reg_data(0x48);
	r61529_reg_data(0x3d);
	r61529_reg_data(0x28);
	r61529_reg_data(0x20);
	r61529_reg_data(0x14);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x04);
	r61529_reg_data(0x06);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x16);
	r61529_reg_data(0x24);
	r61529_reg_data(0x30);
	r61529_reg_data(0x48);
	r61529_reg_data(0x3d);
	r61529_reg_data(0x28);
	r61529_reg_data(0x20);
	r61529_reg_data(0x14);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x04);

	r61529_reg_cmd(0xCA);//Gamma
	r61529_reg_data(0x06);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x16);
	r61529_reg_data(0x24);//26
	r61529_reg_data(0x30);//32
	r61529_reg_data(0x48);
	r61529_reg_data(0x3d);
	r61529_reg_data(0x28);
	r61529_reg_data(0x20);
	r61529_reg_data(0x14);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x04);
	r61529_reg_data(0x06);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x16);
	r61529_reg_data(0x24);
	r61529_reg_data(0x30);
	r61529_reg_data(0x48);
	r61529_reg_data(0x3d);
	r61529_reg_data(0x28);
	r61529_reg_data(0x20);
	r61529_reg_data(0x14);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x04);

	r61529_reg_cmd(0xD0);
	r61529_reg_data(0x95);
	r61529_reg_data(0x06);
	r61529_reg_data(0x08);
	r61529_reg_data(0x10);
	r61529_reg_data(0x3c);

	r61529_reg_cmd(0xD1);
	r61529_reg_data(0x02);
	r61529_reg_data(0x2c);
	r61529_reg_data(0x2c);
	r61529_reg_data(0x3c);

	r61529_reg_cmd(0xE1);
	r61529_reg_data(0x00);
	r61529_reg_data(0x00);
	r61529_reg_data(0x00);
	r61529_reg_data(0x00);
	r61529_reg_data(0x00);
	r61529_reg_data(0x00);

	r61529_reg_cmd(0xE2);
	r61529_reg_data(0x80);

	 /*
	r61529_reg_cmd(0x2A);
	r61529_reg_data(0x00);
	r61529_reg_data(0x00);
	r61529_reg_data(0x01);
	r61529_reg_data(0x3F);

	r61529_reg_cmd(0x2B);
	r61529_reg_data(0x00);
	r61529_reg_data(0x00);
	r61529_reg_data(0x01);
	r61529_reg_data(0xDF);
 	*/
	r61529_reg_cmd(0x11);
  	msleep(120);

	r61529_reg_cmd(0x29);

	return 0;
}

#if 0
static int r61529_init(void)
{
	r61529_reset();

	r61529_reg_cmd(0x11);
	mdelay(500);

	r61529_reg_cmd(0xB0);
	r61529_reg_data(0x04);

	r61529_reg_cmd(0xB3);
	r61529_reg_data(0x02);
	r61529_reg_data(0x00);
	r61529_reg_data(0x00);
	r61529_reg_data(0x00);

	r61529_reg_cmd(0xB4);
	r61529_reg_data(0x00);

	r61529_reg_cmd(0xC0);
	r61529_reg_data(0x03);	//0013
	r61529_reg_data(0xDF);	//480
	r61529_reg_data(0x40);
	r61529_reg_data(0x12);
	r61529_reg_data(0x00);
	r61529_reg_data(0x01);
	r61529_reg_data(0x00);
	r61529_reg_data(0x43);

	r61529_reg_cmd(0xC1);	//frame frequency
	r61529_reg_data(0x05);	//BCn,DIVn[1:0]
	r61529_reg_data(0x28);	//RTNn[4:0]
	r61529_reg_data(0x04);	// BPn[7:0]
	r61529_reg_data(0x04);	// FPn[7:0]
	r61529_reg_data(0x00);

	r61529_reg_cmd(0xC4);
	r61529_reg_data(0x52);
	r61529_reg_data(0x00);
	r61529_reg_data(0x03);
	r61529_reg_data(0x03);

	r61529_reg_cmd(0xC6);
	r61529_reg_data(0x14);

	r61529_reg_cmd(0xC8);	//Gamma
	r61529_reg_data(0x06);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x16);
	r61529_reg_data(0x24);	//26
	r61529_reg_data(0x30);	//32
	r61529_reg_data(0x48);
	r61529_reg_data(0x3d);
	r61529_reg_data(0x28);
	r61529_reg_data(0x20);
	r61529_reg_data(0x14);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x04);
	r61529_reg_data(0x06);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x16);
	r61529_reg_data(0x24);
	r61529_reg_data(0x30);
	r61529_reg_data(0x48);
	r61529_reg_data(0x3d);
	r61529_reg_data(0x28);
	r61529_reg_data(0x20);
	r61529_reg_data(0x14);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x04);

	r61529_reg_cmd(0xC9);	//Gamma
	r61529_reg_data(0x06);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x16);
	r61529_reg_data(0x24);	//26
	r61529_reg_data(0x30);	//32
	r61529_reg_data(0x48);
	r61529_reg_data(0x3d);
	r61529_reg_data(0x28);
	r61529_reg_data(0x20);
	r61529_reg_data(0x14);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x04);
	r61529_reg_data(0x06);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x16);
	r61529_reg_data(0x24);
	r61529_reg_data(0x30);
	r61529_reg_data(0x48);
	r61529_reg_data(0x3d);
	r61529_reg_data(0x28);
	r61529_reg_data(0x20);
	r61529_reg_data(0x14);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x04);

	r61529_reg_cmd(0xCA);	//Gamma
	r61529_reg_data(0x06);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x16);
	r61529_reg_data(0x24);	//26
	r61529_reg_data(0x30);	//32
	r61529_reg_data(0x48);
	r61529_reg_data(0x3d);
	r61529_reg_data(0x28);
	r61529_reg_data(0x20);
	r61529_reg_data(0x14);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x04);
	r61529_reg_data(0x06);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x16);
	r61529_reg_data(0x24);
	r61529_reg_data(0x30);
	r61529_reg_data(0x48);
	r61529_reg_data(0x3d);
	r61529_reg_data(0x28);
	r61529_reg_data(0x20);
	r61529_reg_data(0x14);
	r61529_reg_data(0x0c);
	r61529_reg_data(0x04);

	r61529_reg_cmd(0xD0);
	r61529_reg_data(0x95);
	r61529_reg_data(0x0a);
	r61529_reg_data(0x08);
	r61529_reg_data(0x10);
	r61529_reg_data(0x39);

	r61529_reg_cmd(0xD1);
	r61529_reg_data(0x02);
	r61529_reg_data(0x28);
	r61529_reg_data(0x28);
	r61529_reg_data(0x20);

	r61529_reg_cmd(0x3a);
	r61529_reg_data(0x66);

	//set column address
	r61529_reg_cmd(0x2A);
	r61529_reg_data(0x00);
	r61529_reg_data(0x00);
	r61529_reg_data(0x01);
	r61529_reg_data(0x3F);

	//set page address
	r61529_reg_cmd(0x2B);
	r61529_reg_data(0x00);
	r61529_reg_data(0x00);
	r61529_reg_data(0x01);
	r61529_reg_data(0xDF);

	r61529_reg_cmd(0x11);
	mdelay(440);

	r61529_reg_cmd(0x29);
	r61529_reg_cmd(0x2C);
	mdelay(240);

	gpio_set_value(spi_cs, 0);

	return 0;
}
#endif

/*
static int r61529_sleep_exit(void)
{
	r61529_init();
	return 0;
}
*/

static int r61529_sleep_enter(void)
{
	//r61529_reg_cmd(0x28);	//display off
	r61529_reg_cmd(0x10);	//sleep in
	//mdelay(120);
	msleep(10);
	return 0;
}

/*LCD backlight is controled by MC8416 chip,which has 8 level
*intensity.Check the mc8416 datasheet to get more infor.
*/
static int mc8416_ctrl(const int step)
{
	int i;
	static unsigned int nr_pulse;

	gpio_set_value(GPIO_LCD_BK, 0);
	udelay(1200);//setting the internel data register to zero,according to MC8416 datasheet.

	printk(KERN_ERR"R61529 BL:%d\n",step);

	nr_pulse = r61529_bk_intensity[step];
	if(nr_pulse>16)
		nr_pulse = 16;

	for (i = 0; i < nr_pulse; i++){
		gpio_set_value(GPIO_LCD_BK, 0);
		udelay(10);
		gpio_set_value(GPIO_LCD_BK, 1);
		udelay(10);
	}

	return 0;
}

static int lcdc_r61529_bk_setting(const int step)
{
	if (step == 0)
		gpio_set_value(GPIO_LCD_BK, 0);	//turn off the backlight
	else
		mc8416_ctrl(step);

	return 0;
}

static void lcd_r61529_set_backlight(struct msm_fb_data_type *mfd)
{
	if (r61529_state.display_on==TRUE)
		lcdc_r61529_bk_setting(mfd->bl_level);
}

static void r61529_disp_on(void)
{
	if (r61529_state.disp_powered_up && !r61529_state.display_on) {
		r61529_init();
		r61529_state.display_on = TRUE;
	}
}

static void r61529_disp_powerup(void)
{
	if (!r61529_state.disp_powered_up && !r61529_state.display_on) {
		/* Reset the hardware first */
		/* Include DAC power up implementation here */
		r61529_state.disp_powered_up = TRUE;
	}
}

static int lcdc_r61529_panel_on(struct platform_device *pdev)
{
	if (!r61529_state.disp_initialized) {
		lcdc_r61529_pdata->panel_config_gpio(1);
		config_lcdc_gpio_table(set_gpio_bk_table, 1, 1);
		//spi_dac = *(lcdc_r61529_pdata->gpio_num + 4);
		//gpio_set_value(spi_dac, 0);
		//udelay(15);
		//gpio_set_value(spi_dac, 1);
		spi_init();	/* lcd needs spi */
		r61529_disp_powerup();
		r61529_disp_on();
		r61529_state.disp_initialized = TRUE;
	}
	return 0;
}

static int lcdc_r61529_panel_off(struct platform_device *pdev)
{
	r61529_sleep_enter();
	config_lcdc_gpio_table(lcdc_gpio_table_sleep, ARRAY_SIZE(lcdc_gpio_table_sleep), 1);
	mpp_config_digital_out(13, MPP_CFG(MPP_DLOGIC_LVL_MSME, MPP_DLOGIC_OUT_CTRL_LOW));

	if (r61529_state.disp_powered_up && r61529_state.display_on) {
		lcdc_r61529_pdata->panel_config_gpio(0);
		r61529_state.display_on = FALSE;
		r61529_state.disp_initialized = FALSE;
	}

	return 0;
}

/*
*Add the debug interface for lcd backlight, exists in file system of path:
*\sys\class\les\lcd_bk_debug.
*/
#if 0
static int lcd_bk_registered = 0;
void	lcdc_r61529_bk_debug(struct led_classdev *led_cdev,enum led_brightness brightness)
{
	if (brightness == 0) {
		gpio_set_value(GPIO_LCD_BK, 0);
	} else {
		mdelay(300);
		mc8416_ctrl(r61529_bk_intensity_debug[brightness]);
	}
}

static struct led_classdev lcd_bk_debug = {
	.name			= "lcd_bk_debug",
	.brightness_set	= lcdc_r61529_bk_debug,
};
#endif

static int r61529_probe(struct platform_device *pdev)
{
	if (pdev->id == 0) {
		lcdc_r61529_pdata = pdev->dev.platform_data;
		return 0;
	}
	msm_fb_add_device(pdev);
	lcdc_r61529_bk_setting(5);

#if 0
if (!lcd_bk_registered) {
		if (led_classdev_register(&pdev->dev, &lcd_bk_debug))
			printk(KERN_ERR "lcd_bk_debug failed\n");
		else
			lcd_bk_registered = 1;
	}
#endif

	return 0;
}

static struct platform_driver this_driver = {
	.probe = r61529_probe,
	.driver = {
		   .name = "lcdc_r61529_rgb",
		   },
};

static struct msm_fb_panel_data r61529_panel_data = {
	.on = lcdc_r61529_panel_on,
	.off = lcdc_r61529_panel_off,
	.set_backlight = lcd_r61529_set_backlight,
};

static struct platform_device this_device = {
	.name = "lcdc_r61529_rgb",
	.id = 1,
	.dev = {
		.platform_data = &r61529_panel_data,
		}
};

static int __init lcdc_r61529_panel_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

#ifdef CONFIG_FB_MSM_TRY_MDDI_CATCH_LCDC_PRISM
	if (msm_fb_detect_client("lcdc_r61529_rgb"))
		return 0;
#endif
	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

	pinfo = &r61529_panel_data.panel_info;
	pinfo->xres = 320;
	pinfo->yres = 480;
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bl_max = LCDC_BK_INTEN_MAX;
	pinfo->bl_min = 0;
	pinfo->bpp = 18;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 8000000;//need to change

	pinfo->lcdc.h_back_porch = 20;
	pinfo->lcdc.h_front_porch = 20;
	pinfo->lcdc.h_pulse_width = 10;
	pinfo->lcdc.v_back_porch = 4;
	pinfo->lcdc.v_front_porch = 4;
	pinfo->lcdc.v_pulse_width = 2;
	pinfo->lcdc.border_clr = 0;
	pinfo->lcdc.underflow_clr = 0xff;
	pinfo->lcdc.hsync_skew = 4;

	ret = platform_device_register(&this_device);
	if (ret) {
		platform_driver_unregister(&this_driver);
	}

	return ret;
}
module_init(lcdc_r61529_panel_init);
