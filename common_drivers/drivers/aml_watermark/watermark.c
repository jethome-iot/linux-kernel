// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/fs.h>
#include <linux/of.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include "wm_smc_cmd.h"

//#define REE_WM_DRV_DEBUG

#define INFO_TAG          "[AML_WATERMARK] [INFO] "
#define ERROR_TAG         "[AML_WATERMARK] [ERROR] [Func: %s, Line: %d] "
#define DEBUG_TAG         "[AML_WATERMARK] [DEBUG] [Func: %s, Line: %d] "

#define INFO(fmt, ...)    pr_info(INFO_TAG fmt "\n", ##__VA_ARGS__)

#define ERROR(fmt, ...)   pr_info(ERROR_TAG fmt "\n", __func__, __LINE__, ##__VA_ARGS__)

#ifdef REE_WM_DRV_DEBUG
#define DEBUG(fmt, ...)   pr_info(DEBUG_TAG fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG(fmt, ...)
#endif

#define DRIVER_NAME                         "aml_watermark"
#define IRQ_NAME                            "wm-vsync"

#define SUPP_PLUGIN_CMD_TRIGGER_VXWM_VSYNC  0xAAAA0001

#define SMC_TYPE_VXWM_CHECKING              0xFFFF0010
#define SMC_TYPE_NGWM_CHECKING              0xFFFF0020

#define SMC_TYPE_VXWM_PLUGIN                0xFFFF0010
#define SMC_TYPE_VXWM_MODE_CHANGED          0xFFFF0011
#define SMC_TYPE_NGWM_PLUGIN                0xFFFF0020
#define SMC_TYPE_NGWM_MODE_CHANGED          0xFFFF0021

#define TEEC_SUCCESS                        0x00000000
#define TEEC_ERROR_BAD_STATE                0xFFFF0007

#define TRYING_TIMES                        200

struct wm_dev_s {
	dev_t dev_id;
	struct cdev cdev;
	struct class *class;
	struct device *dev;
};

struct wm_dts_node_s {
	const char *soc_name;
	int irq_id;
};

struct wm_sts_s {
	bool vxwm_enabled;
	bool ngwm_enabled;
};

struct wm_resolution_s {
	u32 horz;
	u32 vert;
};

struct vxwm_mode_s {
	struct wm_resolution_s res;
	u32 slice_num;
};

struct ngwm_mode_s {
	struct wm_resolution_s res;
	u32 frame_rate;
};

struct wm_plugin_ctx_s {
	u32 plugin_times;
	u32 flush_times;
};

struct plugin_buf_s {
	u64 buf;
	u32 size;
};

static struct wm_dev_s g_wm_dev = { 0 };
static struct wm_dts_node_s g_dts_node = { 0 };
static struct wm_sts_s g_wm_sts = { false, false };
static struct vxwm_mode_s g_vxwm_mode = { { 0, 0 }, 0 };
static struct ngwm_mode_s g_ngwm_mode = { { 0, 0 }, 0 };
static struct wm_plugin_ctx_s g_vxwm_plugin = { 0, 0 };
static struct wm_plugin_ctx_s g_ngwm_plugin = { 0, 0 };

static bool g_drv_inited;

static spinlock_t g_wm_lock;

static u32 g_vxwm_trying_times;
static u32 g_ngwm_trying_times;

static u32 get_horz_res(void)
{
	u32 horz_res = 0;
	struct vinfo_s *vinfo = get_current_vinfo();

	if (vinfo)
		horz_res = vinfo->width;

	return horz_res;
}

static u32 get_vert_res(void)
{
	u32 vert_res = 0;
	struct vinfo_s *vinfo = get_current_vinfo();

	if (vinfo) {
		vert_res = vinfo->height;
		if (vert_res == 576 || vert_res == 480) // 576i, 480i
			vert_res = vert_res / 2;
	}

	return vert_res;
}

static u32 get_slice_num(void)
{
	u32 slice_num = 1;
	struct vinfo_s *vinfo = get_current_vinfo();

	if (vinfo)
		slice_num = vinfo->cur_enc_ppc;

	return slice_num;
}

static u32 get_frame_rate(void)
{
	u32 frame_rate = 0;
	struct vinfo_s *vinfo = get_current_vinfo();

	if (vinfo && vinfo->sync_duration_den)
		frame_rate = vinfo->sync_duration_num / vinfo->sync_duration_den;

	return frame_rate;
}

static void init_watermark_status(void)
{
	struct arm_smccc_res vxwm;
	struct arm_smccc_res ngwm;

	memset(&vxwm, 0, sizeof(struct arm_smccc_res));
	memset(&ngwm, 0, sizeof(struct arm_smccc_res));

	arm_smccc_smc(OPTEE_SMC_CHECK_WM_STATUS, SMC_TYPE_VXWM_CHECKING, 0, 0, 0, 0, 0, 0, &vxwm);
	arm_smccc_smc(OPTEE_SMC_CHECK_WM_STATUS, SMC_TYPE_NGWM_CHECKING, 0, 0, 0, 0, 0, 0, &ngwm);

	g_wm_sts.vxwm_enabled = (vxwm.a0 == TEEC_SUCCESS);
	g_wm_sts.ngwm_enabled = (ngwm.a0 == TEEC_SUCCESS);

	if (g_wm_sts.vxwm_enabled)
		DEBUG("vxwm is enable");
	else
		DEBUG("vxwm is disable");
	if (g_wm_sts.ngwm_enabled)
		DEBUG("ngwm is enable");
	else
		DEBUG("ngwm is disable");
}

static bool has_vxwm_plugin(void)
{
	return g_vxwm_plugin.plugin_times != g_vxwm_plugin.flush_times;
}

static bool has_ngwm_plugin(void)
{
	return g_ngwm_plugin.plugin_times != g_ngwm_plugin.flush_times;
}

static bool is_vxwm_mode_changed(void)
{
	return g_vxwm_mode.res.horz != get_horz_res() ||
		g_vxwm_mode.res.vert != get_vert_res() ||
		g_vxwm_mode.slice_num != get_slice_num();
}

static bool is_ngwm_mode_changed(void)
{
	return g_ngwm_mode.res.horz != get_horz_res() ||
		g_ngwm_mode.res.vert != get_vert_res() ||
		g_ngwm_mode.frame_rate != get_frame_rate();
}

static void update_vxwm_plugin(void)
{
	DEBUG("g_vxwm_plugin.plugin_times = %d", g_vxwm_plugin.plugin_times);
	DEBUG("g_vxwm_plugin.flush_times = %d", g_vxwm_plugin.flush_times);

	if (g_vxwm_plugin.plugin_times > g_vxwm_plugin.flush_times)
		g_vxwm_plugin.flush_times++;
}

static void update_ngwm_plugin(void)
{
	DEBUG("g_ngwm_plugin.plugin_times = %d", g_ngwm_plugin.plugin_times);
	DEBUG("g_ngwm_plugin.flush_times = %d", g_ngwm_plugin.flush_times);

	if (g_ngwm_plugin.plugin_times > g_ngwm_plugin.flush_times)
		g_ngwm_plugin.flush_times++;
}

static bool update_vxwm_mode(void)
{
	DEBUG("g_vxwm_mode.res.horz = %d", g_vxwm_mode.res.horz);
	DEBUG("g_vxwm_mode.res.vert = %d", g_vxwm_mode.res.vert);
	DEBUG("g_vxwm_mode.slice_num = %d", g_vxwm_mode.slice_num);

	DEBUG("get_horz_res() = %d", get_horz_res());
	DEBUG("get_vert_res() = %d", get_vert_res());
	DEBUG("get_slice_num() = %d", get_slice_num());

	g_vxwm_mode.res.horz = get_horz_res();
	g_vxwm_mode.res.vert = get_vert_res();
	g_vxwm_mode.slice_num = get_slice_num();

	if (g_vxwm_mode.res.horz == 0)
		ERROR("failed to get resolution horz");
	if (g_vxwm_mode.res.vert == 0)
		ERROR("failed to get resolution vert");

	return g_vxwm_mode.res.horz && g_vxwm_mode.res.vert;
}

static bool update_ngwm_mode(void)
{
	DEBUG("g_ngwm_mode.res.horz = %d", g_ngwm_mode.res.horz);
	DEBUG("g_ngwm_mode.res.vert = %d", g_ngwm_mode.res.vert);
	DEBUG("g_ngwm_mode.frame_rate = %d", g_ngwm_mode.frame_rate);

	DEBUG("get_horz_res() = %d", get_horz_res());
	DEBUG("get_vert_res() = %d", get_vert_res());
	DEBUG("get_frame_rate() = %d", get_frame_rate());

	g_ngwm_mode.res.horz = get_horz_res();
	g_ngwm_mode.res.vert = get_vert_res();
	g_ngwm_mode.frame_rate = get_frame_rate();

	if (g_ngwm_mode.res.horz == 0)
		ERROR("failed to get resolution horz");
	if (g_ngwm_mode.res.vert == 0)
		ERROR("failed to get resolution vert");
	if (g_ngwm_mode.frame_rate == 0)
		ERROR("failed to get frame rate");

	return g_ngwm_mode.res.horz && g_ngwm_mode.res.vert && g_ngwm_mode.frame_rate;
}

static void try_to_flush_vxwm(void)
{
	struct arm_smccc_res res;

	if (has_vxwm_plugin()) {
		memset(&res, 0, sizeof(struct arm_smccc_res));
		arm_smccc_smc(OPTEE_SMC_FLUSH_WM, SMC_TYPE_VXWM_PLUGIN, 0, 0, 0, 0, 0, 0, &res);
		DEBUG("res.a0 = 0x%08X", res.a0);
		if (res.a0 == TEEC_SUCCESS)
			update_vxwm_plugin();
	}

	if (is_vxwm_mode_changed()) {
		g_vxwm_trying_times = TRYING_TIMES;
		update_vxwm_mode();
	}

	if (g_vxwm_trying_times > 0) {
		memset(&res, 0, sizeof(struct arm_smccc_res));
		arm_smccc_smc(OPTEE_SMC_FLUSH_WM, SMC_TYPE_VXWM_MODE_CHANGED,
				0, 0, 0, 0, 0, 0, &res);
		DEBUG("res.a0 = 0x%08X", res.a0);
		if (res.a0 == TEEC_SUCCESS) {
			DEBUG("g_vxwm_trying_times = %d", g_vxwm_trying_times);
			g_vxwm_trying_times--;
		}
	}
}

static void try_to_flush_ngwm(void)
{
	struct arm_smccc_res res;

	if (has_ngwm_plugin()) {
		memset(&res, 0, sizeof(struct arm_smccc_res));
		arm_smccc_smc(OPTEE_SMC_FLUSH_WM, SMC_TYPE_NGWM_PLUGIN, 0, 0, 0, 0, 0, 0, &res);
		DEBUG("res.a0 = 0x%08X", res.a0);
		if (res.a0 == TEEC_SUCCESS)
			update_ngwm_plugin();
	}

	if (is_ngwm_mode_changed()) {
		g_ngwm_trying_times = TRYING_TIMES;
		update_ngwm_mode();
	}
	if (g_ngwm_trying_times > 0) {
		memset(&res, 0, sizeof(struct arm_smccc_res));
		arm_smccc_smc(OPTEE_SMC_FLUSH_WM, SMC_TYPE_NGWM_MODE_CHANGED,
				0, 0, 0, 0, 0, 0, &res);
		DEBUG("res.a0 = 0x%08X", res.a0);
		if (res.a0 == TEEC_SUCCESS) {
			DEBUG("g_ngwm_trying_times = %d", g_ngwm_trying_times);
			g_ngwm_trying_times--;
		}
	}
}

static irqreturn_t vsync_isr(int irq, void *dev_id)
{
	spin_lock(&g_wm_lock);

	if (g_wm_sts.vxwm_enabled)
		try_to_flush_vxwm();

	if (g_wm_sts.ngwm_enabled)
		try_to_flush_ngwm();

	spin_unlock(&g_wm_lock);

	return IRQ_HANDLED;
}

static bool read_dts_node(struct platform_device *pdev)
{
	if (of_property_read_string(pdev->dev.of_node, "soc-name",
				&g_dts_node.soc_name) != 0 || !g_dts_node.soc_name) {
		ERROR("failed to get soc name");
		return false;
	}
	INFO("soc-name: %s", g_dts_node.soc_name);

	g_dts_node.irq_id = of_irq_get_byname(pdev->dev.of_node, IRQ_NAME);
	if (g_dts_node.irq_id <= 0) {
		ERROR("failed to get watermark irq id");
		return false;
	}
	INFO("%s interrupt id: %d", IRQ_NAME, g_dts_node.irq_id);

	return true;
}

static bool register_wm_irq(void)
{
	int err_num = 0;

	err_num = request_irq(g_dts_node.irq_id, &vsync_isr, IRQF_SHARED,
			IRQ_NAME, (void *)&g_dts_node.irq_id);
	if (err_num) {
		ERROR("failed to request %s interrupt, err_num = %d", IRQ_NAME, -err_num);
		return false;
	}

	return true;
}

static void free_wm_irq(void)
{
	if (g_dts_node.irq_id > 0) {
		free_irq(g_dts_node.irq_id, (void *)&g_dts_node.irq_id);
		g_dts_node.irq_id = 0;
	}
}

static long watermark_driver_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long res = TEEC_SUCCESS;

	(void)file;
	(void)arg;

	DEBUG("ioctl cmd = 0x%08X", cmd);
	switch (cmd) {
	case SUPP_PLUGIN_CMD_TRIGGER_VXWM_VSYNC:
		spin_lock(&g_wm_lock);
		g_vxwm_plugin.plugin_times++;
		DEBUG("g_vxwm_plugin.plugin_times = %d", g_vxwm_plugin.plugin_times);
		spin_unlock(&g_wm_lock);
		break;
	default:
		res = TEEC_ERROR_BAD_STATE;
		break;
	}

	return res;
}

static const struct file_operations watermark_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = watermark_driver_ioctl,
	.compat_ioctl = watermark_driver_ioctl,
};

static int watermark_driver_probe(struct platform_device *pdev)
{
	if (g_drv_inited)
		return 0;

	spin_lock_init(&g_wm_lock);

	if (!read_dts_node(pdev))
		return -EINVAL;

	if (g_wm_sts.vxwm_enabled && !update_vxwm_mode())
		return -EINVAL;

	if (g_wm_sts.ngwm_enabled && !update_ngwm_mode())
		return -EINVAL;

	if (!register_wm_irq())
		return -EINVAL;

	g_drv_inited = true;
	INFO("success to initialize ree watermark driver");

	return 0;
}

static int watermark_driver_remove(struct platform_device *pdev)
{
	free_wm_irq();

	return 0;
}

static const struct of_device_id watermark_match[] = {
	{ .compatible = "amlogic,watermark" },
	{},
};

static struct platform_driver watermark_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = watermark_match,
	},
	.probe  = watermark_driver_probe,
	.remove = watermark_driver_remove,
};

static void watermark_device_release(struct device *dev)
{
	(void)dev;
}

static struct platform_device watermark_device = {
	.name = DRIVER_NAME,
	.id = 0,
	.dev.release = watermark_device_release,
};

static int __init aml_watermark_driver_init(void)
{
	int res = 0;

	init_watermark_status();
	if (!g_wm_sts.vxwm_enabled && !g_wm_sts.ngwm_enabled) {
		ERROR("vxwm and ngwm both are disabled");
		res = -EINVAL;
		goto err;
	}

	if (alloc_chrdev_region(&g_wm_dev.dev_id, 0, 1, DRIVER_NAME) < 0) {
		ERROR("failed to alloc char dev region");
		res = -EINVAL;
		goto err;
	}

	g_wm_dev.class = class_create(THIS_MODULE, DRIVER_NAME);
	if (IS_ERR(g_wm_dev.class)) {
		ERROR("failed to create class");
		res = PTR_ERR(g_wm_dev.class);
		goto err1;
	}

	g_wm_dev.dev = device_create(g_wm_dev.class, NULL, g_wm_dev.dev_id, NULL, DRIVER_NAME);
	if (IS_ERR(g_wm_dev.dev)) {
		ERROR("failed to create device");
		res = PTR_ERR(g_wm_dev.dev);
		goto err2;
	}

	cdev_init(&g_wm_dev.cdev, &watermark_fops);
	g_wm_dev.cdev.owner = THIS_MODULE;
	if (cdev_add(&g_wm_dev.cdev, g_wm_dev.dev_id, 1)) {
		ERROR("failed to add cdev");
		res = -EINVAL;
		goto err3;
	}

	if (platform_device_register(&watermark_device) != 0) {
		ERROR("failed to register device %s", watermark_device.name);
		res = -EINVAL;
		goto err4;
	}

	if (platform_driver_register(&watermark_driver) != 0) {
		ERROR("failed to register driver %s", watermark_driver.driver.name);
		res = -EINVAL;
		goto err5;
	}

	return res;

err5:
	platform_device_unregister(&watermark_device);
err4:
	cdev_del(&g_wm_dev.cdev);
err3:
	device_destroy(g_wm_dev.class, g_wm_dev.dev_id);
err2:
	class_destroy(g_wm_dev.class);
err1:
	unregister_chrdev_region(g_wm_dev.dev_id, 1);
err:
	return res;
}
module_init(aml_watermark_driver_init);

static void __exit aml_watermark_driver_exit(void)
{
	device_destroy(g_wm_dev.class, g_wm_dev.dev_id);
	unregister_chrdev_region(g_wm_dev.dev_id, 1);
	class_destroy(g_wm_dev.class);
	cdev_del(&g_wm_dev.cdev);
	platform_device_unregister(&watermark_device);
	platform_driver_unregister(&watermark_driver);
}
module_exit(aml_watermark_driver_exit);

MODULE_AUTHOR("Amlogic");
MODULE_DESCRIPTION("Amlogic REE Watermark Driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
