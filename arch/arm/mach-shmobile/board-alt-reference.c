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
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_data/camera-rcar.h>
#include <linux/platform_data/rcar-du.h>
#include <linux/platform_data/usb-rcar-gen2-phy.h>
#if defined(CONFIG_VIDEO_RENESAS_VSP1)
#include <linux/platform_data/vsp1.h>
#endif
#include <linux/sh_dma.h>
#include <linux/spi/flash.h>
#include <linux/spi/spi.h>
#include <linux/usb/phy.h>
#include <linux/usb/renesas_usbhs.h>
#include <mach/clock.h>
#include <mach/common.h>
#include <mach/dma-register.h>
#include <mach/irqs.h>
#include <mach/rcar-gen2.h>
#include <mach/r8a7794.h>
#include <media/soc_camera.h>
#include <asm/mach/arch.h>
#include <sound/rcar_snd.h>
#include <sound/simple_card.h>

/* DU */
static struct rcar_du_encoder_data alt_du_encoders[] = {
#if defined(CONFIG_DRM_ADV7511) || defined(CONFIG_DRM_ADV7511_MODULE)
	{
		.type = RCAR_DU_ENCODER_HDMI,
		.output = RCAR_DU_OUTPUT_DPAD0,
	},
#elif defined(CONFIG_DRM_RCAR_LVDS)
	{
		.type = RCAR_DU_ENCODER_NONE,
		.output = RCAR_DU_OUTPUT_LVDS0,
		.connector.lvds.panel = {
			.width_mm = 210,
			.height_mm = 158,
			.mode = {
				.clock = 65000,
				.hdisplay = 1024,
				.hsync_start = 1048,
				.hsync_end = 1184,
				.htotal = 1344,
				.vdisplay = 768,
				.vsync_start = 771,
				.vsync_end = 777,
				.vtotal = 806,
				.flags = 0,
			},
		},
	},
#endif
	{
		.type = RCAR_DU_ENCODER_VGA,
		.output = RCAR_DU_OUTPUT_DPAD1,
	},
};

static struct rcar_du_crtc_data alt_du_crtcs[] = {
	{
		.exclk = 148500000,
	},
	{
		.exclk = 74250000,
	},
};

#ifdef CONFIG_DRM_RCAR_LVDS
static int alt_lvds_backlight_on(void)
{
	int ret;
	struct device_node *np;
	int gpio;

	np = of_find_node_by_path("/gpio@e6055000");
	if (np) {
		gpio = of_get_gpio(np, 23);
		of_node_put(np);
	} else {
		pr_warn("Error: Unable to set backlight to ON\n");
		ret = -ENOTSUPP;
		goto error2;
	}

	gpio_request_one(gpio, GPIOF_INIT_HIGH, NULL);
	if (!gpio_get_value(gpio)) {
		pr_warn("Error: LVDS backlight is not enable\n");
		ret = -ENOTSUPP;
		goto error;
	}

	return 0;
 error:
	gpio_free(gpio);
 error2:
	return ret;
}

static int alt_lvds_backlight_off(void)
{
	int ret;
	struct device_node *np;
	int gpio;

	np = of_find_node_by_path("/gpio@e6055000");
	if (np) {
		gpio = of_get_gpio(np, 23);
		of_node_put(np);
	} else {
		pr_warn("Error: Unable to set backlight to OFF\n");
		ret = -ENOTSUPP;
		goto error2;
	}

	gpio_free(gpio);

	return 0;
 error2:
	return ret;
}
#endif
static struct rcar_du_platform_data alt_du_pdata = {
	.encoders = alt_du_encoders,
	.num_encoders = ARRAY_SIZE(alt_du_encoders),
	.crtcs = alt_du_crtcs,
	.num_crtcs = ARRAY_SIZE(alt_du_crtcs),
#ifdef CONFIG_DRM_FBDEV_CRTC
	.fbdev_crtc = 0,
#endif
#ifdef CONFIG_DRM_RCAR_LVDS
	.backlight_on = alt_lvds_backlight_on,
	.backlight_off = alt_lvds_backlight_off,
#endif
	.i2c_ch = 1,
};

static const struct resource du_resources[] __initconst = {
	DEFINE_RES_MEM(0xfeb00000, 0x40000),
	DEFINE_RES_IRQ(gic_spi(256)),
	DEFINE_RES_IRQ(gic_spi(268)),
};

static void __init alt_add_du_device(void)
{
	struct platform_device_info info = {
		.name = "rcar-du-r8a7794",
		.id = -1,
		.res = du_resources,
		.num_res = ARRAY_SIZE(du_resources),
		.data = &alt_du_pdata,
		.size_data = sizeof(alt_du_pdata),
		.dma_mask = DMA_BIT_MASK(32),
	};

	platform_device_register_full(&info);
}

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
	{ "du0", "du.0", "rcar-du-r8a7794" },
	{ "du1", "du.1", "rcar-du-r8a7794" },
	{ "hsusb", NULL, "usb_phy_rcar_gen2" },
	{ "vin0", NULL, "r8a7794-vin.0" },
	{ "vsps", NULL, NULL },
#if defined(CONFIG_VIDEO_RENESAS_VSP1)
	{ "vsp1-du0", NULL, "vsp1.2" },
#else
	{ "vsp1-du0", NULL, NULL },
#endif
	{ "vpc0", NULL, "vpc1" },
	{ "2ddmac", NULL, "tddmac" },
	{ "fdp0", NULL, "fdp0" },
};

/*
 * This is a really crude hack to work around core platform clock issues
 */
static const struct clk_name clk_enables[] __initconst = {
	{ "ether", NULL, "ee700000.ethernet" },
	{ "i2c1", NULL, "e6518000.i2c" },
	{ "mmcif0", NULL, "ee200000.mmc" },
	{ "sdhi0", NULL, "ee100000.sd" },
	{ "sdhi1", NULL, "ee140000.sd" },
	{ "hsusb", NULL, "renesas_usbhs" },
	{ "ehci", NULL, "pci-rcar-gen2.1" },
	{ "pvrsrvkm", NULL, "pvrsrvkm" },
	{ "vcp0", NULL, "vcp1" },
	{ "dmal", NULL, "sh-dma-engine.0" },
};

/* Local DMA slave IDs */
enum {
	RCAR_DMA_SLAVE_ALT_INVALID = 0,
};

#define DMAE_CHANNEL(a, b)			\
{						\
	.offset		= (a) - 0x20,		\
	.dmars		= (a) - 0x20 + 0x40,	\
	.chclr_bit	= (b),			\
	.chclr_offset	= 0x80 - 0x20,		\
}

/* Sys-DMAC */
#define SYS_DMAC_SLAVE(_id, _bit, _addr, toffset, roffset, t, r)	\
{								\
	.slave_id	= SYS_DMAC_SLAVE_## _id ##_TX,		\
	.addr		= _addr + toffset,			\
	.chcr		= CHCR_TX(XMIT_SZ_## _bit ##BIT),	\
	.mid_rid	= t,					\
}, {								\
	.slave_id	= SYS_DMAC_SLAVE_## _id ##_RX,		\
	.addr		= _addr + roffset,			\
	.chcr		= CHCR_RX(XMIT_SZ_## _bit ##BIT),	\
	.mid_rid	= r,					\
}

static const struct sh_dmae_slave_config r8a7794_sys_dmac_slaves[] = {
};

static const struct sh_dmae_channel r8a7794_sys_dmac_channels[] = {
	DMAE_CHANNEL(0x8000, 0),
	DMAE_CHANNEL(0x8080, 1),
	DMAE_CHANNEL(0x8100, 2),
	DMAE_CHANNEL(0x8180, 3),
	DMAE_CHANNEL(0x8200, 4),
	DMAE_CHANNEL(0x8280, 5),
	DMAE_CHANNEL(0x8300, 6),
	DMAE_CHANNEL(0x8380, 7),
	DMAE_CHANNEL(0x8400, 8),
	DMAE_CHANNEL(0x8480, 9),
	DMAE_CHANNEL(0x8500, 10),
	DMAE_CHANNEL(0x8580, 11),
	DMAE_CHANNEL(0x8600, 12),
	DMAE_CHANNEL(0x8680, 13),
	DMAE_CHANNEL(0x8700, 14),
};

static struct sh_dmae_pdata r8a7794_sys_dmac_platform_data = {
	.slave		= r8a7794_sys_dmac_slaves,
	.slave_num	= ARRAY_SIZE(r8a7794_sys_dmac_slaves),
	.channel	= r8a7794_sys_dmac_channels,
	.channel_num	= ARRAY_SIZE(r8a7794_sys_dmac_channels),
	.ts_low_shift	= TS_LOW_SHIFT,
	.ts_low_mask	= TS_LOW_BIT << TS_LOW_SHIFT,
	.ts_high_shift	= TS_HI_SHIFT,
	.ts_high_mask	= TS_HI_BIT << TS_HI_SHIFT,
	.ts_shift	= dma_ts_shift,
	.ts_shift_num	= ARRAY_SIZE(dma_ts_shift),
	.dmaor_init	= DMAOR_DME,
	.chclr_present	= 1,
	.chclr_bitwise	= 1,
	.fourty_bits_addr = 1,
};

static struct resource r8a7794_sys_dmac_resources[] = {
	/* Channel registers and DMAOR for low */
	DEFINE_RES_MEM(0xe6700020, 0x8763 - 0x20),
	DEFINE_RES_IRQ(gic_spi(197)),
	DEFINE_RES_NAMED(gic_spi(200), 15, NULL, IORESOURCE_IRQ),

	/* Channel registers and DMAOR for high */
	DEFINE_RES_MEM(0xe6720020, 0x8763 - 0x20),
	DEFINE_RES_IRQ(gic_spi(220)),
	DEFINE_RES_NAMED(gic_spi(216), 4, NULL, IORESOURCE_IRQ),
	DEFINE_RES_NAMED(gic_spi(308), 11, NULL, IORESOURCE_IRQ),
};

#define r8a7794_register_sys_dmac(id)				\
	platform_device_register_resndata(			\
		&platform_bus, "sh-dma-engine", 2 + id,		\
		&r8a7794_sys_dmac_resources[id * 3],	id * 1 + 3,	\
		&r8a7794_sys_dmac_platform_data,		\
		sizeof(r8a7794_sys_dmac_platform_data))

static void __init alt_add_dmac_prototype(void)
{
	r8a7794_register_sys_dmac(0);
	r8a7794_register_sys_dmac(1);
}

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

/* POWER IC */
static struct i2c_board_info poweric_i2c[] = {
	{ I2C_BOARD_INFO("da9063", 0x58), },
};

static void alt_restart(char mode, const char *cmd)
{
	struct i2c_adapter *adap;
	struct i2c_client *client;
	u8 val;
	int busnum = 8;

	adap = i2c_get_adapter(busnum);
	if (!adap) {
		pr_err("failed to get adapter i2c%d\n", busnum);
		return;
	}

	client = i2c_new_device(adap, &poweric_i2c[0]);
	if (!client)
		pr_err("failed to register %s to i2c%d\n",
		       poweric_i2c[0].type, busnum);

	i2c_put_adapter(adap);

	val = i2c_smbus_read_byte_data(client, 0x13);

	if (val < 0)
		pr_err("couldn't access da9063\n");

	val |= 0x02;

	i2c_smbus_write_byte_data(client, 0x13, val);
}

/* VIN */
static const struct resource vin_resources[] __initconst = {
	/* VIN0 */
	DEFINE_RES_MEM(0xe6ef0000, 0x1000),
	DEFINE_RES_IRQ(gic_spi(188)),
};

static void __init alt_add_vin_device(unsigned idx,
					struct rcar_vin_platform_data *pdata)
{
	struct platform_device_info vin_info = {
		.parent		= &platform_bus,
		.name		= "r8a7794-vin",
		.id		= idx,
		.res		= &vin_resources[idx * 2],
		.num_res	= 2,
		.dma_mask	= DMA_BIT_MASK(32),
		.data		= pdata,
		.size_data	= sizeof(*pdata),
	};

	BUG_ON(idx > 1);

	platform_device_register_full(&vin_info);
}

#define ALT_CAMERA(idx, name, addr, pdata, flag)			\
static struct i2c_board_info i2c_cam##idx##_device = {			\
	I2C_BOARD_INFO(name, addr),					\
};									\
									\
static struct rcar_vin_platform_data vin##idx##_pdata = {		\
	.flags = flag,							\
};									\
									\
static struct soc_camera_link cam##idx##_link = {			\
	.bus_id = idx,							\
	.board_info = &i2c_cam##idx##_device,				\
	.i2c_adapter_id = 1,						\
	.module_name = name,						\
	.priv = pdata,							\
}

ALT_CAMERA(0, "adv7180", 0x20, NULL, RCAR_VIN_BT656);

static void __init alt_add_camera0_device(void)
{
	platform_device_register_data(&platform_bus, "soc-camera-pdrv", 0,
				      &cam0_link, sizeof(cam0_link));
	alt_add_vin_device(0, &vin0_pdata);
}

/* VSP1 */
#if defined(CONFIG_VIDEO_RENESAS_VSP1)
static const struct vsp1_platform_data alt_vsps_pdata __initconst = {
	.features = 0,
	.rpf_count = 5,
	.uds_count = 3,
	.wpf_count = 4,
};

static const struct vsp1_platform_data alt_vspd0_pdata __initconst = {
	.features = VSP1_HAS_LIF,
	.rpf_count = 4,
	.uds_count = 1,
	.wpf_count = 4,
};

static const struct vsp1_platform_data * const alt_vsp1_pdata[] __initconst
									= {
	&alt_vsps_pdata,
	&alt_vspd0_pdata,
};

static const struct resource vsp1_1_resources[] __initconst = {
	DEFINE_RES_MEM(0xfe928000, 0x8000),
	DEFINE_RES_IRQ(gic_spi(267)),
};

static const struct resource vsp1_2_resources[] __initconst = {
	DEFINE_RES_MEM(0xfe930000, 0x8000),
	DEFINE_RES_IRQ(gic_spi(246)),
};

static const struct resource * const vsp1_resources[] __initconst = {
	vsp1_1_resources,
	vsp1_2_resources,
};

static void __init alt_add_vsp1_devices(void)
{
	struct platform_device_info info = {
		.name = "vsp1",
		.size_data = sizeof(*alt_vsp1_pdata[0]),
		.num_res = 2,
		.dma_mask = DMA_BIT_MASK(32),
	};
	unsigned int i;

	for (i = 1; i < ARRAY_SIZE(vsp1_resources); ++i) {
		info.id = i + 1;
		info.data = alt_vsp1_pdata[i];
		info.res = vsp1_resources[i];

		platform_device_register_full(&info);
	}
}
#endif

static void __init alt_add_standard_devices(void)
{
	shmobile_clk_workaround(clk_names, ARRAY_SIZE(clk_names), false);
	shmobile_clk_workaround(clk_enables, ARRAY_SIZE(clk_enables), true);
	r8a7794_add_dt_devices();
	alt_add_dmac_prototype();
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
	alt_add_du_device();

	platform_device_register_resndata(&platform_bus, "usb_phy_rcar_gen2",
					  -1, usbhs_phy_resources,
					  ARRAY_SIZE(usbhs_phy_resources),
					  &usbhs_phy_pdata,
					  sizeof(usbhs_phy_pdata));
	alt_add_usb1_device();
	alt_add_camera0_device();
#if defined(CONFIG_VIDEO_RENESAS_VSP1)
	alt_add_vsp1_devices();
#endif
}

static const char * const alt_boards_compat_dt[] __initconst = {
	"renesas,alt",
	"renesas,alt-reference",
	NULL,
};

DT_MACHINE_START(ALT_DT, "alt")
	.smp		= smp_ops(r8a7794_smp_ops),
	.init_early	= r8a7794_init_early,
	.init_time	= r8a7794_timer_init,
	.init_machine	= alt_add_standard_devices,
	.init_late	= shmobile_init_late,
	.restart	= alt_restart,
	.dt_compat	= alt_boards_compat_dt,
MACHINE_END
