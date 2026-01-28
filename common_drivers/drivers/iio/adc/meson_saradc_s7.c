// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 */

#include "meson_saradc_m8.h"

#define MESON_SAR_ADC_CHAN(_chan) {					\
	.type = IIO_VOLTAGE,						\
	.indexed = 1,							\
	.channel = _chan,						\
	.address = _chan,						\
	.scan_index = _chan,						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |			\
			      BIT(IIO_CHAN_INFO_AVERAGE_RAW) |		\
			      BIT(IIO_CHAN_INFO_PROCESSED),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),		\
	.scan_type = {							\
		.sign = 'u',						\
		.storagebits = 16,					\
		.shift = 0,						\
		.endianness = IIO_CPU,					\
	},								\
	.datasheet_name = "SAR_ADC_CH"#_chan,				\
}

static struct iio_chan_spec meson_s7_sar_adc_iio_channels[] = {
	MESON_SAR_ADC_CHAN(0),
	MESON_SAR_ADC_CHAN(1),
	MESON_SAR_ADC_CHAN(2),
	MESON_SAR_ADC_CHAN(3),
	MESON_SAR_ADC_CHAN(4),
	MESON_SAR_ADC_CHAN(5),
	MESON_SAR_ADC_CHAN(6),
	MESON_SAR_ADC_CHAN(7),
	IIO_CHAN_SOFT_TIMESTAMP(8),
};

static int meson_s7_sar_adc_extra_init(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	/* Disable internal ring counter */
	meson_m8_sar_adc_disable_ring(priv);

	return 0;
}

static const struct meson_sar_adc_diff_ops meson_s7_diff_ops = {
	.extra_init = meson_s7_sar_adc_extra_init,
	.set_test_input = meson_m8_sar_adc_set_test_input,
	.read_fifo = meson_m8_sar_adc_read_fifo,
	.enable_chnl = meson_m8_sar_adc_enable_chnl,
	.read_chnl = meson_m8_sar_adc_read_chnl,
};

static const struct meson_sar_adc_calib meson_sar_adc_calib_s7 = {
	.test_upper = TEST_MUX_VDD,
	.test_lower = TEST_MUX_VSS,
	.test_channel = 4,
};

const struct meson_sar_adc_param meson_sar_adc_s7_param __initconst = {
	.has_bl30_integration = false,
	.clock_rate = 1200000,
	.resolution = 10,
	.disable_ring_counter = 0,
	.dops = &meson_s7_diff_ops,
	.channels = meson_s7_sar_adc_iio_channels,
	.num_channels = ARRAY_SIZE(meson_s7_sar_adc_iio_channels),
	.calib = &meson_sar_adc_calib_s7,
};
