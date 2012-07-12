/*
 * leds-msm-pmic.c - MSM PMIC LEDs driver.
 *
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>

#include <mach/pmic.h>
#include <linux/workqueue.h>
#include <linux/earlysuspend.h>
#define MAX_KEYPAD_BL_LEVEL	32
#ifdef CONFIG_HAS_EARLYSUSPEND
static void msm_pmic_led_early_suspend(struct platform_device *dev,
		pm_message_t state);
static void msm_pmic_led_early_resume(struct platform_device *dev);
static struct early_suspend early_suspend;
#endif

static struct delayed_work keypad_bl_work;
static void msm_keypad_bl_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	int ret;
	printk("msm_keypad_bl_led_set:%d\n",value);
	ret = pmic_set_led_intensity(LED_LCD, value / MAX_KEYPAD_BL_LEVEL);
	ret = pmic_set_led_intensity(LED_KEYPAD, value / MAX_KEYPAD_BL_LEVEL);
	if (ret)
		dev_err(led_cdev->dev, "can't set keypad backlight\n");
}

static struct led_classdev msm_kp_bl_led = {
	.name			= "keyboard-backlight",
	.brightness_set		= msm_keypad_bl_led_set,
	.brightness		= LED_OFF,
};

static void update_kaypad_bl(struct work_struct *work)
{
	msm_keypad_bl_led_set(&msm_kp_bl_led, LED_OFF);
	printk("Close the kaypad back light\n");
}
static int msm_pmic_led_probe(struct platform_device *pdev)
{
	int rc;

	rc = led_classdev_register(&pdev->dev, &msm_kp_bl_led);
	if (rc) {
		dev_err(&pdev->dev, "unable to register led class driver\n");
		return rc;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
        early_suspend.suspend=(void *)msm_pmic_led_early_suspend;
	early_suspend.resume=(void *)msm_pmic_led_early_resume;
	register_early_suspend(&early_suspend);
#endif
	msm_keypad_bl_led_set(&msm_kp_bl_led, LED_FULL);
	schedule_delayed_work(&keypad_bl_work, 3 * HZ);
	return rc;
}

static int __devexit msm_pmic_led_remove(struct platform_device *pdev)
{
	led_classdev_unregister(&msm_kp_bl_led);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void msm_pmic_led_early_suspend(struct platform_device *dev,
		pm_message_t state)
{
	printk("msm_pmic_led_early_suspendxxxxxxxxxxx\n");
	led_classdev_suspend(&msm_kp_bl_led);

}

static void msm_pmic_led_early_resume(struct platform_device *dev)
{
	printk("msm_pmic_led_early_resumeyyyyyyyyyyy\n");
	led_classdev_resume(&msm_kp_bl_led);


}
#else
#define msm_pmic_led_early_suspend NULL
#define msm_pmic_led_early_resume NULL
#endif

static struct platform_driver msm_pmic_led_driver = {
	.probe		= msm_pmic_led_probe,
	.remove		= __devexit_p(msm_pmic_led_remove),
	.suspend	= NULL,
	.resume		= NULL,
	.driver		= {
		.name	= "pmic-leds",
		.owner	= THIS_MODULE,
	},
};

static int __init msm_pmic_led_init(void)
{
	printk("msm_pmic_led_init\n");
	INIT_DELAYED_WORK(&keypad_bl_work, update_kaypad_bl);
	return platform_driver_register(&msm_pmic_led_driver);
}
module_init(msm_pmic_led_init);

static void __exit msm_pmic_led_exit(void)
{
	platform_driver_unregister(&msm_pmic_led_driver);
}
module_exit(msm_pmic_led_exit);

MODULE_DESCRIPTION("MSM PMIC LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:pmic-leds");

