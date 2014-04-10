#ifndef __ASM_R8A7791_H__
#define __ASM_R8A7791_H__

#include <linux/pm_domain.h>
#include <mach/pm-rcar.h>

struct platform_device;

struct r8a7791_pm_domain {
	struct generic_pm_domain genpd;
	struct rcar_sysc_ch ch;
};

static inline struct rcar_sysc_ch *to_r8a7791_ch(struct generic_pm_domain *d)
{
	return &container_of(d, struct r8a7791_pm_domain, genpd)->ch;
}


void r8a7791_add_standard_devices(void);
void r8a7791_add_dt_devices(void);
void r8a7791_clock_init(void);
void r8a7791_pinmux_init(void);
void r8a7791_pm_init(void);
void r8a7791_init_early(void);
extern struct smp_operations r8a7791_smp_ops;

#ifdef CONFIG_PM
extern void __init r8a7791_init_pm_domains(void);
#else
static inline void r8a7791_init_pm_domains(void) {}
#endif /* CONFIG_PM */

void r8a7791_module_reset(unsigned int n, u32 bits, int usecs);

#endif /* __ASM_R8A7791_H__ */
