# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

MODULE_NAME = amlogic-pwm

PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
ccflags-y += $(PR_FMT_DEFINE)

obj-$(CONFIG_AMLOGIC_PWM) = $(MODULE_NAME).o
$(MODULE_NAME)-y += main.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_PWM_MESON) += pwm-meson.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_PWM_MESON) += pwm-meson-sysfs.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_PWM_MESON_TEE) += pwm-meson-tee.o