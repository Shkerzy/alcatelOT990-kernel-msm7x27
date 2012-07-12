/*******************************************************************************
*                                                                              *
*	File Name: 	taos.c 	                                               *
*	Description:	Linux device driver for Taos ambient light and         *
*			proximity sensors.                                     *
*	Author:         John Koshi                                             *
*	History:	09/16/2009 - Initial creation                          *
*			10/09/2009 - Triton version			       *
*			12/21/2009 - Probe/remove mode			       *
*			02/07/2010 - Add proximity			       *
*                                                                              *
********************************************************************************
*	Proprietary to Taos Inc., 1001 Klein Road #300, Plano, TX 75074        *
*******************************************************************************/  
// includes
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/timer.h>
#include <asm/uaccess.h>
#include <linux/input.h>
#include <asm/errno.h>
#include <asm/delay.h>
#include <linux/earlysuspend.h>
#include "taos_common.h"
#include <linux/delay.h>
#include <mach/msm_sensors_input_dev.h>

// device name/id/address/counts
#define TAOS_DEVICE_NAME		"taos"
#define TAOS_DEVICE_ID			"tritonFN"
#define TAOS_ID_NAME_SIZE		10
#define TAOS_TRITON_CHIPIDVAL   	0x00
#define TAOS_TRITON_MAXREGS     	32
#define TAOS_DEVICE_ADDR1		0x29
#define TAOS_DEVICE_ADDR2       	0x39
#define TAOS_DEVICE_ADDR3       	0x49
#define TAOS_MAX_NUM_DEVICES		3
#define TAOS_MAX_DEVICE_REGS		24
    
// TRITON register offsets
#define TAOS_TRITON_CNTRL 		0x00
#define TAOS_TRITON_ALS_TIME 		0X01
#define TAOS_TRITON_PRX_TIME		0x02
#define TAOS_TRITON_WAIT_TIME		0x03
#define TAOS_TRITON_ALS_MINTHRESHLO	0X04
#define TAOS_TRITON_ALS_MINTHRESHHI 	0X05
#define TAOS_TRITON_ALS_MAXTHRESHLO	0X06
#define TAOS_TRITON_ALS_MAXTHRESHHI	0X07
#define TAOS_TRITON_PRX_MINTHRESHLO 	0X08
#define TAOS_TRITON_PRX_MINTHRESHHI 	0X09
#define TAOS_TRITON_PRX_MAXTHRESHLO 	0X0A
#define TAOS_TRITON_PRX_MAXTHRESHHI 	0X0B
#define TAOS_TRITON_INTERRUPT		0x0C
#define TAOS_TRITON_PRX_CFG		0x0D
#define TAOS_TRITON_PRX_COUNT		0x0E
#define TAOS_TRITON_GAIN		0x0F
#define TAOS_TRITON_REVID		0x11
#define TAOS_TRITON_CHIPID      	0x12
#define TAOS_TRITON_STATUS		0x13
#define TAOS_TRITON_ALS_CHAN0LO		0x14
#define TAOS_TRITON_ALS_CHAN0HI		0x15
#define TAOS_TRITON_ALS_CHAN1LO		0x16
#define TAOS_TRITON_ALS_CHAN1HI		0x17
#define TAOS_TRITON_PRX_LO		0x18
#define TAOS_TRITON_PRX_HI		0x19
    
// Triton cmd reg masks
#define TAOS_TRITON_CMD_REG		0X80
#define TAOS_TRITON_CMD_BYTE_RW		0x00
#define TAOS_TRITON_CMD_WORD_BLK_RW	0x20
#define TAOS_TRITON_CMD_SPL_FN		0x60
#define TAOS_TRITON_CMD_PROX_INTCLR	0X05
#define TAOS_TRITON_CMD_ALS_INTCLR	0X06
#define TAOS_TRITON_CMD_PROXALS_INTCLR 	0X07
#define TAOS_TRITON_CMD_TST_REG		0X08
#define TAOS_TRITON_CMD_USER_REG	0X09
    
// Triton cntrl reg masks
#define TAOS_TRITON_CNTL_PROX_INT_ENBL	0X20
#define TAOS_TRITON_CNTL_ALS_INT_ENBL	0X10
#define TAOS_TRITON_CNTL_WAIT_TMR_ENBL	0X08
#define TAOS_TRITON_CNTL_PROX_DET_ENBL	0X04
#define TAOS_TRITON_CNTL_ADC_ENBL	0x02
#define TAOS_TRITON_CNTL_PWRON		0x01
    
// Triton status reg masks
#define TAOS_TRITON_STATUS_ADCVALID	0x01
#define TAOS_TRITON_STATUS_PRXVALID	0x02
#define TAOS_TRITON_STATUS_ADCINTR	0x10
#define TAOS_TRITON_STATUS_PRXINTR	0x20
    
// lux constants
#define	TAOS_MAX_LUX			65535000
#define TAOS_SCALE_MILLILUX		3
#define TAOS_FILTER_DEPTH		3

//timer function
#define ALS_TIMER_FUNC       1
#define PROX_TIMER_FUNC    1

//taos timer count
#if ALS_TIMER_FUNC
static unsigned int als_timer_count;
#endif
#if PROX_TIMER_FUNC
static unsigned int prox_timer_count;
#endif

extern struct mutex __msm_sensors_lock;

// forward declarations
static int taos_probe(struct i2c_client *clientp, const struct i2c_device_id *idp);
static int taos_remove(struct i2c_client *client);
static int taos_open(struct inode *inode, struct file *file);
static int taos_release(struct inode *inode, struct file *file);
static int taos_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int taos_prox_calibrate(void);
static int taos_read(struct file *file, char *buf, size_t count, loff_t * ppos);
static int taos_write(struct file *file, const char *buf, size_t count, loff_t * ppos);
static loff_t taos_llseek(struct file *file, loff_t offset, int orig);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void taos_als_early_suspend(struct early_suspend *h);
static void taos_als_late_resume(struct early_suspend *h);
#endif

static int taos_prox_suspend(struct device *dev);
static int taos_prox_resume(struct device *dev);

static int taos_get_lux(void);
static int taos_lux_filter(int raw_lux);
static int taos_device_name(unsigned char *bufp, char **device_name);
static int taos_prox_poll(struct taos_prox_info *prxp);
#if PROX_TIMER_FUNC
static void taos_prox_poll_timer_func(unsigned long param);
static void taos_prox_poll_timer_start(unsigned int t);
static void taos_prox_timer_del(void);
static void taos_prox_work_f(struct work_struct *work);
#endif
#if ALS_TIMER_FUNC
static void taos_als_poll_timer_func(unsigned long param);
static void taos_als_poll_timer_start(unsigned int t);
static void taos_als_timer_del(void);
static void taos_als_work_f(struct work_struct *work);
#endif

// first device number
static dev_t taos_dev_number;

// class structure for this device
struct class *taos_class;

// module device table
static struct i2c_device_id taos_idtable[] = { {TAOS_DEVICE_ID, 0}, {}
};

MODULE_DEVICE_TABLE(i2c, taos_idtable);
//static DEFINE_MUTEX(taos_mutex_lock);

// client and device
struct i2c_client *my_clientp;

static struct dev_pm_ops taos_pm_ops = {
	.suspend = taos_prox_suspend,
	.resume = taos_prox_resume,
};

// driver definition
static struct i2c_driver taos_driver = {
	.driver = {
		.name = "taos",
		.owner = THIS_MODULE,
		.pm = &taos_pm_ops,
	},
	.probe = taos_probe,
	.remove = __devexit_p(taos_remove),
	.id_table = taos_idtable,
};

// per-device data
struct taos_data {
	struct i2c_client *client;
	struct cdev cdev;
	char taos_name[TAOS_ID_NAME_SIZE];
	struct work_struct   als_workqueue;
	struct work_struct   prox_workqueue;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend taos_early_suspend;
#endif
} *taos_datap;

// file operations
static struct file_operations taos_fops = { .owner = THIS_MODULE, .open = taos_open, .release =
	    taos_release, .read = taos_read, .write = taos_write, .llseek = taos_llseek, .ioctl = taos_ioctl, 
};

// device configuration
struct taos_cfg *taos_cfgp;
static u32 calibrate_target_param = 300000;
static u16 als_time_param = 100;
static u16 scale_factor_param = 1;
static u16 gain_trim_param = 512;
static u8 filter_history_param = 3;
static u8 filter_count_param = 1;
static u8 gain_param = 1;
static u16 prox_threshold_hi_param = 300;
static u16 prox_threshold_lo_param = 200;
static u8 prox_int_time_param = 0xDB;
static u8 prox_adc_time_param = 0xFF;
static u8 wait_time_param = 0xE7;
static u8 prox_intr_filter_param = 0x00;
static u8 prox_config_param = 0x00;
static u8 prox_pulse_cnt_param = 0x06;
static u8 prox_gain_param = 0x20;

// prox info
struct taos_prox_info prox_cal_info[20];
struct taos_prox_info prox_cur_info;
struct taos_prox_info *prox_cur_infop = &prox_cur_info;
static u8 prox_history_hi = 0;
static u8 prox_history_lo = 0;
#if PROX_TIMER_FUNC
static struct timer_list prox_poll_timer;
#endif
static int prox_on = 0;
static u16 sat_als = 0;
static u16 sat_prox = 0;

//als info
#if ALS_TIMER_FUNC
static struct timer_list als_poll_timer;
#endif
static int als_on;

static int als_off_flag;

// device reg init values
    u8 taos_triton_reg_init[16] = {
0x00, 0xFF, 0XFF, 0XFF, 0X00, 0X00, 0XFF, 0XFF, 0X00, 0X00, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00};

// lux time scale
struct time_scale_factor {
	u16 numerator;
	u16 denominator;
	u16 saturation;
};
struct time_scale_factor TritonTime = { 1, 0, 0 };
struct time_scale_factor *lux_timep = &TritonTime;

// gain table
u8 taos_triton_gain_table[] = {
1, 8, 16, 120};

// lux data
struct lux_data {
	u16 ratio;
	u16 clear;
	u16 ir;
};

struct lux_data TritonFN_lux_data[] = { 
	    {9830, 8320, 15360}, {12452, 10554, 22797}, {14746, 6234, 11430}, {17695, 3968, 6400}, {0, 0, 0}
};
struct lux_data *lux_tablep = TritonFN_lux_data;
static int lux_history[TAOS_FILTER_DEPTH] = { -ENODATA, -ENODATA, -ENODATA };

//device enable flags
static int als_dev_open;
static int prox_dev_open;

// driver init
static int __init taos_init(void)
{
	int ret = 0;

	if ((ret = (alloc_chrdev_region(&taos_dev_number, 0, TAOS_MAX_NUM_DEVICES, TAOS_DEVICE_NAME))) < 0) {
		printk(KERN_ERR "TAOS: alloc_chrdev_region() failed in taos_init()\n");
		return (ret);
	}

	taos_class = class_create(THIS_MODULE, TAOS_DEVICE_NAME);
	taos_datap = kzalloc(sizeof(struct taos_data), GFP_KERNEL);
	if (!taos_datap) {
		printk(KERN_ERR "TAOS: kmalloc for struct taos_data failed in taos_init()\n");
		return -ENOMEM;
	}
	cdev_init(&taos_datap->cdev, &taos_fops);
	taos_datap->cdev.owner = THIS_MODULE;
	if ((ret = (cdev_add(&taos_datap->cdev, taos_dev_number, 1))) < 0) {
		printk(KERN_ERR "TAOS: cdev_add() failed in taos_init()\n");
		return (ret);
	}
	device_create(taos_class, NULL, MKDEV(MAJOR(taos_dev_number), 0), &taos_driver, "taos");

	if ((ret = (i2c_add_driver(&taos_driver))) < 0) {
		printk(KERN_ERR "TAOS: i2c_add_driver() failed in taos_init()\n");
		return (ret);
	}

	printk("XXXXXXXXXXXXXX taos_init OK XXXXXXXXXXXXXXXXXX\n");

	return (ret);
}

// driver exit
static void __exit taos_exit(void)
{
	i2c_del_driver(&taos_driver);
	device_destroy(taos_class, MKDEV(MAJOR(taos_dev_number), 0));
	cdev_del(&taos_datap->cdev);
	class_destroy(taos_class);
	unregister_chrdev_region(taos_dev_number, TAOS_MAX_NUM_DEVICES);
	kfree(taos_datap);
}

// client probe
static int taos_probe(struct i2c_client *clientp, const struct i2c_device_id *idp)
{
	int ret = 0;
	int i = 0;
	unsigned char buf[TAOS_MAX_DEVICE_REGS];
	char *device_name;

	printk("XXXXXXXXXXXXXXXX taos_probe XXXXXXXXXXXXXX\n");

	if (!i2c_check_functionality(clientp->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		printk(KERN_ERR "TAOS: taos_probe() - i2c smbus byte data functions unsupported\n");
		return -EOPNOTSUPP;
	}
	if (!i2c_check_functionality(clientp->adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		printk(KERN_ERR "TAOS: taos_probe() - i2c smbus word data functions unsupported\n");
	}
	if (!i2c_check_functionality(clientp->adapter, I2C_FUNC_SMBUS_BLOCK_DATA)) {
		printk(KERN_ERR "TAOS: taos_probe() - i2c smbus block data functions unsupported\n");
	}

	taos_datap->client = clientp;
	i2c_set_clientdata(clientp, taos_datap);
	for (i = 0; i < TAOS_MAX_DEVICE_REGS; i++) {
		if ((ret = (i2c_smbus_write_byte(clientp, (TAOS_TRITON_CMD_REG | (TAOS_TRITON_CNTRL + i))))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte() to control reg failed in taos_probe()\n");
			return (ret);
		}
		buf[i] = i2c_smbus_read_byte(clientp);
	}
	if ((ret = taos_device_name(buf, &device_name)) == 0) {
		printk(KERN_ERR
			"TAOS: chip id that was read found mismatched by taos_device_name(), in taos_probe()\n");
		return -ENODEV;
	}
	if (strcmp(device_name, TAOS_DEVICE_ID)) {
		printk(KERN_ERR "TAOS: chip id that was read does not match expected id in taos_probe()\n");
		return -ENODEV;
	}
	else {
		printk(KERN_ERR "TAOS: chip id of %s that was read matches expected id in taos_probe()\n",
			device_name);
	}
	strlcpy(clientp->name, TAOS_DEVICE_ID, I2C_NAME_SIZE);
	strlcpy(taos_datap->taos_name, TAOS_DEVICE_ID, TAOS_ID_NAME_SIZE);

	if (!(taos_cfgp = kmalloc(sizeof(struct taos_cfg), GFP_KERNEL))) {
		printk(KERN_ERR "TAOS: kmalloc for struct taos_cfg failed in taos_probe()\n");
		return -ENOMEM;
	}
	taos_cfgp->calibrate_target = calibrate_target_param;
	taos_cfgp->als_time = als_time_param;
	taos_cfgp->scale_factor = scale_factor_param;
	taos_cfgp->gain_trim = gain_trim_param;
	taos_cfgp->filter_history = filter_history_param;
	taos_cfgp->filter_count = filter_count_param;
	taos_cfgp->gain = gain_param;
	taos_cfgp->prox_threshold_hi = prox_threshold_hi_param;
	taos_cfgp->prox_threshold_lo = prox_threshold_lo_param;
	taos_cfgp->prox_int_time = prox_int_time_param;
	taos_cfgp->prox_adc_time = prox_adc_time_param;
	taos_cfgp->wait_time = wait_time_param;
	taos_cfgp->prox_intr_filter = prox_intr_filter_param;
	taos_cfgp->prox_config = prox_config_param;
	taos_cfgp->prox_pulse_cnt = prox_pulse_cnt_param;
	taos_cfgp->prox_gain = prox_gain_param;
	sat_als = (256 - taos_cfgp->prox_int_time) << 10;
	sat_prox = (256 - taos_cfgp->prox_adc_time) << 10;
	
	/*dmobile ::power down for init */
	if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL), 0x00))) < 0) {
		printk(KERN_ERR "TAOS:Rambo, i2c_smbus_write_byte_data failed in power down\n");
		return (ret);
	}
#if ALS_TIMER_FUNC
	//init als timer
	init_timer(&als_poll_timer);
	als_poll_timer.function = taos_als_poll_timer_func;
	INIT_WORK(&taos_datap->als_workqueue, taos_als_work_f);
#endif
#if PROX_TIMER_FUNC
	//int prox timer
	init_timer(&prox_poll_timer);
	prox_poll_timer.function = taos_prox_poll_timer_func;
	INIT_WORK(&taos_datap->prox_workqueue, taos_prox_work_f);
#endif

	/* prox calibrate */
	taos_prox_calibrate();

#ifdef CONFIG_HAS_EARLYSUSPEND
	taos_datap->taos_early_suspend.suspend = taos_als_early_suspend;
	taos_datap->taos_early_suspend.resume = taos_als_late_resume;
	register_early_suspend(&taos_datap->taos_early_suspend);
#endif

	printk("XXXXXXXXXXXXXXXX taos_probe OK XXXXXXXXXXXXXX\n");

	return (ret);
}

// client remove
static int __devexit taos_remove(struct i2c_client *client)
{
	i2c_unregister_device(client);

	return 0;
}

static int taos_als_on(void)
{
	int i, ret = 0;
	u8 itime = 0, reg_val = 0, reg_cntrl = 0;

	for (i = 0; i < TAOS_FILTER_DEPTH; i++)
		lux_history[i] = -ENODATA;

	if (!als_dev_open) {
		for (i = 0; i < sizeof(taos_triton_reg_init); i++)
			if ((ret =
			      (i2c_smbus_write_byte_data
			       (taos_datap->client, (TAOS_TRITON_CMD_REG | (TAOS_TRITON_CNTRL + i)),
				taos_triton_reg_init[i]))) < 0) {
				printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_on\n");
				return (ret);
			}

		/*clear status of ALS interrupt*/
		if ((ret =
		      (i2c_smbus_write_byte
		       (taos_datap->client,
			(TAOS_TRITON_CMD_REG | TAOS_TRITON_CMD_SPL_FN | TAOS_TRITON_CMD_ALS_INTCLR)))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_on\n");
			return (ret);
		}

		/*set ALS time*/
		itime = ((((taos_cfgp->als_time * 100 / 50) * 18) / 100) - 1);
		itime = (~itime);
		if ((ret =
		      (i2c_smbus_write_byte_data
		       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_ALS_TIME), itime))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_on\n");
			return (ret);
		}

		/*set wait time*/
		if ((ret =
		      (i2c_smbus_write_byte_data
		       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_WAIT_TIME), taos_cfgp->wait_time))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in taos_als_on\n");
			return (ret);
		}

		/*set ALS gain*/
		if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_GAIN)))) < 0) 		{
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_on\n");
			return (ret);
		}
		reg_val = i2c_smbus_read_byte(taos_datap->client);
		reg_val = reg_val & 0xFC;
		reg_val = reg_val | (taos_cfgp->gain & 0x03);
		if ((ret =
		      (i2c_smbus_write_byte_data
		       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_GAIN), reg_val))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_on\n");
			return (ret);
		}

		/*ALS enable, wait enable and device power on*/
		if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_calibrate\n");
			return (ret);
		}
		reg_val = i2c_smbus_read_byte(taos_datap->client);
		reg_cntrl = reg_val | (TAOS_TRITON_CNTL_ADC_ENBL | TAOS_TRITON_CNTL_PWRON | TAOS_TRITON_CNTL_WAIT_TMR_ENBL);
		if ((ret =
		      (i2c_smbus_write_byte_data
		       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL), reg_cntrl))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_on\n");
			return (ret);
		}

		als_dev_open = 1;
	}

	return ret;
}

static int taos_als_off(void)
{
	int ret = 0;
	u8 reg_val = 0, reg_cntrl = 0;

	if (prox_dev_open) {
		als_off_flag = 1;
		return 0;
	}
	else {
		/*disable ALS and power off*/
		if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_calibrate\n");
			return (ret);
		}
		reg_val = i2c_smbus_read_byte(taos_datap->client);

		reg_cntrl = reg_val & (~(TAOS_TRITON_CNTL_ADC_ENBL | TAOS_TRITON_CNTL_PWRON));

		if ((ret =
		(i2c_smbus_write_byte_data
		(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL), reg_cntrl))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_off\n");
			return (ret);
		}
		als_dev_open = 0;
	}

	return (ret);
}

static int taos_prox_on(void)
{
	int ret = 0, i;
	u8 reg_val = 0;

#if ALS_TIMER_FUNC
	if (als_on) {
		taos_als_timer_del();
		mdelay(180);
	}
#endif

	/*set power off*/
	if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in taos_prox_on\n");
		return (ret);
	}
	reg_val = i2c_smbus_read_byte(taos_datap->client);
	reg_val &= ~TAOS_TRITON_CNTL_PWRON;
	if ((ret =
	(i2c_smbus_write_byte_data
	(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL), reg_val))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in taos_prox_on\n");
		return (ret);
	}

	/*clear interrupt status*/
	if ((ret =
	      (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CMD_SPL_FN | TAOS_TRITON_CMD_ALS_INTCLR | TAOS_TRITON_CMD_PROX_INTCLR)))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte(2) failed in taos_prox_poll()\n");
		return (ret);
	}

	/*set ALS time*/
	if ((ret =
	      (i2c_smbus_write_byte_data
	       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_ALS_TIME), taos_cfgp->prox_int_time))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}

	/*set prox time*/
	if ((ret =
	      (i2c_smbus_write_byte_data
	       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_PRX_TIME), taos_cfgp->prox_adc_time))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}

	/*set wait time*/
	if ((ret =
	      (i2c_smbus_write_byte_data
	       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_WAIT_TIME), taos_cfgp->wait_time))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}

	/*config prox interrupt*/
	if ((ret =
	      (i2c_smbus_write_byte_data
	       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_INTERRUPT), taos_cfgp->prox_intr_filter))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}

	/*set prox config register(wait long??)*/
	if ((ret =
	      (i2c_smbus_write_byte_data
	       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_PRX_CFG), taos_cfgp->prox_config))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}

	/*set prox pulse*/
	if ((ret =
	      (i2c_smbus_write_byte_data
	       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_PRX_COUNT), taos_cfgp->prox_pulse_cnt))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}

	/*set als gain*/
	if ((ret =
	      (i2c_smbus_write_byte_data
	       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_GAIN), (taos_cfgp->prox_gain | taos_cfgp->gain)))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}

	/*set als, prox, wait and power enable*/
	if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL), 0x0F))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}

	for (i = 0; i < TAOS_FILTER_DEPTH; i++)
		lux_history[i] = -ENODATA;

	als_dev_open = 1;
	prox_dev_open = 1;

#if ALS_TIMER_FUNC
	if (als_on)
		taos_als_poll_timer_start(400);
#endif

	return ret;
}

static int taos_prox_off(void)
{
	int ret = 0;
	u8 reg_val = 0;

	if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in taos_prox_off\n");
		return (ret);
	}
	reg_val = i2c_smbus_read_byte(taos_datap->client);
	reg_val &= ~TAOS_TRITON_CNTL_PROX_DET_ENBL;

	if ((ret =
	(i2c_smbus_write_byte_data
	(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL), reg_val))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_off\n");
		return (ret);
	}

	prox_dev_open = 0;

	if (!als_on || als_off_flag)
		ret = taos_als_off();

	als_off_flag = 0;

	prox_history_hi = 0;
	prox_history_lo = 0;

	return ret;
}

static int taos_als_data(void)
{
	int ret = 0, wait_count = 0;
	int lux_val;
	u8 reg_val = 0;

	/*check als power and ADC status*/
	if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_data\n");
		return (ret);
	}
	reg_val = i2c_smbus_read_byte(taos_datap->client);
	if ((reg_val & (TAOS_TRITON_CNTL_ADC_ENBL | TAOS_TRITON_CNTL_PWRON)) !=
	     (TAOS_TRITON_CNTL_ADC_ENBL | TAOS_TRITON_CNTL_PWRON))
		return -ENODATA;

	/*check als data status*/
	if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_STATUS)))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_data\n");
		return (ret);
	}
	reg_val = i2c_smbus_read_byte(taos_datap->client);
	while ((reg_val & TAOS_TRITON_STATUS_ADCVALID) != TAOS_TRITON_STATUS_ADCVALID) {
		reg_val = i2c_smbus_read_byte(taos_datap->client);
		wait_count++;
		if (wait_count > 30) {
			printk(KERN_ERR "TAOS: ALS status invalid for 300 ms in taos_als_data()\n");
			return -ENODATA;
		}
		mdelay(10);
	}

	if ((lux_val = taos_get_lux()) < 0)
		printk(KERN_ERR "TAOS: call to taos_get_lux() returned error %d in ioctl als_data\n", lux_val);

	lux_val = taos_lux_filter(lux_val);

	return lux_val;
}

static int  taos_prox_data(void)
{
	return taos_prox_poll(prox_cur_infop);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
//als suspend
static void taos_als_early_suspend(struct early_suspend *h)
{
	int ret;

	if (als_on) {
#if ALS_TIMER_FUNC
		taos_als_timer_del();
#endif
		ret = taos_als_off();
		if (ret < 0)
			printk("XXXXXXXXXXXXXXXXX TAOS_ALS_EARLY_SUSPEND ERROR!!! XXXXXXXXXXXXX\n");
	}

	printk("XXXXXXXXXXx taos_als_early_suspend XXXXXXXXXXXX\n");
}

//als_resume
static void taos_als_late_resume(struct early_suspend *h)
{
	int ret;

	if ((!als_dev_open && als_on) || als_off_flag) {
		ret = taos_als_on();
		if (ret < 0) {
			printk("XXXXXXXXXXXXXXXXX TAOS_ALS_LARE_RESUME_ ERROR!!! XXXXXXXXXXXXX\n");
			return;
		}
#if ALS_TIMER_FUNC
		taos_als_poll_timer_start(400);
#endif
	}

	printk("XXXXXXXXXXx taos_als_late_resume XXXXXXXXXXXX\n");
}
#endif

//suspend prox
static int taos_prox_suspend(struct device *dev)
{
	int ret = 0;

	if (prox_on) {
#if PROX_TIMER_FUNC
		taos_prox_timer_del();
#endif
		ret = taos_prox_off();
		if (ret < 0) {
			printk("XXXXXXXXXXXXXXXXX TAOS_PROX_SUSPEND ERROR!!! XXXXXXXXXXXXX\n");
			return ret;
		}
	}

	printk("XXXXXXXXXXx taos_prox_suspend XXXXXXXXXXXX\n");

	return ret;
}

//resume prox
static int taos_prox_resume(struct device *dev)
{
	int ret = 0;

	if (prox_on && !prox_dev_open) {
		ret = taos_prox_on();
		if (ret < 0) {
			printk("XXXXXXXXXXXXXXXXX TAOS_PROX_RESUME_ ERROR!!! XXXXXXXXXXXXX\n");
			return ret;
		}
#if PROX_TIMER_FUNC
		taos_prox_poll_timer_start(400);
#endif
	}

	printk("XXXXXXXXXXx taos_prox_resume XXXXXXXXXXXX\n");

	return ret;
}

// open
static int taos_open(struct inode *inode, struct file *file)
{
	struct taos_data *taos_datap;
	int ret = 0;

	taos_datap = container_of(inode->i_cdev, struct taos_data, cdev);
	if (strcmp(taos_datap->taos_name, TAOS_DEVICE_ID) != 0) {
		printk(KERN_ERR "TAOS: device name incorrect during taos_open(), get %s\n", taos_datap->taos_name);
		ret = -ENODEV;
	}

	file->f_pos = 0;
	return (ret);
}

// release
static int taos_release(struct inode *inode, struct file *file)
{
	struct taos_data *taos_datap;
	int ret = 0;

	taos_datap = container_of(inode->i_cdev, struct taos_data, cdev);
	if (strcmp(taos_datap->taos_name, TAOS_DEVICE_ID) != 0) {
		printk(KERN_ERR "TAOS: device name incorrect during taos_release(), get %s\n", taos_datap->taos_name);
		ret = -ENODEV;
	}
#if 0
	del_timer(&prox_poll_timer);
	del_timer(&als_poll_timer);

	taos_als_off();
	taos_prox_off();

	prox_on = 0;
	als_on = 0;
	prox_history_hi = 0;
	prox_history_hi = 0;
#endif

	return (ret);
}


// read
static int taos_read(struct file *file, char *buf, size_t count, loff_t * ppos)
{
	struct taos_data *taos_datap;
	u8 i = 0, xfrd = 0, reg = 0;
	u8 my_buf[TAOS_MAX_DEVICE_REGS];
	int ret = 0;

#if PROX_TIMER_FUNC
	if (prox_on)
		taos_prox_timer_del();
#endif
#if ALS_TIMER_FUNC
	if (als_on)
		taos_als_timer_del();
#endif

	*ppos = file->f_pos;
	if ((*ppos < 0) || (*ppos >= TAOS_MAX_DEVICE_REGS) || (count > TAOS_MAX_DEVICE_REGS)) {
		printk(KERN_ERR "TAOS: reg limit check failed in taos_read()\n");
		return -EINVAL;
	}

	reg = (u8) * ppos;
	taos_datap = container_of(file->f_dentry->d_inode->i_cdev, struct taos_data, cdev);

	while (xfrd < count) {
		if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | reg)))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in taos_read()\n");
			return (ret);
		}
		my_buf[i++] = i2c_smbus_read_byte(taos_datap->client);
		reg++;
		xfrd++;
	}

	if ((ret = copy_to_user(buf, my_buf, xfrd))) {
		printk(KERN_ERR "TAOS: copy_to_user failed in taos_read()\n");
		return -ENODATA;
	}

#if PROX_TIMER_FUNC
	if (prox_on)
		taos_prox_poll_timer_start(300);
#endif
#if ALS_TIMER_FUNC
	if (als_on)
		taos_als_poll_timer_start(250);
#endif

	return ((int)xfrd);
} 

// write
static int taos_write(struct file *file, const char *buf, size_t count, loff_t * ppos)
{
	struct taos_data *taos_datap;
	u8 i = 0, xfrd = 0, reg = 0;
	u8 my_buf[TAOS_MAX_DEVICE_REGS];
	int ret = 0;

#if PROX_TIMER_FUNC
	if (prox_on)
		taos_prox_timer_del();
#endif
#if ALS_TIMER_FUNC
	if (als_on)
		taos_als_timer_del();
#endif

	*ppos = file->f_pos;
	if ((*ppos < 0) || (*ppos >= TAOS_MAX_DEVICE_REGS) || ((*ppos + count) > TAOS_MAX_DEVICE_REGS)) {
		printk(KERN_ERR "TAOS: reg limit check failed in taos_write()\n");
		return -EINVAL;
	}

	reg = (u8) * ppos;
	if ((ret = copy_from_user(my_buf, buf, count))) {
		printk(KERN_ERR "TAOS: copy_to_user failed in taos_write()\n");
		return -ENODATA;
	}

	taos_datap = container_of(file->f_dentry->d_inode->i_cdev, struct taos_data, cdev);
	while (xfrd < count) {
		if ((ret =
		      (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | reg), my_buf[i++]))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in taos_write()\n");
			return (ret);
		}

		reg++;
		xfrd++;
	}

#if PROX_TIMER_FUNC	
	if (prox_on)
		taos_prox_poll_timer_start(300);
#endif
#if ALS_TIMER_FUNC
	if (als_on)
		taos_als_poll_timer_start(250);
#endif

	return ((int)xfrd);
} 

// llseek
static loff_t taos_llseek(struct file *file, loff_t offset, int orig)
{
	int ret = 0;
	loff_t new_pos = 0;

#if PROX_TIMER_FUNC
	if (prox_on)
		taos_prox_timer_del();
#endif
#if ALS_TIMER_FUNC
	if (als_on)
		taos_als_timer_del();
#endif

	if ((offset >= TAOS_MAX_DEVICE_REGS) || (orig < 0) || (orig > 1)) {
		printk(KERN_ERR "TAOS: offset param limit or origin limit check failed in taos_llseek()\n");
		return -EINVAL;
	}

	switch (orig) {
	case 0:
		new_pos = offset;
		break;
	case 1:
		new_pos = file->f_pos + offset;
		break;
	default:
		return -EINVAL;
		break;
	}

	if ((new_pos < 0) || (new_pos >= TAOS_MAX_DEVICE_REGS) || (ret < 0)) {
		printk(KERN_ERR "TAOS: new offset limit or origin limit check failed in taos_llseek()\n");
		return -EINVAL;
	}

	file->f_pos = new_pos;

#if PROX_TIMER_FUNC	
	if (prox_on)
		taos_prox_poll_timer_start(300);
#endif
#if ALS_TIMER_FUNC
	if (als_on)
		taos_als_poll_timer_start(250);
#endif

	return new_pos;
}

// ioctls
static int taos_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct taos_data *taos_datap;
	int prox_sum = 0, prox_mean = 0, prox_max = 0;
	int lux_val = 0, ret = 0, i = 0, tmp = 0;
	u16 gain_trim_val = 0;
	u8 reg_val = 0;
	//int ret_check = 0;
	//int ret_m = 0;
	//u8 reg_val_temp = 0;
	taos_datap = container_of(inode->i_cdev, struct taos_data, cdev);

	switch (cmd) {
#if 0
	case TAOS_IOCTL_SENSOR_CHECK:
		reg_val_temp = 0;
		if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
			printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_CHECK failed\n");
			return (ret);
		}

		reg_val_temp = i2c_smbus_read_byte(taos_datap->client);
		printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_CHECK,prox_adc_time,%d~\n", reg_val_temp);

		if ((reg_val_temp & 0xFF) == 0xF)
			return -ENODATA;
		break;
	case TAOS_IOCTL_SENSOR_CONFIG:
		printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_CONFIG,test01~\n");
		ret = copy_from_user(taos_cfgp, (struct taos_cfg *)arg, sizeof(struct taos_cfg));

		if (ret) {
			printk(KERN_ERR "TAOS: copy_from_user failed in ioctl config_set\n");
			return -ENODATA;
		}

		break;
	case TAOS_IOCTL_SENSOR_ON:
		ret = 0;
		reg_val = 0;
		ret_m = 0;
		printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON, test01~\n");
		
#if 0
		    /*decide the numbers of sensors */ 
		    ret_m = taos_sensor_mark();
		if (ret_m < 0) {
			printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON, error~~\n");
			return -1;
		}
		if (ret_m == 0) {
			printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON, num >0 ~~\n");
			return 1;
		}
		
#endif	/*  */
#if 1
		    /*Register init and turn off */ 
		    for (i = 0; i < TAOS_FILTER_DEPTH; i++) {
			    /*Rambo ?? */ 
			    lux_history[i] = -ENODATA;
		}
		
#endif	/*  */
		    
#if 0		
		    for (i = 0; i < sizeof(taos_triton_reg_init); i++) {
			if (i != 11) {
				if ((ret =
				      (i2c_smbus_write_byte_data
				       (taos_datap->client, (TAOS_TRITON_CMD_REG | (TAOS_TRITON_CNTRL + i)),
					taos_triton_reg_init[i]))) < 0) {
					printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_on\n");
					return (ret);
				}
			}
		}
		
#endif	/*  */
		    /*ALS interrupt clear */ 
		    if ((ret =
			 (i2c_smbus_write_byte
			  (taos_datap->client,
			   (TAOS_TRITON_CMD_REG | TAOS_TRITON_CMD_SPL_FN | TAOS_TRITON_CMD_ALS_INTCLR)))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_on\n");
			return (ret);
		}
		printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON, test02~\n");
		
		    /*Register setting */ 
#if 0
		    if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | 0x00), 0x00))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
			return (ret);
		}
		
#endif	/*  */
		    printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON, test03,prox_int_time,%d~\n",
			    taos_cfgp->prox_int_time);
		if ((ret =
		      (i2c_smbus_write_byte_data
		       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_ALS_TIME),
			taos_cfgp->prox_int_time))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
			return (ret);
		}
		printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON, test04,prox_adc_time,%d~\n", taos_cfgp->prox_adc_time);

		if ((ret =
		      (i2c_smbus_write_byte_data
		       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_PRX_TIME),
			taos_cfgp->prox_adc_time))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
			return (ret);
		}
		printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON, test05,wait_time,%d~\n", taos_cfgp->wait_time);

		if ((ret =
		      (i2c_smbus_write_byte_data
		       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_WAIT_TIME),
			taos_cfgp->wait_time))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
			return (ret);
		}
		printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON, test05-1,prox_intr_filter,%d~\n",
			 taos_cfgp->prox_intr_filter);

		if ((ret =
		      (i2c_smbus_write_byte_data
		       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_INTERRUPT),
			taos_cfgp->prox_intr_filter))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
			return (ret);
		}
		printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON, test06,prox_config,%d~\n", taos_cfgp->prox_config);

		if ((ret =
		      (i2c_smbus_write_byte_data
		       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_PRX_CFG),
			taos_cfgp->prox_config))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
			return (ret);
		}
		printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON, test06,prox_pulse_cnt,%d~\n", taos_cfgp->prox_pulse_cnt);

		if ((ret =
		      (i2c_smbus_write_byte_data
		       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_PRX_COUNT),
			taos_cfgp->prox_pulse_cnt))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
			return (ret);
		}
		
		    /*gain */ 
#if 0
		    reg_val_temp = 0;
		if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_GAIN)))) < 0) {
			printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON failed in ioctl als_calibrate\n");
			return (ret);
		}
		reg_val_temp = i2c_smbus_read_byte(taos_datap->client);
		printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON, test07-1,gain_init&0xC,%d~\n", reg_val_temp & 0xC);
		printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON, test07-2,prox_gain&0xFC,%d~\n",
			(taos_cfgp->prox_gain & 0xFC));
		reg_val_temp = (taos_cfgp->prox_gain & 0xFC) | (reg_val_temp & 0xC);
		printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON, test07,%d~\n", reg_val_temp);
		
#endif	/*  */
		    if ((ret =
			 (i2c_smbus_write_byte_data
			  (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_GAIN), taos_cfgp->prox_gain))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
			return (ret);
		}
		
		    /*turn on */ 
#if 0
		    reg_val_temp = 0;
		if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
			printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON failed in ioctl als_calibrate\n");
			return (ret);
		}
		reg_val_temp = i2c_smbus_read_byte(taos_datap->client);
		printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON, test08-1,%d~\n", reg_val_temp);
		printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON, test08-2,%d~\n", (reg_val_temp & 0x40));
		reg_val_temp = (0xF & 0x3F) | (reg_val_temp & 0x40);
		printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON, test08,%d~\n", reg_val_temp);
		
#endif	/*  */
		    if ((ret =
			 (i2c_smbus_write_byte_data
			  (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL), 0xF))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
			return (ret);
		}

		break;
	case TAOS_IOCTL_SENSOR_OFF:
		ret = 0;
		reg_val = 0;
		ret_check = 0;
		ret_m = 0;
		
#if 0
		    /*chech the num of used sensor */ 
		    ret_check = taos_sensor_check();
		printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_OFF,test01~\n");
		if (ret_check < 0) {
			printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_OFF, error~~\n");
			return -1;
		}
		if (ret_check == 0) {
			printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_ON, num >1 ~~\n");
			return 1;
		}
		
#endif	/*  */
		    /*turn off */ 
		    printk(KERN_ERR "TAOS: TAOS_IOCTL_SENSOR_OFF,test02~\n");
		if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | 0x00), 0x00))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_off\n");
			return (ret);
		}

		break;
#endif

	case TAOS_IOCTL_ALS_ON:
		ret = taos_als_on();
		als_on = 1;
#if ALS_TIMER_FUNC
		als_timer_count++;
		taos_als_poll_timer_start(400);
#endif
		printk("XXXXXXX TAOS_IOCTL_ALS_ON XXXXXXXXXXXXXX\n");
		return (ret);
		break;
	case TAOS_IOCTL_ALS_OFF:
#if ALS_TIMER_FUNC
		taos_als_timer_del();
		als_timer_count--;
#endif
		ret = taos_als_off();
		als_on = 0;
		printk("XXXXXXXXXXXXXXX TAOS_LOCTL_ALS_OFF XXXXXX\n");
		return (ret);
		break;
	case TAOS_IOCTL_ALS_DATA:
		if (als_on)
#if ALS_TIMER_FUNC
			taos_als_timer_del();
#else
			break;
#endif
		else {
			printk(KERN_ERR "TAOS: ioctl als_data was called before ioctl als_on was called\n");
			return -EPERM;
		}

		lux_val = taos_als_data();
#if ALS_TIMER_FUNC
		taos_als_poll_timer_start(250);
#endif
		return (lux_val);
		break;
	case TAOS_IOCTL_ALS_CALIBRATE:
		if (als_on)
#if ALS_TIMER_FUNC
			taos_als_timer_del();
#else
			break;
#endif
		else {
			printk(KERN_ERR "TAOS: ioctl als_calibrate was called before ioctl als_on was called\n");
			return -EPERM;
		}

		if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_calibrate\n");
			return (ret);
		}
		reg_val = i2c_smbus_read_byte(taos_datap->client);
		if ((reg_val & 0x07) != 0x07)
			return -ENODATA;

		if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_STATUS)))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_calibrate\n");
			return (ret);
		}
		reg_val = i2c_smbus_read_byte(taos_datap->client);
		if ((reg_val & 0x01) != 0x01)
			return -ENODATA;

		if ((lux_val = taos_get_lux()) < 0) {
			printk(KERN_ERR "TAOS: call to lux_val() returned error %d in ioctl als_data\n", lux_val);
			return (lux_val);
		}
		gain_trim_val = (u16) (((taos_cfgp->calibrate_target) * 512) / lux_val);
		taos_cfgp->gain_trim = (int)gain_trim_val;
#if ALS_TIMER_FUNC
		taos_als_poll_timer_start(250);
#endif

		return ((int)gain_trim_val);
		break;
	case TAOS_IOCTL_CONFIG_GET:
		ret = copy_to_user((struct taos_cfg *)arg, taos_cfgp, sizeof(struct taos_cfg));
		if (ret) {
			printk(KERN_ERR "TAOS: copy_to_user failed in ioctl config_get\n");
			return -ENODATA;
		}
		return (ret);
		break;
	case TAOS_IOCTL_CONFIG_SET:
		ret = copy_from_user(taos_cfgp, (struct taos_cfg *)arg, sizeof(struct taos_cfg));
		if (ret) {
			printk(KERN_ERR "TAOS: copy_from_user failed in ioctl config_set\n");
			return -ENODATA;
		}
		if (taos_cfgp->als_time < 50)
			taos_cfgp->als_time = 50;
		if (taos_cfgp->als_time > 650)
			taos_cfgp->als_time = 650;
		tmp = (taos_cfgp->als_time + 25) / 50;
		taos_cfgp->als_time = tmp * 50;
		sat_als = (256 - taos_cfgp->prox_int_time) << 10;
		sat_prox = (256 - taos_cfgp->prox_adc_time) << 10;
		break;
	case TAOS_IOCTL_PROX_ON:
		ret = taos_prox_on();
		prox_on = 1;
#if PROX_TIMER_FUNC
		prox_timer_count++;
		taos_prox_poll_timer_start(400);
#endif
		printk("XXXXXXXXX TAOS_LOCTL_PROX_ON XXXXXXXXXX\n");
		return ret;
		break;
	case TAOS_IOCTL_PROX_OFF:
#if PROX_TIMER_FUNC	
		taos_prox_timer_del();
		prox_timer_count--;
#endif
		ret = taos_prox_off();
		prox_on = 0;
		printk("XXXXXXXXXXXXX TAOS_LOCTL_PROX_OFF XXXXXXXXX\n");
		return ret;
		break;
	case TAOS_IOCTL_PROX_DATA:
		if (prox_on)
#if PROX_TIMER_FUNC
			taos_prox_timer_del();
#else
			break;
#endif
		else {
			printk(KERN_ERR "TAOS: ioctl prox_calibrate was called before ioctl prox_on was called\n");
			return -EPERM;
		}

		ret = taos_prox_data();
#if PROX_TIMER_FUNC
		taos_prox_poll_timer_start(300);
#endif
		if (ret < 0)
			return ret;

		ret = copy_to_user((struct taos_prox_info *)arg, prox_cur_infop, sizeof(struct taos_prox_info));
		if (ret) {
			printk(KERN_ERR "TAOS: copy_to_user failed in ioctl prox_data\n");
			return -ENODATA;
		}
		return (ret);
		break;
	case TAOS_IOCTL_PROX_EVENT:
		if (prox_on)
#if PROX_TIMER_FUNC
			taos_prox_timer_del();
#else
			break;
#endif
		else {
			printk(KERN_ERR "TAOS: ioctl prox_calibrate was called before ioctl prox_on was called\n");
			return -EPERM;
		}

		ret = taos_prox_data();
#if PROX_TIMER_FUNC
		taos_prox_poll_timer_start(300);
#endif
		if (ret < 0)
			return ret;

		return (prox_cur_infop->prox_event);
		break;
	case TAOS_IOCTL_PROX_CALIBRATE:
		if (prox_on)
#if PROX_TIMER_FUNC
			taos_prox_timer_del();
#else
			break;
#endif
		else {
			printk(KERN_ERR "TAOS: ioctl prox_calibrate was called before ioctl prox_on was called\n");
			return -EPERM;
		}
#if ALS_TIMER_FUNC
		if (als_on)
			taos_als_timer_del();
#endif
		printk("XXXXXXXXXXXX TAOS_IOCTL_PROX_CALIBRATE: XXXXXXXXX\n");
		/*change wait time to 2.72ms*/
		if ((ret =
		      (i2c_smbus_write_byte_data
		       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_WAIT_TIME), 0xFF))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
			return (ret);
		}

		prox_sum = 0;
		prox_max = 0;
		for (i = 0; i < 5; i++) {
			if ((ret = taos_prox_poll(&prox_cal_info[i])) < 0) {
				printk(KERN_ERR "TAOS: call to prox_poll failed in ioctl prox_calibrate\n");
				return -61;
			}
			prox_sum += prox_cal_info[i].prox_data;
			if (prox_cal_info[i].prox_data > prox_max)
				prox_max = prox_cal_info[i].prox_data;

			mdelay(60);
		}
		prox_mean = prox_sum / 10;
		taos_cfgp->prox_threshold_hi = ((((prox_max - prox_mean) * 200) + 50) / 100) + prox_mean;
		taos_cfgp->prox_threshold_lo = ((((prox_max - prox_mean) * 170) + 50) / 100) + prox_mean;
		
		if (taos_cfgp->prox_threshold_lo < ((sat_prox * 20) / 100)) {
			taos_cfgp->prox_threshold_lo = ((sat_prox * 15) / 100);
			taos_cfgp->prox_threshold_hi = ((sat_prox * 20) / 100);
		}

		if (taos_cfgp->prox_threshold_hi > ((sat_prox * 40) / 100)) {
			taos_cfgp->prox_threshold_lo = sat_prox * 15 / 100;
			taos_cfgp->prox_threshold_hi = sat_prox * 20 / 100;
		}

		/*set wait time*/
		if ((ret =
		      (i2c_smbus_write_byte_data
		       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_WAIT_TIME), taos_cfgp->wait_time))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
			return (ret);
	}

		printk("XXXXXXXXXXX prox_threshold_lo = %d\nXXXXXXXXXXX prox_threshold_hi = %d\n", taos_cfgp->prox_threshold_lo, taos_cfgp->prox_threshold_hi);

#if PROX_TIMER_FUNC
		taos_prox_poll_timer_start(300);
#endif
#if ALS_TIMER_FUNC
		if (als_on)
			taos_als_poll_timer_start(250);
#endif
		break;
	default:
		return -EINVAL;
		break;
	}

	return ret;
}

static int taos_prox_calibrate(void)
{
	int prox_sum = 0, prox_mean = 0, prox_max = 0;
	int ret = 0, i = 0;
	u8 reg_val = 0;

	printk("XXXXXXXXXXXX TAOS_PROX_CALIBRATE: XXXXXXXXX\n");

	/*set power off*/
	if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in taos_prox_on\n");
		return (ret);
	}
	reg_val = i2c_smbus_read_byte(taos_datap->client);
	reg_val &= ~TAOS_TRITON_CNTL_PWRON;
	if ((ret =
	(i2c_smbus_write_byte_data
	(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL), reg_val))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in taos_prox_on\n");
		return (ret);
	}

	/*clear interrupt status*/
	if ((ret =
	      (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CMD_SPL_FN | TAOS_TRITON_CMD_ALS_INTCLR | TAOS_TRITON_CMD_PROX_INTCLR)))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte(2) failed in taos_prox_poll()\n");
		return (ret);
	}

	/* set ALS time 51.68ms */
	if ((ret =
	      (i2c_smbus_write_byte_data
	       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_ALS_TIME), 0xED))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}

	/*set prox time*/
	if ((ret =
	      (i2c_smbus_write_byte_data
	       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_PRX_TIME), taos_cfgp->prox_adc_time))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}

	/*set wait time*/
	if ((ret =
	      (i2c_smbus_write_byte_data
	       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_WAIT_TIME), 0xFF))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}

	/*config prox interrupt*/
	if ((ret =
	      (i2c_smbus_write_byte_data
	       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_INTERRUPT), taos_cfgp->prox_intr_filter))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}

	/*set prox config register(wait long??)*/
	if ((ret =
	      (i2c_smbus_write_byte_data
	       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_PRX_CFG), taos_cfgp->prox_config))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}

	/*set prox pulse*/
	if ((ret =
	      (i2c_smbus_write_byte_data
	       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_PRX_COUNT), taos_cfgp->prox_pulse_cnt))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}

	/*set als gain*/
	if ((ret =
	      (i2c_smbus_write_byte_data
	       (taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_GAIN), (taos_cfgp->prox_gain | taos_cfgp->gain)))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}

	/*set als, prox, wait and power enable*/
	if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL), 0x0F))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}

	for (i = 0; i < 20; i++) {
		if ((ret = taos_prox_poll(&prox_cal_info[i])) < 0) {
			printk(KERN_ERR "TAOS: call to prox_poll failed in ioctl prox_calibrate\n");
			return -61;
		}
		prox_sum += prox_cal_info[i].prox_data;
		if (prox_cal_info[i].prox_data > prox_max)
			prox_max = prox_cal_info[i].prox_data;

		msleep(60);
	}
	prox_mean = prox_sum / 20;
	taos_cfgp->prox_threshold_hi = ((((prox_max - prox_mean) * 200) + 50) / 100) + prox_mean;
	taos_cfgp->prox_threshold_lo = ((((prox_max - prox_mean) * 170) + 50) / 100) + prox_mean;

	/* get smaller value */
	if (taos_cfgp->prox_threshold_lo < ((sat_prox * 2) / 100)) {
		taos_cfgp->prox_threshold_lo = ((sat_prox * 2) / 100);
		taos_cfgp->prox_threshold_hi = ((sat_prox * 5) / 100);
	}

	/* panel down */
	if (taos_cfgp->prox_threshold_hi > ((sat_prox * 60) / 100)) {
		taos_cfgp->prox_threshold_lo = sat_prox * 35 / 100;
		taos_cfgp->prox_threshold_hi = sat_prox * 40 / 100;
	}

	printk("XXXXXXXXXXX prox_threshold_lo = %d\nXXXXXXXXXXX prox_threshold_hi = %d\n", taos_cfgp->prox_threshold_lo, taos_cfgp->prox_threshold_hi);

	/* prox off */
	if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in taos_prox_off\n");
		return (ret);
	}
	reg_val = i2c_smbus_read_byte(taos_datap->client);
	reg_val &= ~TAOS_TRITON_CNTL_PROX_DET_ENBL;

	if ((ret =
	(i2c_smbus_write_byte_data
	(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL), reg_val))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_off\n");
		return (ret);
	}

	prox_history_hi = 0;
	prox_history_lo = 0;

	return (ret);
}

// read/calculate lux value
static int taos_get_lux(void)
{
	u16 raw_clear = 0, raw_ir = 0, raw_lux = 0;
	u32 lux = 0;
	u32 ratio = 0;
	u8 dev_gain = 0;
	struct lux_data *p;
	int ret = 0;
	u8 chdata[4];
	int tmp = 0, i = 0;

	for (i = 0; i < 4; i++) {
		if ((ret =
		      (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | (TAOS_TRITON_ALS_CHAN0LO + i)))))
		     < 0) {
			printk(KERN_ERR
				"TAOS: i2c_smbus_write_byte() to chan0/1/lo/hi reg failed in taos_get_lux()\n");
			return (ret);
		}
		chdata[i] = i2c_smbus_read_byte(taos_datap->client);
		//printk("ch(%d),data=%d\n", i, chdata[i]);
	}
	//printk("ch0=%d\n", chdata[0] + chdata[1] * 256);
	//printk("ch1=%d\n", chdata[2] + chdata[3] * 256);
	tmp = (taos_cfgp->als_time + 25) / 50;	//if atime =100  tmp = (atime+25)/50=2.5   tine = 2.7*(256-atime)=  412.5
	TritonTime.numerator = 1;
	TritonTime.denominator = tmp;
	tmp = 300 * taos_cfgp->als_time;	//tmp = 300*atime  400
	if (tmp > 65535)
		tmp = 65535;
	TritonTime.saturation = tmp;
	raw_clear = chdata[1];
	raw_clear <<= 8;
	raw_clear |= chdata[0];

	raw_ir = chdata[3];
	raw_ir <<= 8;
	raw_ir |= chdata[2];
	if (raw_ir > raw_clear) {
		raw_lux = raw_ir;
		raw_ir = raw_clear;
		raw_clear = raw_lux;
	}
	raw_clear *= taos_cfgp->scale_factor;
	raw_ir *= taos_cfgp->scale_factor;
	dev_gain = taos_triton_gain_table[taos_cfgp->gain & 0x3];
	if (raw_clear >= lux_timep->saturation)
		return (TAOS_MAX_LUX);
	if (raw_ir >= lux_timep->saturation)
		return (TAOS_MAX_LUX);
	if (raw_clear == 0)
		return (0);
	if (dev_gain == 0 || dev_gain > 127) {
		printk(KERN_ERR "TAOS: dev_gain = 0 or > 127 in taos_get_lux()\n");
		return -1;
	}
	if (lux_timep->denominator == 0) {
		printk(KERN_ERR "TAOS: lux_timep->denominator = 0 in taos_get_lux()\n");
		return -1;
	}
	ratio = (raw_ir << 15) / raw_clear;
	for (p = lux_tablep; p->ratio && p->ratio < ratio; p++) ;
		if (!p->ratio)
			return 0;
	lux = ((raw_clear * (p->clear)) - (raw_ir * (p->ir)));
	lux = ((lux + (lux_timep->denominator >> 1)) / lux_timep->denominator) * lux_timep->numerator;
	lux = (lux + (dev_gain >> 1)) / dev_gain;
	lux >>= TAOS_SCALE_MILLILUX;
	if (lux > TAOS_MAX_LUX)
		lux = TAOS_MAX_LUX;

	if (lux < 15000)
		lux *= 18;
	else if (lux < 50000)
		lux *= 8;
	else if (lux < 600000)
		lux *= 4;
	else
		lux *= 2; 

	return lux;
}

static int taos_lux_filter(int lux) 
{
	static u8 middle[] = { 1, 0, 2, 0, 0, 2, 0, 1 };
	int index;

	lux_history[2] = lux_history[1];
	lux_history[1] = lux_history[0];
	lux_history[0] = lux;
	if ((lux_history[2] < 0) || (lux_history[1] < 0) || (lux_history[0] < 0))
		return -ENODATA;
	index = 0;
	if (lux_history[0] > lux_history[1])
		index += 4;
	if (lux_history[1] > lux_history[2])
		index += 2;
	if (lux_history[0] > lux_history[2])
		index++;
	return (lux_history[middle[index]]);
}

// verify device
static int taos_device_name(unsigned char *bufp, char **device_name)
{
	if (bufp[0x12] != 0x20)
		return 0;
	*device_name = "tritonFN";
	return 1;
}

// proximity poll
static int taos_prox_poll(struct taos_prox_info *prxp)
{
	static int event = 0;
	u16 status = 0;
	int i = 0, ret = 0, wait_count = 0;
	u8 chdata[6];

	/*read status == 0x30*/
	if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_STATUS)))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte(1) failed in taos_prox_poll()\n");
		return (ret);
	}
	status = i2c_smbus_read_byte(taos_datap->client);
	while ((status & 0x30) != 0x30) {
		status = i2c_smbus_read_byte(taos_datap->client);
		wait_count++;
		if (wait_count > 30) {
			printk(KERN_ERR "TAOS: Prox status invalid for 300 ms in taos_prox_poll()\n");
			return -ENODATA;
		}
		mdelay(10);
	}

	/*clear PROX interrupt*/
	if ((ret =
	      (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CMD_SPL_FN | 0x05)))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte(2) failed in taos_prox_poll()\n");
		return (ret);
	}

	/*clear ALS interrupt*/
	if ((ret =
	      (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CMD_SPL_FN | 0x06)))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte(3) failed in taos_prox_poll()\n");
		return (ret);
	}

	/*read prox and als data*/
	for (i = 0; i < 6; i++) {
		if ((ret =
		      (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | (TAOS_TRITON_ALS_CHAN0LO + i)))))
		     < 0) {
			printk(KERN_ERR
				"TAOS: i2c_smbus_write_byte() to als/prox data reg failed in taos_prox_poll()\n");
			return (ret);
		}
		chdata[i] = i2c_smbus_read_byte(taos_datap->client);
	}
	prxp->prox_clear = chdata[1];
	prxp->prox_clear <<= 8;
	prxp->prox_clear |= chdata[0];
	//printk("ch0 == %d\n", prxp->prox_clear);
	if (prxp->prox_clear > ((sat_als * 80) / 100)) {
		prxp->prox_data = sat_prox; //strong light calibration.
		prxp->prox_event = 1;
		return 0;
	}

	prxp->prox_data = chdata[5];
	prxp->prox_data <<= 8;
	prxp->prox_data |= chdata[4];
	//printk("prxp->prox_data == %d\n", prxp->prox_data);
	prox_history_hi <<= 1;
	prox_history_hi |= ((prxp->prox_data > taos_cfgp->prox_threshold_hi) ? 1 : 0);
	prox_history_hi &= 0x07;
	prox_history_lo <<= 1;
	prox_history_lo |= ((prxp->prox_data > taos_cfgp->prox_threshold_lo) ? 1 : 0);
	prox_history_lo &= 0x01;
	if (prox_history_hi == 0x07)
		event = 0;
	
	else {
		if (prox_history_lo == 0)
			event = 1;
	}
	prxp->prox_event = event;
	return (ret);
}

#if PROX_TIMER_FUNC
static void taos_prox_work_f(struct work_struct *work)
{
	int ret = 0;

	if (prox_on) {
		//mutex_lock(&taos_mutex_lock);
		ret = taos_prox_data();
		//mutex_unlock(&taos_mutex_lock);

		if (ret < 0) {
			printk(KERN_ERR "TAOS: call to prox_poll failed in taos_prox_poll_timer_func()\n");
			return;
		}

		mutex_lock(&__msm_sensors_lock);
		msm_sensors_input_report_value(EV_MSC, MSC_GESTURE, prox_cur_infop->prox_event);
		msm_sensors_input_sync();
		mutex_unlock(&__msm_sensors_lock);

		//printk("XXXXXXXXXXXXX prox->event == %d XXXXXXXXX\n", prox_cur_infop->prox_event);  //for test
	}
	else
		printk("XXXXXXXXXX taos_prox_work_f status error !! XXXXXXXXXX\n");
}

// prox poll timer function
static void taos_prox_poll_timer_func(unsigned long param)
{
	schedule_work(&taos_datap->prox_workqueue);
	taos_prox_poll_timer_start(400);
}

// start prox poll timer
static void taos_prox_poll_timer_start(unsigned int t)
{
	if (1 == prox_timer_count)
		mod_timer(&prox_poll_timer, jiffies + t * HZ / 1000);
}

static void taos_prox_timer_del(void)
{
	if (1 == prox_timer_count)
		del_timer(&prox_poll_timer);
}
#endif

#if ALS_TIMER_FUNC
//als_workqueue_function
static void taos_als_work_f(struct work_struct *work)
{
	int lux_value;

	if (als_on) {
		//mutex_lock(&taos_mutex_lock);
		lux_value = taos_als_data();
		//mutex_unlock(&taos_mutex_lock);

		if (lux_value < 0) {
			if (-61 == lux_value)
				return;
			printk("XXXXXXXXXXX taos_als_poll_timer_func error XXXXXXXXXXXx\n");
			return;
		}

		mutex_lock(&__msm_sensors_lock);
		msm_sensors_input_report_value(EV_MSC, MSC_RAW, lux_value);
		msm_sensors_input_sync();
		mutex_unlock(&__msm_sensors_lock);
		//printk("XXXXXXXXXXXXX lux_value== %d XXXXXXXXX\n", lux_value);  //for test
	}
	else
		printk("XXXXXXXXXX taos_als_work_f status error !! XXXXXXXXXX\n");
}

//als poll timer func
static void taos_als_poll_timer_func(unsigned long param)
{
	schedule_work(&taos_datap->als_workqueue);
	taos_als_poll_timer_start(250);
}

static void taos_als_poll_timer_start(unsigned int t)
{
	if (1 == als_timer_count)
		mod_timer(&als_poll_timer, jiffies + t * HZ / 1000);
}

static void taos_als_timer_del(void)
{
	if (1 == als_timer_count)
		del_timer(&als_poll_timer);
}
#endif

MODULE_AUTHOR("Ge Nan <nan.ge@tct-sh.com>");
MODULE_DESCRIPTION("TAOS ambient light and proximity sensor driver");
MODULE_LICENSE("GPL");

module_init(taos_init);
module_exit(taos_exit);
