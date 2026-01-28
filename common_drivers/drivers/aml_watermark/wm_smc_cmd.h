/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __WM_SMC_CMD__
#define __WM_SMC_CMD__

#include <linux/arm-smccc.h>

#define OPTEE_SMC_STD_CALL_VAL(func_num) \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_STD_CALL, ARM_SMCCC_SMC_32, \
			   ARM_SMCCC_OWNER_TRUSTED_OS, (func_num))
#define OPTEE_SMC_FAST_CALL_VAL(func_num) \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32, \
			   ARM_SMCCC_OWNER_TRUSTED_OS, (func_num))

/*
 * flush watermark
 *
 * Call register usage:
 * a0      SMC Function ID, OPTEE_SMC_FLUSH_WM
 * a1      smc type: 0xFFFF0010 is vxwm plugin, 0xFFFF0011 is vxwm mode changed,
 *         0xFFFF0020 is ngwm plugin, 0xFFFF0021 is ngwm mode changed
 * a2-a7   not used
 *
 * Normal return register usage:
 * a0      result
 * a1-a7   not used
 */
#define OPTEE_SMC_FUNCID_FLUSH_WM             0xE000
#define OPTEE_SMC_FLUSH_WM \
	OPTEE_SMC_FAST_CALL_VAL(OPTEE_SMC_FUNCID_FLUSH_WM)

/*
 * check watermark status
 *
 * Call register usage:
 * a0      SMC Function ID, OPTEE_SMC_CHECK_WM_STATUS
 * a1      smc type: 0xFFFF0010 is vxwm, 0xFFFF0020 is ngwm
 * a2-a7   not used
 *
 * Normal return register usage:
 * a0      result
 * a1-a7   not used
 */
#define OPTEE_SMC_FUNCID_CHECK_WM_STATUS      0xE003
#define OPTEE_SMC_CHECK_WM_STATUS \
	OPTEE_SMC_FAST_CALL_VAL(OPTEE_SMC_FUNCID_CHECK_WM_STATUS)

#endif // __WM_SMC_CMD__
