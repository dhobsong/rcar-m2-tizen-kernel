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


/* DMA slave IDs */
enum {
	RCAR_DMA_SLAVE_INVALID,
	AUDIO_DMAC_SLAVE_SCU0_TX,
	AUDIO_DMAC_SLAVE_SCU0_RX,
	AUDIO_DMAC_SLAVE_SCU1_TX,
	AUDIO_DMAC_SLAVE_SCU1_RX,
	AUDIO_DMAC_SLAVE_SCU2_TX,
	AUDIO_DMAC_SLAVE_SCU2_RX,
	AUDIO_DMAC_SLAVE_SCU3_TX,
	AUDIO_DMAC_SLAVE_SCU3_RX,
	AUDIO_DMAC_SLAVE_SCU4_TX,
	AUDIO_DMAC_SLAVE_SCU4_RX,
	AUDIO_DMAC_SLAVE_SCU5_TX,
	AUDIO_DMAC_SLAVE_SCU5_RX,
	AUDIO_DMAC_SLAVE_SCU6_TX,
	AUDIO_DMAC_SLAVE_SCU6_RX,
	AUDIO_DMAC_SLAVE_SCU7_TX,
	AUDIO_DMAC_SLAVE_SCU7_RX,
	AUDIO_DMAC_SLAVE_SCU8_TX,
	AUDIO_DMAC_SLAVE_SCU8_RX,
	AUDIO_DMAC_SLAVE_SCU9_TX,
	AUDIO_DMAC_SLAVE_SCU9_RX,
	AUDIO_DMAC_SLAVE_SSI0_TX,
	AUDIO_DMAC_SLAVE_SSI0_RX,
	AUDIO_DMAC_SLAVE_SSI1_TX,
	AUDIO_DMAC_SLAVE_SSI1_RX,
	AUDIO_DMAC_SLAVE_SSI2_TX,
	AUDIO_DMAC_SLAVE_SSI2_RX,
	AUDIO_DMAC_SLAVE_SSI3_TX,
	AUDIO_DMAC_SLAVE_SSI3_RX,
	AUDIO_DMAC_SLAVE_SSI4_TX,
	AUDIO_DMAC_SLAVE_SSI4_RX,
	AUDIO_DMAC_SLAVE_SSI5_TX,
	AUDIO_DMAC_SLAVE_SSI5_RX,
	AUDIO_DMAC_SLAVE_SSI6_TX,
	AUDIO_DMAC_SLAVE_SSI6_RX,
	AUDIO_DMAC_SLAVE_SSI7_TX,
	AUDIO_DMAC_SLAVE_SSI7_RX,
	AUDIO_DMAC_SLAVE_SSI8_TX,
	AUDIO_DMAC_SLAVE_SSI8_RX,
	AUDIO_DMAC_SLAVE_SSI9_TX,
	AUDIO_DMAC_SLAVE_SSI9_RX,
};

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
