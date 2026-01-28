/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AMLOGIC_DisplayPort_TX_DPTX_H
#define _AMLOGIC_DisplayPort_TX_DPTX_H

#include <linux/types.h>
#include <linux/cdev.h>
//#include <drm/amlogic/meson_connector_dev.h>
//#include <drm/drm_dp_helper.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#include <linux/amlogic/media/vout/DisplayPort/DPTX_timing.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vpu/vpu.h>

//#define LCD_DEBUG_INFO
extern u8 dptx_print_level;
//bit[15:0]
#define LOG_E      0 // log level ERROR & Key
#define LOG_I      1 // log level Informative
#define LOG_V      2 // log level Verbose
#define LOG_A      3 // log level All

#define DPTXPR(idx, level, fmt, args...) \
	do { \
		if (!level) \
			pr_err("[DPTX-%u]E: " fmt "\n", idx, ## args); \
		else if (dptx_print_level >= level) \
			pr_info("[DPTX-%u]: " fmt "\n", idx, ## args); \
	} while (0)

#define PR_BUF_MAX              (4 * 1024)

#define DPTX_MAX_DRV              2
#define DPTX_MAX_VOUT             3

/* ******** clk_ctrl ******** */
#define CLK_CTRL_LEVEL              28 /* [30:28] */
#define CLK_CTRL_FRAC_SHIFT         24 /* [24] */
#define CLK_CTRL_FRAC               0  /* [23:0] */

#define RGB_DELAY                   13
#define PRE_DE_DELAY                8

enum dptx_chip_e {
	DPTX_CHIP_T7,
	DPTX_CHIP_MAX,
};

struct dptx_chip_data_s {
	enum dptx_chip_e chip_type;
	const char *chip_name;
	u8  drv_max;
	u32 offset_venc[DPTX_MAX_DRV];     // 0x600
	u32 offset_venc_if[DPTX_MAX_DRV];  // 0x500
	u32 offset_venc_data[DPTX_MAX_DRV];// 0x100

	u32 link_rate;
	u8  TPS_support;
	u8  DACP_support;
	u8 venc_clk_msr_id[DPTX_MAX_DRV];
};

struct dptx_boot_ctrl_s {
	u8 link_rate;
	u8 lane_count;
	u8 vmode_sel_idx;
	u8 vmode_cfmt_sel;
	u8 tx_prepared;
	u8 disp_on;
	u8 uboot_edid_crc;
};

struct dptx_reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	char flag;
};

#define MOD_LEN_MAX        30

struct dptx_venc_cfg_s {
	u8 pll_flag;
	u8 clk_mode;
	u8 ppc;
	u8 clk_change; /* internal used */
	u8 ss_level;
	u8 ss_freq;
	u8 ss_mode;

	unsigned int enc_clk;
	unsigned long long bit_rate; /* Hz */

	unsigned int hstart;
	unsigned int hend;
	unsigned int vstart;
	unsigned int vend;
	u8 pre_de_h;
	u8 pre_de_v;

	unsigned short de_hs_addr;
	unsigned short de_he_addr;
	unsigned short de_vs_addr;
	unsigned short de_ve_addr;

	unsigned short hs_hs_addr;
	unsigned short hs_he_addr;
	unsigned short hs_vs_addr;
	unsigned short hs_ve_addr;

	unsigned short vs_hs_addr;
	unsigned short vs_he_addr;
	unsigned short vs_vs_addr;
	unsigned short vs_ve_addr;

	unsigned short pre_h_de_start;
	unsigned short pre_h_de_end;
	unsigned short pre_v_de_start;
	unsigned short pre_v_de_end;
	unsigned short pre_hso_start;
	unsigned short pre_hso_end;
	unsigned short pre_vso_hstart;
	unsigned short pre_vso_hend;
	unsigned short pre_vso_start;
	unsigned short pre_vso_end;
};

struct dptx_phy_cfg_s {
	u32 flag;
	u32 vswing;
	struct dptx_phy_lane_s {
		u8 status;
		u8 amp;
		u8 preem;
		u8 post_cur;
	} lane[4];
	u16 level_to_phy_lut[10][3];
};

#define DPTX_TYPE_DP          0
#define DPTX_TYPE_eDP         1

struct dptx_link_cfg_s {
	u8 max_lane_count;
	u8 max_link_rate;
	u8 TPS_support;   // bit[0]=TPS3;bit[1]=TPS4;
	u8 DACP_support;  // bit[0]=M1(HDCP);bit[1]=M2(Panel Limit);bit[2]=M3a(eDP ASSR);
	u8 down_ss;
	u8 train_aux_rd_interval;
	u8 dev_type;      // 0=DP;1=eDP
	u8 DPCD_reg_func; // bit[0]=extend DPCD;bit[1]=eDP_DPCD;bit[2]=eDP_DPCD_BL;

	u8 enhanced_framing_en;
	u8 sync_clk_mode;
	// u8 EDID_en;

	/* internal used */
	// unsigned char coding_support;
	u8 training_mode;
	u8 preset_ds[4];

	u8 HPD_level;
	u8 irq_sta;

	u8 link_rate;
	u8 lane_count;
	u8 link_rate_update;

	u8 phy_update;
	u8 curr_ds[4];
	u8 adj_req_ds[4];
};

#define LCD_CLK_SS_BIT_FREQ             0
#define LCD_CLK_SS_BIT_MODE             4

#define DPTX_GPIO_MAX                    0xff
#define DPTX_GPIO_OUTPUT_LOW             0
#define DPTX_GPIO_OUTPUT_HIGH            1
#define DPTX_GPIO_INPUT                  2

#define DPTX_VENC_1PPC          1
#define DPTX_VENC_2PPC          2
#define DPTX_VENC_4PPC          4

struct aux_irq_semaphore_s {
	raw_spinlock_t lock;
	bool v;
};

struct dptx_clk_cfg_s {
	u64 fin;
	u64 fout;
	u8 clk_src; // 0=PLL, 1=FIX_PLL, 2=LINK_CLK
	void *pll_data;

	/* pll parameters */
	unsigned int pll_id;
	unsigned int pll_offset;
	unsigned int pll_od_fb;
	u16 pll_m;
	u16 pll_n;
	u64 pll_fvco;
	u8 pll_od_sel[3];
	u8 pll_tcon_div_sel;

	unsigned int pll_frac;
	unsigned int pll_frac_half_shift;
	unsigned long long pll_fout;
	unsigned int pll_div_fout;

	unsigned int ss_level;
	unsigned int ss_dep_sel;
	unsigned int ss_str_m;
	unsigned int ss_ppm;
	unsigned int ss_freq;
	unsigned int ss_mode;
	unsigned int ss_en;

	u8 pll_clk_div_sel;
	u8 div0;
	u8 div1;
	u8 xd;
	u8 vclk_sel;

	// unsigned int err_fmin;
	unsigned int done;
};

#define DPTX_DRV_TIMING_MAX  8
struct dptx_edid_info_s {
	u8 manufacturer_id[4];  //[8:9]2byte
	u16 product_id;         //[10:11]2byte
	u32 product_sn;         //[12:15]4byte
	u8 week;                //[16]1byte
	u8 year;                //[17]1byte + 1990
	u16 version;            //[18:19]2byte
	u32 established_timing; //[35:37]3byte
	// u32 standard_timing1;   //[38:45]4byte
	// u32 standard_timing2;   //[46:53]4byte
	u16 cfmt_support;       // each bit to CFMT

	char name[14];
	char serial_num[14];
	char asc_string[14];
	// unsigned int VIC_val[8];
	struct dptx_edid_range_limit_s {
		u16 vfreq[2];
		u16 hfreq[2];
		u32 pclk[2];
		u16 h_blank_min, v_blank_min;
	} range;

	u8 dtd_cnt;

	struct dptx_detail_timing_s dtd_timing[DPTX_DRV_TIMING_MAX];

	u8 ext_flag;  //[126]1byte
	u8 did_version;

	u8 edid_crc;
};

#define DPTX_DRV_VMODE_MAX   64
#define VMODE_FLAG_VALID     BIT(0)
#define VMODE_FLAG_FR_RANGE  BIT(1)
#define VMODE_FLAG_PREFERRED BIT(2)
// #define VMODE_FLAG_BW_ENOUGH BIT(3)
struct dptx_vmode_mgr_s {
	struct dptx_vmode_s {
		u32 fr100_int;
		u8 fr_adv; // 0=fr_int; 1=fr_frac; 0xff=raw fr
		u8 base_dtd_idx;
		u8 flag;
		u16 cfmt_support;
	} vmodes[DPTX_DRV_VMODE_MAX];
	u8 vmode_sel_idx;
	u8 vmode_cfmt_sel;
};

#define DPTX_STA_PROBE_DONE   BIT(0)
#define DPTX_STA_DRV_READY    BIT(1)
#define DPTX_STA_HPD_TRI_EN   BIT(2)
#define DPTX_STA_HPD_HIGH     BIT(3)
#define DPTX_STA_LINK_ON      BIT(4)
#define DPTX_STA_DISP_ON      BIT(5)

#define DPTX_GPIO_NAME_MAX    12

struct dptx_drv_s {
	u8 idx;
	u8 status;
	u8 mode; // 0=DP; 1=eDP
	u8 viu_sel; // 1=vout; 2=vou2; 3=vout3
	u8 crtc_sel;
	u8 uboot_edid_crc;
	// atomic_t aux_irq_sta;

	char dev_name[32];
	u8 user_link_rate;  //dft: 4
	u8 user_lane_count; //dft: 0xfff
	u8 user_color_format;
	u16 hpd_ignore;
	u16 vsync_cnt;

	struct gpio_desc *PWR_gpio;
	struct gpio_desc *CFG1_gpio;
	struct gpio_desc *CFG2_gpio;
	struct pinctrl *pin_c;

	char PWR_gpio_name[DPTX_GPIO_NAME_MAX];
	char CFG1_gpio_name[DPTX_GPIO_NAME_MAX];
	char CFG2_gpio_name[DPTX_GPIO_NAME_MAX];

	struct resource *res_vsync_irq[DPTX_MAX_VOUT];
	struct resource *res_HPD_irq;
	char vsync_name[DPTX_MAX_VOUT][16];
	char HPD_name[12];

	struct dptx_chip_data_s *data;
	struct dptx_reg_map_s *reg_map;

	struct dptx_phy_cfg_s phy_cfg;
	struct dptx_clk_cfg_s vid_clk;
	struct dptx_clk_cfg_s link_clk;
	struct dptx_venc_cfg_s venc_cfg;
	struct dptx_link_cfg_s link_cfg;

	struct dptx_edid_info_s edid_info;
	struct dptx_vmode_mgr_s vmode_mgr;
	u8 next_vmd_idx;

	struct dptx_detail_timing_s act_timing;

	struct cdev cdev;
	struct device *dev;
	struct platform_device *pdev;

	struct vout_server_s vout_server[DPTX_MAX_VOUT];
	spinlock_t isr_lock; //interrupt lock
#ifdef CONFIG_AMLOGIC_VPU
	struct vpu_dev_s *vpu_dev;
#endif
	struct vinfo_s vinfo;

	void *if_ctrls;

	//struct connector_hpd_cb drm_hpd_cb;
};

#endif
