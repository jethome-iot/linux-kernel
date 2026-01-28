# SPDX-License-Identifier: GPL-2.0

PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
ccflags-y += $(PR_FMT_DEFINE)

# tell define_trace.h where to find the xhci trace header
# CFLAGS_xhci-trace.o := -I$(src)
# ccflags-y += -I$(srctree)/drivers/usb/host/
ccflags-y += -I.

$(MODULE_NAME)-y += xhci_amlogic/xhci-meson.o xhci_amlogic/xhci-mem-meson.o xhci_amlogic/xhci-ext-caps-meson.o
$(MODULE_NAME)-y += xhci_amlogic/xhci-ring-meson.o xhci_amlogic/xhci-hub-meson.o
$(MODULE_NAME)-y += xhci_amlogic/xhci-trace-meson.o
$(MODULE_NAME)-y += xhci_amlogic/xhci-dbg-meson.o

ifneq ($(CONFIG_USB_XHCI_DBGCAP), )
	$(MODULE_NAME)-y += xhci_amlogic/xhci-dbgcap-meson.o xhci_amlogic/xhci-dbgtty-meson.o
endif

ifneq ($(CONFIG_DEBUG_FS),)
	$(MODULE_NAME)-y		+= xhci_amlogic/xhci-debugfs-meson.o
endif
$(MODULE_NAME)-y += xhci_amlogic/xhci-plat-meson.o

ccflags-y += -I$(src)/xhci_amlogic/


