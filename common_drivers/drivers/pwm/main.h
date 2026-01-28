/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _PWM_MAIN_H__
#define _PWM_MAIN_H__

#if IS_ENABLED(CONFIG_AMLOGIC_PWM_MESON)
int pwm_meson_init(void);
void pwm_meson_exit(void);
#else
static inline int pwm_meson_init(void)
{
	return 0;
}

static inline void pwm_meson_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_PWM_MESON_TEE)
int pwm_meson_tee_init(void);
void pwm_meson_tee_exit(void);
#else
static inline int pwm_meson_tee_init(void)
{
	return 0;
}

static inline void pwm_meson_tee_exit(void)
{
}
#endif

#endif /* _PMM_MAIN_H__ */
