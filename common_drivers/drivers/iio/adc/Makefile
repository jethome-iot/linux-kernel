# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

MODULE_NAME = amlogic-adc

PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
ccflags-y += $(PR_FMT_DEFINE)

obj-$(CONFIG_AMLOGIC_ADC) = $(MODULE_NAME).o
$(MODULE_NAME)-y = main.o

$(MODULE_NAME)-$(CONFIG_AMLOGIC_MESON_SARADC) += \
	meson_saradc.o \
	meson_saradc_c2.o \
	meson_saradc_m8.o \
	meson_saradc_s7.o

$(MODULE_NAME)-$(CONFIG_AMLOGIC_SARADC_COMMON) += \
	amlogic_saradc_common.o

$(MODULE_NAME)-$(CONFIG_AMLOGIC_SARADC) += \
	amlogic_saradc.o

$(MODULE_NAME)-$(CONFIG_AMLOGIC_PDD) += \
	amlogic_pdd.o \
	amlogic_pdd_monitor.o

$(MODULE_NAME)-$(CONFIG_AMLOGIC_SARADC_TO_GPIO) += \
	amlogic_saradc_to_gpio.o
