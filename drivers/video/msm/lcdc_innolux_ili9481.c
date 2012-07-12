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

static int spi_cs;
static int spi_sclk;
//static int spi_sdo;
static int spi_sdi;
//static int spi_dac;

struct innolux_ili9481_state_type {
	boolean disp_initialized;
	boolean display_on;
	boolean disp_powered_up;
};
static struct innolux_ili9481_state_type innolux_ili9481_state = { 0 };

const int innolux_ili9481_bk_intensity[6] = {0,14,15,4,3,1};
//unsigned int innolux_innolux_ili9481_bk_intensity[7] = {0,1,2,3,4,5,6};

static struct msm_panel_common_pdata *lcdc_innolux_ili9481_pdata;

#define GPIO_LCD_BK 90		//lcd backlight

static uint32_t set_gpio_bk_table[] = {
	GPIO_CFG(GPIO_LCD_BK, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA)
};

/*static uint32_t spi_sdi_input[] = {
	GPIO_CFG(103, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA)
};*/

/*static uint32_t spi_sdi_output[] = {
	GPIO_CFG(103, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA)
};*/

static uint32_t lcdc_gpio_table_sleep[] = {
	GPIO_CFG(132, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	//sclk
	GPIO_CFG(131, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	//cs
	GPIO_CFG(103, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	//sdi
	//GPIO_CFG(102, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),       //sdo
	GPIO_CFG(88, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),        //id

	GPIO_CFG(90, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	//bk
	//GPIO_CFG(96, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA) //reset
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

/*
*Just test,not used here!!!.
*/
/*
static int spi_test(void)
{
	int retval = 0;
	int tmp, i;

	spi_write(0, 0x0C);	//get pixel format.

	gpio_set_value(spi_cs, 0);	//put cs low
	config_lcdc_gpio_table(spi_sdi_input, 1, 1);	//change spi_sdi to input

	for (i = 7; i >= 0; i--) {
		gpio_set_value(spi_sclk, 0);
		udelay(0);	//_nop_();
		gpio_set_value(spi_sclk, 1);
		udelay(0);	//_nop_();
		tmp = gpio_get_value(spi_sdi);

		if (tmp == 0)
			retval |= 0;
		else
			retval |= (0x1 << i);
	}

	gpio_set_value(spi_cs, 1);
	config_lcdc_gpio_table(spi_sdi_output, 1, 1);

	return 0;
}
*/

static int innolux_ili9481_reg_data(unsigned char data)
{
	spi_write(1, data);
	return 0;
}

static int innolux_ili9481_reg_cmd(unsigned char cmd)
{
	spi_write(0, cmd);
	return 0;
}

static int spi_init(void)
{
	/* gpio setting */
	spi_sclk = *(lcdc_innolux_ili9481_pdata->gpio_num);
	spi_cs = *(lcdc_innolux_ili9481_pdata->gpio_num + 1);
	spi_sdi = *(lcdc_innolux_ili9481_pdata->gpio_num + 2);
	//spi_sdo  = *(lcdc_ili9481_pdata->gpio_num + 3);

	/* set the output so that we dont disturb the slave device */
	gpio_set_value(spi_sclk, 1);
	gpio_set_value(spi_sdi, 1);

	/* set the chip select de-asserted */
	gpio_set_value(spi_cs, 1);

	return 0;
}

static int innolux_ili9481_reset(void)
{
//	mdelay(10);
	mpp_config_digital_out(13, MPP_CFG(MPP_DLOGIC_LVL_MSME, MPP_DLOGIC_OUT_CTRL_LOW));
	msleep(10);
	mpp_config_digital_out(13, MPP_CFG(MPP_DLOGIC_LVL_MSME, MPP_DLOGIC_OUT_CTRL_HIGH));
	msleep(100);

	return 0;
}

static int innolux_ili9481_init(void)
{
	innolux_ili9481_reset();

	//lcd init.
	//start Initial sequence
	innolux_ili9481_reg_cmd(0x11);	//sleep out
	msleep(120);		//waiting for internal operation,* Must be satisfied*

	//power setting
	innolux_ili9481_reg_cmd(0xD0);
	innolux_ili9481_reg_data(0x07);	//VCI1=VCI
	innolux_ili9481_reg_data(0x45);	//PON=1,BT2-0=2
	innolux_ili9481_reg_data(0x1B);	//VREG1OUT=4.375V

	innolux_ili9481_reg_cmd(0xD1);	//VCOM
	innolux_ili9481_reg_data(0x00);	//REGISTER SETTING
	innolux_ili9481_reg_data(0x14);	//VCOMH=0.785 X VREG1OUT
	innolux_ili9481_reg_data(0x1B);	//VDV=1.24 X VREG1OUT

	innolux_ili9481_reg_cmd(0xD2);
	innolux_ili9481_reg_data(0x01);	//AP2-0=1.0
	innolux_ili9481_reg_data(0x12);	//DC/DC2=FOSC/32, DC/DC1=FOSC/4

	//panel setting
	innolux_ili9481_reg_cmd(0xC0);	//panel driving setting
	innolux_ili9481_reg_data(0x00);	//04 GS=0
	innolux_ili9481_reg_data(0x3B);	//480 LINE
	innolux_ili9481_reg_data(0x00);	//SCAN START LINE:1
	innolux_ili9481_reg_data(0x02);	//NDL
	innolux_ili9481_reg_data(0x01);	//PTG

	//default setting
	//display timing_setting for normal mode
	/*
	   innolux_ili9481_reg_cmd(0xC1);
	   innolux_ili9481_reg_data(0x10);//line inversion,DIV1[1:0]
	   innolux_ili9481_reg_data(0x10);//RTN1[4:0]
	   innolux_ili9481_reg_data(0x88);//BP and FP

	   //display timing setting for partial mode
	   innolux_ili9481_reg_cmd(0xC2);
	   innolux_ili9481_reg_data(0x10);//line inversion,DIV1[1:0]
	   innolux_ili9481_reg_data(0x10);//RTN1[4:0]
	   innolux_ili9481_reg_data(0x88);//BP and FP

	   //display timing setting for idle mode
	   innolux_ili9481_reg_cmd(0xC3);
	   innolux_ili9481_reg_data(0x10);//line inversion,DIV1[1:0]
	   innolux_ili9481_reg_data(0x10);//RTN1[4:0]
	   innolux_ili9481_reg_data(0x88);//BP and FP
	 */

	//display mode setting
	innolux_ili9481_reg_cmd(0xC5);	//Frame rate and Inversion Control
	innolux_ili9481_reg_data(0x05);	//50Hz

	//rgb interface setting
	//select if
	innolux_ili9481_reg_cmd(0xB4);
	innolux_ili9481_reg_data(0x10);	//rgb

	innolux_ili9481_reg_cmd(0xC6);
	innolux_ili9481_reg_data(0x13);	//set signal polarity

	//gamma setting
	innolux_ili9481_reg_cmd(0xC8);
	innolux_ili9481_reg_data(0x00);
	innolux_ili9481_reg_data(0x46);
	innolux_ili9481_reg_data(0x44);
	innolux_ili9481_reg_data(0x50);
	innolux_ili9481_reg_data(0x04);
	innolux_ili9481_reg_data(0x16);
	innolux_ili9481_reg_data(0x33);
	innolux_ili9481_reg_data(0x13);
	innolux_ili9481_reg_data(0x77);
	innolux_ili9481_reg_data(0x05);
	innolux_ili9481_reg_data(0x0F);
	innolux_ili9481_reg_data(0x00);

	//internal command
	innolux_ili9481_reg_cmd(0xE4);	//Internal LSI test registers
	innolux_ili9481_reg_data(0xA0);

	innolux_ili9481_reg_cmd(0xF0);
	innolux_ili9481_reg_data(0x01);

	innolux_ili9481_reg_cmd(0xF3);
	innolux_ili9481_reg_data(0x40);
	innolux_ili9481_reg_data(0x0A);

	innolux_ili9481_reg_cmd(0xF7);
	innolux_ili9481_reg_data(0x80);

	//address
	innolux_ili9481_reg_cmd(0x36);	//set_address_mode
	innolux_ili9481_reg_data(0x0A);	//1b

	innolux_ili9481_reg_cmd(0x3A);	//set pixel_format
	innolux_ili9481_reg_data(0x66);	//5-16bit,6-18bit

	//set column address
	innolux_ili9481_reg_cmd(0x2A);
	innolux_ili9481_reg_data(0x00);
	innolux_ili9481_reg_data(0x00);
	innolux_ili9481_reg_data(0x01);
	innolux_ili9481_reg_data(0x3F);

	//set page address
	innolux_ili9481_reg_cmd(0x2B);
	innolux_ili9481_reg_data(0x00);
	innolux_ili9481_reg_data(0x00);
	innolux_ili9481_reg_data(0x01);
	innolux_ili9481_reg_data(0xDF);

	//normal display
	innolux_ili9481_reg_cmd(0x13);
	msleep(120);
	innolux_ili9481_reg_cmd(0x29);	//display on

	innolux_ili9481_reg_cmd(0x2C);	//write memory start
	//innolux_ili9481_reg_cmd(0x3C);//write memory start

	//inital picture data write into lcd memory ram
	//led light on
	return 0;
}

/*static int innolux_ili9481_sleep_exit(void)
{
	innolux_ili9481_init();
	return 0;
}*/

static int innolux_ili9481_sleep_enter(void)
{
	innolux_ili9481_reg_cmd(0x28);	//display off
	innolux_ili9481_reg_cmd(0x10);	//sleep in
	msleep(120);
	return 0;
}

/*LCD backlight is controled by MC8416 chip,which has 4 level
*intensity.Check the mc8416 datasheet to get more infor.
*/
static int lcdc_innolux_ili9481_bk_setting(const int step)
{
	int i;
	static unsigned int nr_pulse;

	printk(KERN_ERR"INNOLUX ILI9481 BL:%d\n",step);

	if(step==0){
		gpio_set_value(GPIO_LCD_BK,0);
		return 0;
		}

	gpio_set_value(GPIO_LCD_BK, 0);
	udelay(1200);//setting the internel data register to zero,according to MC8416 datasheet.

	nr_pulse = innolux_ili9481_bk_intensity[step];
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

static void lcdc_innolux_ili9481_set_backlight(struct msm_fb_data_type *mfd)
{
	//msleep(80);// add by xuxian, wait 80ms for lcd stable.
	if (innolux_ili9481_state.display_on&&innolux_ili9481_state.disp_initialized&&innolux_ili9481_state.disp_powered_up)
		lcdc_innolux_ili9481_bk_setting(mfd->bl_level);
}

static void innolux_ili9481_disp_on(void)
{
	if (innolux_ili9481_state.disp_powered_up && !innolux_ili9481_state.display_on) {
		innolux_ili9481_init();
		innolux_ili9481_state.display_on = TRUE;
	}
}

static void innolux_ili9481_disp_powerup(void)
{
	if (!innolux_ili9481_state.disp_powered_up && !innolux_ili9481_state.display_on) {
		/* Reset the hardware first */
		/* Include DAC power up implementation here */
		innolux_ili9481_state.disp_powered_up = TRUE;
	}
}

static int lcdc_innolux_ili9481_panel_on(struct platform_device *pdev)
{
	if (!innolux_ili9481_state.disp_initialized) {
		lcdc_innolux_ili9481_pdata->panel_config_gpio(1);
		config_lcdc_gpio_table(set_gpio_bk_table, 1, 1);
		//spi_dac = *(lcdc_ili9481_pdata->gpio_num + 4);
		//gpio_set_value(spi_dac, 0);
		//udelay(15);
		//gpio_set_value(spi_dac, 1);
		spi_init();	/* lcd needs spi */
		innolux_ili9481_disp_powerup();
		innolux_ili9481_disp_on();
		innolux_ili9481_state.disp_initialized = TRUE;
	}

	printk(KERN_ERR"lcd panel on.\n");
	return 0;
}

static int lcdc_innolux_ili9481_panel_off(struct platform_device *pdev)
{
	innolux_ili9481_sleep_enter();
	config_lcdc_gpio_table(lcdc_gpio_table_sleep, ARRAY_SIZE(lcdc_gpio_table_sleep), 1);
	mpp_config_digital_out(13, MPP_CFG(MPP_DLOGIC_LVL_MSME, MPP_DLOGIC_OUT_CTRL_LOW));

	if (innolux_ili9481_state.disp_powered_up && innolux_ili9481_state.display_on) {
		lcdc_innolux_ili9481_pdata->panel_config_gpio(0);
		innolux_ili9481_state.display_on = FALSE;
		innolux_ili9481_state.disp_initialized = FALSE;
	}

	printk(KERN_ERR"lcd panel off.\n");
	return 0;
}

static int ili9481_innolux_probe(struct platform_device *pdev)
{
	if (pdev->id == 0) {
		lcdc_innolux_ili9481_pdata = pdev->dev.platform_data;
		return 0;
	}
	msm_fb_add_device(pdev);
	lcdc_innolux_ili9481_bk_setting(5);
	return 0;
}

static struct platform_driver this_driver = {
	.probe = ili9481_innolux_probe,
	.driver = {
		   .name = "lcdc_innolux_ili9481_rgb",
		   },
};

static struct msm_fb_panel_data ili9481_innolux_panel_data = {
	.on = lcdc_innolux_ili9481_panel_on,
	.off = lcdc_innolux_ili9481_panel_off,
	.set_backlight = lcdc_innolux_ili9481_set_backlight,
};

static struct platform_device this_device = {
	.name = "lcdc_innolux_ili9481_rgb",
	.id = 1,
	.dev = {
		.platform_data = &ili9481_innolux_panel_data,
		}
};

static int __init lcdc_innolux_ili9481_panel_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

#ifdef CONFIG_FB_MSM_TRY_MDDI_CATCH_LCDC_PRISM
	if (msm_fb_detect_client("lcdc_innolux_ili9481_rgb"))
		return 0;
#endif
	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

	pinfo = &ili9481_innolux_panel_data.panel_info;
	pinfo->xres = 320;
	pinfo->yres = 480;
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bl_max = LCDC_BK_INTEN_MAX;
	pinfo->bl_min = 0;
	pinfo->bpp = 18;
	pinfo->fb_num = 2;
	//pinfo->clk_rate = 658240;
	//pinfo->clk_rate = 2000000;
	//pinfo->clk_rate = 3000000;
	//pinfo->clk_rate = 4000000;
	//pinfo->clk_rate = 4500000;
	pinfo->clk_rate = 8100000;

	pinfo->lcdc.h_back_porch = 5;
	pinfo->lcdc.h_front_porch = 5;
	pinfo->lcdc.h_pulse_width = 2;
	pinfo->lcdc.v_back_porch = 5;
	pinfo->lcdc.v_front_porch = 5;
	pinfo->lcdc.v_pulse_width = 5;
	pinfo->lcdc.border_clr = 0;
	pinfo->lcdc.underflow_clr = 0xff;
	pinfo->lcdc.hsync_skew = 4;

	ret = platform_device_register(&this_device);
	if (ret) {
		platform_driver_unregister(&this_driver);
	}

	return ret;
}
module_init(lcdc_innolux_ili9481_panel_init);

