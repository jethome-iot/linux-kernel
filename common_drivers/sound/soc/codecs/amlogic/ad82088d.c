// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * ad82088d.c  --  ad82088d ALSA SoC Audio driver
 *
 * Copyright 1998 Elite Semiconductor Memory Technology
 *
 * Author: ESMT Audio/Power Product BU Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/initval.h>
#include <linux/regmap.h>
#include <linux/version.h>

#include "ad82088d.h"

// after enable,you can use "tinymix" or "amixer" cmd to change eq mode.
// If you do not need to change eq mode, you can ignore it
//#define	AD82088D_CHANGE_EQ_MODE_EN

#ifdef AD82088D_CHANGE_EQ_MODE_EN
	// eq file, add Parameter file in "codec\ad82088d_eq", and then include the header file.
	#include "ad82088d_eq/ad82088d_eq_mode_1.h"
	#include "ad82088d_eq/ad82088d_eq_mode_2.h"

	// the value range of eq mode, modify them according to your request.
	#define AD82088D_EQ_MODE_MIN 1
	#define AD82088D_EQ_MODE_MAX 2
#endif

/* Define how often to check (and clear) the fault status register (in ms) */
#define AD82088D_FAULT_CHECK_INTERVAL		500

// you can read out the register and ram data, and check them.
//#define AD82088D_REG_RAM_CHECK

enum ad82088d_type {
	AD82088D,
};

//static const char * const ad82088d_supply_names[] = {
//	"dvdd",		/* Digital power supply. Connect to 3.3-V supply. */
//	"pvdd",		/* Class-D amp and analog power supply (connected). */
//};

//#define AD82088D_NUM_SUPPLIES	ARRAY_SIZE(ad82088d_supply_names)

static int m_reg_tab[AD82088D_REGISTER_COUNT][2] = {
	{0x00, 0x04},//##AGC_ALPHA
	{0x01, 0x82},//##State_Control_2
	{0x02, 0x00},//##State_Control_3
	{0x03, 0x00},//##Master_volume_control
	{0x04, 0x18},//##Channel_1_volume_control
	{0x05, 0x18},//##Channel_2_volume_control
	{0x06, 0x18},//##Channel_3_volume_control
	{0x07, 0x18},//##Channel_4_volume_control
	{0x08, 0x18},//##Channel_5_volume_control
	{0x09, 0x18},//##Channel_6_volume_control
	{0x0a, 0x00},//##Reserve
	{0x0b, 0x00},//##Reserve
	{0x0c, 0x90},//##State_Control_4
	{0x0d, 0xc0},//##Channel_1_configuration_registers
	{0x0e, 0xc0},//##Channel_2_configuration_registers
	{0x0f, 0xc0},//##Channel_3_configuration_registers
	{0x10, 0xc0},//##Channel_4_configuration_registers
	{0x11, 0xc0},//##Channel_5_configuration_registers
	{0x12, 0xc0},//##Channel_6_configuration_registers
	{0x13, 0xc0},//##Channel_7_configuration_registers
	{0x14, 0xc0},//##Channel_8_configuration_registers
	{0x15, 0x62},//##Q_PWM_DUTY
	{0x16, 0x20},//##clock_and_FS_detection
	{0x17, 0x10},//##clock_det
	{0x18, 0x40},//##DTC
	{0x19, 0x06},//##Error_Delay
	{0x1a, 0xaa},//##State_Control_5
	{0x1b, 0x80},//##State_Control_6
	{0x1c, 0x80},//##State_Control_7
	{0x1d, 0x00},//##Coefficient_RAM_Base_Address
	{0x1e, 0x00},//##First_4-bits_of_coefficients_A1
	{0x1f, 0x00},//##Second_8-bits_of_coefficients_A1
	{0x20, 0x00},//##Third_8-bits_of_coefficients_A1
	{0x21, 0x00},//##Fourth-bits_of_coefficients_A1
	{0x22, 0x00},//##First_4-bits_of_coefficients_A2
	{0x23, 0x00},//##Second_8-bits_of_coefficients_A2
	{0x24, 0x00},//##Third_8-bits_of_coefficients_A2
	{0x25, 0x00},//##Fourth_8-bits_of_coefficients_A2
	{0x26, 0x00},//##First_4-bits_of_coefficients_B1
	{0x27, 0x00},//##Second_8-bits_of_coefficients_B1
	{0x28, 0x00},//##Third_8-bits_of_coefficients_B1
	{0x29, 0x00},//##Fourth_8-bits_of_coefficients_B1
	{0x2a, 0x00},//##First_4-bits_of_coefficients_B2
	{0x2b, 0x00},//##Second_8-bits_of_coefficients_B2
	{0x2c, 0x00},//##Third_8-bits_of_coefficients_B2
	{0x2d, 0x00},//##Fourth-bits_of_coefficients_B2
	{0x2e, 0x00},//##First_4-bits_of_coefficients_A0
	{0x2f, 0x80},//##Second_8-bits_of_coefficients_A0
	{0x30, 0x00},//##Third_8-bits_of_coefficients_A0
	{0x31, 0x00},//##Fourth_8-bits_of_coefficients_A0
	{0x32, 0x00},//##Coefficient_RAM_R/W_control
	{0x33, 0x06},//##State_Control_8
	{0x34, 0x00},//##AGL_control
	{0x35, 0x00},//##Volume_Fine_tune
	{0x36, 0x00},//##Volume_Fine_tune
	{0x37, 0x6c},//##Device_ID_register
	{0x38, 0x00},//##Level_Meter_Clear
	{0x39, 0x00},//##Power_Meter_Clear
	{0x3a, 0x00},//##First_8bits_of_C1_Level_Meter
	{0x3b, 0x07},//##Second_8bits_of_C1_Level_Meter
	{0x3c, 0xa4},//##Third_8bits_of_C1_Level_Meter
	{0x3d, 0xa3},//##Fourth_8bits_of_C1_Level_Meter
	{0x3e, 0x00},//##First_8bits_of_C2_Level_Meter
	{0x3f, 0x07},//##Second_8bits_of_C2_Level_Meter
	{0x40, 0x95},//##Third_8bits_of_C2_Level_Meter
	{0x41, 0x92},//##Fourth_8bits_of_C2_Level_Meter
	{0x42, 0x00},//##First_8bits_of_C3_Level_Meter
	{0x43, 0x00},//##Second_8bits_of_C3_Level_Meter
	{0x44, 0x00},//##Third_8bits_of_C3_Level_Meter
	{0x45, 0x00},//##Fourth_8bits_of_C3_Level_Meter
	{0x46, 0x00},//##First_8bits_of_C4_Level_Meter
	{0x47, 0x00},//##Second_8bits_of_C4_Level_Meter
	{0x48, 0x00},//##Third_8bits_of_C4_Level_Meter
	{0x49, 0x00},//##Fourth_8bits_of_C4_Level_Meter
	{0x4a, 0x00},//##First_8bits_of_C5_Level_Meter
	{0x4b, 0x00},//##Second_8bits_of_C5_Level_Meter
	{0x4c, 0x00},//##Third_8bits_of_C5_Level_Meter
	{0x4d, 0x00},//##Fourth_8bits_of_C5_Level_Meter
	{0x4e, 0x00},//##First_8bits_of_C6_Level_Meter
	{0x4f, 0x00},//##Second_8bits_of_C6_Level_Meter
	{0x50, 0x00},//##Third_8bits_of_C6_Level_Meter
	{0x51, 0x00},//##Fourth_8bits_of_C6_Level_Meter
	{0x52, 0x00},//##First_8bits_of_C7_Level_Meter
	{0x53, 0x00},//##Second_8bits_of_C7_Level_Meter
	{0x54, 0x00},//##Third_8bits_of_C7_Level_Meter
	{0x55, 0x00},//##Fourth_8bits_of_C7_Level_Meter
	{0x56, 0x00},//##First_8bits_of_C8_Level_Meter
	{0x57, 0x00},//##Second_8bits_of_C8_Level_Meter
	{0x58, 0x00},//##Third_8bits_of_C8_Level_Meter
	{0x59, 0x00},//##Fourth_8bits_of_C8_Level_Meter
	{0x5a, 0x07},//##I2S_data_output_selection_register
	{0x5b, 0x00},//##Mono_Key_High_Byte
	{0x5c, 0x00},//##Mono_Key_Low_Byte
	{0x5d, 0x07},//##Hi-res_Item
	{0x5e, 0x00},//##TDM_Word_Width_Selection
	{0x5f, 0x00},//##TDM_Offset
	{0x60, 0x00},//##Channel1_EQ_bypass_1
	{0x61, 0x00},//##Channel1_EQ_bypass_2
	{0x62, 0x00},//##Channel1_EQ_bypass_3
	{0x63, 0x00},//##Channel2_EQ_bypass_1
	{0x64, 0x00},//##Channel2_EQ_bypass_2
	{0x65, 0x00},//##Channel2_EQ_bypass_3
	{0x66, 0x00},//##Compensate_filter
	{0x67, 0x6a},//##Quatenary_Ternary_Switch_Level
	{0x68, 0x0e},//##PWM_shift
	{0x69, 0xff},//##Error_Register
	{0x6a, 0xff},//##Error_Latch_Register
	{0x6b, 0x00},//##Error_Clear_Register
	{0x6c, 0x02},//##FS_and_PMF_read_out
	{0x6d, 0x88},//##OC_Selection
	{0x6e, 0x01},//##PD_Control
	{0x6f, 0x60},//##OC_Bypass_GVDD_Selection
	{0x70, 0x62},//##BSOV_BSUV_Selection
	{0x71, 0x40},//##Testmode_register1
	{0x72, 0xcc},//##Testmode_register2
	{0x73, 0x00},//##ATTACK_HOLD_SET
	{0x74, 0x00},//##Reserve
	{0x75, 0x05},//##First_4bits_of_MBIST_User_Program_Even
	{0x76, 0x55},//##Second_8bits_of_MBIST_User_Program_Even
	{0x77, 0x55},//##Third_8bits_of_MBIST_User_Program_Even
	{0x78, 0x55},//##Fourth_8bits_of_MBIST_User_Program_Even
	{0x79, 0x55},//##Fifth_8bits_of_MBIST_User_Program_Even
	{0x7a, 0x05},//##First_8bits_of_MBIST_User_Program_Odd
	{0x7b, 0x55},//##Second_8bits_of_MBIST_User_Program_Odd
	{0x7c, 0x55},//##Third_8bits_of_MBIST_User_Program_Odd
	{0x7d, 0x55},//##Fourth_8bits_of_MBIST_User_Program_Odd
	{0x7e, 0x55},//##Fifth_8bits_of_MBIST_User_Program_Odd
	{0x7f, 0x06},//##IPD_FBD_read
	{0x80, 0x80},//##pll_control
	{0x81, 0x09},//##pll_control2
	{0x82, 0x00},//##Protection_register_set
	{0x83, 0x00},//##Memory_MBIST_status
	{0x84, 0x00},//##PWM_output_control
	{0x85, 0x00},//##Testmode_control_register
	{0x86, 0x00},//##RAM1_test_register_address
	{0x87, 0x00},//##First_4bits_of_RAM1_data
	{0x88, 0x00},//##Second_8bits_of_RAM1_Data
	{0x89, 0x00},//##Third_8bits_of_RAM1_data
	{0x8a, 0x00},//##Fourth_8bits_of_RAM1_Data
	{0x8b, 0x00},//##Fifth_8bits_of_RAM1_Data
	{0x8c, 0x00},//##RAM1_test_r/w_control
	{0x8d, 0x00},//##RAM2_test_register_address
	{0x8e, 0x00},//##First_4bits_of_RAM2_data
	{0x8f, 0x00},//##Second_8bits_of_RAM2_Data
	{0x90, 0x00},//##Third_8bits_of_RAM2_data
	{0x91, 0x00},//##Fourth_8bits_of_RAM2_Data
	{0x92, 0x00},//##Fifth_8bits_of_RAM2_Data
	{0x93, 0x00},//##RAM2_test_r/w_control
	{0x94, 0x10},//##CLKDET_read_out2
	{0x95, 0x02},//##CLKDET_read_out3
	{0x96, 0x09},//##CLKDET_read_out4
	{0x97, 0x90},//##CLKDET_read_out5
	{0x98, 0x3f},//##CLKDET_read_out6
	{0x99, 0x00},//##CLKDET_read_out7
	{0x9a, 0x02},//##CLKDET_read_out8
	{0x9b, 0x04},//##RE_COUNT_CTRL
	{0x9c, 0xc9},//##RE_COUNT_Stable_CTRL
	{0x9d, 0x00},//##BURN_EN1
	{0x9e, 0x00},//##BURN_EN2
	{0x9f, 0x15},//##OSC_TRIM_CTRL
	{0xa0, 0x00},//##RAM_clock_gating_cell_control
	{0xa1, 0x04},//##FS96k_window
	{0xa2, 0x04},//##FS48k_window
	{0xa3, 0x04},//##FS16k_window
	{0xa4, 0x04},//##FS8k_window
	{0xa5, 0x06},//##GVDD_UV_clear_OC
	{0xa6, 0x80},//##SYNC_LR
};

static int m_ram1_tab[][5] = {
	{0x00, 0x00, 0x00, 0x00, 0x00},//##AGC_AT_RT
	{0x01, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ1_A2
	{0x02, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ1_B1
	{0x03, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ1_B2
	{0x04, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ1_A0
	{0x05, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ2_A1
	{0x06, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ2_A2
	{0x07, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ2_B1
	{0x08, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ2_B2
	{0x09, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ2_A0
	{0x0a, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ3_A1
	{0x0b, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ3_A2
	{0x0c, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ3_B1
	{0x0d, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ3_B2
	{0x0e, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ3_A0
	{0x0f, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ4_A1
	{0x10, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ4_A2
	{0x11, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ4_B1
	{0x12, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ4_B2
	{0x13, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ4_A0
	{0x14, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ5_A1
	{0x15, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ5_A2
	{0x16, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ5_B1
	{0x17, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ5_B2
	{0x18, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ5_A0
	{0x19, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ6_A1
	{0x1a, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ6_A2
	{0x1b, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ6_B1
	{0x1c, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ6_B2
	{0x1d, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ6_A0
	{0x1e, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ7_A1
	{0x1f, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ7_A2
	{0x20, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ7_B1
	{0x21, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ7_B2
	{0x22, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ7_A0
	{0x23, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ8_A1
	{0x24, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ8_A2
	{0x25, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ8_B1
	{0x26, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ8_B2
	{0x27, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ8_A0
	{0x28, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ9_A1
	{0x29, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ9_A2
	{0x2a, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ9_B1
	{0x2b, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ9_B2
	{0x2c, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ9_A0
	{0x2d, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ10_A1
	{0x2e, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ10_A2
	{0x2f, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ10_B1
	{0x30, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ10_B2
	{0x31, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ10_A0
	{0x32, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ11_A1
	{0x33, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ11_A2
	{0x34, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ11_B1
	{0x35, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ11_B2
	{0x36, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ11_A0
	{0x37, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ12_A1
	{0x38, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ12_A2
	{0x39, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ12_B1
	{0x3a, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ12_B2
	{0x3b, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ12_A0
	{0x3c, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ13_A1
	{0x3d, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ13_A2
	{0x3e, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ13_B1
	{0x3f, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ13_B2
	{0x40, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ13_A0
	{0x41, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ14_A1
	{0x42, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ14_A2
	{0x43, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ14_B1
	{0x44, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ14_B2
	{0x45, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ14_A0
	{0x46, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ15_A1
	{0x47, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ15_A2
	{0x48, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ15_B1
	{0x49, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ15_B2
	{0x4a, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ15_A0
	{0x4b, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ16_A1
	{0x4c, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ16_A2
	{0x4d, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ16_B1
	{0x4e, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ16_B1
	{0x4f, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ16_B1
	{0x50, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ17_A1
	{0x51, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ17_A2
	{0x52, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ17_B1
	{0x53, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ17_B2
	{0x54, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ17_A0
	{0x55, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ18_A1
	{0x56, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ18_A2
	{0x57, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ18_B1
	{0x58, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ18_B2
	{0x59, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ18_A0
	{0x5a, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ19_A1
	{0x5b, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ19_A2
	{0x5c, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ19_B1
	{0x5d, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ19_B2
	{0x5e, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ19_A0
	{0x5f, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ20_A1
	{0x60, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ20_A2
	{0x61, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ20_B1
	{0x62, 0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ20_B2
	{0x63, 0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ20_A0
	{0x64, 0x07, 0xff, 0xff, 0xff},//##Channel_1_Mixer1
	{0x65, 0x00, 0x00, 0x00, 0x00},//##Channel_1_Mixer2
	{0x66, 0x00, 0x7e, 0x88, 0xe0},//##Channel_1_Prescale
	{0x67, 0x02, 0x00, 0x00, 0x00},//##Channel_1_Postscale
	{0x68, 0x00, 0x80, 0x00, 0x00},//##I2SO_LCH_GAIN
	{0x69, 0x00, 0x00, 0x00, 0x00},//##DRC1_Power_Meter
	{0x6a, 0x00, 0x00, 0x00, 0x00},//##DRC3_Power_Meter
	{0x6b, 0x00, 0x00, 0x00, 0x00},//##DRC5_Power_Meter
	{0x6c, 0x00, 0x00, 0x00, 0x00},//##DRC7_Power_Meter
	{0x6d, 0x02, 0x00, 0x00, 0x00},//##DRC1_gain
	{0x6e, 0x02, 0x00, 0x00, 0x00},//##DRC3_gain
	{0x6f, 0x02, 0x00, 0x00, 0x00},//##DRC5_gain
	{0x70, 0x02, 0x00, 0x00, 0x00},//##CH1.2_Power_Clipping
	{0x71, 0x00, 0x00, 0x01, 0xa0},//##Noise_Gate_Attack_Level
	{0x72, 0x00, 0x00, 0x05, 0x30},//##Noise_Gate_Release_Level
	{0x73, 0x00, 0x01, 0x00, 0x00},//##DRC1_Energy_Coefficient
	{0x74, 0x00, 0x01, 0x00, 0x00},//##DRC2_Energy_Coefficient
	{0x75, 0x00, 0x01, 0x00, 0x00},//##DRC3_Energy_Coefficient
	{0x76, 0x00, 0x01, 0x00, 0x00},//##DRC4_Energy_Coefficient
	{0x77, 0x01, 0xd7, 0xe6, 0xe0},//##compensate_a0
	{0x78, 0x00, 0x2e, 0x77, 0x40},//##compensate_a1
	{0x79, 0x0f, 0xf9, 0xa1, 0xe0},//##compensate_b1
	{0x7a, 0x0e, 0x01, 0xc0, 0x70},//##DRC1_FF_threshold
	{0x7b, 0x02, 0x00, 0x00, 0x00},//##DRC1_FF_slope
	{0x7c, 0x00, 0x00, 0x40, 0x00},//##DRC1_FF_aa
	{0x7d, 0x00, 0x00, 0x40, 0x00},//##DRC1_FF_da
	{0x7e, 0x02, 0x00, 0x00, 0x00},//##SRS_Gain
	{0x7f, 0x00, 0x04, 0x00, 0x00},//##AGC_ALPHA
};

static int m_ram2_tab[][5] = {
	{0x00, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ1_A1
	{0x01, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ1_A2
	{0x02, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ1_B1
	{0x03, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ1_B2
	{0x04, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ1_A0
	{0x05, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ2_A1
	{0x06, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ2_A2
	{0x07, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ2_B1
	{0x08, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ2_B2
	{0x09, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ2_A0
	{0x0a, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ3_A1
	{0x0b, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ3_A2
	{0x0c, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ3_B1
	{0x0d, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ3_B2
	{0x0e, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ3_A0
	{0x0f, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ4_A1
	{0x10, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ4_A2
	{0x11, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ4_B1
	{0x12, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ4_B2
	{0x13, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ4_A0
	{0x14, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ5_A1
	{0x15, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ5_A2
	{0x16, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ5_B1
	{0x17, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ5_B2
	{0x18, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ5_A0
	{0x19, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ6_A1
	{0x1a, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ6_A2
	{0x1b, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ6_B1
	{0x1c, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ6_B2
	{0x1d, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ6_A0
	{0x1e, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ7_A1
	{0x1f, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ7_A2
	{0x20, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ7_B1
	{0x21, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ7_B2
	{0x22, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ7_A0
	{0x23, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ8_A1
	{0x24, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ8_A2
	{0x25, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ8_B1
	{0x26, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ8_B2
	{0x27, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ8_A0
	{0x28, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ9_A1
	{0x29, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ9_A2
	{0x2a, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ9_B1
	{0x2b, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ9_B2
	{0x2c, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ9_A0
	{0x2d, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ10_A1
	{0x2e, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ10_A2
	{0x2f, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ10_B1
	{0x30, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ10_B2
	{0x31, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ10_A0
	{0x32, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ11_A1
	{0x33, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ11_A2
	{0x34, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ11_B1
	{0x35, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ11_B2
	{0x36, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ11_A0
	{0x37, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ12_A1
	{0x38, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ12_A2
	{0x39, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ12_B1
	{0x3a, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ12_B2
	{0x3b, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ12_A0
	{0x3c, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ13_A1
	{0x3d, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ13_A2
	{0x3e, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ13_B1
	{0x3f, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ13_B2
	{0x40, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ13_A0
	{0x41, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ14_A1
	{0x42, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ14_A2
	{0x43, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ14_B1
	{0x44, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ14_B2
	{0x45, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ14_A0
	{0x46, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ15_A1
	{0x47, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ15_A2
	{0x48, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ15_B1
	{0x49, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ15_B2
	{0x4a, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ15_A0
	{0x4b, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ16_A1
	{0x4c, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ16_A2
	{0x4d, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ16_B1
	{0x4e, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ16_B2
	{0x4f, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ16_A0
	{0x50, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ17_A1
	{0x51, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ17_A2
	{0x52, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ17_B1
	{0x53, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ17_B2
	{0x54, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ17_A0
	{0x55, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ18_A1
	{0x56, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ18_A2
	{0x57, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ18_B1
	{0x58, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ18_B2
	{0x59, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ18_A0
	{0x5a, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ19_A1
	{0x5b, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ19_A2
	{0x5c, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ19_B1
	{0x5d, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ19_B2
	{0x5e, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ19_A0
	{0x5f, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ20_A1
	{0x60, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ20_A2
	{0x61, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ20_B1
	{0x62, 0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ20_B2
	{0x63, 0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ20_A0
	{0x64, 0x00, 0x00, 0x00, 0x00},//##Channel_2_Mixer1
	{0x65, 0x07, 0xff, 0xff, 0xff},//##Channel_2_Mixer2
	{0x66, 0x00, 0x7e, 0x88, 0xe0},//##Channel_2_Prescale
	{0x67, 0x02, 0x00, 0x00, 0x00},//##Channel_2_Postscale
	{0x68, 0x00, 0x80, 0x00, 0x00},//##I2SO_RCH_GAIN
	{0x69, 0x00, 0x00, 0x00, 0x00},//##DRC2_Power_Meter
	{0x6a, 0x00, 0x00, 0x00, 0x00},//##DRC4_Power_Meter
	{0x6b, 0x00, 0x00, 0x00, 0x00},//##DRC6_Power_Meter
	{0x6c, 0x00, 0x00, 0x00, 0x00},//##DRC8_Power_Meter
	{0x6d, 0x02, 0x00, 0x00, 0x00},//##DRC2_gain
	{0x6e, 0x02, 0x00, 0x00, 0x00},//##DRC4_gain
	{0x6f, 0x02, 0x00, 0x00, 0x00},//##DRC6_gain
	{0x70, 0x0e, 0x01, 0xc0, 0x70},//##DRC2_FF_threshold
	{0x71, 0x02, 0x00, 0x00, 0x00},//##DRC2_FF_slope
	{0x72, 0x00, 0x00, 0x40, 0x00},//##DRC2_FF_aa
	{0x73, 0x00, 0x00, 0x40, 0x00},//##DRC2_FF_da
	{0x74, 0x0e, 0x01, 0xc0, 0x70},//##DRC3_FF_threshold
	{0x75, 0x02, 0x00, 0x00, 0x00},//##DRC3_FF_slope
	{0x76, 0x00, 0x00, 0x40, 0x00},//##DRC3_FF_aa
	{0x77, 0x00, 0x00, 0x40, 0x00},//##DRC3_FF_da
	{0x78, 0x0e, 0x01, 0xc0, 0x70},//##DRC4_FF_threshold
	{0x79, 0x02, 0x00, 0x00, 0x00},//##DRC4_FF_slope
	{0x7a, 0x00, 0x00, 0x40, 0x00},//##DRC4_FF_aa
	{0x7b, 0x00, 0x00, 0x40, 0x00},//##DRC4_FF_aa
	{0x7c, 0x00, 0x5a, 0x9d, 0xf0},//##AGC_Attach_threshold
	{0x7d, 0x00, 0x47, 0xfa, 0xd0},//##AGC_Release_threshold
	{0x7e, 0x00, 0x00, 0x08, 0x64},//##AGC_AR_RR
	{0x7f, 0x00, 0x00, 0x08, 0x64},//##AGC_AT_RT
};

struct ad82088d_data {
	int pd_gpio;
	int CHIP_SYNC;
	int CHIP_SYNC_lock;
	int ASR_DET;
	int ASR_DET_lock;
	struct snd_soc_component *component;
	struct regmap *regmap;
	struct i2c_client *ad82088d_client;
	enum ad82088d_type devtype;
	struct delayed_work fault_check_work;
	unsigned int last_fault;
#ifdef AD82088D_CHANGE_EQ_MODE_EN
	unsigned int eq_mode;
	unsigned char (*m_ram_tab)[5];
#endif

};

static int ad82088d_pd_gpio_set(struct snd_soc_component *component, bool enable)
{
	struct ad82088d_data *ad82088d =
		snd_soc_component_get_drvdata(component);

	if (!(gpio_is_valid(ad82088d->pd_gpio))) {
		pr_err("ad82088d the pd gpio is un-valid\n");
		return 0;
	}

	if (enable) {
		gpio_direction_output(ad82088d->pd_gpio, GPIOF_OUT_INIT_HIGH);
		dev_info(component->dev, "ad82088d pd pin status = %d\n",
			gpio_get_value(ad82088d->pd_gpio));
	} else {
		/*gpio_direction_output(ad82088d->pd_gpio, GPIOF_OUT_INIT_LOW);*/
		gpio_set_value(ad82088d->pd_gpio, GPIOF_OUT_INIT_LOW);
		dev_info(component->dev, "ad82088d pd pin status = %d\n",
			gpio_get_value(ad82088d->pd_gpio));
	}

	return 0;
}

static int ad82088d_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	unsigned int rate = params_rate(params);
	unsigned int ssz_ds;
	int ret;

	//pr_info("rate in amp is (%d)\n", rate);
	switch (rate) {
	case 44100:
	case 48000:
		ssz_ds = 0;
		break;
	case 88200:
	case 96000:
		ssz_ds = AD82088D_SSZ_DS;
		break;
	default:
		dev_err(component->dev, "unsupported sample rate: %u\n", rate);
		return -EINVAL;
	}

	ad82088d_pd_gpio_set(component, true);// pull high amp PD pin
	msleep(20);

	ret = snd_soc_component_update_bits(component, AD82088D_STATE_CTRL2_REG,
				  AD82088D_SSZ_DS, ssz_ds);

	if (ret < 0) {
		dev_err(component->dev, "error setting sample rate: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ad82088d_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *component = dai->component;
	u8 serial_format;
	int ret;

	if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) != SND_SOC_DAIFMT_CBS_CFS) {
		dev_vdbg(component->dev, "DAI Format master is not found\n");
		return -EINVAL;
	}

	switch (fmt & (SND_SOC_DAIFMT_FORMAT_MASK |
		       SND_SOC_DAIFMT_INV_MASK)) {
	case (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF):
		/* 1st data bit occur one BCLK cycle after the frame sync */
		serial_format = AD82088D_SAIF_I2S;
		break;
	case (SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_NB_NF):
		/*
		 * Note that although the AD82088D does not have a dedicated DSP
		 * mode it doesn't care about the LRCLK duty cycle during TDM
		 * operation. Therefore we can use the device's I2S mode with
		 * its delaying of the 1st data bit to receive DSP_A formatted
		 * data. See device datasheet for additional details.
		 */
		serial_format = AD82088D_SAIF_I2S;
		break;
	case (SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_NB_NF):
		/*
		 * Similar to DSP_A, we can use the fact that the AD82088D does
		 * not care about the LRCLK duty cycle during TDM to receive
		 * DSP_B formatted data in LEFTJ mode (no delaying of the 1st
		 * data bit).
		 */
		serial_format = AD82088D_SAIF_LEFTJ;
		break;
	case (SND_SOC_DAIFMT_LEFT_J | SND_SOC_DAIFMT_NB_NF):
		/* No delay after the frame sync */
		serial_format = AD82088D_SAIF_LEFTJ;
		break;
	default:
		dev_vdbg(component->dev, "DAI Format is not found\n");
		return -EINVAL;
	}

	ad82088d_pd_gpio_set(component, true);// pull high amp PD pin
	msleep(20);

	ret = snd_soc_component_update_bits(component, AD82088D_STATE_CTRL1_REG,
				  AD82088D_SAIF_FORMAT_MASK,
				  serial_format);
	if (ret < 0) {
		dev_err(component->dev, "error setting SAIF format: %d\n", ret);
		return ret;
	}

	return 0;
}

#ifdef AD82088D_CHANGE_EQ_MODE_EN
static int ad82088d_change_eq_mode(struct snd_soc_component *component, int channel)
{
	struct ad82088d_data *ad82088d = snd_soc_component_get_drvdata(component);
	int eq_seg = 0;
	int i = 0;
	int cmd = 0;

	for (i = 0; i < 20; i++) {
		// ram addr
		regmap_write(ad82088d->regmap, 0x1d, ad82088d->m_ram_tab[eq_seg][0]);

		// write A1
		regmap_write(ad82088d->regmap, 0x1e, ad82088d->m_ram_tab[eq_seg][1]);
		regmap_write(ad82088d->regmap, 0x1f, ad82088d->m_ram_tab[eq_seg][2]);
		regmap_write(ad82088d->regmap, 0x20, ad82088d->m_ram_tab[eq_seg][3]);
		regmap_write(ad82088d->regmap, 0x21, ad82088d->m_ram_tab[eq_seg][4]);

		eq_seg += 1;
		// write A2
		regmap_write(ad82088d->regmap, 0x22, ad82088d->m_ram_tab[eq_seg][1]);
		regmap_write(ad82088d->regmap, 0x23, ad82088d->m_ram_tab[eq_seg][2]);
		regmap_write(ad82088d->regmap, 0x24, ad82088d->m_ram_tab[eq_seg][3]);
		regmap_write(ad82088d->regmap, 0x25, ad82088d->m_ram_tab[eq_seg][4]);

		eq_seg += 1;
		// write B1
		regmap_write(ad82088d->regmap, 0x26, ad82088d->m_ram_tab[eq_seg][1]);
		regmap_write(ad82088d->regmap, 0x27, ad82088d->m_ram_tab[eq_seg][2]);
		regmap_write(ad82088d->regmap, 0x28, ad82088d->m_ram_tab[eq_seg][3]);
		regmap_write(ad82088d->regmap, 0x29, ad82088d->m_ram_tab[eq_seg][4]);

		eq_seg += 1;
		// write B2
		regmap_write(ad82088d->regmap, 0x2a, ad82088d->m_ram_tab[eq_seg][1]);
		regmap_write(ad82088d->regmap, 0x2b, ad82088d->m_ram_tab[eq_seg][2]);
		regmap_write(ad82088d->regmap, 0x2c, ad82088d->m_ram_tab[eq_seg][3]);
		regmap_write(ad82088d->regmap, 0x2d, ad82088d->m_ram_tab[eq_seg][4]);

		eq_seg += 1;
		// write A0
		regmap_write(ad82088d->regmap, 0x2e, ad82088d->m_ram_tab[eq_seg][1]);
		regmap_write(ad82088d->regmap, 0x2f, ad82088d->m_ram_tab[eq_seg][2]);
		regmap_write(ad82088d->regmap, 0x30, ad82088d->m_ram_tab[eq_seg][3]);
		regmap_write(ad82088d->regmap, 0x31, ad82088d->m_ram_tab[eq_seg][4]);

		if (channel == 1)
			cmd = 0x02;
		else if (channel == 2)
			cmd = 0x42;

		regmap_write(ad82088d->regmap, 0x32, cmd);

		eq_seg += 1;

		if (eq_seg > 0x63)
			break;
	}

	for (eq_seg = 0x64; eq_seg < 0x7F; eq_seg++) {
		if (eq_seg >= 0x69 && eq_seg <= 0x6C)
			continue;

		regmap_write(ad82088d->regmap, CFADDR, ad82088d->m_ram_tab[eq_seg][0]);
		regmap_write(ad82088d->regmap, A1CF1, ad82088d->m_ram_tab[eq_seg][1]);
		regmap_write(ad82088d->regmap, A1CF2, ad82088d->m_ram_tab[eq_seg][2]);
		regmap_write(ad82088d->regmap, A1CF3, ad82088d->m_ram_tab[eq_seg][3]);
		regmap_write(ad82088d->regmap, A1CF4, ad82088d->m_ram_tab[eq_seg][4]);

		if (channel == 1)
			cmd = 0x01;
		else if (channel == 2)
			cmd = 0x41;
		else
			cmd = 0;

		regmap_write(ad82088d->regmap, CFUD, cmd);
	}

	return 0;
}

static int ad82088d_eq_mode_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type   = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access = (SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE);
	uinfo->count  = 1;

	uinfo->value.integer.min  = AD82088D_EQ_MODE_MIN;
	uinfo->value.integer.max  = AD82088D_EQ_MODE_MAX;
	uinfo->value.integer.step = 1;

	return 0;
}

static int ad82088d_eq_mode_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82088d_data *ad82088d = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = ad82088d->eq_mode;

	return 0;
}

static int ad82088d_eq_mode_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82088d_data *ad82088d = snd_soc_component_get_drvdata(component);
	int id_reg = 0xff;

	if (ucontrol->value.integer.value[0] > AD82088D_EQ_MODE_MAX ||
		ucontrol->value.integer.value[0] < AD82088D_EQ_MODE_MIN) {
		dev_err(component->dev, "error mode value setting, please check!\n");
		return -1;
	}

	ad82088d_pd_gpio_set(component, true);
	msleep(20);

	regmap_read(ad82088d->regmap, AD82088D_DEVICE_ID_REG, &id_reg);
	if ((id_reg & 0xf0) != AD82088D_DEVICE_ID) {
		dev_err(component->dev, "error device id 0x%02x, please check!\n", id_reg);
		return -1;
	}

	ad82088d->eq_mode = ucontrol->value.integer.value[0];

	//pr_info("change ad82088d eq mode = %d\n", ad82088d->eq_mode);

	if (ad82088d->eq_mode == 1) {
		ad82088d->m_ram_tab = eq_mode_1_ram1_tab;
		ad82088d_change_eq_mode(component, 1);
#ifdef CONFIG_SND_SOC_AD82088D_2CHANNEL
		ad82088d->m_ram_tab = eq_mode_1_ram2_tab;
		ad82088d_change_eq_mode(component, 2);
#endif
	}
	if (ad82088d->eq_mode == 2) {
		ad82088d->m_ram_tab = eq_mode_2_ram1_tab;
		ad82088d_change_eq_mode(component, 1);
#ifdef CONFIG_SND_SOC_AD82088D_2CHANNEL
		ad82088d->m_ram_tab = eq_mode_2_ram2_tab;
		ad82088d_change_eq_mode(component, 2);
#endif
	}

	return 0;
}

static const struct snd_kcontrol_new ad82088d_eq_mode_control[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name  = "AD82088D EQ Mode",  // Just fake the name
		.info  = ad82088d_eq_mode_info,
		.get   = ad82088d_eq_mode_get,
		.put   = ad82088d_eq_mode_put,
	},
};
#endif

static int ad82088d_mute(struct snd_soc_dai *dai, int mute, int stream)
{
	struct snd_soc_component *component = dai->component;
	int ret;

	ret = snd_soc_component_update_bits(component, AD82088D_STATE_CTRL3_REG,
				  AD82088D_MUTE, mute ? AD82088D_MUTE : 0);
	if (ret < 0) {
		dev_err(component->dev, "error (un-)muting device: %d\n", ret);
		return ret;
	}

	return 0;
}

static void ad82088d_fault_check_work(struct work_struct *work)
{
	struct ad82088d_data *ad82088d = container_of(work, struct ad82088d_data,
			fault_check_work.work);
	struct device *dev = ad82088d->component->dev;
	unsigned int curr_fault, tmp_value;
	int ret;

	// if CHIP SYNC is enable at initialization,
	// (enable-> delay 200us -> disable) it again Whenever i2s clk on,
	// both BCLK and LRCK are required to be simultaneously and stably supplied to the AMP.
	if (ad82088d->CHIP_SYNC == 1 && ad82088d->CHIP_SYNC_lock == 0) {
		/* Set CHIP SYNC to enable */
		ret = snd_soc_component_update_bits(ad82088d->component,
				AD82088D_COMPEN_FILTER_CTRL_REG,
				AD82088D_CHIP_SYNC_EN, AD82088D_CHIP_SYNC_EN);

		usleep_range(1000, 1010);

		/* Set CHIP SYNC to disable */
		ret = snd_soc_component_update_bits(ad82088d->component,
				AD82088D_COMPEN_FILTER_CTRL_REG,
				AD82088D_CHIP_SYNC_EN, 0);

		ad82088d->CHIP_SYNC_lock = 1;
	}

	// Force Auto sample rate detection function to turn off during initialization,
	// and now turn it on(I2S clock is out).
	if (ad82088d->ASR_DET != 0 && ad82088d->ASR_DET_lock == 0) {
		regmap_write(ad82088d->regmap, AD82088D_CLK_DET_CTRL, ad82088d->ASR_DET);
		ad82088d->ASR_DET_lock = 1;
	}

	// amp fault check
	ret = regmap_read(ad82088d->regmap, AD82088D_FAULT_REG, &curr_fault);
	if (ret < 0) {
		dev_err(dev, "failed to read FAULT register: %d\n", ret);
		goto out;
	}

	/* Check/handle all errors except SAIF clock errors */
	tmp_value = AD82088D_OCE | AD82088D_BSOVE | AD82088D_OTE |
			AD82088D_UVE | AD82088D_BSUVE | AD82088D_OVPE;
	curr_fault &= tmp_value;

	/*
	 * Only flag errors once for a given occurrence. This is needed as
	 * the AD82088D will take time clearing the fault condition internally
	 * during which we don't want to bombard the system with the same
	 * error message over and over.
	 */
	if (!(curr_fault & AD82088D_OCE) && (ad82088d->last_fault & AD82088D_OCE))
		dev_crit(dev, "experienced an over current hardware fault\n");

	if (!(curr_fault & AD82088D_BSOVE) && (ad82088d->last_fault & AD82088D_BSOVE))
		dev_crit(dev, "experienced a BSOV detection fault\n");

	if (!(curr_fault & AD82088D_OTE) && (ad82088d->last_fault & AD82088D_OTE))
		dev_crit(dev, "experienced an over temperature fault\n");

	if (!(curr_fault & AD82088D_UVE) && (ad82088d->last_fault & AD82088D_UVE))
		dev_crit(dev, "experienced an UV fault\n");

	if (!(curr_fault & AD82088D_BSUVE) && (ad82088d->last_fault & AD82088D_BSUVE))
		dev_crit(dev, "experienced an BSUV fault\n");

	if (!(curr_fault & AD82088D_OVPE) && (ad82088d->last_fault & AD82088D_OVPE))
		dev_crit(dev, "experienced an OVP fault\n");
	/* Store current fault value so we can detect any changes next time */
	ad82088d->last_fault = curr_fault;

	if (curr_fault == tmp_value)
		goto out;

	/*
	 * Periodically toggle SDZ (shutdown bit) H->L->H to clear any latching
	 * faults as long as a fault condition persists. Always going through
	 * the full sequence no matter the first return value to minimizes
	 * chances for the device to end up in shutdown mode.
	 */
	dev_crit(dev, "toggle pd pin H->L->H to clear latching faults\n");

	ad82088d_pd_gpio_set(ad82088d->component, false);// pull low amp PD pin
	msleep(20);
	ad82088d_pd_gpio_set(ad82088d->component, true);// pull high amp PD pin

out:
	/* Schedule the next fault check at the specified interval */
	schedule_delayed_work(&ad82088d->fault_check_work,
			msecs_to_jiffies(AD82088D_FAULT_CHECK_INTERVAL));
}

static int ad82088d_reg_ram_init(struct snd_soc_component *component)
{
	struct ad82088d_data *ad82088d = snd_soc_component_get_drvdata(component);
	int ret;
	int i;
	int reg_data;

	//pr_info("ad82088d i2c address = %p,  %s!\n", component, __func__);

	ad82088d_pd_gpio_set(component, true);// pull high amp PD pin

	msleep(20);

	// software reset amp
	ret = snd_soc_component_update_bits(component, AD82088D_STATE_CTRL5_REG,
			AD82088D_SW_RESET, 0);
	if (ret < 0)
		goto error_snd_soc_component_update_reg;

	usleep_range(5000, 5010);

	ret = snd_soc_component_update_bits(component, AD82088D_STATE_CTRL5_REG,
			AD82088D_SW_RESET, AD82088D_SW_RESET);
	if (ret < 0)
		goto error_snd_soc_component_update_reg;

	msleep(20);// wait 20ms

	/* Set device to mute */
	ret = snd_soc_component_update_bits(component, AD82088D_STATE_CTRL3_REG,
			AD82088D_MUTE, AD82088D_MUTE);
	if (ret < 0)
		goto error_snd_soc_component_update_reg;

	// Write register table
	for (i = 0; i < AD82088D_REGISTER_COUNT; i++) {
		reg_data = m_reg_tab[i][1];

		if (m_reg_tab[i][0] == 0x02)
			continue;

		// set stereo
		if (m_reg_tab[i][0] == 0x1A)
			reg_data &= (~0x40);

		if (m_reg_tab[i][0] == 0x5B)
			reg_data = 0x00;

		if (m_reg_tab[i][0] == 0x5C)
			reg_data = 0x00;

		if (m_reg_tab[i][0] == 0x6D)
			reg_data = 0x88;

		// set stereo end
		// check if CHIP SYNC enable ?
		if (m_reg_tab[i][0] == AD82088D_COMPEN_FILTER_CTRL_REG) {
			if ((reg_data & AD82088D_CHIP_SYNC_EN) == AD82088D_CHIP_SYNC_EN) {
				ad82088d->CHIP_SYNC = 1;
				//pr_info("ad82088d chip sync enable!\n");
			} else {
				ad82088d->CHIP_SYNC = 0;
			}
		}

		// check if Auto sample rate detection enable ?
		ad82088d->ASR_DET_lock = 0;
		if (m_reg_tab[i][0] == AD82088D_CLK_DET_CTRL) {
			if ((reg_data & AD82088D_ASR_DET) == AD82088D_ASR_DET) {
				ad82088d->ASR_DET = reg_data;
				//pr_info("ad82088d Auto sample rate detection enable!\n");

				// Force this feature to turn off during initialization,
				// and turn it on after the I2S clock is out.
				reg_data = 0x10;
			} else {
				ad82088d->ASR_DET = 0;
			}
		}

		ret = regmap_write(ad82088d->regmap, m_reg_tab[i][0], reg_data);
		if (ret < 0)
			goto error_snd_soc_component_update_reg;
	}

	// Write ram1
	for (i = 0; i < AD82088D_RAM_TABLE_COUNT; i++) {
		regmap_write(ad82088d->regmap, CFADDR, m_ram1_tab[i][0]);
		regmap_write(ad82088d->regmap, A1CF1, m_ram1_tab[i][1]);
		regmap_write(ad82088d->regmap, A1CF2, m_ram1_tab[i][2]);
		regmap_write(ad82088d->regmap, A1CF3, m_ram1_tab[i][3]);
		regmap_write(ad82088d->regmap, A1CF4, m_ram1_tab[i][4]);
		regmap_write(ad82088d->regmap, CFUD, 0x01);
	}
	// Write ram2
	for (i = 0; i < AD82088D_RAM_TABLE_COUNT; i++) {
		regmap_write(ad82088d->regmap, CFADDR, m_ram2_tab[i][0]);
		regmap_write(ad82088d->regmap, A1CF1, m_ram2_tab[i][1]);
		regmap_write(ad82088d->regmap, A1CF2, m_ram2_tab[i][2]);
		regmap_write(ad82088d->regmap, A1CF3, m_ram2_tab[i][3]);
		regmap_write(ad82088d->regmap, A1CF4, m_ram2_tab[i][4]);
		regmap_write(ad82088d->regmap, CFUD, 0x41);
	}
	return 0;

error_snd_soc_component_update_reg:
	dev_err(component->dev, "error init device registers: %d\n", ret);

	return ret;
}

static int ad82088d_codec_probe(struct snd_soc_component *component)
{
	struct ad82088d_data *ad82088d = snd_soc_component_get_drvdata(component);
	//unsigned int device_id, expected_device_id;
	int ret;
#ifdef AD82088D_REG_RAM_CHECK
	int i;
	int reg_data;
	int ram_h8_data;
	int ram_m8_data;
	int ram_l8_data;
	int ram_ll8_data;
#endif
	ad82088d->component = component;
	//pr_info("ad82088d i2c address = %p,  %s!\n",component, __func__);

	ret = ad82088d_reg_ram_init(component);
	if (ret < 0)
		goto error_snd_soc_component_update_bits;

	usleep_range(2000, 2010);

	// if CHIP SYNC is enable, need disable it before unmute
	if (ad82088d->CHIP_SYNC == 1) {
		/* Set CHIP SYNC to disable */
		ret = snd_soc_component_update_bits(component, AD82088D_COMPEN_FILTER_CTRL_REG,
				AD82088D_CHIP_SYNC_EN, 0);

		ad82088d->CHIP_SYNC_lock = 0;
	}

	/* Set device to unmute */
	ret = snd_soc_component_update_bits(component, AD82088D_STATE_CTRL3_REG,
				AD82088D_MUTE, 0);
	if (ret < 0)
		goto error_snd_soc_component_update_bits;

	INIT_DELAYED_WORK(&ad82088d->fault_check_work, ad82088d_fault_check_work);

#ifdef AD82088D_CHANGE_EQ_MODE_EN
	ret = snd_soc_add_component_controls(component, ad82088d_eq_mode_control, 1);

	if (ret != 0)
		pr_err("Failed to register ad82088d_eq_mode_control (%d)\n", ret);

#endif

#ifdef AD82088D_REG_RAM_CHECK
	msleep(1000);

	for (i = 0; i < AD82088D_REGISTER_COUNT; i++) {
		regmap_read(ad82088d->regmap, m_reg_tab[i][0], &reg_data);
		//pr_info("read ad82088d register {addr, data} = {0x%02x, 0x%02x}\n",
		//			m_reg_tab[i][0], reg_data);
	}

	for (i = 0; i < AD82088D_RAM_TABLE_COUNT; i++) {
		regmap_write(ad82088d->regmap, CFADDR, m_ram1_tab[i][0]);
		regmap_write(ad82088d->regmap, CFUD, 0x04);

		regmap_read(ad82088d->regmap, A1CF1, &ram_h8_data);
		regmap_read(ad82088d->regmap, A1CF2, &ram_m8_data);
		regmap_read(ad82088d->regmap, A1CF3, &ram_l8_data);
		regmap_read(ad82088d->regmap, A1CF4, &ram_ll8_data);

		//pr_info("ram1 {addr,H8,M8,L8,LL8} = {0x%02x,0x%02x,0x%02x,0x%02x,0x%02x}\n",
		//	m_ram1_tab[i][0], ram_h8_data, ram_m8_data, ram_l8_data, ram_ll8_data);
	}
#endif

	return 0;

error_snd_soc_component_update_bits:
	dev_err(component->dev, "error configuring device registers: %d\n", ret);

//probe_fail:
	//regulator_bulk_disable(ARRAY_SIZE(ad82088d->supplies),
			       //ad82088d->supplies);
	return ret;
}

static void ad82088d_codec_remove(struct snd_soc_component *component)
{
	struct ad82088d_data *ad82088d = snd_soc_component_get_drvdata(component);
	//int ret;

	cancel_delayed_work_sync(&ad82088d->fault_check_work);
	ad82088d->CHIP_SYNC_lock = 0;
	ad82088d->ASR_DET_lock = 0;
};

static int ad82088d_dac_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct ad82088d_data *ad82088d = snd_soc_component_get_drvdata(component);
	int ret;

	if (event & SND_SOC_DAPM_POST_PMU) {
		//pr_info("ad82088d dac event post PMU.\n");
		/* Take AD82088D out of shutdown mode */

		ad82088d_pd_gpio_set(component, true);// pull high amp PD pin

		/*
		 * Observe codec shutdown-to-active time. The datasheet only
		 * lists a nominal value however just use-it as-is without
		 * additional padding to minimize the delay introduced in
		 * starting to play audio (actually there is other setup done
		 * by the ASoC framework that will provide additional delays,
		 * so we should always be safe).
		 */
		msleep(20);
		ret = snd_soc_component_update_bits(component, AD82088D_STATE_CTRL3_REG,
					  AD82088D_MUTE, 0);
		if (ret < 0)
			dev_err(component->dev, "failed to write MUTE register: %d\n", ret);

		/* Turn on AD82088D periodic fault checking/handling */
		ad82088d->last_fault = 0xFF;
		schedule_delayed_work(&ad82088d->fault_check_work,
				msecs_to_jiffies(AD82088D_FAULT_CHECK_INTERVAL));
	} else if (event & SND_SOC_DAPM_PRE_PMD) {
		//pr_info("ad82088d dac event pre PMD.\n");
		/* Disable AD82088D periodic fault checking/handling */
		cancel_delayed_work_sync(&ad82088d->fault_check_work);

		// clear amp sync lock
		ad82088d->CHIP_SYNC_lock = 0;
		// disable ASR_DET function and clear lock before I2S clk stop
		if (ad82088d->ASR_DET != 0)
			regmap_write(ad82088d->regmap, AD82088D_CLK_DET_CTRL, 0x10);

		ad82088d->ASR_DET_lock = 0;

		/* Place AD82088D in shutdown mode to minimize current draw */
		ret = snd_soc_component_update_bits(component, AD82088D_STATE_CTRL3_REG,
					  AD82088D_MUTE, AD82088D_MUTE);
		if (ret < 0)
			dev_err(component->dev, "failed to write MUTE register: %d\n", ret);

		msleep(20);

		ad82088d_pd_gpio_set(component, false);// pull low amp PD pin
	}

	return 0;
}

#ifdef CONFIG_PM
static int ad82088d_suspend(struct snd_soc_component *component)
{
	//struct ad82088d_data *ad82088d = snd_soc_component_get_drvdata(component);
	//int ret;

	//pr_info("ad82088d suspend.\n");

	//regcache_cache_only(ad82088d->regmap, true);
	//regcache_mark_dirty(ad82088d->regmap);

	return 0;
}

static int ad82088d_resume(struct snd_soc_component *component)
{
	//struct ad82088d_data *ad82088d = snd_soc_component_get_drvdata(component);
	int ret;

	//pr_info("ad82088d resume.\n");

	//regcache_cache_only(ad82088d->regmap, false);

	//ret = regcache_sync(ad82088d->regmap);
	//if (ret < 0) {
	//	dev_err(component->dev, "failed to sync regcache: %d\n", ret);
	//	return ret;
	//}

	// why sleep 20ms ???
	//msleep(20);

	ret = ad82088d_reg_ram_init(component);
	if (ret < 0)
		goto resume_error_snd_soc_component_update_bits;

	return 0;

resume_error_snd_soc_component_update_bits:
	dev_err(component->dev, "resume error configuring device registers: %d\n", ret);

	return ret;
}
#else
#define ad82088d_suspend NULL
#define ad82088d_resume NULL
#endif

static bool ad82088d_is_volatile_reg(struct device *dev, unsigned int reg)
{
#ifdef	AD82088D_REG_RAM_CHECK
	if (reg <= AD82088D_MAX_REG)
		return true;
	else
		return false;
#else
	switch (reg) {
	case AD82088D_FAULT_REG:
	case AD82088D_STATE_CTRL1_REG:
	case AD82088D_STATE_CTRL2_REG:
	case AD82088D_STATE_CTRL3_REG:
	case AD82088D_STATE_CTRL5_REG:
	case AD82088D_DEVICE_ID_REG:
		return true;
	default:
		return false;
	}
#endif
}

static const struct regmap_config ad82088d_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = AD82088D_MAX_REG,

	//.reg_defaults = m_reg_tab,
	//
	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = ad82088d_is_volatile_reg,
};

/*
 * DAC digital volumes. From -103.5 to 24 dB in 0.5 dB steps. Note that
 * setting the gain below -100 dB (register value <0x7) is effectively a MUTE
 * as per device datasheet.
 */
static DECLARE_TLV_DB_SCALE(dac_tlv, -10350, 50, 0);

static const struct snd_kcontrol_new ad82088d_snd_controls[] = {
	SOC_SINGLE_TLV("Speaker Driver Playback Volume",
		       AD82088D_VOLUME_CTRL_REG, 0, 0xff, 0, dac_tlv),
};

static const struct snd_soc_dapm_widget ad82088d_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("DAC IN", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC_E("DAC", NULL, SND_SOC_NOPM, 0, 0, ad82088d_dac_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUTPUT("OUT")
};

static const struct snd_soc_dapm_route ad82088d_audio_map[] = {
	{ "DAC", NULL, "DAC IN" },
	{ "OUT", NULL, "DAC" },
};

static const struct snd_soc_component_driver soc_component_dev_ad82088d = {
	.probe			= ad82088d_codec_probe,
	.remove			= ad82088d_codec_remove,
	.suspend		= ad82088d_suspend,
	.resume			= ad82088d_resume,
	.controls		= ad82088d_snd_controls,
	.num_controls		= ARRAY_SIZE(ad82088d_snd_controls),
	.dapm_widgets		= ad82088d_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(ad82088d_dapm_widgets),
	.dapm_routes		= ad82088d_audio_map,
	.num_dapm_routes	= ARRAY_SIZE(ad82088d_audio_map),
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
	.non_legacy_dai_naming	= 1,
};

/* PCM rates supported by the AD82088D driver */
#define AD82088D_RATES	(SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000)

#define AD82088D_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			SNDRV_PCM_FMTBIT_S20_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops ad82088d_speaker_dai_ops = {
	.hw_params	= ad82088d_hw_params,
	.set_fmt	= ad82088d_set_dai_fmt,
	.mute_stream	= ad82088d_mute,
	.no_capture_mute = 1,
};

/*
 * AD82088D DAI structure
 *
 * Note that were are advertising .playback.channels_max = 2 despite this being
 * a mono amplifier. The reason for that is that some serial ports such as ESMT's
 * McASP module have a minimum number of channels (2) that they can output.
 * Advertising more channels than we have will allow us to interface with such
 * a serial port without really any negative side effects as the AD82088D will
 * simply ignore any extra channel(s) asides from the one channel that is
 * configured to be played back.
 */
static struct snd_soc_dai_driver ad82088d_dai[] = {
	{
		.name = "ad82088d",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AD82088D_RATES,
			.formats = AD82088D_FORMATS,
		},
		.ops = &ad82088d_speaker_dai_ops,
	},
};

static int ad82088d_parse_dt(struct ad82088d_data *ad82088d, struct device *dev)
{
	int ret = 0;
	struct device_node *np = dev->of_node;

	ad82088d->pd_gpio = of_get_named_gpio(np, "pd-gpio", 0);
	//pr_info("ad82088d parse dts,the pd-gpio is %d\n", ad82088d->pd_gpio);
	if (!gpio_is_valid(ad82088d->pd_gpio)) {
		pr_err("%s get invalid extamp-pd-gpio %d\n", __func__, ad82088d->pd_gpio);
		ret = -EINVAL;
		return ret;
	}

	ret = devm_gpio_request_one(dev, ad82088d->pd_gpio, GPIOF_OUT_INIT_LOW, "ad82088d-pd-pin");
	if (ret < 0) {
		pr_err("%s get gpio error\n", __func__);
		return ret;
	}

	return 0;
}

static int ad82088d_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct ad82088d_data *data;
	const struct regmap_config *regmap_config;
	int ret;
	//int i;

	pr_info("%s ad82088d probe\n", __func__);
	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->ad82088d_client = client;
	data->devtype = id->driver_data;

	switch (id->driver_data) {
	case AD82088D:
		regmap_config = &ad82088d_regmap_config;
		break;
	default:
		dev_err(dev, "unexpected private driver data\n");
		return -EINVAL;
	}
	data->regmap = devm_regmap_init_i2c(client, regmap_config);
	if (IS_ERR(data->regmap)) {
		ret = PTR_ERR(data->regmap);
		dev_err(dev, "failed to allocate register map: %d\n", ret);
		return ret;
	}

	//for (i = 0; i < ARRAY_SIZE(data->supplies); i++)
	//	data->supplies[i].supply = ad82088d_supply_names[i];

	//ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(data->supplies),
	//			data->supplies);
	//if (ret != 0) {
	//	dev_err(dev, "failed to request supplies: %d\n", ret);
	//	return ret;
	//}

	dev_set_drvdata(dev, data);

	ad82088d_parse_dt(data, &client->dev);

	ret = devm_snd_soc_register_component(&client->dev,
				     &soc_component_dev_ad82088d,
				     ad82088d_dai, ARRAY_SIZE(ad82088d_dai));
	if (ret < 0) {
		dev_err(dev, "failed to register component: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct i2c_device_id ad82088d_id[] = {
	{ "ad82088d", AD82088D },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ad82088d_id);

#ifdef CONFIG_OF
static const struct of_device_id ad82088d_of_match[] = {
	{ .compatible = "ESMT,ad82088d", },
	{ },
};
MODULE_DEVICE_TABLE(of, ad82088d_of_match);
#endif

static struct i2c_driver ad82088d_i2c_driver = {
	.driver = {
		.name = "ad82088d",
		.of_match_table = of_match_ptr(ad82088d_of_match),
	},
	.probe = ad82088d_probe,
	.id_table = ad82088d_id,
};

module_i2c_driver(ad82088d_i2c_driver);

MODULE_AUTHOR("ESMT BU2");
MODULE_DESCRIPTION("AD82088D Audio amplifier driver");
MODULE_LICENSE("GPL");
