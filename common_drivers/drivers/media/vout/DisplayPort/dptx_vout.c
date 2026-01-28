// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/compat.h>
#include <linux/workqueue.h>
#include <linux/mm.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif
#include <linux/amlogic/pm.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/DisplayPort/DPTX.h>
#include <linux/amlogic/media/vout/DisplayPort/DPTX_export.h>
#include <linux/amlogic/media/vout/DisplayPort/DPTX_notify.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/gki_module.h>
#include "dptx_common.h"
#include "dptx_reg_op.h"

#define DPTX_CDEV_NAME  "DisplayPort-TX"

unsigned char dptx_print_level = LOG_I;

static unsigned char dptx_global_init_flag;
static struct dptx_drv_s *dptx_driver[DPTX_MAX_DRV];

struct dptx_cdev_s {
	dev_t           devno;
	struct class    *class;
};

static struct dptx_cdev_s *dptx_cdev;

/* ! FOLLOW UBOOT
 *bit[7:0]: link rate
 *bit[9:8]: lane count: 0=disabled, 1=1lane, 2=2lane, 3=4lane
 *bit[17:10]: vmode_sel_idx, 0xff for non-edid or invalid
 *bit[21:18]: vmode_cfmt_sel, 0xf for invalid
 *******
 *bit[31:30]: DPTX debug_level
 */

struct dptx_boot_ctrl_s dptx_uboot_configs[DPTX_MAX_DRV] = {
	{
		.link_rate = 0,
		.lane_count = 0,
		.vmode_sel_idx = 0,
		.vmode_cfmt_sel = 0,
		.tx_prepared = 0,
		.disp_on = 0,
		.uboot_edid_crc = 0,
	}, {
		.link_rate = 0,
		.lane_count = 0,
		.vmode_sel_idx = 0,
		.vmode_cfmt_sel = 0,
		.tx_prepared = 0,
		.disp_on = 0,
		.uboot_edid_crc = 0,
	},
};

static struct dptx_drv_s *dptx_driver_add(u8 index)
{
	struct dptx_drv_s *dptx = NULL;

	if (index >= DPTX_MAX_DRV)
		return NULL;

	dptx = kzalloc(sizeof(*dptx), GFP_KERNEL);
	if (!dptx)
		return NULL;
	memset(dptx, 0, sizeof(struct dptx_drv_s));

	dptx->idx = index;

	return dptx;
}

struct dptx_drv_s *aml_get_dptx_driver(u8 index)
{
	if (index >= DPTX_MAX_DRV)
		return NULL;

	return dptx_driver[index];
}
EXPORT_SYMBOL(aml_get_dptx_driver);

static inline void dptx_vsync_handler(struct dptx_drv_s *dptx)
{
	//unsigned long long local_time[2];
	unsigned long flags = 0;
	//unsigned int temp;

	if (!dptx)
		return;

	spin_lock_irqsave(&dptx->isr_lock, flags);

	spin_unlock_irqrestore(&dptx->isr_lock, flags);
}

static irqreturn_t dptx_vsync1_isr(int irq, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!dptx)
		return IRQ_HANDLED;

	if (!(dptx->status & DPTX_STA_DRV_READY))
		return IRQ_HANDLED;

	if (dptx->viu_sel == 1) {
		dptx_vsync_handler(dptx);
		if (dptx->vsync_cnt++ >= 50000)
			dptx->vsync_cnt = 0;
	}
	return IRQ_HANDLED;
}

static irqreturn_t dptx_vsync2_isr(int irq, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!dptx)
		return IRQ_HANDLED;

	if (!(dptx->status & DPTX_STA_DRV_READY))
		return IRQ_HANDLED;

	if (dptx->viu_sel == 2) {
		dptx_vsync_handler(dptx);
		if (dptx->vsync_cnt++ >= 50000)
			dptx->vsync_cnt = 0;
	}
	return IRQ_HANDLED;
}

static irqreturn_t dptx_vsync3_isr(int irq, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!dptx)
		return IRQ_HANDLED;

	if (!(dptx->status & DPTX_STA_DRV_READY))
		return IRQ_HANDLED;

	if (dptx->viu_sel == 3) {
		dptx_vsync_handler(dptx);
		if (dptx->vsync_cnt++ >= 50000)
			dptx->vsync_cnt = 0;
	}
	return IRQ_HANDLED;
}

static irqreturn_t dptx_HPD_isr(int irq, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;
	u8 curr_HPD_level, curr_irq;

	if (!dptx)
		return IRQ_HANDLED;

	DPTXPR(dptx->idx, LOG_V, "HPD[%d] triggerd. sta=0x%02x", irq, dptx->status);

	if (!(dptx->status & DPTX_STA_HPD_TRI_EN))
		return IRQ_HANDLED;

	curr_HPD_level = dptx_if_get_hpd_level(dptx);
	curr_irq = dptx_if_get_hpd_irq(dptx);

	DPTXPR(dptx->idx, LOG_V, "HPD[%d] triggerd. ignore=%d level=%u irq=0x%x", irq,
		dptx->status & DPTX_STA_HPD_TRI_EN ? 0 : 1, curr_HPD_level, curr_irq);

	if ((dptx->status & DPTX_STA_HPD_TRI_EN) && curr_irq == 0x2 &&
		((dptx->status & DPTX_STA_HPD_HIGH) ^ (curr_HPD_level << 3))) {
		return IRQ_WAKE_THREAD;
	}

	return IRQ_HANDLED;
}

static irqreturn_t dptx_HPD_post_handler(int irq, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	DPTXPR(dptx->idx, LOG_I, "%s", __func__);

	dptx_notifier_call_chain(DPTX_EVENT_HPD_CHECK, dptx);

	if (dptx->status & DPTX_STA_HPD_HIGH)
		dptx_notifier_call_chain(DPTX_EVENT_PLUG_IN, dptx);
	else
		dptx_notifier_call_chain(DPTX_EVENT_PLUG_OUT, dptx);

	// DPTXPR(dptx->idx, LOG_I, "%s done\n", __func__);
	return IRQ_HANDLED;
}

/* **************************************** */
static int dptx_io_open(struct inode *inode, struct file *file)
{
	struct dptx_drv_s *dptx;

	dptx = container_of(inode->i_cdev, struct dptx_drv_s, cdev);
	file->private_data = dptx;

	DPTXPR(dptx->idx, LOG_I, "%s", __func__);

	return 0;
}

static int dptx_io_release(struct inode *inode, struct file *file)
{
	struct dptx_drv_s *dptx;

	if (!file->private_data)
		return 0;

	dptx = (struct dptx_drv_s *)file->private_data;
	DPTXPR(dptx->idx, LOG_I, "%s", __func__);
	file->private_data = NULL;
	return 0;
}

static long dptx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	// void __user *argp = (void __user *)arg;
	int mcd_nr = -1;
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)file->private_data;

	if (!dptx)
		return -EFAULT;

	mcd_nr = _IOC_NR(cmd);
	DPTXPR(dptx->idx, LOG_I, "%s: dir=0x%x, nr=0x%x\n", __func__, _IOC_DIR(cmd), mcd_nr);

	return 0;
}

static const struct file_operations dptx_fops = {
	.owner          = THIS_MODULE,
	.open           = dptx_io_open,
	.release        = dptx_io_release,
	.unlocked_ioctl = dptx_ioctl,
//#ifdef CONFIG_COMPAT
//	.compat_ioctl   = dptx_compat_ioctl,
//#endif
};

static int dptx_cdev_add(struct dptx_drv_s *dptx, struct device *parent)
{
	dev_t devno;
	int ret = 0;

	if (!dptx_cdev) {
		ret = 1;
		goto lcd_cdev_add_failed1;
	}

	devno = MKDEV(MAJOR(dptx_cdev->devno), dptx->idx);

	cdev_init(&dptx->cdev, &dptx_fops);
	dptx->cdev.owner = THIS_MODULE;
	ret = cdev_add(&dptx->cdev, devno, 1);
	if (ret) {
		ret = 2;
		goto lcd_cdev_add_failed0;
	}

	dptx->dev = device_create(dptx_cdev->class, parent, devno, NULL, "dptx%d", dptx->idx);
	if (IS_ERR_OR_NULL(dptx->dev)) {
		ret = 3;
		goto lcd_cdev_add_failed0;
	}

	dev_set_drvdata(dptx->dev, dptx);
	dptx->dev->of_node = parent->of_node;

	DPTXPR(dptx->idx, LOG_I, "%s OK", __func__);
	return 0;

lcd_cdev_add_failed0:
	cdev_del(&dptx->cdev);
lcd_cdev_add_failed1:
	DPTXPR(dptx->idx, LOG_E, "%s: failed: %d", __func__, ret);
	return -1;
}

static void dptx_cdev_remove(struct dptx_drv_s *dptx)
{
	dev_t devno;

	if (!dptx_cdev || !dptx)
		return;

	devno = MKDEV(MAJOR(dptx_cdev->devno), dptx->idx);
	device_destroy(dptx_cdev->class, devno);
	cdev_del(&dptx->cdev);
}

static int dptx_global_init_once(struct platform_device *pdev)
{
	int ret;

	if (dptx_global_init_flag) {
		dptx_global_init_flag++;
		return 0;
	}
	dptx_global_init_flag++;

	dptx_notifier_init();

	dptx_cdev = kzalloc(sizeof(*dptx_cdev), GFP_KERNEL);
	if (!dptx_cdev)
		return -1;

	ret = alloc_chrdev_region(&dptx_cdev->devno, 0, DPTX_MAX_DRV, DPTX_CDEV_NAME);
	if (ret) {
		ret = 1;
		goto lcd_cdev_init_once_err;
	}

	dptx_cdev->class = class_create(THIS_MODULE, "DisplayPortTX");
	if (IS_ERR_OR_NULL(dptx_cdev->class)) {
		ret = 2;
		goto lcd_cdev_init_once_err_1;
	}

	return 0;

lcd_cdev_init_once_err_1:
	unregister_chrdev_region(dptx_cdev->devno, DPTX_MAX_DRV);
lcd_cdev_init_once_err:
	kfree(dptx_cdev);
	dptx_cdev = NULL;
	DPTXPR(0, LOG_E, "%s: failed: %d\n", __func__, ret);
	return -1;
}

static void dptx_global_remove_once(void)
{
	if (dptx_global_init_flag > 1) {
		dptx_global_init_flag--;
		return;
	}
	dptx_global_init_flag--;

	dptx_notifier_remove();

	if (!dptx_cdev)
		return;

	class_destroy(dptx_cdev->class);
	unregister_chrdev_region(dptx_cdev->devno, DPTX_MAX_DRV);
	kfree(dptx_cdev);
	dptx_cdev = NULL;
}

/* ************************************************************* */
static int dptx_irq_init(struct dptx_drv_s *dptx)
{
	//init_completion(&pdrv->vsync_done);
	if (dptx->res_vsync_irq[0]) {
		snprintf(dptx->vsync_name[0], 15, "dptx%d_vsync", dptx->idx);
		if (request_irq(dptx->res_vsync_irq[0]->start, dptx_vsync1_isr, IRQF_SHARED,
				dptx->vsync_name[0], (void *)dptx)) {
			DPTXPR(dptx->idx, LOG_E, "can't request %s", dptx->vsync_name[0]);
		} else {
			DPTXPR(dptx->idx, LOG_I, "%s requested", dptx->vsync_name[0]);
		}
	}

	if (dptx->res_vsync_irq[1]) {
		snprintf(dptx->vsync_name[1], 15, "dptx%d_vsync2", dptx->idx);
		if (request_irq(dptx->res_vsync_irq[1]->start, dptx_vsync2_isr, IRQF_SHARED,
				dptx->vsync_name[1], (void *)dptx)) {
			DPTXPR(dptx->idx, LOG_E, "can't request %s", dptx->vsync_name[1]);
		} else {
			DPTXPR(dptx->idx, LOG_I, "%s requested", dptx->vsync_name[1]);
		}
	}

	if (dptx->res_vsync_irq[2]) {
		snprintf(dptx->vsync_name[2], 15, "dptx%d_vsync3", dptx->idx);
		if (request_irq(dptx->res_vsync_irq[2]->start, dptx_vsync3_isr, IRQF_SHARED,
				dptx->vsync_name[2], (void *)dptx)) {
			DPTXPR(dptx->idx, LOG_E, "can't request %s", dptx->vsync_name[2]);
		} else {
			DPTXPR(dptx->idx, LOG_I, "%s requested", dptx->vsync_name[2]);
		}
	}

	if (dptx->res_HPD_irq) {
		snprintf(dptx->HPD_name, 12, "DPTX%d-HPD", dptx->idx);
		if (request_threaded_irq(dptx->res_HPD_irq->start,
				dptx_HPD_isr, dptx_HPD_post_handler, IRQF_SHARED,
				dptx->HPD_name, (void *)dptx)) {
			DPTXPR(dptx->idx, LOG_E, "can't request %s", dptx->HPD_name);
		} else {
			DPTXPR(dptx->idx, LOG_I, "%s requested", dptx->HPD_name);
		}
	}

	///* add timer to monitor hpll frequency */
	//timer_setup(&pdrv->vs_none_timer, &lcd_vsync_none_timer_handler, 0);
	///* pdrv->vs_none_timer.data = NULL; */
	//pdrv->vs_none_timer.expires = jiffies + LCD_VSYNC_NONE_INTERVAL;
	///*add_timer(&pdrv->vs_none_timer);*/
	///*LCDPR("add vs_none_timer handler\n"); */

	return 0;
}

static void dptx_irq_serve_remove(struct dptx_drv_s *dptx)
{
	if (dptx->res_vsync_irq[0])
		free_irq(dptx->res_vsync_irq[0]->start, (void *)dptx);
	if (dptx->res_vsync_irq[1])
		free_irq(dptx->res_vsync_irq[1]->start, (void *)dptx);
	if (dptx->res_vsync_irq[2])
		free_irq(dptx->res_vsync_irq[2]->start, (void *)dptx);
	if (dptx->res_HPD_irq)
		free_irq(dptx->res_HPD_irq->start, (void *)dptx);

	//if (pdrv->vsync_none_timer_flag) {
	//	del_timer_sync(&pdrv->vs_none_timer);
	//	pdrv->vsync_none_timer_flag = 0;
	//}
}

static int dptx_config_remove(struct dptx_drv_s *dptx)
{
	dptx_vout_server_deinit(dptx);

	dptx_debug_remove(dptx);

	return 0;
}

static void dptx_bootup_config_init(struct dptx_drv_s *dptx)
{
	unsigned int link_rate = 0;

	dptx->viu_sel = 0;

	dptx->status |= dptx_uboot_configs[dptx->idx].tx_prepared ? DPTX_STA_DRV_READY : 0;

	if (dptx_uboot_configs[dptx->idx].lane_count) {
		dptx->status |= DPTX_STA_LINK_ON;
		dptx->status |= dptx_uboot_configs[dptx->idx].disp_on ? DPTX_STA_DISP_ON : 0;
		dptx->link_cfg.lane_count = dptx_uboot_configs[dptx->idx].lane_count;
		if (dptx->link_cfg.lane_count == 3)
			dptx->link_cfg.lane_count = 4;
		dptx->uboot_edid_crc = dptx_uboot_configs[dptx->idx].uboot_edid_crc;

		dptx->link_cfg.link_rate = dptx_uboot_configs[dptx->idx].link_rate;
		link_rate = dptx->link_cfg.link_rate;
		link_rate *= 27;
		DPTXPR(dptx->idx, LOG_I, "uboot: %u lane, %u.%u GHz, crc:0x%02x",
			dptx->link_cfg.lane_count, link_rate / 100, link_rate % 100,
			dptx->uboot_edid_crc);
	} else {
		DPTXPR(dptx->idx, LOG_I, "uboot: %s",
			dptx->status & DPTX_STA_DRV_READY ? "disconnected" : "not inited");
	}
}

void dptx_pinmux_set(struct dptx_drv_s *dptx, u8 status)
{
	struct pinctrl *pin_sel;
	//unsigned int index;

	DPTXPR(dptx->idx, LOG_I, "%s: %u", __func__, status);

	pin_sel = devm_pinctrl_get_select(dptx->dev, status ? "DPTX-ON" : "DPTX-OFF");
	if (IS_ERR(pin_sel))
		DPTXPR(dptx->idx, LOG_E, "%s %u error", __func__, status);
	else
		DPTXPR(dptx->idx, LOG_I, "%s %u ok (0x%px)", __func__, status, pin_sel);
}

int dptx_config_load_from_dts(struct dptx_drv_s *dptx)
{
	const struct device_node *np;
	//const char *str = "none";
	unsigned short val = 0, para[30];
	unsigned char i;
	int ret = 0;

	if (!dptx->dev->of_node) {
		DPTXPR(dptx->idx, LOG_E, "dev of_node is null");
		return -1;
	}
	np = dptx->dev->of_node;

	ret = of_property_read_u16(np, "HPD-ignore", &val);
	if (!ret) {
		dptx->hpd_ignore = (unsigned char)val;
		DPTXPR(dptx->idx, LOG_I, "HPD ignore: %ums", dptx->hpd_ignore);
	}

	//dptx->PWR_gpio = devm_gpiod_get(dptx->dev, "dptx-gpio-PWR", GPIOD_OUT_HIGH);
	//if (IS_ERR(dptx->PWR_gpio)) {
	//	DPTXPR(dptx->idx, LOG_E, "regist PWR gpio: %p, err: %d",
	//	       dptx->PWR_gpio, IS_ERR(dptx->PWR_gpio));
	//}
	//dptx->CFG1_gpio = devm_gpiod_get(dptx->dev, "dptx-gpio-CFG1", GPIOD_IN);
	//if (IS_ERR(dptx->CFG1_gpio)) {
	//	DPTXPR(dptx->idx, LOG_E, "regist CFG1 gpio: %p, err: %d",
	//	       dptx->CFG1_gpio, IS_ERR(dptx->CFG1_gpio));
	//}
	//dptx->CFG2_gpio = devm_gpiod_get(dptx->dev, "dptx-gpio-CFG2", GPIOD_IN);
	//if (IS_ERR(dptx->CFG2_gpio)) {
	//	DPTXPR(dptx->idx, LOG_E, "regist CFG2 gpio: %p, err: %d",
	//	       dptx->CFG2_gpio, IS_ERR(dptx->CFG2_gpio));
	//}
	memset(&para[0], 0, 30 * sizeof(unsigned char));
	ret = of_property_read_u16_array(np, "drive-strength-lut", &para[0], 30);
	if (ret) {
		DPTXPR(dptx->idx, LOG_E, "DPCD level to PHY LUT not set");
	} else {
		for (i = 0; i < 30; i++)
			dptx->phy_cfg.level_to_phy_lut[i / 10][i % 3] = para[i];
	}

	ret = of_property_read_u16(np, "assigned-link-rate", &val);
	if (!ret) {
		dptx->user_link_rate = (unsigned char)val;
		DPTXPR(dptx->idx, LOG_I, "assigned link rate: %u", dptx->user_link_rate);
	}
	ret = of_property_read_u16(np, "assigned-lane-count", &val);
	if (!ret) {
		dptx->user_lane_count = (unsigned char)val;
		DPTXPR(dptx->idx, LOG_I, "assigned lane count: %u", dptx->user_lane_count);
	} else {
		dptx->user_lane_count = 0xff;
	}

	return 0;
}

static int dptx_config_probe(struct dptx_drv_s *dptx, struct platform_device *pdev)
{
	int ret = 0;

	ret = dptx_config_load_from_dts(dptx);
	if (ret)
		return -1;

	dptx->res_vsync_irq[0] = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "vsync");
	dptx->res_vsync_irq[1] = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "vsync2");
	dptx->res_vsync_irq[2] = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "vsync3");
	dptx->res_HPD_irq      = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "DPTX-HPD");

	dptx_clk_config_probe(dptx);
	dptx_phy_probe(dptx);
	dptx_venc_probe(dptx);
	dptx_debug_probe(dptx);
	dptx_if_IP_probe(dptx);

	dptx_bootup_config_init(dptx);

	dptx_vout_server_init(dptx); // get viu sel here

	/* lock pinmux as when dptx is used ?? or link on */
	if (dptx->viu_sel || dptx->status & DPTX_STA_LINK_ON)
		dptx_pinmux_set(dptx, 1);

	if (dptx->status & DPTX_STA_LINK_ON)
		dptx_timing_config_restore(dptx);

	//lcd_vrr_dev_register(dptx);

	dptx_irq_init(dptx);

	/* add notifier for video sync_duration info refresh */
	dptx_vout_notify_mode_change(dptx);

	dptx_drm_add(dptx->dev);

	dptx_drv_check_HPD(dptx);
	if (dptx->viu_sel)
		dptx_HPD_trigger_set(dptx, 1);

	return 0;
}

#ifdef CONFIG_OF
static struct dptx_chip_data_s dptx_data_t7 = {
	.chip_type = DPTX_CHIP_T7,
	.chip_name = "T7",
	//.reg_map_table = &lcd_reg_t7[0],
	.drv_max = 2,
	.offset_venc      = {0x0, 0x600},
	.offset_venc_if   = {0x0, 0x500},
	.offset_venc_data = {0x0, 0x100},

	.link_rate    = DP_LINK_RATE_HBR,
	.TPS_support  = BIT(0),
	.DACP_support = BIT(2),
	.venc_clk_msr_id = {222, 221},
};

static const struct of_device_id dptx_dt_match_table[] = {
	{
		.compatible = "amlogic, DisplayPort-TX-T7",
		.data = &dptx_data_t7,
	},
	{}
};
#endif

static int dptx_probe(struct platform_device *pdev)
{
	struct dptx_drv_s *dptx;
	const struct of_device_id *match;
	struct dptx_chip_data_s *pdata;
	unsigned int index = 0;
	int ret = 0;

	dptx_global_init_once(pdev);

	if (!pdev->dev.of_node)
		return -1;
	ret = of_property_read_u32(pdev->dev.of_node, "index", &index);
	if (ret) {
		DPTXPR(0, LOG_E, "no index exist");
		return 0;
	}
	if (index >= DPTX_MAX_DRV) {
		DPTXPR(index, LOG_E, "%s: invalid index %d\n", __func__, index);
		return -1;
	}

	match = of_match_device(dptx_dt_match_table, &pdev->dev);
	if (!match) {
		DPTXPR(index, LOG_E, "%s: no match table\n", __func__);
		return -1;
	}
	pdata = (struct dptx_chip_data_s *)match->data;
	DPTXPR(index, LOG_I, "driver version: %s(%d-%s)",
			     DPTX_DRV_VERSION, pdata->chip_type, pdata->chip_name);
	if (index >= pdata->drv_max) {
		DPTXPR(index, LOG_E, "%s: invalid index\n", __func__);
		return -1;
	}

	dptx = dptx_driver_add(index);
	if (!dptx)
		goto lcd_probe_err_0;
	/* set drvdata */
	dptx_driver[index] = dptx;
	dptx->data = pdata;
	platform_set_drvdata(pdev, dptx);
	dptx->pdev = pdev;

#ifdef CONFIG_AMLOGIC_VPU
	/*vpu dev register for lcd*/
	dptx->vpu_dev = vpu_dev_register(VPU_VENCL, DPTX_CDEV_NAME);
#endif
	ret = dptx_ioremap(dptx, pdev);
	if (ret)
		goto lcd_probe_err_1;

	spin_lock_init(&dptx->isr_lock);

	ret = dptx_cdev_add(dptx, &pdev->dev);
	if (ret)
		goto lcd_probe_err_2;

	ret = dptx_config_probe(dptx, pdev);
	if (ret)
		goto lcd_probe_err_2;

	dptx->status |= DPTX_STA_PROBE_DONE;

	DPTXPR(index, LOG_I, "%s ok, status:0x0%2x", __func__, dptx->status);

	return 0;

lcd_probe_err_2:
	dptx_cdev_remove(dptx);
lcd_probe_err_1:
	platform_set_drvdata(pdev, NULL);
	dptx_driver[index] = NULL;
	kfree(dptx);
lcd_probe_err_0:
	return ret;
}

static int dptx_remove(struct platform_device *pdev)
{
	struct dptx_drv_s *dptx = platform_get_drvdata(pdev);
	u8 index;

	if (!dptx)
		return 0;

	dptx->status &= ~DPTX_STA_PROBE_DONE;

	dptx_drm_remove(dptx->dev);

	index = dptx->idx;

	dptx_irq_serve_remove(dptx);
	dptx_cdev_remove(dptx);
	dptx_debug_remove(dptx);
	dptx_config_remove(dptx);

	/* free drvdata */
	platform_set_drvdata(pdev, NULL);

	kfree(dptx->reg_map);
	kfree(dptx);
	dptx_driver[index] = NULL;
	dptx_global_remove_once();

	DPTXPR(index, LOG_I, "%s", __func__);
	return 0;
}

static int dptx_resume(struct platform_device *pdev)
{
	struct dptx_drv_s *dptx = platform_get_drvdata(pdev);

	if (!dptx)
		return 0;

	dptx_notifier_call_chain(DPTX_EVENT_PREPARE, (void *)dptx);

	DPTXPR(dptx->idx, LOG_I, "%s finished (0x%x)", __func__, dptx->status);

	//mutex_unlock(&lcd_power_mutex);
	return 0;
}

static int dptx_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct dptx_drv_s *dptx = platform_get_drvdata(pdev);

	if (!dptx)
		return 0;

	if (dptx->status & DPTX_STA_LINK_ON)
		dptx_notifier_call_chain(DPTX_EVENT_DISP_OFF, (void *)dptx);

	dptx_notifier_call_chain(DPTX_EVENT_UNPREPARE, (void *)dptx);

	DPTXPR(dptx->idx, LOG_I, "%s finished (0x%x)", __func__, dptx->status);

	//mutex_unlock(&lcd_power_mutex);
	return 0;
}

static void dptx_shutdown(struct platform_device *pdev)
{
	struct dptx_drv_s *dptx = platform_get_drvdata(pdev);

	if (!dptx)
		return;

	DPTXPR(dptx->idx, LOG_I, "%s", __func__);

	dptx_notifier_call_chain(DPTX_EVENT_DISABLE, (void *)dptx);
}

static struct platform_driver DisplayPort_platform_driver = {
	.probe    = dptx_probe,
	.remove   = dptx_remove,
	.suspend  = dptx_suspend,
	.resume   = dptx_resume,
	.shutdown = dptx_shutdown,
	.driver = {
		.name = "Meson-DisplayPort-TX",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(dptx_dt_match_table),
#endif
	},
};

int __init DisplayPort_TX_init(void)
{
	if (platform_driver_register(&DisplayPort_platform_driver)) {
		DPTXPR(0, LOG_E, "failed to register DisplayPort driver module\n");
		return -ENODEV;
	}

	return 0;
}

void __exit DisplayPort_TX_exit(void)
{
	platform_driver_unregister(&DisplayPort_platform_driver);
}

static int dptx_boot_ctrl_setup(u8 idx, char *str)
{
	int ret = 0;
	unsigned int data32 = 0;

	if (!str)
		return -EINVAL;

	ret = kstrtouint(str, 16, &data32);
	if (ret) {
		DPTXPR(idx, LOG_E, "%s: invalid (%s)", __func__, str);
		return -EINVAL;
	}

	/*
	 *bit[7:0]: link rate
	 *bit[9:8]: lane count: 0=disabled, 1=1lane, 2=2lane, 3=4lane
	 *bit[17:10]: vmode_sel_idx, 0xff for non-edid or invalid
	 *bit[21:18]: vmode_cfmt_sel, 0xf for invalid
	 *bit[22]: dptx-ip enabled
	 *bit[23]: dptx disp enabled
	 *bit[27:24]: 4bit crc
	 *******
	 *bit[31:30]: DPTX debug_level
	 */

	dptx_uboot_configs[idx].link_rate      = data32 & 0xff;
	dptx_uboot_configs[idx].lane_count     = (data32 >> 8) & 0x3;
	dptx_uboot_configs[idx].vmode_sel_idx  = (data32 >> 10) & 0xff;
	dptx_uboot_configs[idx].vmode_cfmt_sel = (data32 >> 18) & 0xf;
	dptx_uboot_configs[idx].tx_prepared     = (data32 >> 22) & 0x1;
	dptx_uboot_configs[idx].disp_on = (data32 >> 23) & 0x1;
	dptx_uboot_configs[idx].uboot_edid_crc = (data32 >> 24) & 0xff;
	// dptx_print_level = data32 >> 30;

	DPTXPR(idx, LOG_I, "boot[0x%8x] ", data32);
	return 0;
}

static int __dptx0_boot_ctrl_setup(char *str)
{
	dptx_boot_ctrl_setup(0, str);
	return 0;
}

static int __dptx1_boot_ctrl_setup(char *str)
{
	dptx_boot_ctrl_setup(1, str);
	return 0;
}

static int __dptx_debug_setup(char *str)
{
	int ret = 0;
	unsigned int data32 = 0;

	if (!str)
		return -EINVAL;

	ret = kstrtouint(str, 16, &data32);
	if (ret) {
		DPTXPR(0, LOG_E, "%s: invalid (%s)", __func__, str);
		return -EINVAL;
	}

	dptx_print_level = data32 & 0x3;
	return 0;
}

__setup("dptx0=", __dptx0_boot_ctrl_setup);
__setup("dptx1=", __dptx1_boot_ctrl_setup);
__setup("dptx_dbg=", __dptx_debug_setup);

struct dptx_drv_s *aml_dptx_get_driver(u8 drv_idx)
{
	if (drv_idx >= DPTX_MAX_DRV)
		return NULL;

	return dptx_driver[drv_idx];
}
EXPORT_SYMBOL(aml_dptx_get_driver);

void aml_dptx_regist_hpd_cb(struct dptx_drv_s *dptx, struct connector_hpd_cb *hpd_cb)
{
	// dptx->drm_hpd_cb.callback = hpd_cb->callback;
	// dptx->drm_hpd_cb.data = hpd_cb->data;
}
EXPORT_SYMBOL(aml_dptx_regist_hpd_cb);

//MODULE_DESCRIPTION("Meson DisplayPort TX Driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Amlogic, Inc.");
