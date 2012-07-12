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

struct hx8357_state_type {
	boolean disp_initialized;
	boolean display_on;
	boolean disp_powered_up;
};
static struct hx8357_state_type hx8357_state = { 0 };

const int hx8357_bk_intensity[6] = {0,14,15,4,3,1};

static struct msm_panel_common_pdata * lcdc_hx8357_pdata;

#define GPIO_LCD_BK 90		//lcd backlight

static uint32_t set_gpio_bk_table[] = {
	GPIO_CFG(GPIO_LCD_BK, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA)
};

static uint32_t lcdc_gpio_table_sleep[] = {
	GPIO_CFG(132, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	//sclk
	GPIO_CFG(131, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	//cs
	GPIO_CFG(103, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	//sdi
	//GPIO_CFG(102, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),       //sdo
	GPIO_CFG(88, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),        //id

	GPIO_CFG(90, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	//bk
	//GPIO_CFG(96, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_2MA) //reset
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

static int hx8357_reg_data(unsigned char data)
{
	spi_write(1, data);
	return 0;
}

static int hx8357_reg_cmd(unsigned char cmd)
{
	spi_write(0, cmd);
	return 0;
}

static int spi_init(void)
{
	/* gpio setting */
	spi_sclk = *(lcdc_hx8357_pdata->gpio_num);
	spi_cs = *(lcdc_hx8357_pdata->gpio_num + 1);
	spi_sdi = *(lcdc_hx8357_pdata->gpio_num + 2);
	//spi_sdo  = *(lcdc_hx8357_pdata->gpio_num + 3);

	/* set the output so that we dont disturb the slave device */
	gpio_set_value(spi_sclk, 1);
	gpio_set_value(spi_sdi, 1);

	/* set the chip select de-asserted */
	gpio_set_value(spi_cs, 1);

	return 0;
}

static int hx8357_reset(void)
{
//	mdelay(10);
	mpp_config_digital_out(13, MPP_CFG(MPP_DLOGIC_LVL_MSME, MPP_DLOGIC_OUT_CTRL_LOW));
	msleep(10);
	mpp_config_digital_out(13, MPP_CFG(MPP_DLOGIC_LVL_MSME, MPP_DLOGIC_OUT_CTRL_HIGH));
	msleep(100);

	return 0;
}

static int hx8357_init(void)
{

	hx8357_reset();

	//lcd init.
	//start Initial sequence

	hx8357_reg_cmd(0x3A);
	hx8357_reg_data(0x60);

	hx8357_reg_cmd(0xB9);         // Set EXTC
	hx8357_reg_data(0xFF);
	hx8357_reg_data(0x83);
	hx8357_reg_data(0x57);
	msleep(5);


	hx8357_reg_cmd(0xCC);                //Set Panel
	hx8357_reg_data(0x0b);                //BGR_Panel


	hx8357_reg_cmd(0xB3);                //COLOR FORMAT
	hx8357_reg_data(0x43);                //SDO_EN,BYPASS,EPF[1:0],0,0,RM,DM  //43
	hx8357_reg_data(0x0a);                //DPL,HSPL,VSPL,EPL
	hx8357_reg_data(0x06);                //RCM, HPL[5:0]
	hx8357_reg_data(0x06);                //VPL[5:0]

	hx8357_reg_cmd(0xB6);                //
	hx8357_reg_data(0x53);                //VCOMDC


	hx8357_reg_cmd(0xB4);                //
	hx8357_reg_data(0x01);                //NW
	hx8357_reg_data(0x40);                //RTN
	hx8357_reg_data(0x00);                //DIV
	hx8357_reg_data(0x2A);                //DUM
	hx8357_reg_data(0x2A);                //DUM
	hx8357_reg_data(0x0D);                //GDON
	hx8357_reg_data(0x4F);                //GDOFF

	hx8357_reg_cmd(0xC0);                //STBA
	hx8357_reg_data(0x70);                //OPON
	hx8357_reg_data(0x50);                //OPON
	hx8357_reg_data(0x01);                //
	hx8357_reg_data(0x3C);                //
	hx8357_reg_data(0xE8);                //
	hx8357_reg_data(0x08);                //GEN

	hx8357_reg_cmd(0xE3);                //EQ
	hx8357_reg_data(0x17);                //PEQ
	hx8357_reg_data(0x00);                //NEQ

	hx8357_reg_cmd(0x3A);
	hx8357_reg_data(0x60);

	hx8357_reg_cmd(0xE0);                //
	hx8357_reg_data(0x00);                //1
	hx8357_reg_data(0x13);                //2
	hx8357_reg_data(0x1A);                //3
	hx8357_reg_data(0x29);                //4
	hx8357_reg_data(0x2D);                //5
	hx8357_reg_data(0x41);                //6
	hx8357_reg_data(0x49);                //7
	hx8357_reg_data(0x52);                //8
	hx8357_reg_data(0x48);                //9
	hx8357_reg_data(0x41);                //10
	hx8357_reg_data(0x3C);                //11
	hx8357_reg_data(0x33);                //12
	hx8357_reg_data(0x30);                //13
	hx8357_reg_data(0x1C);                //14
	hx8357_reg_data(0x19);                //15
	hx8357_reg_data(0x03);                //16
	hx8357_reg_data(0x00);                //17
	hx8357_reg_data(0x13);                //18
	hx8357_reg_data(0x1A);                //19
	hx8357_reg_data(0x29);                //20
	hx8357_reg_data(0x2D);                //21
	hx8357_reg_data(0x41);                //22
	hx8357_reg_data(0x49);                //23
	hx8357_reg_data(0x52);                //24
	hx8357_reg_data(0x48);                //25
	hx8357_reg_data(0x41);                //26
	hx8357_reg_data(0x3C);                //27
	hx8357_reg_data(0x33);                //28
	hx8357_reg_data(0x31);                //29
	hx8357_reg_data(0x1C);                //30
	hx8357_reg_data(0x19);                //31
	hx8357_reg_data(0x03);                //32
	hx8357_reg_data(0x00);                //33
	hx8357_reg_data(0x01);                //34


/*
	hx8357_reg_cmd(0xB3);                //COLOR FORMAT
	hx8357_reg_data(0x42);                //SDO_EN,BYPASS,EPF[1:0],0,0,RM,DM
	hx8357_reg_data(0x0D);                //DPL,HSPL,VSPL,EPL
	hx8357_reg_data(0x06);                //RCM, HPL[5:0]
	hx8357_reg_data(0x06);                //VPL[5:0]
*/
	hx8357_reg_cmd(0x11);
	msleep(150);
	hx8357_reg_cmd(0x29);
	msleep(25);

	return 0;

}
/*
static int hx8357_sleep_exit(void)
{
	hx8357_init();
	return 0;
}
*/
static int hx8357_sleep_enter(void)
{
	hx8357_reg_cmd(0x28);	//display off
	hx8357_reg_cmd(0x10);	//sleep in
	msleep(120);
	return 0;
}

/*LCD backlight is controled by MC8416 chip,which has 4 level
*intensity.Check the mc8416 datasheet to get more infor.
*/
static int lcdc_hx8357_bk_setting(const int step)
{
	int i;
	static unsigned int nr_pulse;

	printk(KERN_ERR"hx8357 BL:%d\n",step);

	if(step==0){
		gpio_set_value(GPIO_LCD_BK,0);
		return 0;
		}

	gpio_set_value(GPIO_LCD_BK, 0);
	udelay(1200);//setting the internel data register to zero,according to MC4816 datasheet.

	nr_pulse = hx8357_bk_intensity[step];
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

static void lcdc_hx8357_set_backlight(struct msm_fb_data_type *mfd)
{
	msleep(80);// add by xuxian, wait 80ms for lcd stable.
	if (hx8357_state.display_on&&hx8357_state.disp_initialized&&hx8357_state.disp_powered_up)
		lcdc_hx8357_bk_setting(mfd->bl_level);
}

static void hx8357_disp_on(void)
{
	if (hx8357_state.disp_powered_up && !hx8357_state.display_on) {
		hx8357_init();
		hx8357_state.display_on = TRUE;
	}
}

static void hx8357_disp_powerup(void)
{
	if (!hx8357_state.disp_powered_up && !hx8357_state.display_on) {
		/* Reset the hardware first */
		/* Include DAC power up implementation here */
		hx8357_state.disp_powered_up = TRUE;
	}
}

static int lcdc_hx8357_panel_on(struct platform_device *pdev)
{
	if (!hx8357_state.disp_initialized) {
		lcdc_hx8357_pdata->panel_config_gpio(1);
		config_lcdc_gpio_table(set_gpio_bk_table, 1, 1);
		//spi_dac = *(lcdc_hx8357_pdata->gpio_num + 4);
		//gpio_set_value(spi_dac, 0);
		//udelay(15);
		//gpio_set_value(spi_dac, 1);
		spi_init();	/* lcd needs spi */
		hx8357_disp_powerup();
		hx8357_disp_on();
		hx8357_state.disp_initialized = TRUE;
	}

	printk(KERN_ERR"lcd panel on.\n");
	return 0;
}

static int lcdc_hx8357_panel_off(struct platform_device *pdev)
{
	hx8357_sleep_enter();
	config_lcdc_gpio_table(lcdc_gpio_table_sleep, ARRAY_SIZE(lcdc_gpio_table_sleep), 1);
	mpp_config_digital_out(13, MPP_CFG(MPP_DLOGIC_LVL_MSME, MPP_DLOGIC_OUT_CTRL_LOW));

	if (hx8357_state.disp_powered_up && hx8357_state.display_on) {
		lcdc_hx8357_pdata->panel_config_gpio(0);
		hx8357_state.display_on = FALSE;
		hx8357_state.disp_initialized = FALSE;
	}

	printk(KERN_ERR"lcd panel off.\n");
	return 0;
}

static int hx8357_probe(struct platform_device *pdev)
{
	if (pdev->id == 0) {
		lcdc_hx8357_pdata = pdev->dev.platform_data;
		return 0;
	}
	msm_fb_add_device(pdev);
	lcdc_hx8357_bk_setting(5);
	return 0;
}

static struct platform_driver this_driver = {
	.probe = hx8357_probe,
	.driver = {
		   .name = "lcdc_hx8357_rgb",
		   },
};

static struct msm_fb_panel_data hx8357_panel_data = {
	.on = lcdc_hx8357_panel_on,
	.off = lcdc_hx8357_panel_off,
	.set_backlight = lcdc_hx8357_set_backlight,
};

static struct platform_device this_device = {
	.name = "lcdc_hx8357_rgb",
	.id = 1,
	.dev = {
		.platform_data = &hx8357_panel_data,
		}
};

static int __init lcdc_hx8357_panel_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

#ifdef CONFIG_FB_MSM_TRY_MDDI_CATCH_LCDC_PRISM
	if (msm_fb_detect_client("lcdc_hx8357_rgb"))
		return 0;
#endif
	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

	pinfo = &hx8357_panel_data.panel_info;
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
	pinfo->clk_rate = 8100000;

	pinfo->lcdc.h_back_porch = 2;
	pinfo->lcdc.h_front_porch = 2;
	pinfo->lcdc.h_pulse_width = 2;
	pinfo->lcdc.v_back_porch = 2;
	pinfo->lcdc.v_front_porch = 2;
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
module_init(lcdc_hx8357_panel_init);

