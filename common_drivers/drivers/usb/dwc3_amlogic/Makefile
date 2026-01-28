# SPDX-License-Identifier: GPL-2.0
# define_trace.h needs to know how to find our header
CFLAGS_trace.o				:= -I$(src)

#$(MODULE_NAME)-$(CONFIG_USB_DWC3)			+= dwc3-meson.o

$(MODULE_NAME)-$(CONFIG_USB_DWC3)					+= dwc3_amlogic/core-meson.o

ifneq ($(CONFIG_USB_DWC3),)
ifneq ($(CONFIG_TRACING),)
	$(MODULE_NAME)-y				+= dwc3_amlogic/trace-meson.o
endif

ifneq ($(filter y,$(CONFIG_USB_DWC3_HOST) $(CONFIG_USB_DWC3_DUAL_ROLE)),)
	$(MODULE_NAME)-y				+= dwc3_amlogic/host-meson.o
endif

ifneq ($(filter y,$(CONFIG_USB_DWC3_GADGET) $(CONFIG_USB_DWC3_DUAL_ROLE)),)
	$(MODULE_NAME)-y				+= dwc3_amlogic/gadget-meson.o dwc3_amlogic/ep0-meson.o
endif

ifneq ($(CONFIG_USB_DWC3_DUAL_ROLE),)
	$(MODULE_NAME)-y				+= dwc3_amlogic/drd-meson.o
endif

ifneq ($(CONFIG_USB_DWC3_ULPI),)
	$(MODULE_NAME)-y				+= dwc3_amlogic/ulp-mesoni.o
endif

ifneq ($(CONFIG_DEBUG_FS),)
	$(MODULE_NAME)-y				+= dwc3_amlogic/debugfs-meson.o
endif
endif

##
# Platform-specific glue layers go here
#
# NOTICE: Make sure your glue layer doesn't depend on anything
# which is arch-specific and that it compiles on all situations.
#
# We want to keep this requirement in order to be able to compile
# the entire driver (with all its glue layers) on several architectures
# and make sure it compiles fine. This will also help with allmodconfig
# and allyesconfig builds.
##

#obj-$(CONFIG_USB_DWC3_OMAP)		+= dwc3-omap.o
#obj-$(CONFIG_USB_DWC3_EXYNOS)		+= dwc3-exynos.o
#obj-$(CONFIG_USB_DWC3_PCI)		+= dwc3-pci.o
#obj-$(CONFIG_USB_DWC3_HAPS)		+= dwc3-haps.o
#obj-$(CONFIG_USB_DWC3_KEYSTONE)		+= dwc3-keystone.o
#obj-$(CONFIG_USB_DWC3_MESON_G12A)	+= dwc3-meson-g12a.o
#obj-$(CONFIG_USB_DWC3_OF_SIMPLE)	+= dwc3-of-simple.o
#obj-$(CONFIG_USB_DWC3_ST)		+= dwc3-st.o
#obj-$(CONFIG_USB_DWC3_QCOM)		+= dwc3-qcom.o
#obj-$(CONFIG_USB_DWC3_IMX8MP)		+= dwc3-imx8mp.o
#obj-$(CONFIG_USB_DWC3_XILINX)		+= dwc3-xilinx.o
##
