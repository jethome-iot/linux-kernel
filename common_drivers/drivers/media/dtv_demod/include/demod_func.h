/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DEMOD_FUNC_H__

#define __DEMOD_FUNC_H__

#include <linux/types.h>

#include "aml_demod.h"
#include "dvb_frontend.h"
#include "amlfrontend.h"
#ifdef AML_DEMOD_SUPPORT_DTMB
#include "addr_dtmb_top.h"
#endif
#ifdef AML_DEMOD_SUPPORT_ATSC
#include "addr_atsc_demod.h"
#include "addr_atsc_eq.h"
#include "addr_atsc_cntr.h"
#include "atsc_func.h"
#endif
#include "dtv_demod_regs.h"

/*#include "c_stb_define.h"*/
/*#include "c_stb_regs_define.h"*/
#include <linux/io.h>
#include <linux/jiffies.h>

#if defined DEMOD_FPGA_VERSION
#include "fpga_func.h"
#endif

#if defined AML_DEMOD_SUPPORT_DVBS || defined AML_DEMOD_SUPPORT_DVBC
#define SR_LOW_THRD	11000000
#endif

#define PWR_ON    1
#define PWR_OFF   0

/* offset define */
#define DEMOD_REG_ADDR_OFFSET(reg)	(reg & 0xfffff)
#define QAM_BASE			(0x400)	/* is offset */
#define DEMOD_CFG_BASE			(0xC00)	/* is offset */

/* adc */
#define D_HHI_DADC_RDBK0_I			(0x29 << 2)
#define D_HHI_DADC_CNTL4			(0x2b << 2)

/*redefine*/
#define HHI_DEMOD_MEM_PD_REG		(0x43)
/*redefine*/
#define RESET_RESET0_LEVEL			(0x80)

/*----------------------------------*/
/*register offset define in C_stb_regs_define.h*/
#define AO_RTI_GEN_PWR_SLEEP0 ((0x00 << 10) | (0x3a << 2))
#define AO_RTI_GEN_PWR_ISO0 ((0x00 << 10) | (0x3b << 2))
/*----------------------------------*/

#define ALIGN_24	16777216

/* demod register */
#define DEMOD_TOP_REG0		(0x00)
#define DEMOD_TOP_REG4		(0x04)
#define DEMOD_TOP_REG8		(0x08)
#define DEMOD_TOP_REGC		(0x0C)
#define DEMOD_TOP_CFG_REG_4	(0x10)
#define DEMOD_TOP_CFG_REG_5	(0x14)
#define DEMOD_TOP_CFG_REG_6	(0x18)
#define DEMOD_FRONT_REG38	(0x38)
#define DEMOD_FRONT_REG39	(0x39)

#define DEMOD_FRONT_AFIFO_ADC	(0x20)
#define DEMOD_FRONT_AGC_CFG1	(0x21)
#define DEMOD_FRONT_AGC_CFG2	(0x22)
#define DEMOD_FRONT_AGC_CFG3	(0x23)
#define DEMOD_FRONT_AGC_CFG6	(0x26)
#define DEMOD_FRONT_DC_CFG1		(0x28)

/*reset register*/
#define reg_reset			(0x1c)

#define IO_CBUS_PHY_BASE        (0xc0800000)

#define DEMOD_REG0_VALUE                 0x0000d007
#define DEMOD_REG4_VALUE                 0x2e805400
#define DEMOD_REG8_VALUE                 0x201

/* for table end */
#define TABLE_FLG_END		0xffffffff

#define OTP_LIC		((0x0010  << 2) + 0xfe440000)
#define OTP_LIC13	(OTP_LIC + 0x1C)

/* debug info=====================================================*/
extern int aml_demod_debug;
#ifdef AML_DEMOD_SUPPORT_DVBT
extern bool dvbt2_mplp_retune;
#endif
#ifdef AML_DEMOD_SUPPORT_DVBS
extern unsigned char blind_scan_new;
#endif

#define DBG_INFO	BIT(0)
#define DBG_REG		BIT(1)
#define DBG_ATSC	BIT(2)
#define DBG_DTMB	BIT(3)
#define DBG_LOOP	BIT(4)
#define DBG_DVBC	BIT(5)
#define DBG_DVBT	BIT(6)
#define DBG_DVBS	BIT(7)
#define DBG_ISDBT	BIT(8)
#define DBG_TIME	BIT(9)

#define PR_INFO(fmt, args ...)	pr_info("dtv_dmd:" fmt, ##args)

#define PR_TIME(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_TIME) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

#define PR_DBG(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_INFO) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

#define PR_ATSC(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_ATSC) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

#define PR_DVBC(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_DVBC) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

#define PR_DVBT(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_DVBT) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

#define PR_DVBS(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_DVBS) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

#define PR_DTMB(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_DTMB) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

#define PR_ISDBT(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_ISDBT) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

/*polling*/
#define PR_DBGL(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_LOOP) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

#define PR_ERR(fmt, args ...) pr_err("dtv_dmd:" fmt, ##args)
#define PR_WAR(fmt, args...)  pr_warn("dtv_dmd:" fmt, ##args)

#define POWOF2(number) (1 << (number))		/* was: s32 PowOf2(s32 number); */
#define MAKEWORD(X, Y) (((X) << 8) + (Y))

enum demod_reg_mode {
	REG_MODE_DTMB,
	REG_MODE_DVBT_ISDBT,
	REG_MODE_DVBT_T2,
	REG_MODE_ATSC,
	REG_MODE_DVBC_J83B,
	REG_MODE_FRONT,
	REG_MODE_TOP,
	REG_MODE_CFG,
	REG_MODE_BASE,
	REG_MODE_FPGA,
	REG_MODE_COLLECT_DATA,
	REG_MODE_OTHERS
};

enum dtv_demod_reg_access_mode_e {
	ACCESS_WORD,
	ACCESS_BITS,
	ACCESS_BYTE,
	ACCESS_NUM
};

struct aml_demod_reg {
	enum demod_reg_mode mode;
	u8_t rw;/* 0: read, 1: write. */
	u32_t addr;
	u32_t val;
	enum dtv_demod_reg_access_mode_e access_mode;
	u32_t start_bit;
	u32_t bit_width;
};

int is_s1a_dvbs_disabled(void);
int is_s1a_dvbc_disabled(void);

bool tuner_find_by_name(struct dvb_frontend *fe, const char *name);
void tuner_set_params(struct dvb_frontend *fe);
int tuner_get_ch_power(struct dvb_frontend *fe);

/* atsc */
#ifdef AML_DEMOD_SUPPORT_ATSC
unsigned int atsc_read_reg_v4(unsigned int addr);
void atsc_write_reg_v4(unsigned int addr, unsigned int data);
void atsc_write_reg(unsigned int reg_addr, unsigned int reg_data);
unsigned int atsc_read_reg(unsigned int reg_addr);
void atsc_write_reg_bits_v4(u32 addr, const u32 data, const u32 start, const u32 len);
unsigned int atsc_read_iqr_reg(void);
#endif

/* dvbc\j83b */
#if defined AML_DEMOD_SUPPORT_DVBC || defined AML_DEMOD_SUPPORT_J83B
void qam_write_reg(struct aml_dtvdemod *demod,
	unsigned int reg_addr, unsigned int reg_data);
unsigned int qam_read_reg(struct aml_dtvdemod *demod, unsigned int reg_addr);
void qam_write_bits(struct aml_dtvdemod *demod,
		u32 reg_addr, const u32 reg_data,
		const u32 start, const u32 len);
void dvbc_write_reg(unsigned int addr, unsigned int data);
unsigned int dvbc_read_reg(unsigned int addr);
#endif /*AML_DEMOD_SUPPORT_VBC||AML_DEMOD_SUPPORT_J83B*/

/* dtmb */
#ifdef AML_DEMOD_SUPPORT_DTMB
void dtmb_write_reg(unsigned int reg_addr, unsigned int reg_data);
unsigned int dtmb_read_reg(unsigned int reg_addr);
void dtmb_write_reg_bits(u32 addr, const u32 data, const u32 start, const u32 len);
#endif /*AML_DEMOD_SUPPORT_DTMB*/


/* demod functions */
void real_para_clear(struct aml_demod_para_real *para);
unsigned long apb_read_reg_collect(unsigned long addr);
void apb_write_reg_collect(unsigned int addr, unsigned int data);
void apb_write_reg(unsigned int reg, unsigned int val);
unsigned long apb_read_reg_high(unsigned long addr);
unsigned long apb_read_reg(unsigned long reg);
int app_apb_write_reg(int addr, int data);
int app_apb_read_reg(int addr);

void demod_set_cbus_reg(unsigned int data, unsigned int addr);
unsigned int demod_read_cbus_reg(unsigned int addr);
void demod_set_demod_reg(unsigned int data, unsigned int addr);
void demod_set_tvfe_reg(unsigned int data, unsigned int addr);
unsigned int demod_read_demod_reg(unsigned int addr);

/* extern int clk_measure(char index); */

void ofdm_initial(int bandwidth,
		  /* 00:8M 01:7M 10:6M 11:5M */
		  int samplerate,
		  /* 00:45M 01:20.8333M 10:20.7M 11:28.57 */
		  int IF,
		  /* 000:36.13M 001:-5.5M 010:4.57M 011:4M 100:5M */
		  int mode,
		  /* 00:DVBT,01:ISDBT */
		  int tc_mode
		  /* 0: Unsigned, 1:TC */);

/*no use void monitor_isdbt(void);*/
void demod_set_reg(struct aml_dtvdemod *demod, struct aml_demod_reg *demod_reg);
void demod_get_reg(struct aml_dtvdemod *demod, struct aml_demod_reg *demod_reg);

/* void demod_calc_clk(struct aml_demod_sta *demod_sta); */
int demod_set_sys(struct aml_dtvdemod *demod, struct aml_demod_sys *demod_sys);

/* for g9tv */
int adc_dpll_setup(int clk_a, int clk_b, int clk_sys, struct aml_demod_sta *demod_sta);
void demod_power_switch(int pwr_cntl);

union adc_pll_cntl {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned pll_m:9;
		unsigned pll_n:5;
		unsigned pll_od0:2;
		unsigned pll_od1:2;
		unsigned pll_od2:2;
		unsigned pll_xd0:6;
		unsigned pll_xd1:6;
	} b;
};

union adc_pll_cntl2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned output_mux_ctrl:4;
		unsigned div2_ctrl:1;
		unsigned b_polar_control:1;
		unsigned a_polar_control:1;
		unsigned gate_ctrl:6;
		unsigned tdc_buf:8;
		unsigned lm_s:6;
		unsigned lm_w:4;
		unsigned reserved:1;
	} b;
};

union adc_pll_cntl3 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned afc_dsel_in:1;
		unsigned afc_dsel_bypass:1;
		unsigned dco_sdmck_sel:2;
		unsigned dc_vc_in:2;
		unsigned dco_m_en:1;
		unsigned dpfd_lmode:1;
		unsigned filter_acq1:11;
		unsigned enable:1;
		unsigned filter_acq2:11;
		unsigned reset:1;
	} b;
};

union adc_pll_cntl4 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned reve:12;
		unsigned tdc_en:1;
		unsigned dco_sdm_en:1;
		unsigned dco_iup:2;
		unsigned pvt_fix_en:1;
		unsigned iir_bypass_n:1;
		unsigned pll_od3:2;
		unsigned filter_pvt1:4;
		unsigned filter_pvt2:4;
		unsigned reserved:4;
	} b;
};

union demod_dig_clk {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned demod_clk_div:7;
		unsigned reserved0:1;
		unsigned demod_clk_en:1;
		unsigned demod_clk_sel:2;
		unsigned reserved1:5;
		unsigned adc_extclk_div:7;	/* 34 */
		unsigned use_adc_extclk:1;	/* 1 */
		unsigned adc_extclk_en:1;	/* 1 */
		unsigned adc_extclk_sel:3;	/* 1 */
		unsigned reserved2:4;
	} b;
};

union demod_adc_clk {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned pll_m:9;
		unsigned pll_n:5;
		unsigned pll_od:2;
		unsigned pll_xd:5;
		unsigned reserved0:3;
		unsigned pll_ss_clk:4;
		unsigned pll_ss_en:1;
		unsigned reset:1;
		unsigned pll_pd:1;
		unsigned reserved1:1;
	} b;
};

union demod_cfg0 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned mode:4;
		unsigned ts_sel:4;
		unsigned test_bus_clk:1;
		unsigned adc_ext:1;
		unsigned adc_rvs:1;
		unsigned adc_swap:1;
		unsigned adc_format:1;
		unsigned adc_regout:1;
		unsigned adc_regsel:1;
		unsigned adc_regadj:5;
		unsigned adc_value:10;
		unsigned adc_test:1;
		unsigned ddr_sel:1;
	} b;
};

union demod_cfg1 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned reserved:8;
		unsigned ref_top:2;
		unsigned ref_bot:2;
		unsigned cml_xs:2;
		unsigned cml_1s:2;
		unsigned vdda_sel:2;
		unsigned bias_sel_sha:2;
		unsigned bias_sel_mdac2:2;
		unsigned bias_sel_mdac1:2;
		unsigned fast_chg:1;
		unsigned rin_sel:3;
		unsigned en_ext_vbg:1;
		unsigned en_cmlgen_res:1;
		unsigned en_ext_vdd12:1;
		unsigned en_ext_ref:1;
	} b;
};

union demod_cfg2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned en_adc:1;
		unsigned biasgen_ibipt_sel:2;
		unsigned biasgen_ibic_sel:2;
		unsigned biasgen_rsv:4;
		unsigned biasgen_en:1;
		unsigned biasgen_bias_sel_adc:2;
		unsigned biasgen_bias_sel_cml1:2;
		unsigned biasgen_bias_sel_ref_op:2;
		unsigned clk_phase_sel:1;
		unsigned reserved:15;
	} b;
};

union demod_cfg3 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned dc_arb_mask:3;
		unsigned dc_arb_enable:1;
		unsigned reserved:28;
	} b;
};

struct agc_power_tab {
	char name[128];
	int level;
	int ncalcE;
	int *calcE;
};

struct dtmb_cfg {
	int dat;
	int adr;
	int rw;
};

void dtvpll_lock_init(void);
void dtvpll_init_flag(int on);
void demod_set_irq_mask(void);
void demod_clr_irq_stat(void);
int demod_set_adc_core_clk(int adc_clk, int sys_clk, struct aml_demod_sta *demod_sta);
void demod_set_adc_core_clk_fix(int clk_adc, int clk_dem);
void calculate_cordic_para(void);
/*extern int aml_fe_analog_set_frontend(struct dvb_frontend *fe);*/
int get_dtvpll_init_flag(void);
void demod_set_mode_ts(struct aml_dtvdemod *demod, enum fe_delivery_system delsys);
void demod_top_write_reg(unsigned int addr, unsigned int data);
void demod_top_write_bits(u32 reg_addr, const u32 reg_data, const u32 start, const u32 len);
unsigned int demod_top_read_reg(unsigned int addr);
void demod_init_mutex(void);
void demod_mutex_lock(void);
void demod_mutex_unlock(void);
void front_write_reg(unsigned int addr, unsigned int data);
void front_write_bits(u32 reg_addr, const u32 reg_data,
		    const u32 start, const u32 len);
unsigned int front_read_reg(unsigned int addr);

#ifdef AML_DEMOD_SUPPORT_ISDBT
unsigned int isdbt_read_reg_v4(unsigned int addr);
void  isdbt_write_reg_v4(unsigned int addr, unsigned int data);
#endif

int dd_hiu_reg_write(unsigned int reg, unsigned int val);
unsigned int dd_hiu_reg_read(unsigned int addr);
void dtvdemod_dmc_reg_write(unsigned int reg, unsigned int val);
unsigned int dtvdemod_dmc_reg_read(unsigned int addr);
void dtvdemod_ddr_reg_write(unsigned int reg, unsigned int val);
unsigned int dtvdemod_ddr_reg_read(unsigned int addr);
int reset_reg_write(unsigned int reg, unsigned int val);
unsigned int reset_reg_read(unsigned int addr);

int clocks_set_sys_defaults(struct aml_dtvdemod *demod, unsigned int adc_clk);
void demod_set_demod_default(void);
unsigned int demod_get_adc_clk(struct aml_dtvdemod *demod);
unsigned int demod_get_sys_clk(struct aml_dtvdemod *demod);

/*register access api new*/
/* isdbt */
#if defined AML_DEMOD_SUPPORT_ISDBT || defined AML_DEMOD_SUPPORT_DVBT
int dvbt_isdbt_set_ch(struct aml_dtvdemod *demod,
		struct aml_demod_dvbt *demod_dvbt);
void dvbt_isdbt_wr_reg(unsigned int addr, unsigned int data);
void dvbt_isdbt_wr_reg_new(unsigned int addr, unsigned int data);
void dvbt_isdbt_wr_bits_new(u32 reg_addr, const u32 reg_data,
		    const u32 start, const u32 len);
unsigned int dvbt_isdbt_rd_reg(unsigned int addr);
unsigned int dvbt_isdbt_rd_reg_new(unsigned int addr);
#endif /*AML_DEMOD_SUPPORT_ISDBT*/

/* dvbt */
#ifdef AML_DEMOD_SUPPORT_DVBT
void dvbt_t2_wrb(unsigned int addr, unsigned char data);
void dvbt_t2_write_w(unsigned int addr, unsigned int data);
void dvbt_t2_wr_byte_bits(u32 addr, const u32 data, const u32 start, const u32 len);
void dvbt_t2_wr_word_bits(u32 addr, const u32 data, const u32 start, const u32 len);
unsigned int dvbt_t2_read_w(unsigned int addr);
char dvbt_t2_rdb(unsigned int addr);
#endif /*AML_DEMOD_SUPPORT_DVBT*/

void riscv_ctl_write_reg(unsigned int addr, unsigned int data);
unsigned int riscv_ctl_read_reg(unsigned int addr);

/* dvbs */
#if defined AML_DEMOD_SUPPORT_DVBS || defined AML_DEMOD_SUPPORT_DVBC
void dvbs_write_bits(u32 reg_addr, const u32 reg_data,
		    const u32 start, const u32 len);
void dvbs_wr_byte(unsigned int addr, unsigned char data);
unsigned char dvbs_rd_byte(unsigned int addr);
void dvbs2_reg_initial(unsigned int symb_rate_kbs, unsigned int is_blind_scan);
#endif

int aml_demod_init(void);
void aml_demod_exit(void);

void t3_revb_set_ambus_state(bool enable, bool is_t2);
void t5w_write_ambus_reg(u32 addr,
	const u32 data, const u32 start, const u32 len);
unsigned int t5w_read_ambus_reg(unsigned int addr);

void demod_enable_frontend_agc(struct aml_dtvdemod *demod,
		enum fe_delivery_system delsys, bool enable);
#if defined AML_DEMOD_SUPPORT_DVBS || defined AML_DEMOD_SUPPORT_DVBC
void fe_l2a_set_symbol_rate(struct fe_l2a_internal_param *pparams, unsigned int symbol_rate);
void fe_l2a_get_agc2accu(struct fe_l2a_internal_param *pparams, unsigned int *pintegrator);
void float_division(long long dividend, unsigned int divisor, int *integer, unsigned int *decimal);
#endif
enum fe_bandwidth dtvdemod_convert_bandwidth(unsigned int input);
int delsys_set(struct dvb_frontend *fe, unsigned int delsys);

int timer_set_max(struct aml_dtvdemod *demod,
		enum ddemod_timer_s tmid, unsigned int max_val);
int timer_begain(struct aml_dtvdemod *demod, enum ddemod_timer_s tmid);
int timer_disable(struct aml_dtvdemod *demod, enum ddemod_timer_s tmid);
int timer_is_en(struct aml_dtvdemod *demod, enum ddemod_timer_s tmid);
int timer_not_enough(struct aml_dtvdemod *demod, enum ddemod_timer_s tmid);
int timer_is_enough(struct aml_dtvdemod *demod, enum ddemod_timer_s tmid);
int timer_tuner_not_enough(struct aml_dtvdemod *demod);

#endif
