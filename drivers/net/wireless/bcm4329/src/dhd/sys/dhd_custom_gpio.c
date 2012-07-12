/*
* Customer code to add GPIO control during WLAN start/stop
* Copyright (C) 1999-2010, Broadcom Corporation
* 
*      Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2 (the "GPL"),
* available at http://www.broadcom.com/licenses/GPLv2.php, with the
* following added to such license:
* 
*      As a special exception, the copyright holders of this software give you
* permission to link this software with independent modules, and to copy and
* distribute the resulting executable under terms of your choice, provided that
* you also meet, for each linked independent module, the terms and conditions of
* the license of that module.  An independent module is a module which is not
* derived from this software.  The special exception does not apply to any
* modifications of the software.
* 
*      Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a license
* other than the GPL, without Broadcom's express prior written consent.
*
* $Id: dhd_custom_gpio.c,v 1.1.4.7 2010/06/03 21:27:48 Exp $
*/


#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#include <bcmutils.h>

#include <dngl_stats.h>
#include <dhd.h>

#include <wlioctl.h>
#include <wl_iw.h>

#define WL_ERROR(x) printf x
#define WL_TRACE(x)

#ifdef CUSTOMER_HW
extern  void bcm_wlan_power_off(int);
extern  void bcm_wlan_power_on(int);
#endif /* CUSTOMER_HW */
#ifdef CUSTOMER_HW2
int wifi_set_carddetect(int on);
int wifi_set_power(int on, unsigned long msec);
int wifi_get_irq_number(unsigned long *irq_flags_ptr);
#endif

#if defined(OOB_INTR_ONLY)

#if defined(BCMLXSDMMC)
extern int sdioh_mmc_irq(int irq);
#endif /* (BCMLXSDMMC)  */

#ifdef CUSTOMER_HW3
#include <mach/gpio.h>
#endif

/* Customer specific Host GPIO defintion  */
static int dhd_oob_gpio_num = -1; /* GG 19 */

module_param(dhd_oob_gpio_num, int, 0644);
MODULE_PARM_DESC(dhd_oob_gpio_num, "DHD oob gpio number");

int dhd_customer_oob_irq_map(unsigned long *irq_flags_ptr)
{
	int  host_oob_irq = 0;

#ifdef CUSTOMER_HW2
	host_oob_irq = wifi_get_irq_number(irq_flags_ptr);

#else /* for NOT  CUSTOMER_HW2 */
#if defined(CUSTOM_OOB_GPIO_NUM)
	if (dhd_oob_gpio_num < 0) {
		dhd_oob_gpio_num = CUSTOM_OOB_GPIO_NUM;
	}
#endif

	if (dhd_oob_gpio_num < 0) {
		WL_ERROR(("%s: ERROR customer specific Host GPIO is NOT defined \n",
			__FUNCTION__));
		return (dhd_oob_gpio_num);
	}

	WL_ERROR(("%s: customer specific Host GPIO number is (%d)\n",
	         __FUNCTION__, dhd_oob_gpio_num));

#if defined CUSTOMER_HW
	host_oob_irq = MSM_GPIO_TO_INT(dhd_oob_gpio_num);
#elif defined CUSTOMER_HW3
	gpio_request(dhd_oob_gpio_num, "oob irq");
	host_oob_irq = gpio_to_irq(dhd_oob_gpio_num);
	gpio_direction_input(dhd_oob_gpio_num);
#endif /* CUSTOMER_HW */
#endif /* CUSTOMER_HW2 */

	return (host_oob_irq);
}
#endif /* defined(OOB_INTR_ONLY) */

/* Customer function to control hw specific wlan gpios */
void
dhd_customer_gpio_wlan_ctrl(int onoff)
{
	switch (onoff) {
		case WLAN_RESET_OFF:
			WL_TRACE(("%s: call customer specific GPIO to insert WLAN RESET\n",
				__FUNCTION__));
#ifdef CUSTOMER_HW
			bcm_wlan_power_off(2);
#endif /* CUSTOMER_HW */
#ifdef CUSTOMER_HW2
			wifi_set_power(0, 0);
#endif
			WL_ERROR(("=========== WLAN placed in RESET ========\n"));
		break;

		case WLAN_RESET_ON:
			WL_TRACE(("%s: callc customer specific GPIO to remove WLAN RESET\n",
				__FUNCTION__));
#ifdef CUSTOMER_HW
			bcm_wlan_power_on(2);
#endif /* CUSTOMER_HW */
#ifdef CUSTOMER_HW2
			wifi_set_power(1, 0);
#endif
			WL_ERROR(("=========== WLAN going back to live  ========\n"));
		break;

		case WLAN_POWER_OFF:
			WL_TRACE(("%s: call customer specific GPIO to turn off WL_REG_ON\n",
				__FUNCTION__));
#ifdef CUSTOMER_HW
			bcm_wlan_power_off(1);
#endif /* CUSTOMER_HW */
		break;

		case WLAN_POWER_ON:
			WL_TRACE(("%s: call customer specific GPIO to turn on WL_REG_ON\n",
				__FUNCTION__));
#ifdef CUSTOMER_HW
			bcm_wlan_power_on(1);
#endif /* CUSTOMER_HW */
			/* Lets customer power to get stable */
			OSL_DELAY(200);
		break;
	}
}

#ifdef GET_CUSTOM_MAC_ENABLE

//JRD function.
/* FIXME:hard code.
 * filename: /system/wlan/broadcom/macaddr
 * 	this file will be updated by tool [nvcmd] when boot-up. 
 * 	refer to script [init.rc].
 * string format: [00:12:34:AB:cd:ef]
 * 	with hex value.
 * */
#define MAC_LEN		6
#define MAC_STR_LEN	17
#define MAC_CHAR_NUM	2

static inline int is_in_range(unsigned char c, 
		unsigned char low, unsigned char high)
{
	if((c > high) || (c < low)){
		return 0;
	}else{
		return 1;
	}
}

//convert a hex string to long integer.
static int hex_strtol(const char *p)
{
	int i;
	int acc = 0;
	unsigned char c;
	
	for(i=0; i<MAC_CHAR_NUM; i++){
		c = (unsigned char)(p[i]);
		if (is_in_range(c, '0', '9'))
			c -= '0';
		else if (is_in_range(c, 'a', 'f'))
			c -= ('a' - 10);
		else if (is_in_range(c, 'A', 'F'))
			c -= ('A' - 10);
		else
			break;

		acc = (acc << 4) + c;
	}

	return acc;
}

static int str2wa(const char *str, struct ether_addr *wa)
{
	struct ether_addr tmp;
	int l;
	const char *ptr = str;
	int i;
	int result = 0;
	
	for (i = 0; i < MAC_LEN; i++) {
		l = hex_strtol(ptr);
		if((l > 255) || (l < 0)){
			result = -1;
			break;	
		}

		tmp.octet[i] = (uint8_t)l;

		if(i == MAC_LEN - 1){
			break; //done
		}
		
		ptr = strchr(ptr, ':');
		if(ptr == NULL){
			result = -1;
			break;
		}
		ptr++;
	}

	if(result == 0){
		memcpy((char *)wa, (char*)(&tmp), MAC_LEN);
	}

	return result;
}


static int jrd_get_mac_addr(struct ether_addr *eaddr)
{
	struct file *fp = NULL;
	char fn[100] = {0};
	char dp[MAC_STR_LEN + 1] = {0};
	long l;
	loff_t pos;
	
	//FIXME:hard code.
	strcpy(fn, "/system/wlan/broadcom/macaddr");

	fp = filp_open(fn, O_RDONLY, 0);
	if(IS_ERR(fp)){
		printk(KERN_INFO "Unable to open '%s'.\n", fn);
		goto err_open;
	}

	l = fp->f_path.dentry->d_inode->i_size;
	if(l != MAC_STR_LEN){
		printk(KERN_INFO "Invalid macaddr file '%s'\n", fn);
		goto err_format;
	}

	pos = 0;
	if(vfs_read(fp, dp, l, &pos) != l)
	{
		printk(KERN_INFO "Failed to read '%s'.\n", fn);
		goto err_format;
	}

	dp[MAC_STR_LEN] = '\0';
	str2wa(dp, eaddr);
	
	filp_close(fp, NULL);
	return 0;

err_format:
	filp_close(fp, NULL);
err_open:
	return -1;
}


/* Function to get custom MAC address */
int
dhd_custom_get_mac_address(unsigned char *buf)
{
	WL_TRACE(("%s Enter\n", __FUNCTION__));
	if (!buf)
		return -EINVAL;

	/* Customer access to MAC address stored outside of DHD driver */
	
	//JRD mac
	{
		mm_segment_t oldfs= {0}; 
		//Friday Dec 10th, 2010.
		static struct ether_addr ea_jrd = {{0x00, 0x20, 0x10, 0xDE, 0xC1, 0x0F}};

		oldfs = get_fs(); 
		set_fs(get_ds()); 
		jrd_get_mac_addr(&ea_jrd);
		set_fs(oldfs);

		bcopy((char *)&ea_jrd, buf, sizeof(struct ether_addr));
	}

#ifdef EXAMPLE_GET_MAC
	/* EXAMPLE code */
	{
		struct ether_addr ea_example = {{0x00, 0x11, 0x22, 0x33, 0x44, 0xFF}};
		bcopy((char *)&ea_example, buf, sizeof(struct ether_addr));
	}
#endif /* EXAMPLE_GET_MAC */

	return 0;
}
#endif /* GET_CUSTOM_MAC_ENABLE */
