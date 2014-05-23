/*
 * Alt board support - Reference DT implementation
 *
 * Copyright (C) 2014  Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/dma-mapping.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_data/rcar-du.h>
#include <linux/platform_data/usb-rcar-gen2-phy.h>
#include <linux/platform_data/vsp1.h>
#include <linux/spi/flash.h>
#include <linux/spi/spi.h>
#include <linux/usb/phy.h>
#include <linux/usb/renesas_usbhs.h>
#include <mach/clock.h>
#include <mach/common.h>
#include <mach/irqs.h>
#include <mach/rcar-gen2.h>
#include <mach/r8a7794.h>
#include <asm/mach/arch.h>
#include <sound/rcar_snd.h>
#include <sound/simple_card.h>

/*
 * This is a really crude hack to provide clkdev support to platform
 * devices until they get moved to DT.
 */
static const struct clk_name clk_names[] __initconst = {
	{ "cmt0", NULL, "sh_cmt.0" },
	{ "scifa0", NULL, "sh-sci.0" },
	{ "scifa1", NULL, "sh-sci.1" },
	{ "scifb0", NULL, "sh-sci.2" },
	{ "scifb1", NULL, "sh-sci.3" },
	{ "scifb2", NULL, "sh-sci.4" },
	{ "scifa2", NULL, "sh-sci.5" },
	{ "scif0", NULL, "sh-sci.6" },
	{ "scif1", NULL, "sh-sci.7" },
	{ "hscif0", NULL, "sh-sci.8" },
	{ "hscif1", NULL, "sh-sci.9" },
	{ "scif2", NULL, "sh-sci.10" },
	{ "scif3", NULL, "sh-sci.11" },
	{ "scif4", NULL, "sh-sci.12" },
	{ "scif5", NULL, "sh-sci.13" },
	{ "scifa3", NULL, "sh-sci.14" },
	{ "scifa4", NULL, "sh-sci.15" },
	{ "scifa5", NULL, "sh-sci.16" },
	{ "hscif2", NULL, "sh-sci.17" },
	{ "hsusb", NULL, "usb_phy_rcar_gen2" },
};

/*
 * This is a really crude hack to work around core platform clock issues
 */
static const struct clk_name clk_enables[] __initconst = {
	{ "ether", NULL, "ee700000.ethernet" },
	{ "mmcif0", NULL, "ee200000.mmc" },
	{ "sdhi0", NULL, "ee100000.sd" },
	{ "sdhi1", NULL, "ee140000.sd" },
	{ "hsusb", NULL, "renesas_usbhs" },
	{ "ehci", NULL, "pci-rcar-gen2.1" },
};

/* USBHS PHY */
static const struct rcar_gen2_phy_platform_data usbhs_phy_pdata __initconst = {
	.chan0_pci = 0,	/* Channel 0 is USBHS */
	.chan2_pci = 1,	/* Channel 2 is PCI USB */
};

static const struct resource usbhs_phy_resources[] __initconst = {
	DEFINE_RES_MEM(0xe6590100, 0x100),
};

/* Internal PCI1 */
static const struct resource pci1_resources[] __initconst = {
	DEFINE_RES_MEM(0xee0d0000, 0x10000),	/* CFG */
	DEFINE_RES_MEM(0xee0c0000, 0x10000),	/* MEM */
	DEFINE_RES_IRQ(gic_spi(113)),
};

static const struct platform_device_info pci1_info __initconst = {
	.parent		= &platform_bus,
	.name		= "pci-rcar-gen2",
	.id		= 1,
	.res		= pci1_resources,
	.num_res	= ARRAY_SIZE(pci1_resources),
	.dma_mask	= DMA_BIT_MASK(32),
};

static void __init alt_add_usb1_device(void)
{
	platform_device_register_full(&pci1_info);
}

static void __init alt_add_standard_devices(void)
{
	shmobile_clk_workaround(clk_names, ARRAY_SIZE(clk_names), false);
	shmobile_clk_workaround(clk_enables, ARRAY_SIZE(clk_enables), true);
	r8a7794_add_dt_devices();
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);

	platform_device_register_resndata(&platform_bus, "usb_phy_rcar_gen2",
					  -1, usbhs_phy_resources,
					  ARRAY_SIZE(usbhs_phy_resources),
					  &usbhs_phy_pdata,
					  sizeof(usbhs_phy_pdata));
	alt_add_usb1_device();
}

static const char * const alt_boards_compat_dt[] __initconst = {
	"renesas,alt",
	"renesas,alt-reference",
	NULL,
};

DT_MACHINE_START(ALT_DT, "alt")
	.smp		= smp_ops(r8a7794_smp_ops),
	.init_early	= r8a7794_init_early,
	.init_time	= rcar_gen2_timer_init,
	.init_machine	= alt_add_standard_devices,
	.init_late	= shmobile_init_late,
	.dt_compat	= alt_boards_compat_dt,
MACHINE_END
