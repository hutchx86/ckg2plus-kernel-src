/*
 * Driver for OTG USB GPIO config.
 *
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio_keys.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>
#include <linux/pinctrl/consumer.h>
#include <linux/syscore_ops.h>

#define GPIO_OTG_HDD 101
#define GPIO_OTG_USB   102

#define USE_PIN_STATE 1


struct otg_pins_platform_data {
        unsigned int data;
};

struct otg_pins_drvdata {
	const struct otg_pins_platform_data *pdata;
	struct pinctrl *key_pinctrl;
	struct mutex disable_lock;
};

static struct device *global_dev;
static struct syscore_ops otg_pins_syscore_pm_ops;

static void otg_pins_syscore_resume(void);

/**
 * otg_pins_pinctrl_configure()
 * configure state of otg pins
 */


static int otg_pins_pinctrl_configure(struct otg_pins_drvdata *ddata,
							bool active)
{
	struct pinctrl_state *set_state;
	int retval;

        pr_debug("otg_pins_pinctrl_configure\n");

	if(active) {
            set_state =
                    pinctrl_lookup_state(ddata->key_pinctrl,
                                            "on");
        } else {
            set_state =
                    pinctrl_lookup_state(ddata->key_pinctrl,
                                            "default");
        }
        if (IS_ERR(set_state)) {
                pr_err("pinctrl state state err\n");
                return PTR_ERR(set_state);
        }

	retval = pinctrl_select_state(ddata->key_pinctrl, set_state);
	if (retval) {
		return retval;
	}

	return 0;
}

/*
 * Handlers for alternative sources of platform_data
 */

#ifdef CONFIG_OF
/*
 * Translate OpenFirmware node properties into platform_data
 */
static struct otg_pins_platform_data *
otg_pins_get_devtree_pdata(struct device *dev)
{
	struct device_node *node, *pp;
	struct otg_pins_platform_data *pdata;
	int error;
	size_t size;

	node = dev->of_node;
	if (!node)
		return ERR_PTR(-ENODEV);

	size = sizeof(struct otg_pins_platform_data);
	pdata = devm_kzalloc(dev,
			     size,
			     GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	for_each_child_of_node(node, pp) {
		int gpio;
		enum of_gpio_flags flags;

		if (!of_find_property(pp, "gpios", NULL)) {
			dev_warn(dev, "Found button without gpios\n");
			continue;
		}

		gpio = of_get_gpio_flags(pp, 0, &flags);
		pr_debug("otg_pins_get_devtree_pdata %d flag %d\n", gpio, flags);

		if (gpio < 0) {
			error = gpio;
			if (error != -EPROBE_DEFER)
				dev_err(dev,
					"Failed to get gpio flags, error: %d\n",
					error);
			return ERR_PTR(error);
		} else if(GPIO_OTG_USB == gpio || GPIO_OTG_HDD == gpio) {
			pr_debug("pinctrl_gpio_direction_output %d\n", gpio);
                        gpio_direction_output(gpio, 0);
		}

	}

	return pdata;
}

static const struct of_device_id otg_pins_of_match[] = {
	{ .compatible = "gpio-otg-pins", },
	{ },
};
MODULE_DEVICE_TABLE(of, otg_pins_of_match);

#else

static inline struct otg_pins_platform_data *
otg_pins_get_devtree_pdata(struct device *dev)
{
	return ERR_PTR(-ENODEV);
}

#endif

static int otg_pins_remove(struct platform_device *pdev)
{
//	sysfs_remove_group(&pdev->dev.kobj, &otg_pins_attr_group);
	unregister_syscore_ops(&otg_pins_syscore_pm_ops);

	device_init_wakeup(&pdev->dev, 0);

	return 0;
}


static ssize_t store_otg_usb_enable(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
#if USE_PIN_STATE

        struct pinctrl_state *set_state;
        int retval = 0;
        struct platform_device *pdev = to_platform_device(dev);
        struct otg_pins_drvdata *ddata = platform_get_drvdata(pdev);

        char state[16];

        if(buf == NULL || size == 0)
            return size;

        memset(state, 0x0, sizeof(state));
        memcpy(state, buf, (size - 1));

        pr_info("store_otg_usb_enable: %s, len %d\n", state, strlen(state));

        if (ddata->key_pinctrl) {

            set_state = pinctrl_lookup_state(ddata->key_pinctrl,
                                                state);
        }
        if (IS_ERR(set_state)) {
                pr_err("store_otg_usb_enable pinctrl state state err\n");
                return size;
        }

        retval = pinctrl_select_state(ddata->key_pinctrl, set_state);

        pr_debug("pinctrl_select_state: %d\n", retval);

        return size;

#else
        char *pvalue = NULL;
        unsigned int value = 1;

        pr_info("[store_otg_usb_enable] \n");

        if(buf != NULL && size != 0)
        {
                pr_info("[store_otg_usb_enable] buf is %s and size is %d \n", buf, size);
		gpio_direction_output(GPIO_OTG_USB, value);
                gpio_direction_output(GPIO_OTG_HDD, value);
        }
        return size;
#endif
}

static DEVICE_ATTR(msm_otg_usb_enable, 0660, NULL, store_otg_usb_enable);


/*
 * ATTRIBUTES:
 *
 */

static struct attribute *otg_pins_attrs[] = {
        &dev_attr_msm_otg_usb_enable.attr,
        NULL,
};

static struct attribute_group otg_pins_attr_group = {
        .attrs = otg_pins_attrs,
};


static int otg_pins_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct otg_pins_platform_data *pdata = dev_get_platdata(dev);
	struct otg_pins_drvdata *ddata;
	size_t size;
	int i, error;
	int wakeup = 0;
	struct pinctrl_state *set_state;

	pr_debug("otg_pins_probe\n");

	if (!pdata) {
		pr_debug("otg_pins_get_devtree_pdata\n");
		pdata = otg_pins_get_devtree_pdata(dev);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
	}

	size = sizeof(struct otg_pins_drvdata);
	ddata = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!ddata) {
		dev_err(dev, "failed to allocate state\n");
		return -ENOMEM;
	}

	global_dev = dev;
	ddata->pdata = pdata;
	mutex_init(&ddata->disable_lock);

	platform_set_drvdata(pdev, ddata);

	/* Get pinctrl if target uses pinctrl */
	ddata->key_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ddata->key_pinctrl)) {
		if (PTR_ERR(ddata->key_pinctrl) == -EPROBE_DEFER)
			return -EPROBE_DEFER;

		pr_debug("Target does not use pinctrl\n");
		ddata->key_pinctrl = NULL;
	}

        error = sysfs_create_group(&pdev->dev.kobj, &otg_pins_attr_group);
        if (error) {
                dev_err(dev, "Unable to export keys/switches, error: %d\n",
                        error);
                goto err_create_sysfs;
        }

	if (ddata->key_pinctrl) {
		error = otg_pins_pinctrl_configure(ddata, false);
		if (error) {
			dev_err(dev, "cannot set ts pinctrl active state\n");
			return error;
		}
	}

	device_init_wakeup(&pdev->dev, wakeup);

	otg_pins_syscore_pm_ops.resume = otg_pins_syscore_resume;

	register_syscore_ops(&otg_pins_syscore_pm_ops);

	return 0;

err_remove_group:
//	sysfs_remove_group(&pdev->dev.kobj, &otg_pins_attr_group);
err_create_sysfs:
err_setup_key:
	if (ddata->key_pinctrl) {
		set_state =
		pinctrl_lookup_state(ddata->key_pinctrl,
						"suspend");
		if (IS_ERR(set_state))
			dev_err(dev, "cannot get pinctrl sleep state\n");
		else
			pinctrl_select_state(ddata->key_pinctrl, set_state);
	}

	return error;
}

static void otg_pins_syscore_resume(void){}

static int otg_pins_suspend(struct device *dev)
{
	return 0;
}

static int otg_pins_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(otg_pins_pm_ops, otg_pins_suspend, otg_pins_resume);

static struct platform_driver otg_pins_device_driver = {
	.probe		= otg_pins_probe,
	.remove		= otg_pins_remove,
	.driver		= {
		.name	= "gpio-otg-pins",
		.owner	= THIS_MODULE,
		.pm	= &otg_pins_pm_ops,
		.of_match_table = of_match_ptr(otg_pins_of_match),
	}
};

static int __init otg_pins_init(void)
{
	return platform_driver_register(&otg_pins_device_driver);
}

static void __exit otg_pins_exit(void)
{
	platform_driver_unregister(&otg_pins_device_driver);
}

late_initcall(otg_pins_init);
module_exit(otg_pins_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("UBNT");
MODULE_DESCRIPTION("OTG USB driver for GPIOs");
MODULE_ALIAS("platform:gpio-otg-pins");
