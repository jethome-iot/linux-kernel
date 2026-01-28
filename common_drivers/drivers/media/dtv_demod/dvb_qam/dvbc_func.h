/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DVBC_FUNC_H__
#define __DVBC_FUNC_H__

enum qam_md_e {
	QAM_MODE_16,
	QAM_MODE_32,
	QAM_MODE_64,
	QAM_MODE_128,
	QAM_MODE_256,
	QAM_MODE_AUTO,
	QAM_MODE_NUM
};

#define QAM_MODE_CFG		0x2
#define SYMB_CNT_CFG		0x3
#define SR_OFFSET_ACC		0x8
#define SR_SCAN_SPEED		0xc
#define TIM_SWEEP_RANGE_CFG	0xe

#define ST_DRIVER_ID   0
#define ST_DRIVER_BASE (ST_DRIVER_ID << 16)
#define MAXNAMESIZE 30
#define POWOF4(number) (1 << ((number) << 1)) /* was: s32 PowOf2(s32 number); */

enum dvbc_sym_speed {
	SYM_SPEED_NORMAL,
	SYM_SPEED_MIDDLE,
	SYM_SPEED_HIGH
};

struct stchip_register_t {
	unsigned short addr;     /* Address */
	unsigned char value;    /* Current value */
};

enum fe_sat_search_algo {
	FE_SAT_BLIND_SEARCH, /* offset freq and SR are Unknown */
	FE_SAT_COLD_START,   /* only the SR is known */
	FE_SAT_WARM_START    /* offset freq and SR are known */
};

enum fe_sat_search_standard {
	FE_SAT_AUTO_SEARCH,
	FE_SAT_SEARCH_DVBS1, /* Search Standard*/
	FE_SAT_SEARCH_DVBS2,
	FE_SAT_SEARCH_DSS,
	FE_SAT_SEARCH_TURBOCODE
};

enum fe_sat_rate { /*DVBS1, DSS and turbo code puncture rate*/
	FE_SAT_PR_1_2 = 0,
	FE_SAT_PR_2_3,
	FE_SAT_PR_3_4,
	FE_SAT_PR_4_5, /*for turbo code only*/
	FE_SAT_PR_5_6,
	FE_SAT_PR_6_7, /*for DSS only */
	FE_SAT_PR_7_8,
	FE_SAT_PR_8_9, /*for turbo code only*/
	FE_SAT_PR_UNKNOWN
};

enum fe_sat_search_iq_inv {
	FE_SAT_IQ_AUTO,
	FE_SAT_IQ_AUTO_NORMAL_FIRST,
	FE_SAT_IQ_FORCE_NORMAL,
	FE_SAT_IQ_FORCE_SWAPPED
};

enum fe_sat_modcode {
	/* 0x00..1F: Legacy list, DVBS2
	 * 0x20..3F: DVBS1
	 * 0x40..7F: DVBS2-X With: _S: Short frame (only here to make it work compile, otherwise
	 *		there will be a redefinition of enumerator)
	 *		_L: Linear constellation,
	 *		_R_xx: reserved + Modcode number hex Pon, Poff Pilots configuration
	 */
	FE_SAT_DUMMY_PLF  = 0x00,
	FE_SAT_QPSK_14   = 0x01,
	FE_SAT_QPSK_13   = 0x02,
	FE_SAT_QPSK_25    = 0x03,
	FE_SAT_QPSK_12    = 0x04,
	FE_SAT_QPSK_35   = 0x05,
	FE_SAT_QPSK_23   = 0x06,
	FE_SAT_QPSK_34    = 0x07,
	FE_SAT_QPSK_45    = 0x08,
	FE_SAT_QPSK_56   = 0x09,
	FE_SAT_QPSK_89   = 0x0A,
	FE_SAT_QPSK_910   = 0x0B,
	FE_SAT_8PSK_35    = 0x0C,
	FE_SAT_8PSK_23   = 0x0D,
	FE_SAT_8PSK_34   = 0x0E,
	FE_SAT_8PSK_56    = 0x0F,
	FE_SAT_8PSK_89    = 0x10,
	FE_SAT_8PSK_910  = 0x11,
	FE_SAT_16APSK_23 = 0x12,
	FE_SAT_16APSK_34  = 0x13,
	FE_SAT_16APSK_45  = 0x14,
	FE_SAT_16APSK_56 = 0x15,
	FE_SAT_16APSK_89 = 0x16,
	FE_SAT_16APSK_910 = 0x17,
	FE_SAT_32APSK_34  = 0x18,
	FE_SAT_32APSK_45 = 0x19,
	FE_SAT_32APSK_56 = 0x1A,
	FE_SAT_32APSK_89  = 0x1B,
	FE_SAT_32APSK_910 = 0x1C,
	FE_SAT_MODCODE_UNKNOWN = 0x1D,
	FE_SAT_MODCODE_UNKNOWN_1E = 0x1E,
	FE_SAT_MODCODE_UNKNOWN_1F  = 0x1F,
/* ---------------------------------------- */
	FE_SAT_DVBS1_QPSK_12 = 0x20,
	FE_SAT_DVBS1_QPSK_23 = 0x21,
	FE_SAT_DVBS1_QPSK_34 = 0x22,
	FE_SAT_DVBS1_QPSK_56 = 0x23,
	FE_SAT_DVBS1_QPSK_67 = 0x24,
	FE_SAT_DVBS1_QPSK_78 = 0x25,
	FE_SAT_MODCODE_UNKNOWN_26 = 0x26,
	FE_SAT_MODCODE_UNKNOWN_27 = 0x27,
	FE_SAT_MODCODE_UNKNOWN_28 = 0x28,
	FE_SAT_MODCODE_UNKNOWN_29 = 0x29,
	FE_SAT_MODCODE_UNKNOWN_2A = 0x2A,
	FE_SAT_MODCODE_UNKNOWN_2B = 0x2B,
	FE_SAT_MODCODE_UNKNOWN_2C = 0x2C,
	FE_SAT_MODCODE_UNKNOWN_2D = 0x2D,
	FE_SAT_MODCODE_UNKNOWN_2E = 0x2E,
	FE_SAT_MODCODE_UNKNOWN_2F = 0x2F,
	FE_SAT_MODCODE_UNKNOWN_30 = 0x30,
	FE_SAT_MODCODE_UNKNOWN_31 = 0x31,
	FE_SAT_MODCODE_UNKNOWN_32 = 0x32,
	FE_SAT_MODCODE_UNKNOWN_33 = 0x33,
	FE_SAT_MODCODE_UNKNOWN_34 = 0x34,
	FE_SAT_MODCODE_UNKNOWN_35 = 0x35,
	FE_SAT_MODCODE_UNKNOWN_36 = 0x36,
	FE_SAT_MODCODE_UNKNOWN_37 = 0x37,
	FE_SAT_MODCODE_UNKNOWN_38 = 0x38,
	FE_SAT_MODCODE_UNKNOWN_39 = 0x39,
	FE_SAT_MODCODE_UNKNOWN_3A = 0x3A,
	FE_SAT_MODCODE_UNKNOWN_3B = 0x3B,
	FE_SAT_MODCODE_UNKNOWN_3C = 0x3C,
	FE_SAT_MODCODE_UNKNOWN_3D = 0x3D,
	FE_SAT_MODCODE_UNKNOWN_3E = 0x3E,
	FE_SAT_MODCODE_UNKNOWN_3F = 0x3F,
/* ---------------------------------------- */
	FE_SATX_VLSNR1          = 0x40,
	FE_SATX_VLSNR2        = 0x41,
	FE_SATX_QPSK_13_45      = 0x42,
	FE_SATX_QPSK_9_20     = 0x43,
	FE_SATX_QPSK_11_20      = 0x44,
	FE_SATX_8APSK_5_9_L   = 0x45,
	FE_SATX_8APSK_26_45_L   = 0x46,
	FE_SATX_8PSK_23_36    = 0x47,
	FE_SATX_8PSK_25_36      = 0x48,
	FE_SATX_8PSK_13_18    = 0x49,
	FE_SATX_16APSK_1_2_L    = 0x4A,
	FE_SATX_16APSK_8_15_L = 0x4B,
	FE_SATX_16APSK_5_9_L    = 0x4C,
	FE_SATX_16APSK_26_45  = 0x4D,
	FE_SATX_16APSK_3_5      = 0x4E,
	FE_SATX_16APSK_3_5_L  = 0x4F,
	FE_SATX_16APSK_28_45    = 0x50,
	FE_SATX_16APSK_23_36  = 0x51,
	FE_SATX_16APSK_2_3_L    = 0x52,
	FE_SATX_16APSK_25_36  = 0x53,
	FE_SATX_16APSK_13_18    = 0x54,
	FE_SATX_16APSK_7_9    = 0x55,
	FE_SATX_16APSK_77_90    = 0x56,
	FE_SATX_32APSK_2_3_L  = 0x57,
	FE_SATX_32APSK_R_58     = 0x58,
	FE_SATX_32APSK_32_45  = 0x59,
	FE_SATX_32APSK_11_15    = 0x5A,
	FE_SATX_32APSK_7_9    = 0x5B,
	FE_SATX_64APSK_32_45_L  = 0x5C,
	FE_SATX_64APSK_11_15  = 0x5D,
	FE_SATX_64APSK_R_5E     = 0x5E,
	FE_SATX_64APSK_7_9    = 0x5F,
	FE_SATX_64APSK_R_60     = 0x60,
	FE_SATX_64APSK_4_5    = 0x61,
	FE_SATX_64APSK_R_62     = 0x62,
	FE_SATX_64APSK_5_6    = 0x63,
	FE_SATX_128APSK_3_4     = 0x64,
	FE_SATX_128APSK_7_9   = 0x65,
	FE_SATX_256APSK_29_45_L = 0x66,
	FE_SATX_256APSK_2_3_L = 0x67,
	FE_SATX_256APSK_31_45_L = 0x68,
	FE_SATX_256APSK_32_45 = 0x69,
	FE_SATX_256APSK_11_15_L = 0x6A,
	FE_SATX_256APSK_3_4   = 0x6B,
	FE_SATX_QPSK_11_45      = 0x6C,
	FE_SATX_QPSK_4_15     = 0x6D,
	FE_SATX_QPSK_14_45      = 0x6E,
	FE_SATX_QPSK_7_15     = 0x6F,
	FE_SATX_QPSK_8_15       = 0x70,
	FE_SATX_QPSK_32_45    = 0x71,
	FE_SATX_8PSK_7_15       = 0x72,
	FE_SATX_8PSK_8_15     = 0x73,
	FE_SATX_8PSK_26_45      = 0x74,
	FE_SATX_8PSK_32_45    = 0x75,
	FE_SATX_16APSK_7_15     = 0x76,
	FE_SATX_16APSK_8_15   = 0x77,
	FE_SATX_16APSK_26_45_S  = 0x78,
	FE_SATX_16APSK_3_5_S  = 0x79,
	FE_SATX_16APSK_32_45    = 0x7A,
	FE_SATX_32APSK_2_3    = 0x7B,
	FE_SATX_32APSK_32_45_S  = 0x7C,
/* ---------------------------------------- */
	FE_SATX_8PSK            = 0x7D,
	FE_SATX_32APSK        = 0x7E,
	FE_SATX_256APSK       = 0x7F,  /* POFF Modes */
	FE_SATX_16APSK        = 0x80,
	FE_SATX_64APSK        = 0x81,
	FE_SATX_1024APSK      = 0x82,  /* PON Modes  */
	FE_SAT_MODCODE_MAX      /* Only used for range checking */
};

enum fe_sat_modulation {
	FE_SAT_MOD_QPSK,
	FE_SAT_MOD_8PSK,
	FE_SAT_MOD_16APSK,
	FE_SAT_MOD_32APSK,
	FE_SAT_MOD_VLSNR,
	FE_SAT_MOD_64APSK,
	FE_SAT_MOD_128APSK,
	FE_SAT_MOD_256APSK,
	FE_SAT_MOD_8PSK_L,
	FE_SAT_MOD_16APSK_L,
	FE_SAT_MOD_32APSK_L,
	FE_SAT_MOD_64APSK_L,
	FE_SAT_MOD_256APSK_L,
	FE_SAT_MOD_1024APSK,
	FE_SAT_MOD_UNKNOWN
};	/* sat modulation type*/

enum stchip_error {
	CHIPERR_NO_ERROR = 0x0,       /* No error encountered */
	CHIPERR_INVALID_HANDLE = 0x1,     /* Using of an invalid chip handle */
	CHIPERR_INVALID_REG_ID = 0x2,     /* Using of an invalid register */
	CHIPERR_INVALID_FIELD_ID = 0x4,   /* Using of an Invalid field */
	CHIPERR_INVALID_FIELD_SIZE = 0x8, /* Using of a field with an invalid size */
	CHIPERR_I2C_NO_ACK = 0x10,         /* No acknowledge from the chip */
	CHIPERR_I2C_BURST = 0x20,           /* Two many registers accessed in burst mode */
	CHIPERR_TYPE_ERR = 0x40,
	CHIPERR_CONFIG_ERR = 0x80
};

enum fe_sat_tracking_standard {
	FE_SAT_DVBS1_STANDARD, /* Found Standard*/
	FE_SAT_DVBS2_STANDARD,
	FE_SAT_DSS_STANDARD,
	FE_SAT_TURBOCODE_STANDARD,
	FE_SAT_UNKNOWN_STANDARD
};

enum fe_sat_pilots {
	FE_SAT_PILOTS_OFF,
	FE_SAT_PILOTS_ON,
	FE_SAT_UNKNOWN_PILOTS
};

enum fe_sat_rolloff {
	/* same order than MATYPE_ROLLOFF field */
	FE_SAT_35,
	FE_SAT_25,
	FE_SAT_20,
	FE_SAT_10,
	FE_SAT_05,
	FE_SAT_15,
};

enum fe_sat_frame {
	FE_SAT_NORMAL_FRAME,
	FE_SAT_SHORT_FRAME,
	FE_SAT_LONG_FRAME  = FE_SAT_NORMAL_FRAME,
	FE_SAT_UNKNOWN_FRAME
};

enum fe_sat_iq_inversion {
	FE_SAT_IQ_NORMAL,
	FE_SAT_IQ_SWAPPED
};

struct fe_sat_search_result {
	bool		 locked;	/* Transponder found */
	unsigned int frequency;	/* Found frequency */
	unsigned int symbol_rate;/* Found Symbol rate*/
	enum fe_sat_tracking_standard	standard;	/* Found Standard DVBS1,DVBS2 or DSS*/
	enum fe_sat_rate			puncture_rate;/* Found Puncture rate  For DVBS1*/
	enum fe_sat_modcode			modcode;/* Found Modcode only for DVBS2*/
	enum fe_sat_modulation		modulation;	/* Found modulation type*/
	enum fe_sat_pilots			pilots;	/* pilots Found for DVBS2*/
	enum fe_sat_frame			frame_length;/* Found frame length for DVBS2*/
	enum fe_sat_iq_inversion	spectrum;/* IQ specrum swap setting*/
	enum fe_sat_rolloff		roll_off;/* Rolloff factor (0.2, 0.25 or 0.35)*/
};

enum {
	ST_NO_ERROR = ST_DRIVER_BASE,
	ST_ERROR_BAD_PARAMETER,             /* Bad parameter passed       */
	ST_ERROR_NO_MEMORY,                 /* Memory allocation failed   */
	ST_ERROR_UNKNOWN_DEVICE,            /* Unknown device name        */
	ST_ERROR_ALREADY_INITIALIZED,       /* Device already initialized */
	ST_ERROR_NO_FREE_HANDLES,           /* Cannot open device again   */
	ST_ERROR_OPEN_HANDLE,               /* At least one open handle   */
	ST_ERROR_INVALID_HANDLE,            /* Handle is not valid        */
	ST_ERROR_FEATURE_NOT_SUPPORTED,     /* Feature unavailable        */
	ST_ERROR_INTERRUPT_INSTALL,         /* Interrupt install failed   */
	ST_ERROR_INTERRUPT_UNINSTALL,       /* Interrupt uninstall failed */
	ST_ERROR_TIMEOUT,                   /* Timeout occurred            */
	ST_ERROR_DEVICE_BUSY,               /* Device is currently busy   */
	STFRONTEND_ERROR_I2C
};

enum fe_lla_error {
	FE_LLA_NO_ERROR				= ST_NO_ERROR,
	FE_LLA_INVALID_HANDLE		= ST_ERROR_INVALID_HANDLE,
	FE_LLA_ALLOCATION			= ST_ERROR_NO_MEMORY,
	FE_LLA_BAD_PARAMETER		= ST_ERROR_BAD_PARAMETER,
	FE_LLA_ALREADY_INITIALIZED	= ST_ERROR_BAD_PARAMETER,
	FE_LLA_I2C_ERROR			= STFRONTEND_ERROR_I2C,
	FE_LLA_SEARCH_FAILED,
	FE_LLA_TRACKING_FAILED,
	FE_LLA_NODATA,
	FE_LLA_TUNER_NOSIGNAL,
	FE_LLA_TUNER_JUMP,
	FE_LLA_TUNER_4_STEP,
	FE_LLA_TUNER_8_STEP,
	FE_LLA_TUNER_16_STEP,
	FE_LLA_TERM_FAILED,
	FE_LLA_DISEQC_FAILED,
	FE_LLA_NOT_SUPPORTED
}; /*Error Type*/

enum stchip_fieldtype {
	CHIP_UNSIGNED,
	CHIP_SIGNED
};

struct stchip_field {
	unsigned short reg;      /* Register index */
	unsigned char pos;      /* Bit position */
	unsigned char bits;     /* Bit width */
	unsigned int mask;     /* Mask compute with width and position */
	enum stchip_fieldtype type;     /* Signed or unsigned */
	char name[48]; /* Name */
};

struct stchip_instindex {
	unsigned short regidx;     /* Register reference index */
	unsigned int fldidx;     /* Field reference index */
};

struct stchip_instance {
	unsigned short path;           /* Path index */
	struct stchip_instindex index[16];	/* 1st Register/Field index */
};

/* how to access I2C bus */
enum stchip_mode {
	/* <trans><addr><addr...><data><data...>  (e.g. oxford stbus bridge, fixed transaction) */
	STCHIP_MODE_I2C2STBUS,
	STCHIP_MODE_SUBADR_8,       /* <addr><reg8><data><data>        (e.g. demod chip) */
	STCHIP_MODE_SUBADR_16,      /* <addr><reg8><data><data>        (e.g. demod chip) */
	STCHIP_MODE_NOSUBADR,       /* <addr><data>|<data><data><data> (e.g. tuner chip) */
	/* <addr><data>|<data><data><data> (e.g. tuner chip) only for read */
	STCHIP_MODE_NOSUBADR_RD,
	STCHIP_MODE_STI5197,        /* 5197: base address FDE11000 for QAM IP registers */
	STCHIP_MODE_MXL,		    /* Access mode for MXL201RF tuner */
	STCHIP_MODE_STIH273,		/* whatever */
	STCHIP_MODE_STV0368,		/* 0368: specific use of PAGESHIFT register */
	/* 0368: specific use of MAILBOX communication scheme (!!!requires xp70 F/W!!!)*/
	STCHIP_MODE_STV0368MAILBOX,
	STCHIP_MODE_SUBADR_8_SR,
	STCHIP_MODE_STIH237
};

struct stchip_info {
	unsigned char	i2caddr;          /* Chip I2C address */
	char			name[MAXNAMESIZE];/* Name of the chip */
	unsigned int	nb_insts;          /* Number of instances in the chip */
	int				nb_regs;           /* Number of registers in the chip */
	unsigned int	nb_fields;         /* Number of fields in the chip */
	struct stchip_register_t *p_reg_map_image; /* Pointer to register map */
	struct stchip_field		*p_field_map_image;   /* Pointer to field map */
	struct stchip_instance	*p_inst_map;         /* Pointer to an Instances Map */
	enum stchip_error		error;            /* Error state */
	enum stchip_mode chipmode;/* Access bus in demod (SubAdr) or tuner (NoSubAdr) mode*/
	unsigned char		chipid;           /* Chip cut ID */

	bool				repeater;         /* Is repeater enabled or not ? */
	struct stchip_info *repeater_host;   /* Owner of the repeater */
	/* Pointer to repeater routine */
	enum stchip_error		(*repeater_fn)(struct stchip_info *hchip, bool state);

	/* Parameters needed for non sub address devices */
	unsigned int	wr_start;		  /* Id of the first writable register */
	unsigned int	wr_size;           /* Number of writable registers */
	unsigned short	rd_start;		  /* Id of the first readable register */
	unsigned int	rd_size;			  /* Number of readable registers */
	/* Last accessed register index in the register map Image */
	signed int		last_reg_index;
	/* Abort flag when set to on no register access and no wait are done*/
	bool			abort;

	void			*p_data;			/* pointer to chip data */
	unsigned char	tuner_nb;/* number of tuner from 0 to 3, field added to match Oxford */
	//struct io_access		io_fcn;
	unsigned char	*p_reg_mem_base;	/* Pointer to register map base*/
};

//typedef stchip_info_t  stchip_handle_t;  /* Handle to a chip */

struct fe_l2a_internal_param {
	unsigned int	tuner_bw;
	signed int		tuner_frequency; /* Current tuner frequency (KHz) */
	enum fe_lla_error	demod_error;
	signed int		tuner_index_jump;
	signed int		tuner_index_jump1;
	unsigned int	sr;
	unsigned int	state;
	struct stchip_info handle_demod; /*  Handle to a demodulator */
	struct stchip_info handle_anafe; /*  Handle to AFE */
	struct stchip_info handle_soc; /*  Handle to SOC */
	struct stchip_info handle_tuner; /* Handle to tuner */
	struct stchip_info handle_vglna; /* Handle to the chip VGLNA*/
	enum fe_sat_rolloff roll_off; /* manual RollOff for DVBS1/DSS only */

	/* Demod */
	/* Search standard:Auto, DVBS1/DSS only or DVBS2 only*/
	enum fe_sat_search_standard	demod_search_standard;
	/* Algorithm for search Blind, Cold or Warm*/
	enum fe_sat_search_algo		demod_search_algo;
	/* I,Q inversion search : auto, auto normal first, normal or inverted */
	enum fe_sat_search_iq_inv	demod_search_iq_inv;
	enum fe_sat_rate			demod_puncture_rate;
	enum fe_sat_modcode			demod_modcode;
	enum fe_sat_modulation		demod_modulation;
	struct fe_sat_search_result	demod_results; /* Results of the search */
	/* Last error encountered */
	unsigned int				demod_symbol_rate; /* Symbol rate (Bds) */
	unsigned int				demod_search_range; /* Search range (Hz) */
	unsigned int				quartz; /* Demod reference frequency (Hz)*/
	unsigned int				master_clock; /* Master clock frequency (Hz) */
	/* Temporary definition for LO frequency*/
	unsigned int				lo_frequency;

	/* Global I,Q inversion I,Q connection from tuner to demod*/
	enum fe_sat_iq_inversion	tuner_global_iqv_inv;

	//struct ram_byte			pid_flt;
	//struct gse_ram_byte		gse_flt;
	//struct tuner_access		tuner_fcn;
	//struct io_access		demod_io_fcn;
	//struct io_access		tuner_io_fcn;
};

struct aml_dtvdemod;
struct aml_demod_sts;
struct aml_demod_dvbc;
struct dvb_frontend;

const char *get_qam_name(enum qam_md_e qam);
int amdemod_qam(enum fe_modulation qam);
enum fe_modulation amdemod_qam_fe(enum qam_md_e qam);
int dvbc_get_power_strength(int agc_gain, int tuner_strength);
int dvbc_status(struct aml_dtvdemod *demod, struct aml_demod_sts *demod_sts,
		struct seq_file *seq);
void demod_dvbc_set_qam(struct aml_dtvdemod *demod, unsigned int qam, bool auto_sr);
void demod_dvbc_store_qam_cfg(struct aml_dtvdemod *demod);
void demod_dvbc_fsm_reset(struct aml_dtvdemod *demod);
u32 dvbc_get_status(struct aml_dtvdemod *demod);
u32 dvbc_get_snr(struct aml_dtvdemod *demod);
u32 dvbc_get_ch_sts(struct aml_dtvdemod *demod);
u32 dvbc_get_per(struct aml_dtvdemod *demod);
void dvbc_cfg_sr_scan_speed(struct aml_dtvdemod *demod, enum dvbc_sym_speed spd);
void dvbc_cfg_tim_sweep_range(struct aml_dtvdemod *demod, enum dvbc_sym_speed spd);
void dvbc_cfg_sr_cnt(struct aml_dtvdemod *demod, enum dvbc_sym_speed spd);
u32 dvbc_get_symb_rate(struct aml_dtvdemod *demod);
void demod_dvbc_qam_reset(struct aml_dtvdemod *demod);
void dvbc_reg_initial(struct aml_dtvdemod *demod, struct dvb_frontend *fe);
void dvbc_reg_initial_old(struct aml_dtvdemod *demod);
int dvbc_isr_islock(void);
void dvbc_isr(struct aml_demod_sta *demod_sta);
u32 dvbc_set_qam_mode(unsigned char mode);
int dvbc_set_ch(struct aml_dtvdemod *demod, struct aml_demod_dvbc *demod_dvbc,
		struct dvb_frontend *fe);
u32 dvbc_set_auto_symtrack(struct aml_dtvdemod *demod);
int dvbc_timer_init(void);
void dvbc_timer_exit(void);
int dvbc_cci_task(void *data);
int dvbc_get_cci_task(struct aml_dtvdemod *demod);
void dvbc_create_cci_task(struct aml_dtvdemod *demod);
void dvbc_kill_cci_task(struct aml_dtvdemod *demod);
void demod_dvbc_restore_qam_cfg(struct aml_dtvdemod *demod);
void dvbc_init_reg_ext(struct aml_dtvdemod *demod);
u32 dvbc_get_qam_mode(struct aml_dtvdemod *demod);
void dvbc_cfg_sw_hw_sr_max(struct aml_dtvdemod *demod, unsigned int max_sr);
int dvbc_auto_qam_process(struct aml_dtvdemod *demod, unsigned int *qam_mode);
int dvbc_blind_scan_process(struct aml_dtvdemod *demod);
#endif
