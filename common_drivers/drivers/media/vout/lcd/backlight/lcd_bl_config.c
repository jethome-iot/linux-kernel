// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/backlight.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/pwm.h>
#include <linux/amlogic/pwm-meson.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/compat.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_resman.h>
#include <linux/amlogic/media/vout/lcd/lcd_model.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_AMLOGIC_BL_EXTERN
#include <linux/amlogic/media/vout/lcd/aml_bl_extern.h>
#endif
#ifdef CONFIG_AMLOGIC_BL_LDIM
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#endif
#include "lcd_bl.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

struct bl_method_match_s {
	char *name;
	enum bl_ctrl_method_e type;
};

static struct bl_method_match_s bl_method_match_table[] = {
	{"gpio",          BL_CTRL_GPIO},
	{"pwm",           BL_CTRL_PWM},
	{"pwm_combo",     BL_CTRL_PWM_COMBO},
	{"local_dimming", BL_CTRL_LOCAL_DIMMING},
	{"extern",        BL_CTRL_EXTERN},
	{"invalid",       BL_CTRL_MAX},
};

char *bl_method_type_to_str(int type)
{
	int i;
	char *str = bl_method_match_table[BL_CTRL_MAX].name;

	for (i = 0; i < BL_CTRL_MAX; i++) {
		if (type == bl_method_match_table[i].type) {
			str = bl_method_match_table[i].name;
			break;
		}
	}
	return str;
}

static void bl_config_print(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;
	struct bl_pwm_config_s *bl_pwm;
	char str[128];
	int len = 0;

	if (bconf->method == BL_CTRL_MAX) {
		BLPR("[%d]: no backlight exist\n", bdrv->index);
		return;
	}

	if ((lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) == 0) {
		len += sprintf(str + len, "bri_bypass:%d, step_on_flag:%d, en_seq_rev:%d",
			bdrv->brightness_bypass, bdrv->step_on_flag, bconf->en_sequence_reverse);
		if (bconf->ldim_flag)
			sprintf(str + len, ", ldim_flag:%d", bconf->ldim_flag);
		BLPR("[%d]: config from %s: %s, method: %s(%d), %s\n",
			bdrv->index, get_lcd_config_load(bdrv->config_load),
			bconf->name, bl_method_type_to_str(bconf->method), bconf->method,
			str);
		return;
	}

	BLPR("level_default = %d\n", bconf->level_default);
	BLPR("level_min     = %d\n", bconf->level_min);
	BLPR("level_max     = %d\n", bconf->level_max);
	BLPR("level_mid     = %d\n", bconf->level_mid);
	BLPR("level_mid_map = %d\n", bconf->level_mid_mapping);

	BLPR("en_gpio       = %d\n", bconf->en_gpio);
	BLPR("en_gpio_on    = %d\n", bconf->en_gpio_on);
	BLPR("en_gpio_off   = %d\n", bconf->en_gpio_off);
	BLPR("power_on_dly  = %dms\n", bconf->power_on_delay);
	BLPR("power_off_dly = %dms\n\n", bconf->power_off_delay);

	switch (bconf->method) {
	case BL_CTRL_PWM:
		BLPR("pwm_on_delay  = %dms\n", bconf->pwm_on_delay);
		BLPR("pwm_off_delay = %dms\n", bconf->pwm_off_delay);
		BLPR("en_sequence_reverse = %d\n", bconf->en_sequence_reverse);
		if (bconf->bl_pwm) {
			bl_pwm = bconf->bl_pwm;
			BLPR("pwm_index     = %d\n", bl_pwm->index);
			BLPR("pwm_port      = %s(0x%x)\n",
			     bl_pwm_num_to_str(bl_pwm->pwm_port), bl_pwm->pwm_port);
			BLPR("pwm_method    = %d\n", bl_pwm->pwm_method);
			if (bl_pwm->pwm_port == BL_PWM_VS)
				BLPR("pwm_freq      = %d x vfreq\n", bl_pwm->pwm_freq);
			else
				BLPR("pwm_freq      = %uHz\n", bl_pwm->pwm_freq);
			BLPR("pwm_phase = %u\n", bl_pwm->pwm_phase);
			BLPR("pwm_level_max = %u\n", bl_pwm->level_max);
			BLPR("pwm_level_min = %u\n", bl_pwm->level_min);
			BLPR("pwm_duty_max  = %d\n", bl_pwm->pwm_duty_max);
			BLPR("pwm_duty_min  = %d\n", bl_pwm->pwm_duty_min);
			BLPR("pwm_cnt       = %u\n", bl_pwm->pwm_cnt);
			BLPR("pwm_max       = %u\n", bl_pwm->pwm_max);
			BLPR("pwm_min       = %u\n", bl_pwm->pwm_min);
		}
		break;
	case BL_CTRL_PWM_COMBO:
		BLPR("pwm_on_delay        = %dms\n", bconf->pwm_on_delay);
		BLPR("pwm_off_delay       = %dms\n", bconf->pwm_off_delay);
		BLPR("en_sequence_reverse = %d\n", bconf->en_sequence_reverse);
		/* pwm_combo_0, switch channel default is 0*/
		if (bconf->bl_pwm_combo0) {
			bl_pwm = bconf->bl_pwm_combo0;
			BLPR("pwm_combo0_index     = %d\n", bl_pwm->index);
			BLPR("pwm_combo0_port      = %s(0x%x)\n",
			     bl_pwm_num_to_str(bl_pwm->pwm_port), bl_pwm->pwm_port);
			BLPR("pwm_combo0_method    = %d\n", bl_pwm->pwm_method);
			if (bl_pwm->pwm_port == BL_PWM_VS)
				BLPR("pwm_combo0_freq      = %d x vfreq\n", bl_pwm->pwm_freq);
			else
				BLPR("pwm_combo0_freq      = %uHz\n", bl_pwm->pwm_freq);
			BLPR("pwm_combo0_phase = %u\n", bl_pwm->pwm_phase);
			BLPR("pwm_combo0_level_max = %u\n", bl_pwm->level_max);
			BLPR("pwm_combo0_level_min = %u\n", bl_pwm->level_min);
			BLPR("pwm_combo0_duty_max  = %d\n", bl_pwm->pwm_duty_max);
			BLPR("pwm_combo0_duty_min  = %d\n", bl_pwm->pwm_duty_min);
			BLPR("pwm_combo0_pwm_cnt   = %u\n", bl_pwm->pwm_cnt);
			BLPR("pwm_combo0_pwm_max   = %u\n", bl_pwm->pwm_max);
			BLPR("pwm_combo0_pwm_min   = %u\n", bl_pwm->pwm_min);
		}
		/* pwm_combo_1 */
		if (bconf->bl_pwm_combo1) {
			bl_pwm = bconf->bl_pwm_combo1;
			BLPR("pwm_combo1_index     = %d\n", bl_pwm->index);
			BLPR("pwm_combo1_port      = %s(0x%x)\n",
			     bl_pwm_num_to_str(bl_pwm->pwm_port), bl_pwm->pwm_port);
			BLPR("pwm_combo1_method    = %d\n", bl_pwm->pwm_method);
			if (bl_pwm->pwm_port == BL_PWM_VS)
				BLPR("pwm_combo1_freq      = %d x vfreq\n", bl_pwm->pwm_freq);
			else
				BLPR("pwm_combo1_freq      = %uHz\n", bl_pwm->pwm_freq);
			BLPR("pwm_combo1_phase = %u\n", bl_pwm->pwm_phase);
			BLPR("pwm_combo1_level_max = %u\n", bl_pwm->level_max);
			BLPR("pwm_combo1_level_min = %u\n", bl_pwm->level_min);
			BLPR("pwm_combo1_duty_max  = %d\n", bl_pwm->pwm_duty_max);
			BLPR("pwm_combo1_duty_min  = %d\n", bl_pwm->pwm_duty_min);
			BLPR("pwm_combo1_pwm_cnt   = %u\n", bl_pwm->pwm_cnt);
			BLPR("pwm_combo1_pwm_max   = %u\n", bl_pwm->pwm_max);
			BLPR("pwm_combo1_pwm_min   = %u\n", bl_pwm->pwm_min);
		}
		break;
	default:
		break;
	}
}

static int bl_pwm_switch_init(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;

	if (bconf->bl_pwm_switch_port < BL_PWM_MAX && bconf->bl_pwm_switch_freq > 0) {
		bconf->bl_pwm_switch = kzalloc(sizeof(*bconf->bl_pwm_switch), GFP_KERNEL);
		if (!bconf->bl_pwm_switch)
			return -1;

		switch (bconf->method) {
		case BL_CTRL_PWM:
			bconf->bl_pwm_switch = memcpy(bconf->bl_pwm_switch,
				bconf->bl_pwm, sizeof(*bconf->bl_pwm));
			bconf->bl_pwm_default = bconf->bl_pwm;
			break;
		case BL_CTRL_PWM_COMBO:
			bconf->bl_pwm_switch = memcpy(bconf->bl_pwm_switch,
				bconf->bl_pwm_combo0, sizeof(*bconf->bl_pwm_combo0));
			bconf->bl_pwm_default = bconf->bl_pwm_combo0;
			break;
		default:
			break;
		}

		bconf->bl_pwm_switch->pwm_port = bconf->bl_pwm_switch_port;
		bconf->bl_pwm_switch->pwm_freq = bconf->bl_pwm_switch_freq;
		bl_pwm_config_init(bconf->bl_pwm_switch);

		BLPR("[%d]: pwm_switch port_freq: dft: %s(0x%x),%d, switch: %s(0x%x),%d\n",
			bdrv->index, bl_pwm_num_to_str(bconf->bl_pwm_default->pwm_port),
			bconf->bl_pwm_default->pwm_port, bconf->bl_pwm_default->pwm_freq,
			bl_pwm_num_to_str(bconf->bl_pwm_switch_port),
			bconf->bl_pwm_switch_port, bconf->bl_pwm_switch_freq);
	} else {
		bconf->bl_pwm_switch = NULL;
	}

	return 0;
}

#ifdef CONFIG_OF
static int bl_config_load_from_dts(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;
	const char *str;
	unsigned int para[10];
	char bl_propname[20];
	struct device_node *child;
	struct bl_pwm_config_s *bl_pwm;
	struct bl_pwm_config_s *pwm_combo0, *pwm_combo1;
	int val, ret = 0;

	/* select backlight by index */
	bconf->index = aml_bl_index_get(bdrv->index);
	if (bconf->index == 0xff) {
		bconf->method = BL_CTRL_MAX;
		return -1;
	}
	sprintf(bl_propname, "backlight_%d", bconf->index);
	child = of_get_child_by_name(bdrv->dev->of_node, bl_propname);
	if (!child) {
		BLERR("[%d]: failed to get %s\n", bdrv->index, bl_propname);
		return -1;
	}

	ret = of_property_read_string(child, "bl_name", &str);
	if (ret) {
		BLERR("[%d]: failed to get bl_name\n", bdrv->index);
		str = "backlight";
	}
	strscpy(bconf->name, str, BL_NAME_MAX);

	ret = of_property_read_u32_array(child, "bl_level_default_uboot_kernel", &para[0], 2);
	if (ret) {
		BLERR("[%d]: failed to get bl_level_default_uboot_kernel\n", bdrv->index);
		bconf->level_uboot = BL_LEVEL_DEFAULT;
		bconf->level_default = BL_LEVEL_DEFAULT;
	} else {
		bconf->level_uboot = para[0] & BL_LEVEL_MASK;
		bconf->level_default = para[1] & BL_LEVEL_MASK;

		bdrv->brightness_bypass = ((para[1] >> BL_POLICY_BRIGHTNESS_BYPASS_BIT) &
					   BL_POLICY_BRIGHTNESS_BYPASS_MASK);
		bdrv->step_on_flag = ((para[1] >> BL_POLICY_POWER_ON_BIT) &
				      BL_POLICY_POWER_ON_MASK);
	}

	ret = of_property_read_u32_array(child, "bl_level_attr", &para[0], 4);
	if (ret) {
		BLERR("[%d]: failed to get bl_level_attr\n", bdrv->index);
		bconf->level_min = BL_LEVEL_MIN;
		bconf->level_max = BL_LEVEL_MAX;
		bconf->level_mid = BL_LEVEL_MID;
		bconf->level_mid_mapping = BL_LEVEL_MID_MAPPED;
	} else {
		bconf->level_max = para[0];
		bconf->level_min = para[1];
		bconf->level_mid = para[2];
		bconf->level_mid_mapping = para[3];
	}

	ret = of_property_read_u32(child, "bl_ctrl_method", &val);
	if (ret) {
		BLERR("[%d]: failed to get bl_ctrl_method\n", bdrv->index);
		bconf->method = BL_CTRL_MAX;
	} else {
		bconf->method = (val >= BL_CTRL_MAX) ? BL_CTRL_MAX : val;
	}

	ret = of_property_read_u32_array(child, "bl_power_attr", &para[0], 5);
	if (ret) {
		BLERR("[%d]: failed to get bl_power_attr\n", bdrv->index);
		bconf->en_gpio = BL_GPIO_MAX;
		bconf->en_gpio_on = BL_GPIO_OUTPUT_HIGH;
		bconf->en_gpio_off = BL_GPIO_OUTPUT_LOW;
		bconf->power_on_delay = 100;
		bconf->power_off_delay = 30;
	} else {
		if (para[0] >= BL_GPIO_NUM_MAX) {
			bconf->en_gpio = BL_GPIO_MAX;
		} else {
			bconf->en_gpio = para[0];
			bl_gpio_probe(bdrv, bconf->en_gpio);
		}
		bconf->en_gpio_on = para[1];
		bconf->en_gpio_off = para[2];
		bconf->power_on_delay = para[3];
		bconf->power_off_delay = para[4];
	}
	ret = of_property_read_u32(child, "en_sequence_reverse", &val);
	if (ret)
		bconf->en_sequence_reverse = 0;
	else
		bconf->en_sequence_reverse = val;

	ret = of_property_read_u32(child, "bl_ldim_region_row_col", &para[0]);
	if (ret) {
		ret = of_property_read_u32(child, "bl_ldim_zone_row_col", &para[0]);
		if (ret == 0)
			bconf->ldim_flag = 1;
	} else {
		bconf->ldim_flag = 1;
	}

	switch (bconf->method) {
	case BL_CTRL_PWM:
		bconf->bl_pwm = kzalloc(sizeof(*bconf->bl_pwm), GFP_KERNEL);
		if (!bconf->bl_pwm)
			return -1;

		bl_pwm = bconf->bl_pwm;
		bl_pwm->index = 0;
		bl_pwm->drv_index = bdrv->index;

		bl_pwm->level_max = bconf->level_max;
		bl_pwm->level_min = bconf->level_min;

		ret = of_property_read_string(child, "bl_pwm_port", &str);
		if (ret) {
			BLERR("[%d]: failed to get bl_pwm_port\n", bdrv->index);
			bl_pwm->pwm_port = BL_PWM_MAX;
		} else {
			bl_pwm->pwm_port = bl_pwm_str_to_num(str);
			BLPR("[%d]: bl pwm_port: %s(0x%x)\n",
			     bdrv->index, str, bl_pwm->pwm_port);
		}
		ret = of_property_read_u32_array(child, "bl_pwm_attr", &para[0], 4);
		if (ret) {
			BLERR("[%d]: failed to get bl_pwm_attr\n", bdrv->index);
			bl_pwm->pwm_method = BL_PWM_POSITIVE;
			if (bl_pwm->pwm_port == BL_PWM_VS)
				bl_pwm->pwm_freq = BL_FREQ_VS_DEFAULT;
			else
				bl_pwm->pwm_freq = BL_FREQ_DEFAULT;
			bl_pwm->pwm_duty_max = 80;
			bl_pwm->pwm_duty_min = 20;
			bl_pwm->pwm_phase = 0;
		} else {
			bl_pwm->pwm_method = para[0];
			if (bl_pwm->pwm_port == BL_PWM_VS) {
				bl_pwm->pwm_freq = para[1] & 0xff;
				bl_pwm->pwm_phase = (para[1] >> 8) & 0xffffff;
			} else {
				bl_pwm->pwm_freq = para[1];
				bl_pwm->pwm_phase = 0;
			}
			bl_pwm->pwm_duty_max = para[2];
			bl_pwm->pwm_duty_min = para[3];
		}
		if (bl_pwm->pwm_port == BL_PWM_VS) {
			if (bl_pwm->pwm_freq > 8) {
				BLERR("[%d]: bl_pwm_vs wrong freq %d\n",
				      bdrv->index, bl_pwm->pwm_freq);
				bl_pwm->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (bl_pwm->pwm_freq > XTAL_HALF_FREQ_HZ)
				bl_pwm->pwm_freq = XTAL_HALF_FREQ_HZ;
			if (bl_pwm->pwm_freq < 50)
				bl_pwm->pwm_freq = 50;
		}
		ret = of_property_read_u32_array(child, "bl_pwm_power", &para[0], 4);
		if (ret) {
			BLERR("[%d]: failed to get bl_pwm_power\n",
			      bdrv->index);
			bconf->pwm_on_delay = 0;
			bconf->pwm_off_delay = 0;
		} else {
			bconf->pwm_on_delay = para[2];
			bconf->pwm_off_delay = para[3];
		}

		bl_pwm->pwm_duty = bl_pwm->pwm_duty_min;
		/* init pwm config */
		bl_pwm_config_init(bl_pwm);
		break;
	case BL_CTRL_PWM_COMBO:
		bconf->bl_pwm_combo0 = kzalloc(sizeof(*bconf->bl_pwm_combo0), GFP_KERNEL);
		if (!bconf->bl_pwm_combo0)
			return -1;
		bconf->bl_pwm_combo1 = kzalloc(sizeof(*bconf->bl_pwm_combo1), GFP_KERNEL);
		if (!bconf->bl_pwm_combo1)
			return -1;

		pwm_combo0 = bconf->bl_pwm_combo0;
		pwm_combo1 = bconf->bl_pwm_combo1;

		pwm_combo0->index = 0;
		pwm_combo1->index = 1;
		pwm_combo0->drv_index = bdrv->index;
		pwm_combo1->drv_index = bdrv->index;

		ret = of_property_read_string_index(child, "bl_pwm_combo_port", 0, &str);
		if (ret) {
			BLERR("[%d]: failed to get bl_pwm_combo_port\n", bdrv->index);
			pwm_combo0->pwm_port = BL_PWM_MAX;
		} else {
			pwm_combo0->pwm_port = bl_pwm_str_to_num(str);
		}
		ret = of_property_read_string_index(child, "bl_pwm_combo_port", 1, &str);
		if (ret) {
			BLERR("[%d]: failed to get bl_pwm_combo_port\n", bdrv->index);
			pwm_combo1->pwm_port = BL_PWM_MAX;
		} else {
			pwm_combo1->pwm_port = bl_pwm_str_to_num(str);
		}
		BLPR("[%d]: pwm_combo_port: %s(0x%x), %s(0x%x)\n",
		     bdrv->index,
		     bl_pwm_num_to_str(pwm_combo0->pwm_port),
		     pwm_combo0->pwm_port,
		     bl_pwm_num_to_str(pwm_combo1->pwm_port),
		     pwm_combo1->pwm_port);
		ret = of_property_read_u32_array(child, "bl_pwm_combo_level_mapping",
						 &para[0], 4);
		if (ret) {
			BLERR("[%d]: failed to get bl_pwm_combo_level_mapping\n",
			      bdrv->index);
			pwm_combo0->level_max = BL_LEVEL_MAX;
			pwm_combo0->level_min = BL_LEVEL_MID;
			pwm_combo1->level_max = BL_LEVEL_MID;
			pwm_combo1->level_min = BL_LEVEL_MIN;
		} else {
			pwm_combo0->level_max = para[0];
			pwm_combo0->level_min = para[1];
			pwm_combo1->level_max = para[2];
			pwm_combo1->level_min = para[3];
		}
		ret = of_property_read_u32_array(child, "bl_pwm_combo_attr", &para[0], 8);
		if (ret) {
			BLERR("[%d]: failed to get bl_pwm_combo_attr\n", bdrv->index);
			pwm_combo0->pwm_method = BL_PWM_POSITIVE;
			if (pwm_combo0->pwm_port == BL_PWM_VS)
				pwm_combo0->pwm_freq = BL_FREQ_VS_DEFAULT;
			else
				pwm_combo0->pwm_freq = BL_FREQ_DEFAULT;
			pwm_combo0->pwm_duty_max = 80;
			pwm_combo0->pwm_duty_min = 20;
			pwm_combo1->pwm_method = BL_PWM_NEGATIVE;
			if (pwm_combo1->pwm_port == BL_PWM_VS)
				pwm_combo1->pwm_freq = BL_FREQ_VS_DEFAULT;
			else
				pwm_combo1->pwm_freq = BL_FREQ_DEFAULT;
			pwm_combo1->pwm_duty_max = 80;
			pwm_combo1->pwm_duty_min = 20;
			pwm_combo0->pwm_phase = 0;
			pwm_combo1->pwm_phase = 0;
		} else {
			pwm_combo0->pwm_method = para[0];
			if (pwm_combo0->pwm_port == BL_PWM_VS) {
				pwm_combo0->pwm_freq = para[1] & 0xff;
				pwm_combo0->pwm_phase = (para[1] >> 8) & 0xffffff;
			} else {
				pwm_combo0->pwm_freq = para[1];
				pwm_combo0->pwm_phase = 0;
			}
			pwm_combo0->pwm_duty_max = para[2];
			pwm_combo0->pwm_duty_min = para[3];
			pwm_combo1->pwm_method = para[4];
			if (pwm_combo1->pwm_port == BL_PWM_VS) {
				pwm_combo1->pwm_freq = para[5] & 0xff;
				pwm_combo1->pwm_phase = (para[5] >> 8) & 0xffffff;
			} else {
				pwm_combo1->pwm_freq = para[5];
				pwm_combo1->pwm_phase = 0;
			}
			pwm_combo1->pwm_duty_max = para[6];
			pwm_combo1->pwm_duty_min = para[7];
		}
		if (pwm_combo0->pwm_port == BL_PWM_VS) {
			if (pwm_combo0->pwm_freq > 4) {
				BLERR("[%d]: bl_pwm_0_vs wrong freq %d\n",
				      bdrv->index, pwm_combo0->pwm_freq);
				pwm_combo0->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (pwm_combo0->pwm_freq > XTAL_HALF_FREQ_HZ)
				pwm_combo0->pwm_freq = XTAL_HALF_FREQ_HZ;
			if (pwm_combo0->pwm_freq < 50)
				pwm_combo0->pwm_freq = 50;
		}
		if (pwm_combo1->pwm_port == BL_PWM_VS) {
			if (pwm_combo1->pwm_freq > 4) {
				BLERR("[%d]: bl_pwm_1_vs wrong freq %d\n",
				      bdrv->index, pwm_combo1->pwm_freq);
				pwm_combo1->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (pwm_combo1->pwm_freq > XTAL_HALF_FREQ_HZ)
				pwm_combo1->pwm_freq = XTAL_HALF_FREQ_HZ;
			if (pwm_combo1->pwm_freq < 50)
				pwm_combo1->pwm_freq = 50;
		}
		ret = of_property_read_u32_array(child, "bl_pwm_combo_power", &para[0], 6);
		if (ret) {
			BLERR("[%d]: failed to get bl_pwm_combo_power\n", bdrv->index);
			bconf->pwm_on_delay = 0;
			bconf->pwm_off_delay = 0;
		} else {
			bconf->pwm_on_delay = para[4];
			bconf->pwm_off_delay = para[5];
		}

		pwm_combo0->pwm_duty = pwm_combo0->pwm_duty_min;
		pwm_combo1->pwm_duty = pwm_combo1->pwm_duty_min;
		/* init pwm config */
		bl_pwm_config_init(pwm_combo0);
		bl_pwm_config_init(pwm_combo1);
		break;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		bconf->ldim_flag = 1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		/* get bl_extern_index from dts */
		ret = of_property_read_u32(child, "bl_extern_index", &para[0]);
		if (ret) {
			BLERR("[%d]: failed to get bl_extern_index\n", bdrv->index);
		} else {
			bconf->bl_extern_index = para[0];
			bl_extern_dev_index_add(bdrv->index, para[0]);
			BLPR("[%d]: get bl_extern_index = %d\n",
			     bdrv->index, bconf->bl_extern_index);
		}
		break;
#endif
	default:
		break;
	}

#ifdef CONFIG_AMLOGIC_BL_LDIM
	if (bconf->ldim_flag) {
		ret = aml_ldim_get_config_dts(child);
		if (ret < 0)
			return -1;
	}
#endif

	return 0;
}
#endif

static int bl_config_load_from_unifykey(struct aml_bl_drv_s *bdrv, char *key_name)
{
	struct bl_config_s *bconf = &bdrv->bconf;
	unsigned char *para;
	int key_len, len;
	unsigned char *p;
	const char *str;
	unsigned char temp;
	struct aml_lcd_unifykey_header_s *bl_header;
	struct bl_pwm_config_s *bl_pwm;
	struct bl_pwm_config_s *pwm_combo0, *pwm_combo1;
	unsigned int level, tempfreq;
	int ret;

	if (!key_name)
		return -1;

	ret = lcd_unifykey_get_size(key_name, &key_len);
	if (ret)
		return -1;

	para = kcalloc(key_len, (sizeof(unsigned char)), GFP_KERNEL);
	if (!para)
		return -1;

	ret = lcd_unifykey_get(key_name, para, key_len);
	if (ret < 0) {
		kfree(para);
		return -1;
	}

	/* step 1: check header */
	bl_header = (struct aml_lcd_unifykey_header_s *)para;
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		lcd_unifykey_header_print(para);

	/* step 2: check backlight parameters */
	switch (bl_header->version) {
	case 2:
		len = 10 + 30 + 12 + 8 + 32 + 10;
		break;
	default:
		len = 10 + 30 + 12 + 8 + 32;
		break;
	}
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		BLERR("[%d]: ukey length is incorrect\n", bdrv->index);
		kfree(para);
		return -1;
	}

	/* basic: 30byte */
	p = para;
	str = (const char *)(p + LCD_UKEY_HEAD_SIZE);
	strscpy(bconf->name, str, BL_NAME_MAX);
	bconf->index = 0;

	/* level: 6byte */
	bconf->level_uboot = (*(p + LCD_UKEY_BL_LEVEL_UBOOT) |
		((*(p + LCD_UKEY_BL_LEVEL_UBOOT + 1)) << 8));
	level = (*(p + LCD_UKEY_BL_LEVEL_KERNEL) |
		 ((*(p + LCD_UKEY_BL_LEVEL_KERNEL + 1)) << 8));
	bconf->level_max = (*(p + LCD_UKEY_BL_LEVEL_MAX) |
		((*(p + LCD_UKEY_BL_LEVEL_MAX + 1)) << 8));
	bconf->level_min = (*(p + LCD_UKEY_BL_LEVEL_MIN) |
		((*(p  + LCD_UKEY_BL_LEVEL_MIN + 1)) << 8));
	bconf->level_mid = (*(p + LCD_UKEY_BL_LEVEL_MID) |
		((*(p + LCD_UKEY_BL_LEVEL_MID + 1)) << 8));
	bconf->level_mid_mapping = (*(p + LCD_UKEY_BL_LEVEL_MID_MAP) |
		((*(p + LCD_UKEY_BL_LEVEL_MID_MAP + 1)) << 8));

	bconf->level_default = level & BL_LEVEL_MASK;
	bdrv->brightness_bypass =
		((level >> BL_POLICY_BRIGHTNESS_BYPASS_BIT) &
		 BL_POLICY_BRIGHTNESS_BYPASS_MASK);
	bdrv->step_on_flag = ((level >> BL_POLICY_POWER_ON_BIT) & BL_POLICY_POWER_ON_MASK);

	/* method: 8byte */
	temp = *(p + LCD_UKEY_BL_METHOD);
	bconf->method = (temp >= BL_CTRL_MAX) ? BL_CTRL_MAX : temp;

	if (*(p + LCD_UKEY_BL_EN_GPIO) >= BL_GPIO_NUM_MAX) {
		bconf->en_gpio = BL_GPIO_MAX;
	} else {
		bconf->en_gpio = *(p + LCD_UKEY_BL_EN_GPIO);
		bl_gpio_probe(bdrv, bconf->en_gpio);
	}
	bconf->en_gpio_on = *(p + LCD_UKEY_BL_EN_GPIO_ON);
	bconf->en_gpio_off = *(p + LCD_UKEY_BL_EN_GPIO_OFF);
	bconf->power_on_delay = (*(p + LCD_UKEY_BL_ON_DELAY) |
		((*(p + LCD_UKEY_BL_ON_DELAY + 1)) << 8));
	bconf->power_off_delay = (*(p + LCD_UKEY_BL_OFF_DELAY) |
		((*(p + LCD_UKEY_BL_OFF_DELAY + 1)) << 8));

	if (bl_header->version == 2) {
		bconf->en_sequence_reverse = (*(p + LCD_UKEY_BL_CUST_VAL_0) |
				((*(p + LCD_UKEY_BL_CUST_VAL_0 + 1)) << 8));
		/* check ldim_flag */
		if ((*(p + LCD_UKEY_BL_LDIM_ROW) > 0) && (*(p + LCD_UKEY_BL_LDIM_COL) > 0))
			bconf->ldim_flag = 1;

		/* load switch info */
		bconf->bl_pwm_switch_port = *(p + LCD_UKEY_BL_CUST_VAL_1);
		bconf->bl_pwm_switch_freq = (*(p + LCD_UKEY_BL_CUST_VAL_2) |
			((*(p + LCD_UKEY_BL_CUST_VAL_2 + 1)) << 8) |
			((*(p + LCD_UKEY_BL_CUST_VAL_2 + 2)) << 8) |
			((*(p + LCD_UKEY_BL_CUST_VAL_2 + 3)) << 8));
	} else {
		bconf->en_sequence_reverse = 0;
	}

	/* pwm: 24byte */
	switch (bconf->method) {
	case BL_CTRL_PWM:
		bconf->bl_pwm = kzalloc(sizeof(*bconf->bl_pwm), GFP_KERNEL);
		if (!bconf->bl_pwm) {
			kfree(para);
			return -1;
		}
		bl_pwm = bconf->bl_pwm;
		bl_pwm->index = 0;
		bl_pwm->drv_index = bdrv->index;

		bl_pwm->level_max = bconf->level_max;
		bl_pwm->level_min = bconf->level_min;

		bconf->pwm_on_delay = (*(p + LCD_UKEY_BL_PWM_ON_DELAY) |
			((*(p + LCD_UKEY_BL_PWM_ON_DELAY + 1)) << 8));
		bconf->pwm_off_delay = (*(p + LCD_UKEY_BL_PWM_OFF_DELAY) |
			((*(p + LCD_UKEY_BL_PWM_OFF_DELAY + 1)) << 8));
		bl_pwm->pwm_method =  *(p + LCD_UKEY_BL_PWM_METHOD);
		bl_pwm->pwm_port = *(p + LCD_UKEY_BL_PWM_PORT);
		tempfreq = (*(p + LCD_UKEY_BL_PWM_FREQ) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 1)) << 8) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 2)) << 8) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 3)) << 8));
		if (bl_pwm->pwm_port == BL_PWM_VS) {
			bl_pwm->pwm_freq = tempfreq & 0xff;
			bl_pwm->pwm_phase = (tempfreq >> 8) & 0xffffff;
			if (bl_pwm->pwm_freq > 8) {
				BLERR("bl_pwm_vs wrong freq %d\n", bl_pwm->pwm_freq);
				bl_pwm->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			bl_pwm->pwm_freq = tempfreq;
			bl_pwm->pwm_phase = 0;
			if (bl_pwm->pwm_freq > XTAL_HALF_FREQ_HZ)
				bl_pwm->pwm_freq = XTAL_HALF_FREQ_HZ;
		}
		bl_pwm->pwm_duty_max = *(p + LCD_UKEY_BL_PWM_DUTY_MAX);
		bl_pwm->pwm_duty_min = *(p + LCD_UKEY_BL_PWM_DUTY_MIN);

		bl_pwm->pwm_duty = bl_pwm->pwm_duty_min;
		bl_pwm_config_init(bl_pwm);
		break;
	case BL_CTRL_PWM_COMBO:
		bconf->bl_pwm_combo0 = kzalloc(sizeof(*bconf->bl_pwm_combo0), GFP_KERNEL);
		if (!bconf->bl_pwm_combo0) {
			kfree(para);
			return -1;
		}
		bconf->bl_pwm_combo1 = kzalloc(sizeof(*bconf->bl_pwm_combo1), GFP_KERNEL);
		if (!bconf->bl_pwm_combo1) {
			kfree(bconf->bl_pwm_combo0);
			kfree(para);
			return -1;
		}
		pwm_combo0 = bconf->bl_pwm_combo0;
		pwm_combo1 = bconf->bl_pwm_combo1;
		pwm_combo0->index = 0;
		pwm_combo1->index = 1;
		pwm_combo0->drv_index = bdrv->index;
		pwm_combo1->drv_index = bdrv->index;

		bconf->pwm_on_delay = (*(p + LCD_UKEY_BL_PWM_ON_DELAY) |
			((*(p + LCD_UKEY_BL_PWM_ON_DELAY + 1)) << 8));
		bconf->pwm_off_delay = (*(p + LCD_UKEY_BL_PWM_OFF_DELAY) |
			((*(p + LCD_UKEY_BL_PWM_OFF_DELAY + 1)) << 8));

		pwm_combo0->pwm_method = *(p + LCD_UKEY_BL_PWM_METHOD);
		pwm_combo0->pwm_port = *(p + LCD_UKEY_BL_PWM_PORT);
		tempfreq = (*(p + LCD_UKEY_BL_PWM_FREQ) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 1)) << 8) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 2)) << 8) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 3)) << 8));
		if (pwm_combo0->pwm_port == BL_PWM_VS) {
			pwm_combo0->pwm_freq = tempfreq & 0xff;
			pwm_combo0->pwm_phase = (tempfreq >> 8) & 0xffffff;
			if (pwm_combo0->pwm_freq > 8) {
				BLERR("bl_pwm_0_vs wrong freq %d\n", pwm_combo0->pwm_freq);
				pwm_combo0->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			pwm_combo0->pwm_freq = tempfreq;
			pwm_combo0->pwm_phase = 0;
			if (pwm_combo0->pwm_freq > XTAL_HALF_FREQ_HZ)
				pwm_combo0->pwm_freq = XTAL_HALF_FREQ_HZ;
		}
		pwm_combo0->pwm_duty_max = *(p + LCD_UKEY_BL_PWM_DUTY_MAX);
		pwm_combo0->pwm_duty_min = *(p + LCD_UKEY_BL_PWM_DUTY_MIN);

		pwm_combo1->pwm_method = *(p + LCD_UKEY_BL_PWM2_METHOD);
		pwm_combo1->pwm_port = *(p + LCD_UKEY_BL_PWM2_PORT);
		tempfreq = (*(p + LCD_UKEY_BL_PWM2_FREQ) |
			((*(p + LCD_UKEY_BL_PWM2_FREQ + 1)) << 8) |
			((*(p + LCD_UKEY_BL_PWM2_FREQ + 2)) << 8) |
			((*(p + LCD_UKEY_BL_PWM2_FREQ + 3)) << 8));
		if (pwm_combo1->pwm_port == BL_PWM_VS) {
			pwm_combo1->pwm_freq = tempfreq & 0xff;
			pwm_combo1->pwm_phase = (tempfreq >> 8) & 0xffffff;
			if (pwm_combo1->pwm_freq > 8) {
				BLERR("bl_pwm_1_vs wrong freq %d\n", pwm_combo1->pwm_freq);
				pwm_combo1->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			pwm_combo1->pwm_freq = tempfreq;
			pwm_combo1->pwm_phase = 0;
			if (pwm_combo1->pwm_freq > XTAL_HALF_FREQ_HZ)
				pwm_combo1->pwm_freq = XTAL_HALF_FREQ_HZ;
		}
		pwm_combo1->pwm_duty_max = *(p + LCD_UKEY_BL_PWM2_DUTY_MAX);
		pwm_combo1->pwm_duty_min = *(p + LCD_UKEY_BL_PWM2_DUTY_MIN);

		pwm_combo0->level_max = (*(p + LCD_UKEY_BL_PWM_LEVEL_MAX) |
			((*(p + LCD_UKEY_BL_PWM_LEVEL_MAX + 1)) << 8));
		pwm_combo0->level_min = (*(p + LCD_UKEY_BL_PWM_LEVEL_MIN) |
			((*(p + LCD_UKEY_BL_PWM_LEVEL_MIN + 1)) << 8));
		pwm_combo1->level_max = (*(p + LCD_UKEY_BL_PWM2_LEVEL_MAX) |
			((*(p + LCD_UKEY_BL_PWM2_LEVEL_MAX + 1)) << 8));
		pwm_combo1->level_min = (*(p + LCD_UKEY_BL_PWM2_LEVEL_MIN) |
			((*(p + LCD_UKEY_BL_PWM2_LEVEL_MIN + 1)) << 8));

		pwm_combo0->pwm_duty = pwm_combo0->pwm_duty_min;
		pwm_combo1->pwm_duty = pwm_combo1->pwm_duty_min;
		bl_pwm_config_init(pwm_combo0);
		bl_pwm_config_init(pwm_combo1);
		break;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		bconf->ldim_flag = 1;
		break;
#endif
	default:
		break;
	}

#ifdef CONFIG_AMLOGIC_BL_LDIM
	if (bconf->ldim_flag) {
		ret = aml_ldim_get_config_unifykey(para);
		if (ret < 0) {
			kfree(para);
			return -1;
		}
	}
#endif

	/* switch and default channel init */
	bl_pwm_switch_init(bdrv);

	kfree(para);
	return 0;
}

/* config from json =============================================================================*/
static struct num_str_s bl_ctrl_method[] = {
	{BL_CTRL_GPIO,          "BL_CTRL_GPIO"},
	{BL_CTRL_PWM,           "BL_CTRL_PWM"},
	{BL_CTRL_PWM_COMBO,     "BL_CTRL_PWM_COMBO"},
	{BL_CTRL_LOCAL_DIMMING, "BL_CTRL_LOCAL_DIMMING"},
	{BL_CTRL_EXTERN,        "BL_CTRL_EXTERN"},
	{BL_CTRL_MAX,           "BL_CTRL_MAX"},
};

static inline int bl_ctrl_method_str2num(const char *str)
{
	return strnum_get_num(str, bl_ctrl_method, ARRAY_SIZE(bl_ctrl_method), BL_CTRL_MAX);
}

static int bl_gpio_name_to_index(struct aml_bl_drv_s *bdrv, const char *name)
{
	int i = 0;

	if (!bdrv || !name)
		return BL_GPIO_MAX;

	for (i = 0; i < BL_GPIO_NUM_MAX; i++)
		if (!strcmp(bdrv->bconf.bl_gpio[i].name, name))
			return i;
	return BL_GPIO_MAX;
}

static int bl_config_load_from_json(struct aml_bl_drv_s *bdrv)
{
	int cnt = 0, i = 0;
	struct json_parse_s *jsp;
	struct bl_config_s *bconf = &bdrv->bconf;
	struct bl_pwm_config_s *bl_pwm, *pwms[3] = {NULL, NULL, NULL};
	const char *str = NULL;
	struct json_s *parent, *child, *child2, *child3;

	jsp = get_panel_jsp(bdrv->index);
	if (!json_parse_ok(jsp)) {
		BLERR("panel%d json not ready\n", bdrv->index);
		return -1;
	}

	parent = json_get_object_child(jsp, jsp->root, "backlight");
	if (!parent) {
		BLERR("failed find /backlight\n");
		return -1;
	}

//basic
	child = json_get_object_child(jsp, parent, "basic_info");
	if (!child) {
		BLERR("failed find basic_info\n");
		return -1;
	}

	str = json_get_obj_str(jsp, child, "name", NULL);
	if (str)
		strscpy(bconf->name, str, BL_NAME_MAX);

	bdrv->brightness_bypass = json_get_obj_u32(jsp, child, "brightness_bypass", 0);
	bdrv->step_on_flag = json_get_obj_u32(jsp, child, "step_on_flag", 0);

//level setup
	child = json_get_object_child(jsp, parent, "level_setup");
	if (!child) {
		BLERR("failed find level_setup\n");
		return -1;
	}

	child2 = json_get_object_child(jsp, child, "range");
	bconf->level_min         = json_get_arr_u32(jsp, child2, 0, BL_LEVEL_MIN);
	bconf->level_max         = json_get_arr_u32(jsp, child2, 1, BL_LEVEL_MAX);
	bconf->level_mid         = json_get_obj_u32(jsp, child, "mid", BL_LEVEL_MID);
	bconf->level_mid_mapping = json_get_obj_u32(jsp, child, "mid_mapping", BL_LEVEL_MID_MAPPED);
	bconf->level_default     = json_get_obj_u32(jsp, child, "kernel", BL_LEVEL_DEFAULT);
	bconf->level_uboot       = json_get_obj_u32(jsp, child, "uboot", BL_LEVEL_DEFAULT);

//control method
	child = json_get_object_child(jsp, parent, "control_method");
	if (!child) {
		BLERR("failed find control_method\n");
		return -1;
	}
	bconf->method  = bl_ctrl_method_str2num(json_get_obj_str(jsp, child, "method", NULL));
	bconf->en_gpio = bl_gpio_name_to_index(bdrv, json_get_obj_str(jsp, child, "en_gpio", NULL));
	bconf->en_gpio_on          = json_get_obj_u32(jsp, child, "en_gpio_on", 1);
	bconf->en_gpio_off         = json_get_obj_u32(jsp, child, "en_gpio_off", 0);
	//bconf->power_on_delay    = json_get_obj_u32(jsp, child, "bl_on_delay_ms", 0);
	//bconf->power_off_delay   = json_get_obj_u32(jsp, child, "bl_off_delay_ms", 0);
	bconf->pwm_on_delay        = json_get_obj_u32(jsp, child, "pwm_on_delay_ms", 0);
	bconf->pwm_off_delay       = json_get_obj_u32(jsp, child, "pwm_off_delay_ms", 0);
	bconf->en_sequence_reverse = json_get_obj_u32(jsp, child, "en_sequence_reverse", 0);
	bconf->bl_pwm_switch_port = json_get_obj_u32(jsp, child, "pwm_switch_port", BL_PWM_MAX);
	bconf->bl_pwm_switch_freq = json_get_obj_u32(jsp, child, "pwm_switch_freq", 0);
	bl_gpio_probe(bdrv, bconf->en_gpio);

	if (bconf->method == BL_CTRL_LOCAL_DIMMING) {
#ifdef CONFIG_AMLOGIC_BL_LDIM
		if (bdrv->index == 0) {
			bconf->ldim_flag = 1;
			return aml_ldim_get_config_json(bdrv->index);
		} else {
			return -1;
		}
#else
		BLERR("%s not support ldim\n", __func__);
		return -1;
#endif
	}

//pwms
	if (bconf->method != BL_CTRL_PWM && bconf->method != BL_CTRL_PWM_COMBO)
		return 0;

	child = json_get_object_child(jsp, child, "pwms");
	if (!child) {
		BLERR("failed find pwms\n");
		return -1;
	}
	cnt = json_get_array_size(jsp, child);
	cnt = lcd_s32_constraint(cnt, 0, 2);
	for (i = 0; i < cnt; i++) {
		child2 = json_get_array_child(jsp, child, i);
		if (!child2) {
			BLPR("fail find pwm[%d]\n", i);
			for (i--; i >= 0; i--) {
				kfree(pwms[i]);
				pwms[i] = NULL;
			}
			return -1;
		}

		pwms[i] = kzalloc(sizeof(*bl_pwm), GFP_KERNEL);
		if (!pwms[i]) {
			BLPR("error malloc bl_pwm\n");
			for (i--; i >= 0; i--) {
				kfree(pwms[i]);
				pwms[i] = NULL;
			}
			return -1;
		}

		bl_pwm = pwms[i];
		bl_pwm->drv_index = bdrv->index;
		bl_pwm->index = i;

		str = json_get_obj_str(jsp, child2, "port", NULL);
		bl_pwm->pwm_port      = bl_pwm_str_to_num(str ? str : "invalid");
		bl_pwm->pwm_method    = json_get_obj_u32(jsp, child2, "polarity", 1);
		bl_pwm->pwm_phase     = json_get_obj_u32(jsp, child2, "phase", 0);
		bl_pwm->pwm_freq      = json_get_obj_u32(jsp, child2, "freq", 180);

		if (bl_pwm->pwm_freq > XTAL_HALF_FREQ_HZ)
			bl_pwm->pwm_freq = XTAL_HALF_FREQ_HZ;

		child3 = json_get_object_child(jsp, child2, "level_range");
		if (!child3)
			BLPR("failed find pwms[%d]/level_range\n", i);
		bl_pwm->level_min = json_get_arr_u32(jsp, child3, 0, bconf->level_min);
		bl_pwm->level_max = json_get_arr_u32(jsp, child3, 1, bconf->level_max);

		child3 = json_get_object_child(jsp, child2, "duty_range");
		if (!child3)
			BLPR("failed find pwms[%d]/level_range\n", i);
		bl_pwm->pwm_duty_min = json_get_arr_u32(jsp, child3, 0, 0);
		bl_pwm->pwm_duty_max = json_get_arr_u32(jsp, child3, 1, 100);
		bl_pwm->pwm_duty = json_get_obj_u32(jsp, child2, "duty", bl_pwm->pwm_duty_min);
		bl_pwm_config_init(bl_pwm);
	}

	bconf->bl_pwm = pwms[0];
	bconf->bl_pwm_combo0 = pwms[0];
	bconf->bl_pwm_combo1 = pwms[1];

	bl_pwm_switch_init(bdrv);

	return 0;
}

static int bl_str_to_pwm_port(const char *str)
{
	char *start;

	start = strchr(str, 'P');
	if (!start)
		return BL_PWM_MAX;

	return bl_pwm_str_to_num(start);
}

static int bl_config_load_from_ini(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;
	struct bl_pwm_config_s *bl_pwm;
	struct bl_pwm_config_s *pwm_combo0, *pwm_combo1;
	void *inip, *psec;
	const char *str;
	unsigned int val, version;

	inip = get_lcd_ini_parse_mem(bdrv->index);
	if (!inip) {
		BLERR("[%d]: %s: parse_mem not ready\n", bdrv->index, __func__);
		return -1;
	}

	psec = lcd_ini_get_section(inip, "Backlight_Attr");
	if (!psec) {
		BLERR("[%d]: %s: not find Backlight_Attr\n", bdrv->index, __func__);
		return -1;
	}
	version = lcd_ini_get_val(inip, psec, "version", 0);

	str = lcd_ini_get_str(inip, psec, "bl_name", "null");
	strscpy(bconf->name, str, sizeof(bconf->name));

	bconf->level_uboot = lcd_ini_get_val(inip, psec, "bl_level_uboot", 0);
	val = lcd_ini_get_val(inip, psec, "bl_level_kernel", 0);
	bconf->level_default = val & BL_LEVEL_MASK;
	bdrv->brightness_bypass =
		((val >> BL_POLICY_BRIGHTNESS_BYPASS_BIT) &
		 BL_POLICY_BRIGHTNESS_BYPASS_MASK);
	bdrv->step_on_flag = ((val >> BL_POLICY_POWER_ON_BIT) & BL_POLICY_POWER_ON_MASK);

	bconf->level_max = lcd_ini_get_val(inip, psec, "bl_level_max", 0);
	bconf->level_min = lcd_ini_get_val(inip, psec, "bl_level_min", 0);
	bconf->level_mid = lcd_ini_get_val(inip, psec, "bl_level_mid", 0);
	bconf->level_mid_mapping = lcd_ini_get_val(inip, psec, "bl_level_mid_mapping", 0);

	str = lcd_ini_get_str(inip, psec, "bl_method", "null");
	bconf->method = bl_ctrl_method_str2num(str);

	bconf->en_gpio = lcd_ini_get_val(inip, psec, "bl_en_gpio", 0xff);
	bconf->en_gpio_on = lcd_ini_get_val(inip, psec, "bl_en_gpio_on", 0);
	bconf->en_gpio_off = lcd_ini_get_val(inip, psec, "bl_en_gpio_off", 0);
	bconf->power_on_delay = lcd_ini_get_val(inip, psec, "bl_on_delay", 0);
	bconf->power_off_delay = lcd_ini_get_val(inip, psec, "bl_off_delay", 0);
	bl_gpio_probe(bdrv, bconf->en_gpio);

	bconf->en_sequence_reverse = lcd_ini_get_val(inip, psec, "bl_custome_val_0", 0);
	bconf->bl_pwm_switch_port = lcd_ini_get_val(inip, psec, "bl_custome_val_1", 0);
	bconf->bl_pwm_switch_freq = lcd_ini_get_val(inip, psec, "bl_custome_val_2", 0);
	//val = lcd_ini_get_val(inip, psec, "bl_custome_val_3", 0);
	//val = lcd_ini_get_val(inip, psec, "bl_custome_val_4", 0);

	if (lcd_ini_get_str(inip, psec, "bl_ldim_row", 0) > 0 &&
	    lcd_ini_get_str(inip, psec, "bl_ldim_col", 0) > 0)
		bconf->ldim_flag = 1;

	switch (bconf->method) {
	case BL_CTRL_PWM:
		bconf->bl_pwm = kzalloc(sizeof(*bconf->bl_pwm), GFP_KERNEL);
		if (!bconf->bl_pwm)
			return -1;
		bl_pwm = bconf->bl_pwm;
		bl_pwm->index = 0;
		bl_pwm->drv_index = bdrv->index;

		bl_pwm->level_max = bconf->level_max;
		bl_pwm->level_min = bconf->level_min;

		str = lcd_ini_get_str(inip, psec, "pwm_method", "null");
		bl_pwm->pwm_method = bl_str_to_pwm_method(str, BL_PWM_POSITIVE);

		str = lcd_ini_get_str(inip, psec, "pwm_port", "null");
		bl_pwm->pwm_port = bl_str_to_pwm_port(str);

		val = lcd_ini_get_val(inip, psec, "pwm_freq", 0);
		if (bl_pwm->pwm_port == BL_PWM_VS) {
			bl_pwm->pwm_freq = val & 0xff;
			bl_pwm->pwm_phase = (val >> 8) & 0xffffff;
		} else {
			bl_pwm->pwm_freq = val;
			bl_pwm->pwm_phase = 0;
		}

		bl_pwm->pwm_duty_max = lcd_ini_get_val(inip, psec, "pwm_duty_max", 0);
		bl_pwm->pwm_duty_min = lcd_ini_get_val(inip, psec, "pwm_duty_min", 0);

		bconf->pwm_on_delay = lcd_ini_get_val(inip, psec, "pwm_on_delay", 0);
		bconf->pwm_off_delay = lcd_ini_get_val(inip, psec, "pwm_off_delay", 0);

		bl_pwm->pwm_duty = bl_pwm->pwm_duty_min;
		bl_pwm_config_init(bl_pwm);
		break;
	case BL_CTRL_PWM_COMBO:
		bconf->bl_pwm_combo0 = kzalloc(sizeof(*bconf->bl_pwm_combo0), GFP_KERNEL);
		if (!bconf->bl_pwm_combo0)
			return -1;
		bconf->bl_pwm_combo1 = kzalloc(sizeof(*bconf->bl_pwm_combo1), GFP_KERNEL);
		if (!bconf->bl_pwm_combo1) {
			kfree(bconf->bl_pwm_combo0);
			return -1;
		}
		pwm_combo0 = bconf->bl_pwm_combo0;
		pwm_combo1 = bconf->bl_pwm_combo1;
		pwm_combo0->index = 0;
		pwm_combo1->index = 1;
		pwm_combo0->drv_index = bdrv->index;
		pwm_combo1->drv_index = bdrv->index;

		str = lcd_ini_get_str(inip, psec, "pwm_method", "null");
		pwm_combo0->pwm_method = bl_str_to_pwm_method(str, BL_PWM_POSITIVE);

		str = lcd_ini_get_str(inip, psec, "pwm_port", "null");
		pwm_combo0->pwm_port = bl_str_to_pwm_port(str);

		val = lcd_ini_get_val(inip, psec, "pwm_freq", 0);
		if (pwm_combo0->pwm_port == BL_PWM_VS) {
			pwm_combo0->pwm_freq = val & 0xff;
			pwm_combo0->pwm_phase = (val >> 8) & 0xffffff;
		} else {
			pwm_combo0->pwm_freq = val;
			pwm_combo0->pwm_phase = 0;
		}

		pwm_combo0->pwm_duty_max = lcd_ini_get_val(inip, psec, "pwm_duty_max", 0);
		pwm_combo0->pwm_duty_min = lcd_ini_get_val(inip, psec, "pwm_duty_min", 0);

		str = lcd_ini_get_str(inip, psec, "pwm2_method", "null");
		pwm_combo1->pwm_method = bl_str_to_pwm_method(str, BL_PWM_POSITIVE);

		str = lcd_ini_get_str(inip, psec, "pwm2_port", "null");
		pwm_combo1->pwm_port = bl_str_to_pwm_port(str);

		val = lcd_ini_get_val(inip, psec, "pwm2_freq", 0);
		if (pwm_combo1->pwm_port == BL_PWM_VS) {
			pwm_combo1->pwm_freq = val & 0xff;
			pwm_combo1->pwm_phase = (val >> 8) & 0xffffff;
		} else {
			pwm_combo1->pwm_freq = val;
			pwm_combo1->pwm_phase = 0;
		}

		pwm_combo1->pwm_duty_max = lcd_ini_get_val(inip, psec, "pwm2_duty_max", 0);
		pwm_combo1->pwm_duty_min = lcd_ini_get_val(inip, psec, "pwm2_duty_min", 0);

		pwm_combo0->level_max = lcd_ini_get_val(inip, psec, "pwm_level_max", 0);
		pwm_combo0->level_min = lcd_ini_get_val(inip, psec, "pwm_level_min", 0);
		pwm_combo1->level_max = lcd_ini_get_val(inip, psec, "pwm2_level_max", 0);
		pwm_combo1->level_min = lcd_ini_get_val(inip, psec, "pwm2_level_min", 0);

		bconf->pwm_on_delay = lcd_ini_get_val(inip, psec, "pwm_on_delay", 0);
		bconf->pwm_off_delay = lcd_ini_get_val(inip, psec, "pwm_off_delay", 0);

		pwm_combo0->pwm_duty = pwm_combo0->pwm_duty_min;
		pwm_combo1->pwm_duty = pwm_combo1->pwm_duty_min;
		bl_pwm_config_init(pwm_combo0);
		bl_pwm_config_init(pwm_combo1);
		break;
#ifdef CONFIG_AML_LCD_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		bconf->ldim_flag = 1;
		break;
#endif
	default:
		break;
	}

#ifdef CONFIG_AMLOGIC_BL_LDIM
	if (bconf->ldim_flag)
		return aml_ldim_get_config_ini(inip, psec);
#endif

	bl_pwm_switch_init(bdrv);

	return 0;
}

int lcd_bl_check_config_load(struct aml_bl_drv_s *drv)
{
	drv->config_load = lcd_panel_config_load_detect(drv->index, drv->key_valid, __func__);
	if (drv->config_load == LCD_CONFIG_NONE || drv->config_load == LCD_CONFIG_ERR)
		return -1;

	return 0;
}

int bl_config_load(struct aml_bl_drv_s *bdrv, struct platform_device *pdev, int level_bootup)
{
	char ukey_name[15];
	phandle pwm_phandle;
	int ret = 0;
	unsigned char file_type = PANEL_FILE_INVILD;

	if (bdrv->index == 0)
		sprintf(ukey_name, "backlight");
	else
		sprintf(ukey_name, "backlight%d", bdrv->index);

	switch (bdrv->config_load) {
#ifdef CONFIG_OF
	case LCD_CONFIG_DTS:
		ret = bl_config_load_from_dts(bdrv);
		break;
#endif
	case LCD_CONFIG_UKEY:
		ret = bl_config_load_from_unifykey(bdrv, ukey_name);
		break;
	case LCD_CONFIG_FILE:
		file_type = get_lcd_panel_file_type(bdrv->index);
		if (file_type == PANEL_FILE_JSON)
			ret = bl_config_load_from_json(bdrv);
		else if (file_type == PANEL_FILE_INI)
			ret = bl_config_load_from_ini(bdrv);
		break;
	default:
		ret = -1;
		break;
	}

	if (ret)
		return -1;

	if (level_bootup)
		bdrv->bconf.level_uboot = level_bootup;
	bdrv->level_init_on = (bdrv->bconf.level_uboot > 0) ?
				bdrv->bconf.level_uboot : BL_LEVEL_DEFAULT;

	bl_config_print(bdrv);

#ifdef CONFIG_AMLOGIC_BL_LDIM
	if (bdrv->bconf.ldim_flag)
		aml_ldim_probe(pdev);
#endif

	bdrv->res_vsync_irq[0] = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "vsync");
	if (!bdrv->res_vsync_irq[0]) {
		ret = -ENODEV;
		BLERR("[%d]: bl_vsync_irq[0] resource error\n", bdrv->index);
		return -1;
	}

	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		if (bdrv->bconf.bl_pwm->pwm_port >= BL_PWM_VS)
			break;
		ret = of_property_read_u32(bdrv->dev->of_node, "bl_pwm_config", &pwm_phandle);
		if (ret) {
			BLERR("%s: not match bl_pwm_config node\n", __func__);
			return -1;
		}
		ret = bl_pwm_channel_register(bdrv->dev, pwm_phandle, bdrv->bconf.bl_pwm);
		if (ret)
			return -1;
		break;
	case BL_CTRL_PWM_COMBO:
		ret = of_property_read_u32(bdrv->dev->of_node, "bl_pwm_config", &pwm_phandle);
		if (ret) {
			BLERR("%s: not match bl_pwm_config node\n", __func__);
			return -1;
		}
		if (bdrv->bconf.bl_pwm_combo0->pwm_port < BL_PWM_VS) {
			ret = bl_pwm_channel_register(bdrv->dev, pwm_phandle,
						      bdrv->bconf.bl_pwm_combo0);
			if (ret)
				return -1;
		}
		if (bdrv->bconf.bl_pwm_combo1->pwm_port < BL_PWM_VS) {
			ret = bl_pwm_channel_register(bdrv->dev, pwm_phandle,
						      bdrv->bconf.bl_pwm_combo1);
			if (ret)
				return -1;
		}
		break;
	default:
		break;
	}

	/* switch channel register */
	if (bdrv->bconf.bl_pwm_switch && bdrv->bconf.bl_pwm_switch->pwm_port < BL_PWM_MAX) {
		BLPR("[%d]: bl_pwm_switch_port channel register: %d\n",
				bdrv->index, bdrv->bconf.bl_pwm_switch_port);
		ret = of_property_read_u32(bdrv->dev->of_node, "bl_pwm_config", &pwm_phandle);
		if (ret) {
			BLERR("%s: not match bl_pwm_config node\n", __func__);
			return -1;
		}
		ret = bl_pwm_channel_register(bdrv->dev, pwm_phandle,
					      bdrv->bconf.bl_pwm_switch);
		if (ret)
			return -1;

		bdrv->bconf.bl_pwm_switch_flag = 0;
		bdrv->state &= ~BL_STATE_PWM_SWITCH;
	}

	ret = bl_config_load_post(bdrv);
	return ret;
}
