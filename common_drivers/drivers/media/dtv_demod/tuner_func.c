// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/i2c.h>
#include "aml_demod.h"
#include "demod_func.h"
/*#include "aml_fe.h"*/

bool tuner_find_by_name(struct dvb_frontend *fe, const char *name)
{
	if (!strncmp(fe->ops.tuner_ops.info.name, name, strlen(name)))
		return true;
	else
		return false;
}

/*add to replase aml_fe_analog_set_frontend*/
void tuner_set_params(struct dvb_frontend *fe)
{
	int ret = -1;

	if (fe->ops.tuner_ops.set_params)
		ret = fe->ops.tuner_ops.set_params(fe);
	else
		PR_ERR("set_params NULL\n");
}

int tuner_get_ch_power(struct dvb_frontend *fe)
{
	int strength = 0;
	s16 strengtha = 0;

	if (fe != NULL) {
		if (fe->ops.tuner_ops.get_strength) {
			fe->ops.tuner_ops.get_strength(fe, &strengtha);
			strength = (int)strengtha;
		} else {
			PR_INFO("get_strength NULL\n");
		}
	}

	return strength;
}

struct agc_power_tab *tuner_get_agc_power_table(int type)
{
	/*type :  0-NULL, 1-DCT7070, 2-Maxliner, 3-FJ2207, 4-TD1316 */
	static int calcE_FJ2207[31] = {
		87, 118, 138, 154, 172, 197, 245,
		273, 292, 312, 327, 354, 406, 430,
		448, 464, 481, 505, 558, 583, 599,
		616, 632, 653, 698, 725, 745, 762,
		779, 801, 831 };
	static int calcE_Maxliner[79] = {
		543, 552, 562, 575, 586, 596, 608,
		618, 627, 635, 645, 653, 662, 668,
		678, 689, 696, 705, 715, 725, 733,
		742, 752, 763, 769, 778, 789, 800,
		807, 816, 826, 836, 844, 854, 864,
		874, 884, 894, 904, 913, 923, 932,
		942, 951, 961, 970, 980, 990, 1000,
		1012, 1022, 1031, 1040, 1049, 1059,
		1069, 1079, 1088, 1098, 1107, 1115,
		1123, 1132, 1140, 1148, 1157, 1165,
		1173, 1179, 1186, 1192, 1198, 1203,
		1208, 1208, 1214, 1217, 1218, 1220 };

	static struct agc_power_tab power_tab[] = {
		[0] = { "null", 0, 0, NULL },
		[1] = {
			.name	= "DCT7070",
			.level	= 0,
			.ncalcE = 0,
			.calcE	= NULL,
		},
		[2] = {
			.name	= "Maxlear",
			.level	= -22,
			.ncalcE = sizeof(calcE_Maxliner) / sizeof(int),
			.calcE	= calcE_Maxliner,
		},
		[3] = {
			.name	= "FJ2207",
			.level	= -62,
			.ncalcE = sizeof(calcE_FJ2207) / sizeof(int),
			.calcE	= calcE_FJ2207,
		},
		[4] = {
			.name	= "TD1316",
			.level	= 0,
			.ncalcE = 0,
			.calcE	= NULL,
		},
	};

	if (type >= 2 && type <= 3)
		return &power_tab[type];
	else
		return &power_tab[3];
};

int agc_power_to_dbm(int agc_gain, int ad_power, int offset, int tuner)
{
	struct agc_power_tab *ptab = tuner_get_agc_power_table(tuner);
	int est_rf_power;
	int j;

	for (j = 0; j < ptab->ncalcE; j++)
		if (agc_gain <= ptab->calcE[j])
			break;

	est_rf_power = ptab->level - j - (ad_power >> 4) + 12 + offset;

	return est_rf_power;
}
