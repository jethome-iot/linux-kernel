/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AMLOGIC_SARADC_MAIN_H_
#define __AMLOGIC_SARADC_MAIN_H_

#if IS_ENABLED(CONFIG_AMLOGIC_MESON_SARADC)
int meson_sar_adc_driver_init(void);
void meson_sar_adc_driver_exit(void);
#else
static inline int meson_sar_adc_driver_init(void)
{
	return 0;
}

static inline void meson_sar_adc_driver_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_SARADC_COMMON)
int amlogic_saradc_common_driver_init(void);
void amlogic_saradc_common_driver_exit(void);
#else
static inline int amlogic_saradc_common_driver_init(void)
{
	return 0;
}

static inline void amlogic_saradc_common_driver_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_SARADC)
int amlogic_saradc_driver_init(void);
void amlogic_saradc_driver_exit(void);
#else
static inline int amlogic_saradc_driver_init(void)
{
	return 0;
}

static inline void amlogic_saradc_driver_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_PDD)
int amlogic_pdd_driver_init(void);
void amlogic_pdd_driver_exit(void);
int amlogic_pdd_monitor_driver_init(void);
void amlogic_pdd_monitor_driver_exit(void);
#else
static inline int amlogic_pdd_driver_init(void)
{
	return 0;
}

static inline void amlogic_pdd_driver_exit(void)
{
}

static inline int amlogic_pdd_monitor_driver_init(void)
{
	return 0;
}

static inline void amlogic_pdd_monitor_driver_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_SARADC_TO_GPIO)
int amlogic_saradc_to_gpio_driver_init(void);
void amlogic_saradc_to_gpio_driver_exit(void);
#else
static inline int amlogic_saradc_to_gpio_driver_init(void)
{
	return 0;
}

static inline void amlogic_saradc_to_gpio_driver_exit(void)
{
}
#endif

#endif /* __AMLOGIC_SARADC_MAIN_H_ */
