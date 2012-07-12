/* drivers/input/keyboard/ft5x02_i2c.c
 *
 *
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>
#include "ft5x02_i2c.h"
#define FT5X02_I2C_NAME "ft5x02-ts"

#include <linux/proc_fs.h>
#include "ft5x02_i2c.h"

#include <mach/vreg.h>


#undef DEBUG_FT5X02

/*requested touch panel firmware version*/
#define REQUIRED_VERSION 0x13

/*the max point supported by IC ft5x02 usually 2 or 5*/
#define MAX_POINT 5	

/*the y axis of down position*/
int down_position_y = 0;

#define CDBG(fmt, arg...) printk(fmt, ##arg)

static struct workqueue_struct *ft5x02_wq;

struct ts_event {
	u16	x1;
	u16	y1;
	u16	x2;
	u16	y2;
	u16	x3;
	u16	y3;
	u16	x4;
	u16	y4;
	u16	x5;
	u16	y5;
	u16	pressure;
    s16 touch_ID1;
	s16 touch_ID2;
    s16 touch_ID3;
    s16 touch_ID4;
	s16 touch_ID5;
	u8 touch_point;
};

struct ft5x02_ts_data {
	uint16_t addr;
	struct i2c_client *client;
	struct ts_event		event;
	struct input_dev *input_dev;
	int use_irq;
	struct hrtimer timer;
	struct work_struct  work;
	uint16_t max[2];
	uint32_t flags;
	int (*power)(int on);
	struct early_suspend early_suspend;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ft5x02_ts_early_suspend(struct early_suspend *h);
static void ft5x02_ts_late_resume(struct early_suspend *h);
#endif

struct i2c_client *ft5x02_client = NULL;

static char write_reg(unsigned char addr, char v);
char read_reg(unsigned char addr);
static int32_t read_info(void * buf);

/********************************/

int32_t ft5x02_i2c_txdata(unsigned short saddr,
	unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		},
	};

	if (i2c_transfer(ft5x02_client->adapter, msg, 1) < 0) {
		CDBG("ft5x02_i2c_txdata faild\n");
		return -EIO;
	}

	return 0;
}

int ft5x02_i2c_rxdata(int txlen, unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
	{
		.addr   = ft5x02_client->addr,
		.flags = 0,
		.len   = txlen,
		.buf   = rxdata,
	},
	{
		.addr   = ft5x02_client->addr,
		.flags = I2C_M_RD,
		.len   = length,
		.buf   = rxdata,
	},
	};

	if (i2c_transfer(ft5x02_client->adapter, msgs, 2) < 0) {
		CDBG("ft5x02_i2c_rxdata failed!\n");
		return -EIO;
	}
	return 0;
}

static char write_reg(unsigned char addr, char v)
{
	char tmp[4], ecc = 0;
	int32_t rc = 0;

	memset(tmp, 0, 2);
	tmp[0] = 0xfc;
	ecc ^= 0xfc;
	tmp[1] = addr;
	ecc ^= addr;
	tmp[2] = v;
	ecc ^= v;
	tmp[3] = ecc;

	rc = ft5x02_i2c_txdata(ft5x02_client->addr, tmp, 4);
	if (rc < 0){
		CDBG("ft5x02 write reg failed!\n");
		return rc;
	}
	return 0;
}

char read_reg(unsigned char addr)
{
	char tmp[2];
	int32_t rc = 0;

	memset(tmp, 0, 2);
	tmp[0] = 0xfc;
	tmp[1] = addr +0x40;
	rc = ft5x02_i2c_rxdata(2, tmp, 2);
	if (rc < 0){
		CDBG("ft5x02_i2c_read failed!\n");
		return rc;
	}
	return tmp[0];
}

static int32_t read_info(void * buf)
{
	char tmp[32];
	int32_t rc = 0, len;

	memset(tmp, 0, 32);
	tmp[0] = 0xf9;
	len = 26;
	rc = ft5x02_i2c_rxdata(1, tmp, len);
	if (rc < 0){
		CDBG("ft5x02_i2c_read failed!\n");
		return rc;
	}
	memcpy(buf, tmp, len);
	return len;
}

static int ft5x02_init_panel(struct ft5x02_ts_data *ts)
{
	int ret;

	ret = write_reg(0x3c, 0x01);
	if(ret < 0)
		return -1;
	ret = write_reg(0x3a, 0x01);
	if(ret < 0)
		return -1;
	ret = write_reg(0x06, 0x01);
	if(ret < 0)
		return -1;
	ret = write_reg(0x07, 200);
	if(ret < 0)
		return -1;
	ret = write_reg(0x08, 0x10);
	if(ret < 0)
		return -1;
	ret = write_reg(0x09, 0x28);
	if(ret < 0)
		return -1;
	

	return 0;
}

static void ft5x0x_ts_release(struct ft5x02_ts_data *data)
{
	input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
	input_report_key(data->input_dev, KEY_MENU, 0);
	input_report_key(data->input_dev, KEY_SEARCH, 0);
	input_report_key(data->input_dev, KEY_BACK, 0);
	input_sync(data->input_dev);
}

static int ft5x02_read_data(struct ft5x02_ts_data *data )
{
	struct ts_event *event  	= &data->event;

	FTS_BYTE* buf			= FTS_NULL;
	FTS_BYTE read_cmd[2]		= {0};
	FTS_BYTE cmd_len 	   	= 0;
	FTS_BYTE data_buf[26]	= {0}; 
	
	buf = data_buf;
	read_cmd[0] 	        		= I2C_STARTTCH_READ;
	cmd_len 			 		= 1;

	/*get the data info by i2c*/
	read_info(buf);

	event->pressure = 255;

	/*check the pointer*/
	POINTER_CHECK(buf);
	
	/*check packet head: 0xAAAA.*/
	if(buf[1]!= 0xaa || buf[0] != 0xaa)
	{
		return CTPM_ERR_PROTOCOL;
	}
	
	/*check data length*/
	if((buf[2] & 0x3f) != PROTOCOL_LEN)
	{
		return CTPM_ERR_PROTOCOL;
	}		
	/*check the touch point*/
	event->touch_point = buf[3] & 0x0f;
	if(event->touch_point > CTPM_26BYTES_POINTS_MAX)
	{
		return CTPM_ERR_PROTOCOL;
	}	

    	/*get the point info from buf*/
    	switch (event->touch_point) {
		if (MAX_POINT == CTPM_26BYTES_POINTS_MAX)	{
			case 5:
				event->x5	 = (s16)(buf[21] & 0x0F)<<8 | (s16)buf[22];
				event->y5	 = (s16)(buf[23] & 0x0F)<<8 | (s16)buf[24];
				event->touch_ID5 = (s16)(buf[23] & 0xF0)>>4;

			case 4:
				event->x4 	 = (s16)(buf[17] & 0x0F)<<8 | (s16)buf[18];
				event->y4 	 = (s16)(buf[19] & 0x0F)<<8 | (s16)buf[20];
				event->touch_ID4 = (s16)(buf[19] & 0xF0)>>4;

			case 3:
				event->x3 	 = (s16)(buf[13] & 0x0F)<<8 | (s16)buf[14];
				event->y3 	 = (s16)(buf[15] & 0x0F)<<8 | (s16)buf[16];
				event->touch_ID3 = (s16)(buf[16] & 0xF0)>>4;
		}
			case 2:
				event->x2 	 = (s16)(buf[9] & 0x0F)<<8 | (s16)buf[10];
				event->y2 	 = (s16)(buf[11] & 0x0F)<<8 | (s16)buf[12];
		    		event->touch_ID2 = (s16)(buf[11] & 0xF0)>>4;

			case 1:
				event->x1 	 = (s16)(buf[5] & 0x0F)<<8 | (s16)buf[6];
				event->y1 	 = (s16)(buf[7] & 0x0F)<<8 | (s16)buf[8];
				event->touch_ID1 = (s16)(buf[7] & 0xF0)>>4;
				
				/*get the Y axis of down positon*/
				down_position_y  = (buf[5]>>6) == 0 ? event->y1:0; 
            			break;
			default:
				ft5x0x_ts_release(data);
		  		return 1;
	}

    return 0;
}

static void ft5x02_report_value(struct ft5x02_ts_data *data )
{
	struct ts_event *event = &data->event;

	switch(event->touch_point) {
                /*report TRACKING_ID is used here to identify the different point*/
		if (MAX_POINT == 5)	{
			case 5:
				input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, event->touch_ID5);			
				input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
				input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->x5);
				input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->y5);
				input_mt_sync(data->input_dev);
//				printk("===x5 = %d,y5 = %d ====\n",event->x5,event->y5);
			case 4:
				input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, event->touch_ID4);			
				input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
				input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->x4);
				input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->y4);
				input_mt_sync(data->input_dev);
			case 3:
				input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, event->touch_ID3);			
				input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
				input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->x3);
				input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->y3);
				input_mt_sync(data->input_dev);
		}
			case 2:
				input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, event->touch_ID2);			
				input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
				input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->x2);
				input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->y2);
				input_mt_sync(data->input_dev);
			case 1:

			 if(event->y1 < 480){

				input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, event->touch_ID1);			
				input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
				input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->x1);
				input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->y1);
				input_mt_sync(data->input_dev);
		
			}
			/*make sure that the y axis of down position > 480*/

			 else if((event->y1 > 500)&&(down_position_y>480)){ 
                                       
				input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
                                if(event->x1 > 9 && event->x1 < 81){
                                	 input_report_key(data->input_dev, KEY_MENU, 1);  
                          	}else if((event->x1>124) && (event->x1<196)){                             
                                	 input_report_key(data->input_dev, KEY_SEARCH, 1);             
                          	}else if((event->x1>239) && (event->x1<311)){
                            		input_report_key(data->input_dev, KEY_BACK, 1);
                          	}

                      	}

		default:
			break;
	}

	input_sync(data->input_dev);

}	/*end ft5x0x_report_value*/

/****** Function read touch info and report it ******/
static void ft5x02_ts_work_func(struct work_struct *work)
{
	int ret = -1;

	struct ft5x02_ts_data *ft5x0x_ts =
		container_of(work, struct ft5x02_ts_data, work);
	
	ret = ft5x02_read_data(ft5x0x_ts);	
	if (ret == 0) {	
		ft5x02_report_value(ft5x0x_ts);
	}
}

static enum hrtimer_restart ft5x02_ts_timer_func(struct hrtimer *timer)
{
	struct ft5x02_ts_data *ts = container_of(timer, struct ft5x02_ts_data, timer);

	queue_work(ft5x02_wq, &ts->work);

	hrtimer_start(&ts->timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static irqreturn_t ft5x02_ts_irq_handler(int irq, void *dev_id)
{
	struct ft5x02_ts_data *ts = dev_id;

	queue_work(ft5x02_wq, &ts->work);
	return IRQ_HANDLED;
}

int ft5x02_power(int on_off)
{
	struct vreg * vreg_tp;
	int rc;
	vreg_tp= vreg_get(NULL, "gp6"); 
	
	if (IS_ERR(vreg_tp)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
		       		__func__, PTR_ERR(vreg_tp));
		return PTR_ERR(vreg_tp);
	}
	if (on_off) {
		rc = vreg_set_level(vreg_tp, 3000);
		if (rc) {
			printk(KERN_ERR "%s: vreg set level failed (%d)\n",
		      			__func__, rc);
			return -EIO;
		}
		rc = vreg_enable(vreg_tp);
		if (rc) {
			printk(KERN_ERR "%s: vreg enable failed (%d)\n",
			__func__, rc);
			return -EIO;
		}
	} else {
	       rc = vreg_set_level(vreg_tp, 0);
		if (rc) {
			printk(KERN_ERR "%s: vreg set level failed (%d)\n",
		       			__func__, rc);
			return -EIO;
		}
		
		rc=vreg_disable(vreg_tp);
		if (rc) {
			printk(KERN_ERR "%s: vreg disable failed (%d)\n",
		       			__func__, rc);
			return -EIO;
		}
		
	}
	msleep(80);
	return 0;
}

static int ft5x02_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ft5x02_ts_data *ts;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "ft5x02_ts_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
	INIT_WORK(&ts->work, ft5x02_ts_work_func);
	client->irq=MSM_GPIO_TO_INT(FT5X02_INT);
	ts->client = client;
	ft5x02_client = client;
	i2c_set_clientdata(client, ts);
	ts->power = ft5x02_power;
	if (ts->power) {
		ret = ts->power(1);
		if (ret < 0) {
			printk(KERN_ERR "ft5x02_ts_probe power on failed\n");
			goto err_power_failed;
		}
	}

	/* read i2c data from chip */
	ret = read_reg(0x3d);
	printk("ft5x02 id %02x\n", ret);
	
     /****************upgrade tp firmware******************/ 
	 /**	notice: request 300kHZ i2c speed when upgrade  **/
       ret = read_reg(0x3b);
	printk("focaltec's version is %x\n",ret);
	
	if (ret <= 0) {	
		printk(KERN_ERR "ft5x02 read 0x3b failed\n");
             goto err_detect_failed; 		 
	}

	if ((ret != REQUIRED_VERSION) && (ret != 0x07)&&(ret >= 0)) {
		printk("upgrade.........\n");
		ret = fts_ctpm_fw_upgrade_with_i_file();         //upgrade firmware
		if (ret != ERR_OK){
			printk(KERN_ERR "ft5x02 upgrade failed\n");
                  goto err_upgrade_failed;
		}
	}
      /***************upgrade tp firmware end**************/  	

	ret = ft5x02_init_panel(ts); /* will also switch back to page 0x04 */
	if (ret < 0) {
		printk(KERN_ERR "ft5x02_init_panel failed\n");
		goto err_detect_failed;
	}

	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		printk(KERN_ERR "ft5x02_ts_probe: Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}
	ts->input_dev->name = "ft5x02-touchscreen";
	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(BTN_2, ts->input_dev->keybit);
	set_bit(KEY_MENU, ts->input_dev->keybit);
	set_bit(KEY_SEARCH, ts->input_dev->keybit);
	set_bit(KEY_BACK, ts->input_dev->keybit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,0, 320, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,0, 480, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR,0, 255,0, 0);
	input_set_abs_params(ts->input_dev, ABS_HAT0X, 0, 320, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_HAT0Y, 0, 480+96, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, 4, 0, 0);

	/* ts->input_dev->name = ts->keypad_info->name; */
	ret = input_register_device(ts->input_dev);
	if (ret) {
		printk(KERN_ERR "ft5x02_ts_probe: Unable to register %s input device\n", ts->input_dev->name);
		goto err_input_register_device_failed;
	}
	ret = request_irq(MSM_GPIO_TO_INT(19), ft5x02_ts_irq_handler, IRQF_TRIGGER_RISING, client->name, ts);
	if (ret == 0)
		ts->use_irq = 1;
	else
		dev_err(&client->dev, "request_irq failed\n");
	if (!ts->use_irq) {
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = ft5x02_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = ft5x02_ts_early_suspend;
	ts->early_suspend.resume = ft5x02_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	printk(KERN_INFO "ft5x02_ts_probe: Start touchscreen %s in %s mode\n", ts->input_dev->name, ts->use_irq ? "interrupt" : "polling");

	return 0;
err_input_register_device_failed:
	input_free_device(ts->input_dev);

err_input_dev_alloc_failed:
err_detect_failed:
err_power_failed:
err_upgrade_failed:
	kfree(ts);
err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
}

static int ft5x02_ts_remove(struct i2c_client *client)
{
	struct ft5x02_ts_data *ts = i2c_get_clientdata(client);
	unregister_early_suspend(&ts->early_suspend);
	if (ts->use_irq)
		free_irq(client->irq, ts);
	else
		hrtimer_cancel(&ts->timer);
	input_unregister_device(ts->input_dev);
	kfree(ts);
	return 0;
}

static int ft5x02_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;
	struct ft5x02_ts_data *ts = i2c_get_clientdata(client);
	printk("Enter %s\n",__func__);
	disable_irq(client->irq);
	ret = cancel_work_sync(&ts->work);
	if (ret && ts->use_irq) /* if work was pending disable-count is now 2 */
		enable_irq(client->irq);
	cancel_work_sync(&ts->work);
	ret=write_reg(0x3a,Hib_Mode);//set tp power state :Hibernate mode
	if(ret<0)
		printk(KERN_ERR " set tp power state failed\n");

	if (ts->power) {
		ret = ts->power(0);
		if (ret < 0)
			printk(KERN_ERR "ft5x02_ts_resume power off failed\n");
	}	
	return 0;
}

static int ft5x02_ts_resume(struct i2c_client *client)
{
	int ret;


	
	struct ft5x02_ts_data *ts = i2c_get_clientdata(client);
	printk("Enter %s\n",__func__);
	if (ts->power) {
		ret = ts->power(1);
		if (ret < 0)
			printk(KERN_ERR "ft5x02_ts_resume power on failed\n");
	}
	gpio_set_value(FT5x02_WK,1);
	gpio_set_value(FT5x02_WK,0);
	udelay(500);
	gpio_set_value(FT5x02_WK,1);
	
	ret = write_reg(0x3a, Mtr_Mode);//set tp power state:Monitor mode



	
	if(ret < 0)
		return -1;

	if (ts->use_irq)
		enable_irq(client->irq);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ft5x02_ts_early_suspend(struct early_suspend *h)
{
	struct ft5x02_ts_data *ts;
	ts = container_of(h, struct ft5x02_ts_data, early_suspend);
	ft5x02_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void ft5x02_ts_late_resume(struct early_suspend *h)
{
	struct ft5x02_ts_data *ts;
	ts = container_of(h, struct ft5x02_ts_data, early_suspend);
	ft5x02_ts_resume(ts->client);
}
#endif

static const struct i2c_device_id ft5x02_ts_id[] = {
	{FT5X02_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver ft5x02_ts_driver = {
	.probe		= ft5x02_ts_probe,
	.remove		= ft5x02_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= ft5x02_ts_suspend,
	.resume		= ft5x02_ts_resume,
#endif
	.id_table	= ft5x02_ts_id,
	.driver = {
		.name	= FT5X02_I2C_NAME,
	},
};

static int __devinit ft5x02_ts_init(void)
{
	int ret;

	ret = gpio_tlmm_config(GPIO_CFG(FT5X02_INT, 0, GPIO_CFG_INPUT,
				GPIO_CFG_NO_PULL, GPIO_CFG_4MA), GPIO_CFG_ENABLE);
        if (ret) {
                printk(KERN_ERR "%s: gpio_tlmm_config=%d\n",
                                 __func__, ret);
        }

	ret = gpio_tlmm_config(GPIO_CFG(FT5x02_WK, 0, GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL, GPIO_CFG_4MA), GPIO_CFG_ENABLE);
        if (ret) {
                printk(KERN_ERR "%s: gpio_tlmm_config=%d\n",
                                 __func__, ret);
        }

	gpio_set_value(FT5x02_WK,1);
		
	ft5x02_wq = create_singlethread_workqueue("ft5x02_wq");
	if (!ft5x02_wq)
		return -ENOMEM;
	ret = i2c_add_driver(&ft5x02_ts_driver);
	return ret;
}

static void __exit ft5x02_ts_exit(void)
{

	i2c_del_driver(&ft5x02_ts_driver);
	if (ft5x02_wq)
		destroy_workqueue(ft5x02_wq);
}

module_init(ft5x02_ts_init);
module_exit(ft5x02_ts_exit);

MODULE_DESCRIPTION("Ft5x02 Touchscreen Driver");
MODULE_LICENSE("GPL");

