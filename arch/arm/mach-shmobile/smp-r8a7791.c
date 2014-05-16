/*
 * SMP support for r8a7791
 *
 * Copyright (C) 2014 Renesas Electronics Corporation
 * Copyright (C) 2013 Renesas Solutions Corp.
 * Copyright (C) 2013 Magnus Damm
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <asm/smp_plat.h>
#include <mach/common.h>
#include <mach/r8a7791.h>

#define RST		0xe6160000
#define CA15BAR		0x0020
#define CA15RESCNT	0x0040
#define RAM		0xe6300000
#define APMU		0xe6151000
#define CA15DBGRCR	0x1180

static void __init r8a7791_smp_prepare_cpus(unsigned int max_cpus)
{
	void __iomem *p;
	u32 bar, val;

	/* let APMU code install data related to shmobile_boot_vector */
	shmobile_smp_apmu_prepare_cpus(max_cpus);

	/* RAM for jump stub, because BAR requires 256KB aligned address */
	p = ioremap_nocache(RAM, shmobile_boot_size);
	memcpy_toio(p, shmobile_boot_vector, shmobile_boot_size);
	iounmap(p);

	/* setup reset vectors */
	p = ioremap_nocache(RST, 0x63);
	bar = (RAM >> 8) & 0xfffffc00;
	writel_relaxed(bar, p + CA15BAR);
	writel_relaxed(bar | 0x10, p + CA15BAR);

	/* enable clocks to all CPUs */
	writel_relaxed((readl_relaxed(p + CA15RESCNT) & ~0x0f) | 0xa5a50000,
		       p + CA15RESCNT);
	iounmap(p);

	/* setup for debug mode */
	if (rcar_gen2_read_mode_pins() & MD(21)) {
		p = ioremap_nocache(APMU, 0x2000);
		val = readl_relaxed(p + CA15DBGRCR);
		writel_relaxed((val | 0x01f80000), p + CA15DBGRCR);
		iounmap(p);
	}
}

struct smp_operations r8a7791_smp_ops __initdata = {
	.smp_prepare_cpus	= r8a7791_smp_prepare_cpus,
	.smp_boot_secondary	= shmobile_smp_apmu_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_disable		= shmobile_smp_cpu_disable,
	.cpu_die		= shmobile_smp_apmu_cpu_die,
	.cpu_kill		= shmobile_smp_apmu_cpu_kill,
#endif
};
