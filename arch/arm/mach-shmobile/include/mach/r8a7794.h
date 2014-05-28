#ifndef __ASM_R8A7794_H__
#define __ASM_R8A7794_H__

#include <linux/pm_domain.h>
#include <mach/pm-rcar.h>
#include <mach/rcar-gen2.h>

struct platform_device;

struct r8a7794_pm_domain {
	struct generic_pm_domain genpd;
	struct rcar_sysc_ch ch;
};

static inline struct rcar_sysc_ch *to_r8a7794_ch(struct generic_pm_domain *d)
{
	return &container_of(d, struct r8a7794_pm_domain, genpd)->ch;
}


void r8a7794_add_standard_devices(void);
void r8a7794_add_dt_devices(void);
void r8a7794_clock_init(void);
void r8a7794_pinmux_init(void);
void r8a7794_pm_init(void);
void r8a7794_init_early(void);
void r8a7794_timer_init(void);
extern struct smp_operations r8a7794_smp_ops;

#ifdef CONFIG_PM
extern void __init r8a7794_init_pm_domains(void);
#else
static inline void r8a7794_init_pm_domains(void) {}
#endif /* CONFIG_PM */

void r8a7794_module_reset(unsigned int n, u32 bits, int usecs);

#endif /* __ASM_R8A7794_H__ */
