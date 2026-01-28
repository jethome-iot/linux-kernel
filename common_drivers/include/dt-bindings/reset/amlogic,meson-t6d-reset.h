/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DT_BINDINGS_AMLOGIC_MESON_T6D_RESET_H
#define _DT_BINDINGS_AMLOGIC_MESON_T6D_RESET_H

/* RESET0 */
#define RESET_USB0_COMB			0
#define RESET_USB0			1
#define RESET_USB1_COMB			2
#define RESET_USB1			3
#define RESET_USB2_COMB			4
#define RESET_USB2			5
#define RESET_U2PHY20			6
#define RESET_U2PHY21			7
#define RESET_U2PHY22			8
/*					9*/
#define RESET_I2C_MON			10
#define RESET_HDMI20_AES		11
#define RESET_HDMIRX			12
#define RESET_HDMIRX_APB		13
#define RESET_I2C_MON_APB		14
#define RESET_VPU_HDMI_AXI		15
/*					16*/
#define RESET_BRG_VCBUS_DEC		17
#define RESET_VCBUS			18
#define RESET_VID_PLL_DIV		19
#define RESET_VDI6			20
#define RESET_GE2D			21
/*					22*/
#define RESET_VID_LOCK			23
#define RESET_VENC0			24
#define RESET_VDAC			25
#define RESET_VENC2			26
#define RESET_VENC1			27
#define RESET_RDMA			28
/*					29*/
#define RESET_VIU			30
#define RESET_VENC			31

/* RESET1 */
#define RESET_AUDIO			32
#define RESET_MALI_CAPB3		33
#define RESET_MALI			34
#define RESET_DDRPLL			35
#define RESET_DMC			36
#define RESET_DOS_CAPB3			37
#define RESET_DOS			38
/*					39-44*/
#define RESET_AMFC_APB			45
#define RESET_AMFC			46
/*					47*/
#define RESET_ETH			48

/*					49-50*/
#define RESET_COMBO_DPHY_CHAN0		51
/*					52-56*/
#define RESET_DEMOD			57
/*					58-61*/
#define RESET_DDRPHY0			62
/*					63*/

/* RESET2 */
#define RESET_IOTM			64
#define RESET_IR_CTRL			65
/*					66-67*/
#define RESET_ETH_AXI			68
#define RESET_TCON			69
/*					70-71*/
#define RESET_SMART_CARD		72
#define RESET_SPICC_0			73
/*					74*/
#define RESET_LED_CTRL			75
/*					76-79*/
#define RESET_MSR_CLK			80
/*					81*/
#define RESET_SAR_ADC			82
/*					83-87*/
#define RESET_ACODEC			88
#define RESET_CEC			89
/*					90*/
#define RESET_WATCHDOG			91
/*					92*/
#define RESET_TVFE			93
#define RESET_ATV_DMD			94
#define RESET_ADEC			95

/* RESET3 */
/*					96-127*/

/* RESET4 */
#define RESET_PWM_A			128
#define RESET_PWM_B			129
#define RESET_PWM_C			130
#define RESET_PWM_D			131
#define RESET_PWM_E			132
#define RESET_PWM_F			133
#define RESET_PWM_G			134
#define RESET_PWM_H			135
/*					136-137*/
#define RESET_UART_A			138
#define RESET_UART_B			139
#define RESET_UART_C			140
/*					141-142*/
#define RESET_CIPLUS			143
#define RESET_I2C_S_A			144
#define RESET_I2C_M_A			145
#define RESET_I2C_M_B			146
#define RESET_I2C_M_C			147
#define RESET_I2C_M_D			148
/*					149-153*/
#define RESET_SD_EMMC_C			154
/*					155*/
#define RESET_TS_CPU			156
#define RESET_BRG_PERIPH_SYNC		157
#define RESET_TS_TOP			158
/*					159*/

/* RESET5 */
/*					160-166*/
#define RESET_BRG_SYS_APB_DEC		167
/*					168-182*/
#define RESET_BRG_NICSYS_HDMI		183
#define RESET_BRG_NICSYS_IOTM		184
#define RESET_BRG_NICSYS_AMFC		185
#define RESET_BRG_NICSYS_EMMCC		186
#define RESET_BRG_NICSYS_MAIN		187
#define RESET_BRG_NICSYS_VAPB		188
#define RESET_BRG_NICSYS_SYS		189
#define RESET_BRG_NICSYS_CPU		190
#define RESET_BRG_NICSYS_ALL		191

/* RESET6 */
/*					192*/
#define RESET_BRG_HEVCF_DMC_PIPEL	193
#define RESET_BRG_VPU_TOP_APB_PIPEL	194
/*					195-198*/
#define RESET_BRG_HDMIRXTONICSYS_PIPEL	199
#define RESET_BRG_VPU1TODDR0_PIPEL	200
#define RESET_BRG_VPU0TODDR0_PIPEL	201
/*					202*/
#define RESET_BRG_GE2DTODDR0_PIPEL	203
/*					204*/
#define RESET_BRG_MALITONICGPU_PIPEL	205
/*					206-207*/
#define RESET_BRG_ETH_HDMIRX_APB_PIPEL	208
#define RESET_BRG_DOS_APB_PIPEL		209
#define RESET_BRG_GE2D_APB_PIPEL	210
#define RESET_BRG_ACODEC_APB_PIPEL	211
/*					212-216*/
#define RESET_BRG_USB_WRAPPER_APB_SYNC	217
/*					218-221*/
#define RESET_BRG_AM2AXI1		222
/*					223*/

#endif
