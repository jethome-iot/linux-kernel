# SPDX-License-Identifier: GPL-2.0

MODULE_NAME = amlogic-spinor
spinor-objs := core.o sfdp.o swp.o otp.o sysfs.o esmt.o gigadevice.o macronix.o micron-st.o winbond.o xmc.o
obj-$(CONFIG_AMLOGIC_MTD_SPI_NOR) += spinor.o

PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
ccflags-y += $(PR_FMT_DEFINE)
