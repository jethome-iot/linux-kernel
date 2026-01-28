# SPDX-License-Identifier: GPL-2.0-only

MODULE_NAME = amlogic-soc-erofs

$(MODULE_NAME)-$(CONFIG_AMLOGIC_EROFS) = super.o inode.o data.o namei.o dir.o utils.o pcpubuf.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_EROFS_XATTR) += xattr.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_EROFS_ZIP) += decompressor.o zmap.o zdata.o decompressor_cryptor.o

obj-$(CONFIG_AMLOGIC_EROFS) = $(MODULE_NAME).o
