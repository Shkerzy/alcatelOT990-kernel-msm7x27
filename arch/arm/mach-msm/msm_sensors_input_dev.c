#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>

static struct input_dev *sensors_input_dev;
DEFINE_MUTEX(__msm_sensors_lock);

int msm_sensors_input_report_value(int event, int code, int value)
{
	switch (event) {

	case EV_ABS:
		input_report_abs(sensors_input_dev, code, value);
		break;

	case EV_REL:
		input_report_rel(sensors_input_dev, code, value);
		break;

	case EV_MSC:
		input_event(sensors_input_dev, EV_MSC, code, value);
		break;

	default:
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(msm_sensors_input_report_value);

void msm_sensors_input_sync(void)
{
	input_sync(sensors_input_dev);
}
EXPORT_SYMBOL(msm_sensors_input_sync);

int msm_sensors_input_set_params(int event, int axis, int min, int max, int fuzz, int flat)
{
	switch (event) {

	case EV_ABS:
		input_set_abs_params(sensors_input_dev, axis, min, max, fuzz, flat);
		break;

	default:
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(msm_sensors_input_set_params);

static int __init msm_sensors_input_dev_init(void)
{
	int err = 0;

	sensors_input_dev = input_allocate_device();
	if (NULL == sensors_input_dev) {
		printk("allocate sensors input device error\n");
		return -ENOMEM;
	}

	set_bit(EV_ABS, sensors_input_dev->evbit);
	set_bit(EV_REL, sensors_input_dev->evbit);
	set_bit(EV_MSC, sensors_input_dev->evbit);

	__set_bit(REL_X, sensors_input_dev->relbit);
	__set_bit(REL_Y, sensors_input_dev->relbit);
	__set_bit(REL_Z, sensors_input_dev->relbit);

	set_bit(MSC_RAW, sensors_input_dev->mscbit);
	set_bit(MSC_GESTURE, sensors_input_dev->mscbit);

	sensors_input_dev->name = "sensors_input";
	err = input_register_device(sensors_input_dev);

	if (err) {
		input_free_device(sensors_input_dev);
		printk("register sensors input device error!!!\n");
		return err;
	}

	return 0;
}
subsys_initcall_sync(msm_sensors_input_dev_init);

static void __exit msm_sensors_input_dev_exit(void)
{
	input_unregister_device(sensors_input_dev);
}
module_exit(msm_sensors_input_dev_exit);

MODULE_AUTHOR("nan ge <nan.ge@tct-sh.com>");
MODULE_DESCRIPTION("input device file of all sensors");
MODULE_LICENSE("GPL");
