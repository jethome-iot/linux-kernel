// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/types.h>
#include <linux/amlogic/amfc_regs.h>
#include <linux/amlogic/amfc.h>
#include <linux/libfdt.h>
#include <linux/sizes.h>
#include <linux/errno.h>

#include <linux/compiler.h>
#include <asm/unaligned.h>
#include <asm/memory.h>

#ifdef STATIC
#define PREBOOT
#else
#endif

#define AMFC_DECOMPRESS_DEBUG	0

#define __arch_getl(a)		(*(volatile unsigned int *)(long)(a))
#define __arch_putl(v,a)	(*(volatile unsigned int *)(long)(a) = (v))
#define readl(c)		({ u32 __v = __arch_getl(c); dmb(); __v; })
#define writel(v,c)		({ u32 __v = v; dmb(); __arch_putl(__v,c); __v; })

struct amfc_cmd_list acl  __attribute__((aligned(64)));

#define ADDR_SRC		0
#define ADDR_DST		1

extern int serial_putc(unsigned char c);

void serial_put_hex(unsigned long hex)
{
	int i;
	int d;

	for(i = 0; i < 8; i++) {
		d = ((hex >> ((7-i)*4)) & 0xf);
		if (d < 10)
			serial_putc(d + '0');
		else
			serial_putc(d + 'A' - 10);
	}
}

void serial_puts(char *s)
{
	while (*s)
		serial_putc(*s++);
}

#if AMFC_DECOMPRESS_DEBUG
void dump_mem(void *addr, int size)
{
	int i;
	unsigned int *p = addr;

	serial_putc('\r');
	serial_putc('\n');
	for (i = 0; i < size / 4; i += 4) {
		serial_put_hex((unsigned long)(p + i));
		serial_putc(':');
		serial_put_hex((unsigned long)p[i]);
		serial_putc(' ');
		serial_put_hex((unsigned long)p[i + 1]);
		serial_putc(' ');
		serial_put_hex((unsigned long)p[i + 2]);
		serial_putc(' ');
		serial_put_hex((unsigned long)p[i + 3]);
		serial_putc('\r');
		serial_putc('\n');
	}
}
#endif

static unsigned int amfc_hw_read(unsigned int reg)
{
	return readl(CONFIG_AMFC_BASE + reg);
}

static void amfc_hw_write(unsigned int value, unsigned int reg)
{
	writel(value, CONFIG_AMFC_BASE + reg);
}

static void set_up_addr(struct amfc_cmd_list *acl, unsigned long long addr, int type)
{
	unsigned int low, high = 0;

	low  = addr & 0xffffffff;
	high = (addr >> 32) & 0xf;

	switch (type) {
	case ADDR_SRC:
		acl->src_addr = low;
		acl->control |= (high << 20);
		break;

	case ADDR_DST:
		acl->dst_addr = low;
		acl->control |= (high << 16);
		break;

	default:
		break;
	}
}

/*
 * src: source buffer that need decompress
 * dst: destination buffer that need store
 * src_size: size of source buffer
 * dst_size: size for destination buffer, caller should make sure
 *           dst size is enough
 * return value: decompressed size if success, negative value if failed
 */
#ifdef PREBOOT
STATIC int __decompress(unsigned char *src, long src_size,
			long (*fill)(void*, unsigned long),
			long (*flush)(void*, unsigned long),
			unsigned char *dst, long dst_size,
			long *pos,
			void (*error)(char *x))
{
	unsigned int status;
	unsigned long inv_end;

	/* caller pass 0, but raw size appened at end of src buffer*/
	if (!dst_size) {
		src_size -= sizeof(int);
		dst_size = src[src_size] |
			   src[src_size + 1] << 8  |
			   src[src_size + 2] << 16 |
			   src[src_size + 3] << 24;
	}

#if AMFC_DECOMPRESS_DEBUG
	serial_put_hex((unsigned long)src);
	serial_putc('-');
	serial_put_hex((unsigned long)dst);
	serial_putc('-');
	serial_put_hex((unsigned long)src_size);
	serial_putc('-');
	serial_put_hex((unsigned long)dst_size);
#endif
	inv_end = (unsigned long)dst + dst_size;

	/* setup command list */
	memset(&acl, 0, sizeof(acl));
	acl.src_size = src_size;
	acl.dst_size = dst_size;
	set_up_addr(&acl, (unsigned long)src, ADDR_SRC);
	set_up_addr(&acl, (unsigned long)dst, ADDR_DST);
	acl.algorithm = ALGORITHM_ZSTD;
	acl.end       = 1;
	acl.compress  = CMD_DECOMPRESS;
	acl.owner     = 1;
	acl.status    = 0xffffffff;

#if AMFC_DECOMPRESS_DEBUG
	dump_mem(src, 64);
#endif
	cache_clean_flush((unsigned long)src, (unsigned long)src + src_size);
	cache_clean_flush((unsigned long)&acl, (unsigned long)&acl + sizeof(acl));

	cache_clean_flush((unsigned long)dst, inv_end);

	/* sw reset */
	amfc_hw_write(0x80000000, AMFC_GL_CMD1_CONTROL);
	amfc_hw_write(0x03, AMFC_GL_CMD1_IRQCLR);
	amfc_hw_write((unsigned long)&acl >> ADDR_SHIFT, AMFC_GL_CMD1_DESC_BASE_ADDR);
	/* trigger */
	amfc_hw_write(1, AMFC_GL_CMD1_CONTROL);

#if AMFC_DECOMPRESS_DEBUG
	serial_puts("\r\nACL:");
	dump_mem(&acl, 64);
	serial_puts("\r\nRegs:");
	dump_mem((void *)((unsigned long)CONFIG_AMFC_BASE), 160);
#endif
	/* wait for done or error */
	while (1) {
		status = amfc_hw_read(AMFC_GL_CMD1_STATUS);
		if ((status & IRQ_MASK))
			break;
	}

	status = amfc_hw_read(AMFC_GL_CMD1_STATUS);
	if (!(status & AMFC_ERROR_MASK)) {
		amfc_hw_write(0x01, AMFC_GL_CMD1_IRQCLR);
	#if AMFC_DECOMPRESS_DEBUG
		serial_puts("\r\n Decompress OK:");
		serial_put_hex(acl.result_size);
	#endif
		return 0;
	} else {
		amfc_hw_write(0x03, AMFC_GL_CMD1_IRQCLR);
		serial_puts("\r\n Decompress failed:");
		serial_put_hex(status);
		return -EINVAL;
	}
}
#endif
