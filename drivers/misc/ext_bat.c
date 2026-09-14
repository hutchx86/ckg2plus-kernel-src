/*
 * Driver for external battery on GPIO config.
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
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>
#include <linux/pinctrl/consumer.h>
#include <linux/syscore_ops.h>

#define GPIO_BACKUP_EN 103
#define GPIO_BAT_OFF   104


struct gpio_bat_platform_data {
        unsigned int data;
};

struct gpio_bat_drvdata {
	const struct gpio_bat_platform_data *pdata;
	struct pinctrl *bat_pinctrl;
	struct mutex disable_lock;
};

static struct device *global_dev;
static struct syscore_ops gpio_bat_syscore_pm_ops;

static void gpio_bat_syscore_resume(void);

/**
 * gpio_bat_pinctrl_configure() - set state of pinctrl
 */


static int gpio_bat_pinctrl_configure(struct gpio_bat_drvdata *ddata,
							bool active)
{
	struct pinctrl_state *set_state;
	int retval;

        pr_debug("gpio_bat_pinctrl_configure\n");

        if(active) {
            set_state =
                    pinctrl_lookup_state(ddata->bat_pinctrl,
                                            "default");
        } else {
            set_state =
                    pinctrl_lookup_state(ddata->bat_pinctrl,
                                            "off");
        }
        if (IS_ERR(set_state)) {
                pr_err("pinctrl state state err\n");
                return PTR_ERR(set_state);
        }

	retval = pinctrl_select_state(ddata->bat_pinctrl, set_state);
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
static struct gpio_bat_platform_data *
gpio_bat_get_devtree_pdata(struct device *dev)
{
	struct device_node *node, *pp;
	struct gpio_bat_platform_data *pdata;
	int error;
	size_t size;

	node = dev->of_node;
	if (!node)
		return ERR_PTR(-ENODEV);

	size = sizeof(struct gpio_bat_platform_data);
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
		pr_debug("gpio_bat_get_devtree_pdata %d flag %d\n", gpio, flags);

		if (gpio < 0) {
			error = gpio;
			if (error != -EPROBE_DEFER)
				dev_err(dev,
					"Failed to get gpio flags, error: %d\n",
					error);
			return ERR_PTR(error);
		} else if(GPIO_BAT_OFF == gpio || GPIO_BACKUP_EN == gpio) {
			pr_debug("pinctrl_gpio_direction_output %d\n", gpio);
                        gpio_direction_output(gpio, 1);
		}

	}

	return pdata;
}

static const struct of_device_id gpio_bat_of_match[] = {
	{ .compatible = "gpio-bat-ext", },
	{ },
};
MODULE_DEVICE_TABLE(of, gpio_bat_of_match);

#else

static inline struct gpio_bat_platform_data *
gpio_bat_get_devtree_pdata(struct device *dev)
{
	return ERR_PTR(-ENODEV);
}

#endif

static int gpio_bat_remove(struct platform_device *pdev)
{
//	sysfs_remove_group(&pdev->dev.kobj, &gpio_bat_attr_group);
	unregister_syscore_ops(&gpio_bat_syscore_pm_ops);

	device_init_wakeup(&pdev->dev, 0);

	return 0;
}

static ssize_t store_bat_off_pin(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{

        char *pvalue = NULL;
        unsigned int value = 0;
        pr_info("[store_bat_off_pin] \n");
        if(buf != NULL && size != 0)
        {
                pr_debug("[store_bat_off_pin] buf is %s and size is %zu\n", buf, size);
                value = simple_strtoul(buf,&pvalue,16);

                pr_debug("[store_bat_off_pin] reg_address = %u !\n", value);
                if(value) {
                    gpio_direction_output(GPIO_BACKUP_EN, 0);
                } else {
                    gpio_direction_output(GPIO_BACKUP_EN, 1);
                }
                msleep(20);
		gpio_direction_output(GPIO_BAT_OFF, value);
        }
        return size;
}

static DEVICE_ATTR(msm_bat_off_pin, 0660, NULL, store_bat_off_pin);


/*
 * ATTRIBUTES:
 *
 */

static struct attribute *gpio_bat_attrs[] = {
        &dev_attr_msm_bat_off_pin.attr,
        NULL,
};

static struct attribute_group gpio_bat_attr_group = {
        .attrs = gpio_bat_attrs,
};


static int gpio_bat_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct gpio_bat_platform_data *pdata = dev_get_platdata(dev);
	struct gpio_bat_drvdata *ddata;
	size_t size;
	int error;
	int wakeup = 0;
	struct pinctrl_state *set_state;

	pr_info("gpio_bat_probe\n");

	if (!pdata) {
		pr_debug("gpio_bat_get_devtree_pdata\n");
		pdata = gpio_bat_get_devtree_pdata(dev);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
	}

	size = sizeof(struct gpio_bat_drvdata);
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
	ddata->bat_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ddata->bat_pinctrl)) {
		if (PTR_ERR(ddata->bat_pinctrl) == -EPROBE_DEFER)
			return -EPROBE_DEFER;

		pr_debug("Target does not use pinctrl\n");
		ddata->bat_pinctrl = NULL;
	}

        error = sysfs_create_group(&pdev->dev.kobj, &gpio_bat_attr_group);
        if (error) {
                dev_err(dev, "Unable to export , error: %d\n",
                        error);
                goto err_create_sysfs;
        }

	if (ddata->bat_pinctrl) {
		error = gpio_bat_pinctrl_configure(ddata, true);
		if (error) {
			dev_err(dev, "cannot set ts pinctrl active state\n");
			return error;
		}
	}

	device_init_wakeup(&pdev->dev, wakeup);

	gpio_bat_syscore_pm_ops.resume = gpio_bat_syscore_resume;

	register_syscore_ops(&gpio_bat_syscore_pm_ops);

	return 0;

//	sysfs_remove_group(&pdev->dev.kobj, &gpio_bat_attr_group);
err_create_sysfs:
	if (ddata->bat_pinctrl) {
		set_state =
		pinctrl_lookup_state(ddata->bat_pinctrl,
						"default");
		if (IS_ERR(set_state))
			dev_err(dev, "cannot get pinctrl default state\n");
		else
			pinctrl_select_state(ddata->bat_pinctrl, set_state);
	}

	return error;
}

static void gpio_bat_syscore_resume(void){}

static int gpio_bat_suspend(struct device *dev)
{
	return 0;
}

static int gpio_bat_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(gpio_bat_pm_ops, gpio_bat_suspend, gpio_bat_resume);

static struct platform_driver gpio_bat_device_driver = {
	.probe		= gpio_bat_probe,
	.remove		= gpio_bat_remove,
	.driver		= {
		.name	= "gpio-bat-ext",
		.owner	= THIS_MODULE,
		.pm	= &gpio_bat_pm_ops,
		.of_match_table = of_match_ptr(gpio_bat_of_match),
	}
};

static int __init gpio_bat_init(void)
{
	return platform_driver_register(&gpio_bat_device_driver);
}

static void __exit gpio_bat_exit(void)
{
	platform_driver_unregister(&gpio_bat_device_driver);
}

late_initcall(gpio_bat_init);
module_exit(gpio_bat_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("UBNT");
MODULE_DESCRIPTION("Ext Bat driver for GPIOs");
MODULE_ALIAS("platform:gpio-bat-ext");
