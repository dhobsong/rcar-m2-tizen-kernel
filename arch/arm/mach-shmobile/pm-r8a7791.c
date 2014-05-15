/*
 * r8a7791 Power management support
 *
 * Copyright (C) 2013-2014  Renesas Electronics Corporation
 * Copyright (C) 2011  Renesas Solutions Corp.
 * Copyright (C) 2011  Magnus Damm
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_clock.h>
#include <asm/io.h>
#include <mach/pm-rcar.h>
#include <mach/r8a7791.h>

/* SYSC */
#define SYSCIER 0x0c
#define SYSCIMR 0x10

#if defined(CONFIG_PM) || defined(CONFIG_SMP)

static void __init r8a7791_sysc_init(void)
{
	void __iomem *base = rcar_sysc_init(0xe6180000);

	/* enable all interrupt sources, but do not use interrupt handler */
	iowrite32(0x0131000e, base + SYSCIER);
	iowrite32(0, base + SYSCIMR);
}

#else /* CONFIG_PM || CONFIG_SMP */

static inline void r8a7791_sysc_init(void) {}

#endif /* CONFIG_PM || CONFIG_SMP */

#ifdef CONFIG_PM

#define CPG_BASE 0xe6150000
#define CPG_LEN 0x1000

/* Software Reset */
#define SRCR0		0x00a0
#define SRCR1		0x00a8
#define SRCR2		0x00b0
#define SRCR3		0x00b8
#define SRCR4		0x00bc
#define SRCR5		0x00c4
#define SRCR6		0x01c8
#define SRCR7		0x01cc
#define SRCR8		0x0920
#define SRCR9		0x0924
#define SRCR10		0x0928
#define SRCR11		0x092c
#define SRSTCLR0	0x0940
#define SRSTCLR1	0x0944
#define SRSTCLR2	0x0948
#define SRSTCLR3	0x094c
#define SRSTCLR4	0x0950
#define SRSTCLR5	0x0954
#define SRSTCLR6	0x0958
#define SRSTCLR7	0x095c
#define SRSTCLR8	0x0960
#define SRSTCLR9	0x0964
#define SRSTCLR10	0x0968
#define SRSTCLR11	0x096c

#define SRST_REG(n)	{ .srcr = SRCR##n, .srstclr = SRSTCLR##n, }

static struct software_reset_reg {
	u16	srcr;
	u16	srstclr;
} r8a7791_reset_regs[] = {
	[0] = SRST_REG(0),
	[1] = SRST_REG(1),
	[2] = SRST_REG(2),
	[3] = SRST_REG(3),
	[4] = SRST_REG(4),
	[5] = SRST_REG(5),
	[6] = SRST_REG(6),
	[7] = SRST_REG(7),
	[8] = SRST_REG(8),
	[9] = SRST_REG(9),
	[10] = SRST_REG(10),
	[11] = SRST_REG(11),
};

static DEFINE_SPINLOCK(r8a7791_reset_lock);

void r8a7791_module_reset(unsigned int n, u32 bits, int usecs)
{
	void __iomem *cpg_base;
	unsigned long flags;
	u32 srcr;

	if (n >= ARRAY_SIZE(r8a7791_reset_regs)) {
		pr_err("SRCR%u is not available\n", n);
		return;
	}

	if (usecs <= 0)
		usecs = 50; /* give a short delay for at least one RCLK cycle */

	cpg_base = ioremap(CPG_BASE, CPG_LEN);

	spin_lock_irqsave(&r8a7791_reset_lock, flags);
	srcr = readl_relaxed(cpg_base + r8a7791_reset_regs[n].srcr);
	writel_relaxed(srcr | bits, cpg_base + r8a7791_reset_regs[n].srcr);
	readl_relaxed(cpg_base + r8a7791_reset_regs[n].srcr); /* sync */
	spin_unlock_irqrestore(&r8a7791_reset_lock, flags);

	udelay(usecs);

	writel_relaxed(bits, cpg_base + r8a7791_reset_regs[n].srstclr);
	readl_relaxed(cpg_base + r8a7791_reset_regs[n].srstclr); /* sync */

	iounmap(cpg_base);
}

static int pd_power_down(struct generic_pm_domain *genpd)
{
	struct rcar_sysc_ch *r8a7791_ch = to_r8a7791_ch(genpd);
	int ret;

	ret =  rcar_sysc_power_down(to_r8a7791_ch(genpd));

	if (r8a7791_ch->chan_offs == 0xc0) {
		/*
		 * Issue software reset to 3DG functional blocks right after
		 * the SGX power shut-off to avoid a hardware lock-up issue
		 * triggered when we bring the SGX power up next time.
		 */
		r8a7791_module_reset(1, BIT(12), 2); /* DVFS */
		r8a7791_module_reset(8, BIT(0), 2); /* CONST */
	}

	return ret;
}

static int pd_power_up(struct generic_pm_domain *genpd)
{
	return rcar_sysc_power_up(to_r8a7791_ch(genpd));
}

static bool pd_is_off(struct generic_pm_domain *genpd)
{
	return rcar_sysc_power_is_off(to_r8a7791_ch(genpd));
}

static bool pd_active_wakeup(struct device *dev)
{
	return true;
}

static void r8a7791_init_pm_domain(struct r8a7791_pm_domain *r8a7791_pd)
{
	struct generic_pm_domain *genpd = &r8a7791_pd->genpd;

	pm_genpd_init(genpd, NULL, true);
	genpd->dev_ops.stop = pm_clk_suspend;
	genpd->dev_ops.start = pm_clk_resume;
	genpd->dev_ops.active_wakeup = pd_active_wakeup;
	genpd->dev_irq_safe = true;
	genpd->power_off = pd_power_down;
	genpd->power_on = pd_power_up;

	if (pd_is_off(&r8a7791_pd->genpd))
		pd_power_up(&r8a7791_pd->genpd);
}

static struct r8a7791_pm_domain r8a7791_pm_domains[] = {
	{
		.genpd.name = "pvrsrvkm",
		.ch = {
			.chan_offs = 0xc0, /* PWRSR2 .. PWRER2 */
			.isr_bit = 20, /* SGX */
		},
	},
};

void __init r8a7791_init_pm_domains(void)
{
	int j;

	for (j = 0; j < ARRAY_SIZE(r8a7791_pm_domains); j++)
		r8a7791_init_pm_domain(&r8a7791_pm_domains[j]);
}

#endif /* CONFIG_PM */

void __init r8a7791_pm_init(void)
{
	static int once;

	if (!once++)
		r8a7791_sysc_init();
}
