/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __AML_LCD_COMMON_H__
#define __AML_LCD_COMMON_H__
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vout/lcd/lcd_resman.h>
#include "lcd_reg.h"

/* 20220430: initial version*/
/* 20220610: add c3 support*/
/* 20220619: c3 mipi-dsi display ok*/
/* 20220622: c3 support bt656/1120*/
/* 20220916: port k5.4 code to k5.15*/
/* 20221012: correct t5w vbyone reset reg*/
/* 20221028: fix lane lock && fix t7 mipi lprx reg set*/
/* 20221111: modify edp transmit_unit_size to 48(temporary)*/
/* 20221115: support force unfit mipi-dsi bit_rate_max*/
/* 20221116: add pinmux lock for c3*/
/* 20221123: add ioctl functions, include: power,mute,phy,ss*/
/* 20221207: support drm display mode timing for different frame rate*/
/* 20221208: remove black pattern when enable*/
/* 20221215: remove unnecessary tcon top reset*/
/* 20221216: optimize clk code*/
/* 20230105: update clk ss support*/
/* 20230110: optimize config probe workqueue*/
/* 20230222: update tcon tee memory debug info*/
/* 20230303: fix hdmi mode 47hz & 95hz timing*/
/* 20230313: update tcon debug info print*/
/* 20230319: optimize phy code*/
/* 20230505: t3x support */
/* 20230510: support tcon fw*/
/* 20230525: update tcon debug support */
/* 20230615: txhd2 support */
/* 20230705: t3x fix tconless phy setting */
/* 20230706: Resolve conflicts where DLG changeed and vrr set tcon data at the same time*/
/* 20230710: Remove redundant lcd enable settings*/
/* 20230802: add t5m,t5w,t3x set phy lane amp*/
/* 20230815: add full-link-training and EDID-timing for eDP */
/* 20230816: optimize clk accuracy*/
/* 20230821: update clk ss support*/
/* 20230823: add dma driver for tcon lut*/
/* 20230824: support high resolution vsync measure debug*/
/* 20230906: support pdf action */
/* 20230907: t3x revB OD secure support*/
/* 20230912: bypass phy data buffer */
/* 20230915: update phy setting for txhd2 */
/* 20230918: support ultra refresh rate function*/
/* 20231011: t3x dual display support */
/* 20231012: optimize clk management*/
/* 20231113: update vrr_dev register flow for tablet mode*/
/* 20231205: add lcd config check*/
/* 20231218: update timing management*/
/* 20240118: MIPI DSI arch adjust*/
/* 20240129: update display mode management*/
/* 20240218: optimize lcd config check sequence*/
/* 20240222: update custom control support*/
/* 20240226: add tcon init_table pre_proc*/
/* 20240307: update swpdf support*/
/* 20240319: add tcon pre_proc_clk_en control*/
/* 20240403: update lcd status, notifier event and bypass ufr switch when power off */
/* 20240513: update tcon ufr switch mode 3 flow */
/* 20240515: update lcd ufr switch flow and process time record */
/* 20240529: add lcd frame_lock function */
/* 20240601: lcd tablet multi timing support */
/* 20240607: lcd tcon support extern header */
/* 20240618: lcd tcon new ctrl_type(resolution) for demura multi lut */
/* 20240620: optimize tcon multi data set flow */
/* 20240704: lcd tcon support user info */
/* 20240710: add support for S6 */
/* 20240712: lcd tcon lut dma flow optimize */
/* 20240806: support phy tuning function */
/* 20240909: update phy tuning: get real state from register */
/* 20240923: support reserved memory to transmit panel parameter to kernel */
/* 20241023: optimize mute/unmute flow */
/* 20241127: add lcd config json parse driver */
/* 20241210: add lcd config ini parse support */
#define LCD_DRV_VERSION    "20241210"

#define CFMT_RGB565          0x05
#define CFMT_RGB_6bit        0x06
#define CFMT_RGB_8bit        0x08
#define CFMT_RGB_10bit       0x0a
#define CFMT_RGB_12bit       0x0c
#define CFMT_YCbCr422_8bit   0x18
#define CFMT_YCbCr422_10bit  0x1a
#define CFMT_YCbCr422_12bit  0x1c
#define CFMT_YCbCr444_8bit   0x28
#define CFMT_YCbCr444_10bit  0x2a
#define CFMT_YCbCr444_12bit  0x2c
#define CFMT_YCbCr420_8bit   0x38
#define CFMT_YCbCr420_10bit  0x3a
#define CFMT_YCbCr420_12bit  0x3c

struct color_fmt_info_s {
	unsigned int cfmt;
	unsigned char bits;
	char name[32];
};

extern struct mutex lcd_vout_mutex;
extern spinlock_t lcd_reg_spinlock;
extern int lcd_vout_serve_bypass;
extern struct mutex lcd_tcon_dbg_mutex;

struct num_str_s {
	int  num;
	char str[32];
};

unsigned int str_add_vmode(char *buf, unsigned char newline,
		unsigned short width, unsigned short height, unsigned short fr);

/* lcd common */
int string_to_numbers(const char *str, unsigned int nums[]);
int strnum_get_num(const char *str, struct num_str_s *arr, int size_arr, int dft);
void lcd_delay_us(int us);
void lcd_delay_ms(int ms);
unsigned char *lcd_init_table_load_array(char *name, unsigned char cmd_size,
					 unsigned int *buf, int buf_len,
					 int tbl_max, int *tbl_cnt);
unsigned char aml_lcd_i2c_bus_get_str(const char *str);
int lcd_type_str_to_type(const char *str);
char *lcd_type_type_to_str(int type);
unsigned char lcd_mode_str_to_mode(const char *str);
char *lcd_mode_mode_to_str(int mode);
void *lcd_alloc_dma_buffer(struct aml_lcd_drv_s *pdrv, unsigned int size, dma_addr_t *paddr);
u8 *lcd_vmap(ulong addr, u32 size);
void lcd_unmap_phyaddr(u8 *vaddr);
int  lcd_debug_parse_param(char *buf_orig, char **parm, int max_parm);
void lcd_debug_info_print(char *print_buf);
int lcd_detail_timing_print(struct lcd_detail_timing_s *dt, char *buf, int offset, int max_len);
int lcd_phy_cfg_print(struct phy_config_s *cfg, char *buf, int offset, int max_len);
int lcd_phy_attr_print(struct phy_attr_s *phy, u32 lane_num, char *buf, int offset, int max_len);

void lcd_cpu_gpio_probe(struct aml_lcd_drv_s *pdrv, unsigned int index);
void lcd_cpu_gpio_set(struct aml_lcd_drv_s *pdrv, unsigned int index, int value);
unsigned int lcd_cpu_gpio_get(struct aml_lcd_drv_s *pdrv, unsigned int index);
void lcd_rgb_pinmux_set(struct aml_lcd_drv_s *pdrv, int status);
void lcd_bt_pinmux_set(struct aml_lcd_drv_s *pdrv, int status);
void lcd_vbyone_pinmux_set(struct aml_lcd_drv_s *pdrv, int status);
void lcd_mlvds_pinmux_set(struct aml_lcd_drv_s *pdrv, int status);
void lcd_p2p_pinmux_set(struct aml_lcd_drv_s *pdrv, int status);
void lcd_edp_pinmux_set(struct aml_lcd_drv_s *pdrv, int status);
void lcd_mipi_pinmux_set(struct aml_lcd_drv_s *pdrv, int status);

void lcd_act_timing_dbg_print(struct aml_lcd_drv_s *pdrv);
unsigned int lcd_config_timing_check(struct aml_lcd_drv_s *pdrv,
				     struct lcd_detail_timing_s *ptiming);
int lcd_base_config_load_from_dts(struct aml_lcd_drv_s *pdrv);
void lcd_mlvds_phy_ckdi_config(struct aml_lcd_drv_s *pdrv);
unsigned char lcd_panel_config_load_detect(int index, int key_valid, const char *func_name);
int lcd_check_config_load(struct aml_lcd_drv_s *drv);
int lcd_get_config(struct aml_lcd_drv_s *pdrv);
void lcd_optical_vinfo_update(struct aml_lcd_drv_s *pdrv);

void lcd_vbyone_bit_rate_config(struct aml_lcd_drv_s *pdrv);
void lcd_mlvds_bit_rate_config(struct aml_lcd_drv_s *pdrv);
void lcd_p2p_bit_rate_config(struct aml_lcd_drv_s *pdrv);
void lcd_mipi_dsi_bit_rate_config(struct aml_lcd_drv_s *pdrv);
void lcd_edp_bit_rate_config(struct aml_lcd_drv_s *pdrv);
void lcd_clk_frame_rate_init(struct lcd_detail_timing_s *ptiming);
void lcd_default_to_basic_timing_init_config(struct aml_lcd_drv_s *pdrv);
void lcd_enc_timing_init_config(struct aml_lcd_drv_s *pdrv);
void lcd_base_to_enc_timing_init_config(struct aml_lcd_drv_s *pdrv);
void lcd_enc_h_timing_change(struct aml_lcd_drv_s *pdrv);

int lcd_fr_is_fixed(struct aml_lcd_drv_s *pdrv);
int lcd_fr_is_frac(struct aml_lcd_drv_s *pdrv, unsigned int frame_rate);
int lcd_vmode_frac_is_support(struct aml_lcd_drv_s *pdrv, unsigned int frame_rate);
void lcd_frame_rate_change(struct aml_lcd_drv_s *pdrv);
void lcd_if_enable_retry(struct aml_lcd_drv_s *pdrv);
void lcd_vout_notify_mode_change_pre(struct aml_lcd_drv_s *pdrv);
void lcd_vout_notify_mode_change(struct aml_lcd_drv_s *pdrv);
void lcd_vinfo_update(struct aml_lcd_drv_s *pdrv);
void lcd_vrr_dev_update(struct aml_lcd_drv_s *pdrv);
void lcd_vrr_dev_register(struct aml_lcd_drv_s *pdrv);
void lcd_vrr_dev_unregister(struct aml_lcd_drv_s *pdrv);
void lcd_fr_lock_init(struct aml_lcd_drv_s *pdrv);
void lcd_fr_lock(struct aml_lcd_drv_s *pdrv);

void lcd_queue_work(struct work_struct *work);
inline void lcd_queue_delayed_work(struct delayed_work *delayed_work, int ms);

int lcd_cus_ctrl_load_from_unifykey(struct aml_lcd_drv_s *pdrv, unsigned char *buf,
		unsigned int max_size, unsigned char version);
int lcd_cus_ctrl_load_from_ini(struct aml_lcd_drv_s *pdrv, void *inip, void *psec,
			       unsigned char version);
int lcd_cus_ctrl_load_from_dts(struct aml_lcd_drv_s *pdrv, struct device_node *child);

struct lcd_detail_timing_s *lcd_timing_alloc(struct aml_lcd_drv_s *pdrv);
void lcd_timing_free_last(struct aml_lcd_drv_s *pdrv);
struct phy_attr_s *lcd_phy_alloc(struct aml_lcd_drv_s *pdrv);
void lcd_phy_free_last(struct aml_lcd_drv_s *pdrv);

/* lcd phy */
unsigned int lcd_phy_vswing_level_to_value(struct aml_lcd_drv_s *pdrv, unsigned int level);
unsigned int lcd_phy_preem_level_to_value(struct aml_lcd_drv_s *pdrv, unsigned int level);
unsigned int lcd_phy_support_lane_phase(struct aml_lcd_drv_s *pdrv);
int lcd_phy_param_preset(struct aml_lcd_drv_s *pdrv);
int lcd_phy_param_get(struct aml_lcd_drv_s *pdrv, struct phy_config_s *phy_cfg,
		      struct phy_attr_s *phy);

int lcd_phy_param_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
int lcd_phy_analog_reg_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
void lcd_phy_set(struct aml_lcd_drv_s *pdrv, int status);
int lcd_phy_probe(struct aml_lcd_drv_s *pdrv);
int lcd_phy_config_init(struct lcd_data_s *pdata);

void lcd_phy_tcon_chpi_bbc_init_tl1(struct aml_lcd_drv_s *pdrv);

/* lcd dphy */
void lcd_lane_map_preset(struct aml_lcd_drv_s *pdrv);
void lcd_lane_map_update(struct aml_lcd_drv_s *pdrv);
void lcd_lane_map_set(struct aml_lcd_drv_s *pdrv);
int lcd_lane_sel_get(struct aml_lcd_drv_s *pdrv, struct phy_config_s *phy_cfg);
void lcd_mipi_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off);
void lcd_edp_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off);
void lcd_lvds_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off);
void lcd_vbyone_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off);
void lcd_mlvds_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off);
void lcd_p2p_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off);
int lcd_dphy_reg_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);

/* lcd lvds*/
void lcd_lvds_enable(struct aml_lcd_drv_s *pdrv);
void lcd_lvds_disable(struct aml_lcd_drv_s *pdrv);

/* lcd vbyone*/
void lcd_vbyone_enable(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_disable(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_link_maintain_clear(void);
void lcd_vbyone_wait_timing_stable(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_cdr_training_hold(struct aml_lcd_drv_s *pdrv, int flag);
void lcd_vbyone_wait_hpd(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_power_on_wait_stable(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_wait_stable(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_interrupt_enable(struct aml_lcd_drv_s *pdrv, int flag);
int lcd_vbyone_interrupt_up(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_interrupt_down(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_debug_cdr(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_debug_lock(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_debug_reset(struct aml_lcd_drv_s *pdrv);

/* lcd tcon */
unsigned int lcd_tcon_reg_read(struct aml_lcd_drv_s *pdrv, unsigned int addr);
void lcd_tcon_reg_write(struct aml_lcd_drv_s *pdrv,
			unsigned int addr, unsigned int val);
int lcd_tcon_probe(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_global_reset(struct aml_lcd_drv_s *pdrv);
unsigned int lcd_tcon_table_read(unsigned int addr);
unsigned int lcd_tcon_table_write(unsigned int addr, unsigned int val);
int lcd_tcon_core_update(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_od_set(struct aml_lcd_drv_s *pdrv, int flag);
int lcd_tcon_od_get(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_core_reg_get(struct aml_lcd_drv_s *pdrv,
			  unsigned char *buf, unsigned int size);
int lcd_tcon_top_init(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_enable(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_reload(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_reload_pre(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_disable(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_dbg_check(struct aml_lcd_drv_s *pdrv, struct lcd_detail_timing_s *ptiming);
void lcd_tcon_vsync_isr(struct aml_lcd_drv_s *pdrv);

/* tcon debug */
int lcd_tcon_info_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
ssize_t lcd_tcon_debug_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t lcd_tcon_debug_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count);
ssize_t lcd_tcon_status_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t lcd_tcon_reg_debug_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t lcd_tcon_reg_debug_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count);
ssize_t lcd_tcon_fw_dbg_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t lcd_tcon_fw_dbg_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count);
ssize_t lcd_tcon_pdf_dbg_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t lcd_tcon_pdf_dbg_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count);
ssize_t lcd_tcon_rdma_dbg_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t lcd_tcon_rdma_dbg_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count);
ssize_t lcd_tcon_info_dbg_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t lcd_tcon_cmpr_dbg_show(struct device *dev, struct device_attribute *attr, char *buf);
long lcd_tcon_ioctl_handler(struct aml_lcd_drv_s *pdrv, int mcd_nr, unsigned long arg);

/* lcd debug */
int lcd_debug_info_len(int num);
int lcd_debug_probe(struct aml_lcd_drv_s *pdrv);
int lcd_debug_remove(struct aml_lcd_drv_s *pdrv);

/* lcd clk */
extern spinlock_t lcd_clk_lock;
int meson_clk_measure(unsigned int clk_mux);
void lcd_clk_frac_generate(struct aml_lcd_drv_s *pdrv);
void lcd_clk_generate_parameter(struct aml_lcd_drv_s *pdrv);

int lcd_get_ss(struct aml_lcd_drv_s *pdrv, char *buf);
int lcd_get_ss_num(struct aml_lcd_drv_s *pdrv,
	unsigned int *level, unsigned int *ppm, unsigned int *freq, unsigned int *mode);
int lcd_set_ss(struct aml_lcd_drv_s *pdrv, unsigned int level,
				unsigned int freq, unsigned int mode);
int lcd_encl_clk_msr(struct aml_lcd_drv_s *pdrv);
void lcd_clk_pll_reset(struct aml_lcd_drv_s *pdrv);
void lcd_update_clk_frac(struct aml_lcd_drv_s *pdrv);
void lcd_set_clk(struct aml_lcd_drv_s *pdrv);
int lcd_clk_set_dummy(struct aml_lcd_drv_s *pdrv, int status);
void lcd_disable_clk(struct aml_lcd_drv_s *pdrv);
void lcd_clk_change(struct aml_lcd_drv_s *pdrv);
int lcd_mlvds_clk_phase_set(struct aml_lcd_drv_s *pdrv);
void lcd_clk_gate_switch(struct aml_lcd_drv_s *pdrv, int status);
int lcd_clk_clkmsr_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
int lcd_clk_config_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
int lcd_clk_reg_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
int lcd_clk_path_change(struct aml_lcd_drv_s *pdrv, int sel);
void lcd_clk_ss_param_init(struct aml_lcd_drv_s *pdrv);
void lcd_clk_config_parameter_init(struct aml_lcd_drv_s *pdrv);
void lcd_clk_config_probe(struct aml_lcd_drv_s *pdrv);
void lcd_clk_config_remove(struct aml_lcd_drv_s *pdrv);
void lcd_clk_init(void);
void aml_lcd_prbs_test(struct aml_lcd_drv_s *pdrv, unsigned int ms, unsigned int mode_flag);

/* lcd venc */
unsigned int lcd_get_encl_line_cnt(struct aml_lcd_drv_s *pdrv);
unsigned int lcd_get_encl_frm_cnt(struct aml_lcd_drv_s *pdrv);
unsigned int lcd_get_max_line_cnt(struct aml_lcd_drv_s *pdrv);
void lcd_wait_vsync(struct aml_lcd_drv_s *pdrv);
int lcd_venc_reg_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);

void lcd_gamma_debug_test_en(struct aml_lcd_drv_s *pdrv, int flag);
void lcd_debug_test(struct aml_lcd_drv_s *pdrv, unsigned int num);
void lcd_set_venc_timing(struct aml_lcd_drv_s *pdrv);
void lcd_set_venc(struct aml_lcd_drv_s *pdrv);
void lcd_venc_set_dummy(struct aml_lcd_drv_s *pdrv);
void lcd_venc_change(struct aml_lcd_drv_s *pdrv);
void lcd_venc_vrr_recovery(struct aml_lcd_drv_s *pdrv);
void lcd_venc_enable(struct aml_lcd_drv_s *pdrv, int flag);
void lcd_mute_set(struct aml_lcd_drv_s *pdrv, unsigned char flag);
int lcd_mute_state_get(struct aml_lcd_drv_s *pdrv);
int lcd_get_venc_init_config(struct aml_lcd_drv_s *pdrv);
int lcd_venc_config_init(struct lcd_data_s *pdata);
void lcd_screen_black(struct aml_lcd_drv_s *pdrv);
void lcd_screen_restore(struct aml_lcd_drv_s *pdrv);

void lcd_venc_adj_vtotal(struct aml_lcd_drv_s *pdrv, unsigned int vtotal);

/* lcd driver */
void lcd_power_screen_black(struct aml_lcd_drv_s *pdrv);
void lcd_power_screen_restore(struct aml_lcd_drv_s *pdrv);
void lcd_proc_time_clear(struct aml_lcd_drv_s *pdrv);
#ifdef CONFIG_AMLOGIC_LCD_TV
void lcd_tv_vout_server_init(struct aml_lcd_drv_s *pdrv);
void lcd_tv_vout_server_remove(struct aml_lcd_drv_s *pdrv);
int lcd_mode_tv_init(struct aml_lcd_drv_s *pdrv);
int lcd_mode_tv_remove(struct aml_lcd_drv_s *pdrv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_TABLET

/* @lcd_common.c */
void lcd_mipi_dsi_init_table_detect(struct aml_lcd_drv_s *pdrv, struct device_node *m_node);
void lcd_dsi_tx_ctrl(struct aml_lcd_drv_s *pdrv, unsigned char en);
unsigned long long lcd_dsi_get_min_bitrate(struct aml_lcd_drv_s *pdrv);
/* @lcd_debug.c */
void lcd_dsi_info_print(struct lcd_config_s *pconf);
void lcd_dsi_post_config_load(struct aml_lcd_drv_s *pdrv);
void lcd_dsi_if_bind(struct aml_lcd_drv_s *pdrv);
void lcd_dsi_set_operation_mode(struct aml_lcd_drv_s *pdrv, unsigned char op_mode);
void lcd_dsi_dphy_test(struct aml_lcd_drv_s *pdrv, unsigned char test_item);
void lcd_dsi_write_cmd(struct aml_lcd_drv_s *pdrv, unsigned char *payload);
unsigned char lcd_dsi_read(struct aml_lcd_drv_s *pdrv,
			unsigned char *payload, unsigned char *rd_data, unsigned char rd_byte_len);
/* @lcd_addons/dsi_check_panel.c */
int mipi_dsi_check_state(struct aml_lcd_drv_s *pdrv, unsigned char reg, unsigned char cnt);

void dptx_EDID_dump(struct aml_lcd_drv_s *pdrv);
int dptx_aux_write_single(struct aml_lcd_drv_s *pdrv, unsigned int addr, unsigned char val);
int dptx_aux_read(struct aml_lcd_drv_s *pdrv, unsigned int addr, int len, unsigned char *buf);
void dptx_DPCD_dump(struct aml_lcd_drv_s *pdrv);
int edp_debug_test(struct aml_lcd_drv_s *pdrv, char *str, int num);

void lcd_tablet_vout_server_init(struct aml_lcd_drv_s *pdrv);
void lcd_tablet_vout_server_remove(struct aml_lcd_drv_s *pdrv);
int lcd_mode_tablet_init(struct aml_lcd_drv_s *pdrv);
int lcd_mode_tablet_remove(struct aml_lcd_drv_s *pdrv);
#endif

void lcd_resource_add(struct aml_lcd_drv_s *pdrv, unsigned int res_type, unsigned int res_index);
int lcd_resource_is_ready(struct aml_lcd_drv_s *pdrv);
int lcd_drm_add(struct device *dev);
void lcd_drm_remove(struct device *dev);

#endif
