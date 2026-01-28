// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>

struct amlogic_pdd_monitor {
	struct miscdevice miscdev;
	wait_queue_head_t waitq;
	unsigned long status;		/* Internal status bits */
#define _PDDM_DEV_OPEN		0	/* Opened ? */
#define _PDDM_IRQ_TRIGGER	1	/* IRQ triggered ? */
};

static inline struct amlogic_pdd_monitor *pdd_monitor_get_priv(struct file *file)
{
	return container_of(file->private_data, struct amlogic_pdd_monitor, miscdev);
}

static int pdd_monitor_open(struct inode *inode, struct file *file)
{
	struct amlogic_pdd_monitor *priv = pdd_monitor_get_priv(file);

	/* The pdd_monitor is single open! */
	if (test_and_set_bit(_PDDM_DEV_OPEN, &priv->status))
		return -EBUSY;

	return 0;
}

static int pdd_monitor_release(struct inode *inode, struct file *file)
{
	struct amlogic_pdd_monitor *priv = pdd_monitor_get_priv(file);

	clear_bit(_PDDM_DEV_OPEN, &priv->status);

	return 0;
}

static ssize_t pdd_monitor_write(struct file *file, const char __user *buffer,
				 size_t count, loff_t *ppos)
{
	struct amlogic_pdd_monitor *priv = pdd_monitor_get_priv(file);

	clear_bit(_PDDM_IRQ_TRIGGER, &priv->status);
	*ppos = 0;

	return count;
}

static ssize_t pdd_monitor_read(struct file *file, char __user *buffer, size_t count,
				loff_t *ppos)
{
	struct amlogic_pdd_monitor *priv = pdd_monitor_get_priv(file);
	char *value = test_bit(_PDDM_IRQ_TRIGGER, &priv->status) ? "1\n" : "0\n";
	size_t len = strlen(value);
	size_t copy_size = len < count ? len : count;

	if (*ppos)
		return 0;

	if (copy_to_user(buffer, value, copy_size) == 0) {
		*ppos = copy_size;
		return copy_size;
	}

	return  0;
}

static __poll_t pdd_monitor_poll(struct file *file, poll_table *wait)
{
	struct amlogic_pdd_monitor *priv = pdd_monitor_get_priv(file);

	poll_wait(file, &priv->waitq, wait);

	if (test_bit(_PDDM_IRQ_TRIGGER, &priv->status))
		return EPOLLIN | EPOLLRDNORM;

	return 0;
}

static const struct file_operations pdd_monitor_fops = {
	.owner		= THIS_MODULE,
	.open		= pdd_monitor_open,
	.release	= pdd_monitor_release,
	.read		= pdd_monitor_read,
	.write		= pdd_monitor_write,
	.poll		= pdd_monitor_poll,
};

static irqreturn_t amlogic_pdd_monitor_irq(int irq, void *data)
{
	struct device *dev = data;
	struct amlogic_pdd_monitor *priv = dev_get_drvdata(dev);

	set_bit(_PDDM_IRQ_TRIGGER, &priv->status);
	wake_up_interruptible(&priv->waitq);
	dev_warn(dev, "DETECTED DOWN\n");

	return IRQ_HANDLED;
}

static int amlogic_pdd_monitor_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int irq;
	int ret;
	struct amlogic_pdd_monitor *priv;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	init_waitqueue_head(&priv->waitq);
	priv->status = 0;

	dev_set_drvdata(&pdev->dev, priv);

	irq = irq_of_parse_and_map(dev->of_node, 0);
	if (!irq)
		return dev_err_probe(dev, -EINVAL, "Failed to map interrupts\n");
	dev_info(dev, "IRQ number: %d\n", irq);

	ret = devm_request_irq(dev, irq, amlogic_pdd_monitor_irq,
			       IRQF_SHARED, dev_name(dev), dev);
	if (ret)
		return ret;

	priv->miscdev.fops = &pdd_monitor_fops;
	priv->miscdev.minor = MISC_DYNAMIC_MINOR;
	priv->miscdev.name = "pdd-monitor";
	priv->miscdev.parent = dev;
	ret = misc_register(&priv->miscdev);
	if (ret)
		return dev_err_probe(dev, ret, "Unable to register misc device\n");

	return 0;
}

static int amlogic_pdd_monitor_remove(struct platform_device *pdev)
{
	struct amlogic_pdd_monitor *priv = dev_get_drvdata(&pdev->dev);

	misc_deregister(&priv->miscdev);

	return 0;
}

static const struct of_device_id amlogic_pdd_monitor_of_match[] = {
	{ .compatible = "amlogic,pdd-monitor", },
	{ /* sentinel */ },
};

static struct platform_driver amlogic_pdd_monitor_driver = {
	.probe = amlogic_pdd_monitor_probe,
	.remove = amlogic_pdd_monitor_remove,
	.driver = {
		.name = "amlogic-pdd-monitor",
		.of_match_table = amlogic_pdd_monitor_of_match,
	},
};

int __init amlogic_pdd_monitor_driver_init(void)
{
	return platform_driver_register(&amlogic_pdd_monitor_driver);
}

void __exit amlogic_pdd_monitor_driver_exit(void)
{
	platform_driver_unregister(&amlogic_pdd_monitor_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Huqiang Qin <huqiang.qin@amlogic.com>");
MODULE_DESCRIPTION("Amlogic power down detection monitor driver");
