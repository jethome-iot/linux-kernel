/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DTMB_FUNC_H__
#define __DTMB_FUNC_H__

#define THRD_TUNER_STRENGTH_DTMB (-100)
#define TIMEOUT_DTMB 2500

enum {
	AMLOGIC_DTMB_STEP0,
	AMLOGIC_DTMB_STEP1,
	AMLOGIC_DTMB_STEP2,
	AMLOGIC_DTMB_STEP3,
	AMLOGIC_DTMB_STEP4,
	AMLOGIC_DTMB_STEP5,	/* time eq */
	AMLOGIC_DTMB_STEP6,	/* set normal mode sc */
	AMLOGIC_DTMB_STEP7,
	AMLOGIC_DTMB_STEP8,	/* set time eq mode */
	AMLOGIC_DTMB_STEP9,	/* reset */
	AMLOGIC_DTMB_STEP10,	/* set normal mode mc */
	AMLOGIC_DTMB_STEP11,
};

enum {
	DTMB_IDLE = 0,
	DTMB_AGC_READY = 1,
	DTMB_TS1_READY = 2,
	DTMB_TS2_READY = 3,
	DTMB_FE_READY = 4,
	DTMB_PNPHASE_READY = 5,
	DTMB_SFO_INIT_READY = 6,
	DTMB_TS3_READY = 7,
	DTMB_PM_INIT_READY = 8,
	DTMB_CHE_INIT_READY = 9,
	DTMB_FEC_READY = 10
};

/*
 * dtmb register write / read
 */

/*test only*/
enum REG_DTMB_D9 {
	DTMB_D9_IF_GAIN,
	DTMB_D9_RF_GAIN,
	DTMB_D9_POWER,
	DTMB_D9_ALL,
};

int dtmb_get_power_strength(int agc_gain);
int dtmb_set_ch(struct aml_dtvdemod *demod,
		struct aml_demod_dtmb *demod_dtmb);
void dtmb_reset(void);
int dtmb_check_status_gxtv(struct dvb_frontend *fe);
int dtmb_check_status_txl(struct dvb_frontend *fe);
int dtmb_bch_check(struct dvb_frontend *fe);
void dtmb_bch_check_new(struct dvb_frontend *fe, bool reset);
void dtmb_register_reset(void);
int dtmb_information(struct seq_file *seq);
void dtmb_set_mem_st(unsigned int mem_start);
int dtmb_read_agc(enum REG_DTMB_D9 type, unsigned int *buf);
unsigned int dtmb_reg_r_che_snr(void);
unsigned int dtmb_reg_r_fec_lock(void);
unsigned int dtmb_reg_r_bch(void);
int check_dtmb_fec_lock(void);
int dtmb_constell_check(void);
void dtmb_no_signal_check_v3(struct aml_dtvdemod *demod);
void dtmb_no_signal_check_finishi_v3(struct aml_dtvdemod *demod);
unsigned int dtmb_detect_first(void);
int dtmb_convert_snr(int in_snr);

#endif /* __DTMB_FUNC_H__ */
