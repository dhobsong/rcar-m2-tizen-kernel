/*
 * Lager board support - Reference DT implementation
 *
 * Copyright (C) 2013  Renesas Solutions Corp.
 * Copyright (C) 2013  Simon Horman
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
#include <linux/init.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_data/rcar-du.h>
#include <linux/platform_data/usb-rcar-gen2-phy.h>
#include <linux/usb/phy.h>
#include <linux/usb/renesas_usbhs.h>
#include <mach/clock.h>
#include <mach/common.h>
#include <mach/irqs.h>
#include <mach/rcar-gen2.h>
#include <mach/r8a7790.h>
#include <asm/mach/arch.h>
#include <sound/rcar_snd.h>
#include <sound/simple_card.h>

/* DU */
static struct rcar_du_encoder_data lager_du_encoders[] = {
	{
		.type = RCAR_DU_ENCODER_VGA,
		.output = RCAR_DU_OUTPUT_DPAD0,
	}, {
		.type = RCAR_DU_ENCODER_NONE,
		.output = RCAR_DU_OUTPUT_LVDS1,
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
};

static struct rcar_du_platform_data lager_du_pdata = {
	.encoders = lager_du_encoders,
	.num_encoders = ARRAY_SIZE(lager_du_encoders),
};

static const struct resource du_resources[] __initconst = {
	DEFINE_RES_MEM(0xfeb00000, 0x70000),
	DEFINE_RES_MEM_NAMED(0xfeb90000, 0x1c, "lvds.0"),
	DEFINE_RES_MEM_NAMED(0xfeb94000, 0x1c, "lvds.1"),
	DEFINE_RES_IRQ(gic_spi(256)),
	DEFINE_RES_IRQ(gic_spi(268)),
	DEFINE_RES_IRQ(gic_spi(269)),
};

static void __init lager_add_du_device(void)
{
	struct platform_device_info info = {
		.name = "rcar-du-r8a7790",
		.id = -1,
		.res = du_resources,
		.num_res = ARRAY_SIZE(du_resources),
		.data = &lager_du_pdata,
		.size_data = sizeof(lager_du_pdata),
		.dma_mask = DMA_BIT_MASK(32),
	};

	platform_device_register_full(&info);
}

/* Sound */
static struct rsnd_ssi_platform_info rsnd_ssi[] = {
	RSND_SSI(AUDIOPP_DMAC_SLAVE_CMD0_TO_SSI0, gic_spi(370), 0),
	RSND_SSI(AUDIOPP_DMAC_SLAVE_SSI1_TO_SCU1, gic_spi(371), RSND_SSI_CLK_PIN_SHARE),
};

static struct rsnd_src_platform_info rsnd_src[2] = {
	RSND_SRC(0, AUDIO_DMAC_SLAVE_SCU0_TX),
	RSND_SRC(0, AUDIO_DMAC_SLAVE_SCU1_RX),
};

static struct rsnd_dvc_platform_info rsnd_dvc = {
};

static struct rsnd_dai_platform_info rsnd_dai = {
	.playback = { .ssi = &rsnd_ssi[0], .src = &rsnd_src[0], .dvc = &rsnd_dvc, },
	.capture  = { .ssi = &rsnd_ssi[1], .src = &rsnd_src[1], },
};

static struct rcar_snd_info rsnd_info = {
	.flags		= RSND_GEN2,
	.ssi_info	= rsnd_ssi,
	.ssi_info_nr	= ARRAY_SIZE(rsnd_ssi),
	.src_info	= rsnd_src,
	.src_info_nr	= ARRAY_SIZE(rsnd_src),
	.dvc_info	= &rsnd_dvc,
	.dvc_info_nr	= 1,
	.dai_info	= &rsnd_dai,
	.dai_info_nr	= 1,
};

static struct asoc_simple_card_info rsnd_card_info = {
	.name		= "SSI01-AK4643",
	.codec		= "ak4642-codec.2-0012",
	.platform	= "rcar_sound",
	.daifmt		= SND_SOC_DAIFMT_LEFT_J | SND_SOC_DAIFMT_CBM_CFM,
	.cpu_dai = {
		.name	= "rcar_sound",
	},
	.codec_dai = {
		.name	= "ak4642-hifi",
		.sysclk	= 11289600,
	},
};

static void __init lager_add_rsnd_device(void)
{
	struct resource rsnd_resources[] = {
		[RSND_GEN2_SCU]  = DEFINE_RES_MEM(0xec500000, 0x1000),
		[RSND_GEN2_ADG]  = DEFINE_RES_MEM(0xec5a0000, 0x100),
		[RSND_GEN2_SSIU] = DEFINE_RES_MEM(0xec540000, 0x1000),
		[RSND_GEN2_SSI]  = DEFINE_RES_MEM(0xec541000, 0x1280),
	};

	struct platform_device_info cardinfo = {
		.parent         = &platform_bus,
		.name           = "asoc-simple-card",
		.id             = -1,
		.data           = &rsnd_card_info,
		.size_data      = sizeof(struct asoc_simple_card_info),
		.dma_mask       = DMA_BIT_MASK(32),
	};

	platform_device_register_resndata(
		&platform_bus, "rcar_sound", -1,
		rsnd_resources, ARRAY_SIZE(rsnd_resources),
		&rsnd_info, sizeof(rsnd_info));

	platform_device_register_full(&cardinfo);
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
	{ "du0", "du.0", "rcar-du-r8a7790" },
	{ "du1", "du.1", "rcar-du-r8a7790" },
	{ "du2", "du.2", "rcar-du-r8a7790" },
	{ "lvds0", "lvds.0", "rcar-du-r8a7790" },
	{ "lvds1", "lvds.1", "rcar-du-r8a7790" },
	{ "hsusb", NULL, "usb_phy_rcar_gen2" },
	{ "ssi0", "ssi.0", "rcar_sound" },
	{ "ssi1", "ssi.1", "rcar_sound" },
	{ "src0", "src.0", "rcar_sound" },
	{ "src1", "src.1", "rcar_sound" },
	{ "dvc0", "dvc.0", "rcar_sound" },
};

/*
 * This is a really crude hack to work around core platform clock issues
 */
static const struct clk_name clk_enables[] __initconst = {
	{ "ether", NULL, "ee700000.ethernet" },
	{ "msiof1", NULL, "e6e10000.spi" },
	{ "mmcif1", NULL, "ee220000.mmc" },
	{ "qspi_mod", NULL, "e6b10000.spi" },
	{ "sdhi0", NULL, "ee100000.sd" },
	{ "sdhi2", NULL, "ee140000.sd" },
	{ "thermal", NULL, "e61f0000.thermal" },
	{ "hsusb", NULL, "renesas_usbhs" },
	{ "ehci", NULL, "pci-rcar-gen2.1" },
	{ "ehci", NULL, "pci-rcar-gen2.2" },
	{ "ssi", NULL, "rcar_sound" },
	{ "scu", NULL, "rcar_sound" },
	{ "dmal", NULL, "sh-dma-engine.0" },
	{ "dmah", NULL, "sh-dma-engine.1" },
};

/* USBHS */
static const struct resource usbhs_resources[] __initconst = {
	DEFINE_RES_MEM(0xe6590000, 0x100),
	DEFINE_RES_IRQ(gic_spi(107)),
};

struct usbhs_private {
	struct renesas_usbhs_platform_info info;
	struct usb_phy *phy;
	int pwen_gpio;
};

#define usbhs_get_priv(pdev) \
	container_of(renesas_usbhs_get_info(pdev), struct usbhs_private, info)

static int usbhs_power_ctrl(struct platform_device *pdev,
				void __iomem *base, int enable)
{
	struct usbhs_private *priv = usbhs_get_priv(pdev);

	if (!priv->phy)
		return -ENODEV;

	if (enable) {
		int retval = usb_phy_init(priv->phy);

		if (!retval)
			retval = usb_phy_set_suspend(priv->phy, 0);
		return retval;
	}

	usb_phy_set_suspend(priv->phy, 1);
	usb_phy_shutdown(priv->phy);
	return 0;
}

static int usbhs_hardware_init(struct platform_device *pdev)
{
	struct usbhs_private *priv = usbhs_get_priv(pdev);
	struct usb_phy *phy;
	int ret;
	struct device_node *np;

	np = of_find_node_by_path("/gpio@e6055000");
	if (np) {
		priv->pwen_gpio = of_get_gpio(np, 18);
		of_node_put(np);
	} else {
		pr_warn("Error: Unable to get PWEN GPIO line\n");
		ret = -ENOTSUPP;
		goto error2;
	}

	/* USB0 Function - use PWEN as GPIO input to detect DIP Switch SW5
	 * setting to avoid VBUS short circuit due to wrong cable.
	 * PWEN should be pulled up high if USB Function is selected by SW5
	 */
	gpio_request_one(priv->pwen_gpio, GPIOF_IN, NULL); /* USB0_PWEN */
	if (!gpio_get_value(priv->pwen_gpio)) {
		pr_warn("Error: USB Function not selected - check SW5 + SW6\n");
		ret = -ENOTSUPP;
		goto error;
	}

	phy = usb_get_phy_dev(&pdev->dev, 0);
	if (IS_ERR(phy)) {
		ret = PTR_ERR(phy);
		goto error;
	}

	priv->phy = phy;
	return 0;
 error:
	gpio_free(priv->pwen_gpio);
 error2:
	return ret;
}

static int usbhs_hardware_exit(struct platform_device *pdev)
{
	struct usbhs_private *priv = usbhs_get_priv(pdev);

	if (!priv->phy)
		return 0;

	usb_put_phy(priv->phy);
	priv->phy = NULL;
	gpio_free(priv->pwen_gpio);
	return 0;
}

static int usbhs_get_id(struct platform_device *pdev)
{
	return USBHS_GADGET;
}

static u32 lager_usbhs_pipe_type[] = {
	USB_ENDPOINT_XFER_CONTROL,
	USB_ENDPOINT_XFER_ISOC,
	USB_ENDPOINT_XFER_ISOC,
	USB_ENDPOINT_XFER_BULK,
	USB_ENDPOINT_XFER_BULK,
	USB_ENDPOINT_XFER_BULK,
	USB_ENDPOINT_XFER_INT,
	USB_ENDPOINT_XFER_INT,
	USB_ENDPOINT_XFER_INT,
	USB_ENDPOINT_XFER_BULK,
	USB_ENDPOINT_XFER_BULK,
	USB_ENDPOINT_XFER_BULK,
	USB_ENDPOINT_XFER_BULK,
	USB_ENDPOINT_XFER_BULK,
	USB_ENDPOINT_XFER_BULK,
	USB_ENDPOINT_XFER_BULK,
};

static struct usbhs_private usbhs_priv __initdata = {
	.info = {
		.platform_callback = {
			.power_ctrl	= usbhs_power_ctrl,
			.hardware_init	= usbhs_hardware_init,
			.hardware_exit	= usbhs_hardware_exit,
			.get_id		= usbhs_get_id,
		},
		.driver_param = {
			.buswait_bwait	= 4,
			.pipe_type = lager_usbhs_pipe_type,
			.pipe_size = ARRAY_SIZE(lager_usbhs_pipe_type),
		},
	}
};

static void __init lager_register_usbhs(void)
{
	usb_bind_phy("renesas_usbhs", 0, "usb_phy_rcar_gen2");
	platform_device_register_resndata(&platform_bus,
					  "renesas_usbhs", -1,
					  usbhs_resources,
					  ARRAY_SIZE(usbhs_resources),
					  &usbhs_priv.info,
					  sizeof(usbhs_priv.info));
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
	DEFINE_RES_MEM(0xee0b0000, 0x10000),	/* CFG */
	DEFINE_RES_MEM(0xee0a0000, 0x10000),	/* MEM */
	DEFINE_RES_IRQ(gic_spi(112)),
};

static const struct platform_device_info pci1_info __initconst = {
	.parent		= &platform_bus,
	.name		= "pci-rcar-gen2",
	.id		= 1,
	.res		= pci1_resources,
	.num_res	= ARRAY_SIZE(pci1_resources),
	.dma_mask	= DMA_BIT_MASK(32),
};

static void __init lager_add_usb1_device(void)
{
	platform_device_register_full(&pci1_info);
}

/* Internal PCI2 */
static const struct resource pci2_resources[] __initconst = {
	DEFINE_RES_MEM(0xee0d0000, 0x10000),	/* CFG */
	DEFINE_RES_MEM(0xee0c0000, 0x10000),	/* MEM */
	DEFINE_RES_IRQ(gic_spi(113)),
};

static const struct platform_device_info pci2_info __initconst = {
	.parent		= &platform_bus,
	.name		= "pci-rcar-gen2",
	.id		= 2,
	.res		= pci2_resources,
	.num_res	= ARRAY_SIZE(pci2_resources),
	.dma_mask	= DMA_BIT_MASK(32),
};

static void __init lager_add_usb2_device(void)
{
	platform_device_register_full(&pci2_info);
}

static void __init lager_add_standard_devices(void)
{
	shmobile_clk_workaround(clk_names, ARRAY_SIZE(clk_names), false);
	shmobile_clk_workaround(clk_enables, ARRAY_SIZE(clk_enables), true);
	r8a7790_add_dt_devices();
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);

	lager_add_du_device();
	platform_device_register_resndata(&platform_bus, "usb_phy_rcar_gen2",
					  -1, usbhs_phy_resources,
					  ARRAY_SIZE(usbhs_phy_resources),
					  &usbhs_phy_pdata,
					  sizeof(usbhs_phy_pdata));
	lager_register_usbhs();
	lager_add_usb1_device();
	lager_add_usb2_device();
	lager_add_rsnd_device();
}

static const char *lager_boards_compat_dt[] __initdata = {
	"renesas,lager",
	"renesas,lager-reference",
	NULL,
};

DT_MACHINE_START(LAGER_DT, "lager")
	.smp		= smp_ops(r8a7790_smp_ops),
	.init_early	= r8a7790_init_early,
	.init_time	= rcar_gen2_timer_init,
	.init_machine	= lager_add_standard_devices,
	.init_late	= shmobile_init_late,
	.dt_compat	= lager_boards_compat_dt,
MACHINE_END
