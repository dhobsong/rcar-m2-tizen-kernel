/*
 * Lager board support - Reference DT implementation
 *
 * Copyright (C) 2014  Renesas Electronics Corporation
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
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sh_mmcif.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_data/camera-rcar.h>
#include <linux/platform_data/rcar-du.h>
#include <linux/platform_data/usb-rcar-gen2-phy.h>
#include <linux/sh_dma.h>
#include <linux/spi/flash.h>
#include <linux/spi/spi.h>
#include <linux/usb/phy.h>
#include <linux/usb/renesas_usbhs.h>
#include <linux/platform_data/vsp1.h>
#include <mach/clock.h>
#include <mach/common.h>
#include <mach/dma-register.h>
#include <mach/irqs.h>
#include <mach/rcar-gen2.h>
#include <mach/r8a7790.h>
#include <media/soc_camera.h>
#include <asm/mach/arch.h>
#include <sound/rcar_snd.h>
#include <sound/simple_card.h>

/* DU */
static struct rcar_du_encoder_data lager_du_encoders[] = {
#if defined(CONFIG_DRM_ADV7511) || defined(CONFIG_DRM_ADV7511_MODULE)
	{
		.type = RCAR_DU_ENCODER_HDMI,
		.output = RCAR_DU_OUTPUT_LVDS0,
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
	}, {
		.type = RCAR_DU_ENCODER_VGA,
		.output = RCAR_DU_OUTPUT_DPAD0,
	},
#else
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
#endif
};

static struct rcar_du_crtc_data lager_du_crtcs[] = {
	{
		.exclk = 148500000,
	},
	{
		.exclk = 148500000,
	},
	{
		.exclk = 0,
	},
};

static struct rcar_du_platform_data lager_du_pdata = {
	.encoders = lager_du_encoders,
	.num_encoders = ARRAY_SIZE(lager_du_encoders),
	.crtcs = lager_du_crtcs,
	.num_crtcs = ARRAY_SIZE(lager_du_crtcs),
#ifdef CONFIG_DRM_FBDEV_CRTC
	.fbdev_crtc = 0,
#endif
	.i2c_ch = 2,
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
	{ "vin0", NULL, "r8a7790-vin.0" },
	{ "vin1", NULL, "r8a7790-vin.1" },
	{ "vsp1-rt", NULL, "vsp1.0" },
	{ "vsp1-sy", NULL, "vsp1.1" },
	{ "vsp1-du0", NULL, "vsp1.2" },
	{ "vsp1-du1", NULL, "vsp1.3" },
	{ "vcp1", NULL, "vcp2" },
	{ "vcp0", NULL, "vcp1" },
	{ "vpc1", NULL, "vpc2" },
	{ "vpc0", NULL, "vpc1" },
	{ "ssp1", NULL, "ssp_dev" },
	{ "2ddmac", NULL, "tddmac" },
	{ "fdp2", NULL, "fdp2" },
	{ "fdp1", NULL, "fdp1" },
	{ "fdp0", NULL, "fdp0" },
};

/*
 * This is a really crude hack to work around core platform clock issues
 */
static const struct clk_name clk_enables[] __initconst = {
	{ "hsusb", NULL, "renesas_usbhs" },
	{ "ehci", NULL, "pci-rcar-gen2.1" },
	{ "ehci", NULL, "pci-rcar-gen2.2" },
	{ "ssi", NULL, "rcar_sound" },
	{ "scu", NULL, "rcar_sound" },
	{ "dmal", NULL, "sh-dma-engine.0" },
	{ "dmah", NULL, "sh-dma-engine.1" },
	{ "pvrsrvkm", NULL, "pvrsrvkm" },
};

/* Local DMA slave IDs */
enum {
	RCAR_DMA_SLAVE_LAGER_INVALID = 0,
	SYS_DMAC_SLAVE_MMCIF0_TX = 64,
	SYS_DMAC_SLAVE_MMCIF0_RX,
	SYS_DMAC_SLAVE_MMCIF1_TX,
	SYS_DMAC_SLAVE_MMCIF1_RX,
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

static const struct sh_dmae_slave_config r8a7790_sys_dmac_slaves[] = {
	SYS_DMAC_SLAVE(MMCIF0, 32, 0xee200000, 0x34, 0x34, 0xd1, 0xd2),
	SYS_DMAC_SLAVE(MMCIF1, 32, 0xee220000, 0x34, 0x34, 0xe1, 0xe2),
};

static const struct sh_dmae_channel r8a7790_sys_dmac_channels[] = {
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

static struct sh_dmae_pdata r8a7790_sys_dmac_platform_data = {
	.slave		= r8a7790_sys_dmac_slaves,
	.slave_num	= ARRAY_SIZE(r8a7790_sys_dmac_slaves),
	.channel	= r8a7790_sys_dmac_channels,
	.channel_num	= ARRAY_SIZE(r8a7790_sys_dmac_channels),
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

static struct resource r8a7790_sys_dmac_resources[] = {
	/* Channel registers and DMAOR for low */
	DEFINE_RES_MEM(0xe6700020, 0x8763 - 0x20),
	DEFINE_RES_IRQ(gic_spi(197)),
	DEFINE_RES_NAMED(gic_spi(200), 15, NULL, IORESOURCE_IRQ),

	/*
	 * HI is not supported
	 * since IRQ has strange mapping
	 */
};

#define r8a7790_register_sys_dmac(id)				\
	platform_device_register_resndata(			\
		&platform_bus, "sh-dma-engine", 2 + id,		\
		&r8a7790_sys_dmac_resources[id * 3],	3,	\
		&r8a7790_sys_dmac_platform_data,		\
		sizeof(r8a7790_sys_dmac_platform_data))

static void __init lager_add_dmac_prototype(void)
{
	r8a7790_register_sys_dmac(0);
}

static struct sh_mmcif_plat_data mmcif1_pdata = {
	.caps		= MMC_CAP_4_BIT_DATA |
			  MMC_CAP_8_BIT_DATA |
			  MMC_CAP_NONREMOVABLE,
	.ccs_unsupported = true,
	.slave_id_tx	= SYS_DMAC_SLAVE_MMCIF1_TX,
	.slave_id_rx	= SYS_DMAC_SLAVE_MMCIF1_RX,
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
};

/* VIN */
static const struct resource vin_resources[] __initconst = {
	/* VIN0 */
	DEFINE_RES_MEM(0xe6ef0000, 0x1000),
	DEFINE_RES_IRQ(gic_spi(188)),
	/* VIN1 */
	DEFINE_RES_MEM(0xe6ef1000, 0x1000),
	DEFINE_RES_IRQ(gic_spi(189)),
};

static void __init lager_add_vin_device(unsigned idx,
					struct rcar_vin_platform_data *pdata)
{
	struct platform_device_info vin_info = {
		.parent		= &platform_bus,
		.name		= "r8a7790-vin",
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

#define KOELSCH_CAMERA(idx, name, addr, pdata, flag)			\
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
	.i2c_adapter_id = 2,						\
	.module_name = name,						\
	.priv = pdata,							\
}

KOELSCH_CAMERA(0, "adv7612", 0x4c, NULL, RCAR_VIN_BT709);
KOELSCH_CAMERA(1, "adv7180", 0x20, NULL, RCAR_VIN_BT656);

static void __init lager_add_camera0_device(void)
{
	platform_device_register_data(&platform_bus, "soc-camera-pdrv", 0,
				      &cam0_link, sizeof(cam0_link));
	lager_add_vin_device(0, &vin0_pdata);
}

static void __init lager_add_camera1_device(void)
{
	platform_device_register_data(&platform_bus, "soc-camera-pdrv", 1,
				      &cam1_link, sizeof(cam1_link));
	lager_add_vin_device(1, &vin1_pdata);
}

/* VSP1 */
static const struct vsp1_platform_data lager_vspr_pdata __initconst = {
	.features = VSP1_HAS_LUT | VSP1_HAS_SRU,
	.rpf_count = 5,
	.uds_count = 1,
	.wpf_count = 4,
};

static const struct vsp1_platform_data lager_vsps_pdata __initconst = {
	.features = VSP1_HAS_SRU,
	.rpf_count = 5,
	.uds_count = 3,
	.wpf_count = 4,
};

static const struct vsp1_platform_data lager_vspd0_pdata __initconst = {
	.features = VSP1_HAS_LIF | VSP1_HAS_LUT,
	.rpf_count = 4,
	.uds_count = 1,
	.wpf_count = 4,
};

static const struct vsp1_platform_data lager_vspd1_pdata __initconst = {
	.features = VSP1_HAS_LIF | VSP1_HAS_LUT,
	.rpf_count = 4,
	.uds_count = 1,
	.wpf_count = 4,
};

static const struct vsp1_platform_data * const lager_vsp1_pdata[4] __initconst = {
	&lager_vspr_pdata,
	&lager_vsps_pdata,
	&lager_vspd0_pdata,
	&lager_vspd1_pdata,
};

static const struct resource vspr_resources[] __initconst = {
	DEFINE_RES_MEM(0xfe920000, 0x8000),
	DEFINE_RES_IRQ(gic_spi(266)),
};

static const struct resource vsps_resources[] __initconst = {
	DEFINE_RES_MEM(0xfe928000, 0x8000),
	DEFINE_RES_IRQ(gic_spi(267)),
};

static const struct resource vspd0_resources[] __initconst = {
	DEFINE_RES_MEM(0xfe930000, 0x8000),
	DEFINE_RES_IRQ(gic_spi(246)),
};

static const struct resource vspd1_resources[] __initconst = {
	DEFINE_RES_MEM(0xfe938000, 0x8000),
	DEFINE_RES_IRQ(gic_spi(247)),
};

static const struct resource * const vsp1_resources[4] __initconst = {
	vspr_resources,
	vsps_resources,
	vspd0_resources,
	vspd1_resources,
};

static void __init lager_add_vsp1_devices(void)
{
	struct platform_device_info info = {
		.name = "vsp1",
		.size_data = sizeof(*lager_vsp1_pdata[0]),
		.num_res = 2,
		.dma_mask = DMA_BIT_MASK(32),
	};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(vsp1_resources); ++i) {
		info.id = i;
		info.data = lager_vsp1_pdata[i];
		info.res = vsp1_resources[i];

		platform_device_register_full(&info);
	}
}

/* MSIOF spidev */
static const struct spi_board_info spi_bus[] __initconst = {
	{
		.modalias	= "spidev",
		.max_speed_hz	= 6000000,
		.mode		= SPI_MODE_3,
		.bus_num	= 2,
		.chip_select	= 0,
	},
};

#define lager_add_msiof_device spi_register_board_info

/* POWER IC */
static struct i2c_board_info poweric_i2c[] = {
	{ I2C_BOARD_INFO("da9063", 0x58), },
};

static void lager_restart(char mode, const char *cmd)
{
	struct i2c_adapter *adap;
	struct i2c_client *client;
	u8 val;
	int busnum = 3;

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

static struct of_dev_auxdata lager_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("renesas,mmcif-r8a7790", 0xee220000, "sh_mmcif",
			&mmcif1_pdata),
	{},
};

static void __init lager_add_standard_devices(void)
{
	shmobile_clk_workaround(clk_names, ARRAY_SIZE(clk_names), false);
	shmobile_clk_workaround(clk_enables, ARRAY_SIZE(clk_enables), true);
	r8a7790_add_dt_devices();
	lager_add_dmac_prototype();
	of_platform_populate(NULL, of_default_bus_match_table,
			     lager_auxdata_lookup, NULL);

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
	lager_add_camera0_device();
	lager_add_camera1_device();
	lager_add_vsp1_devices();
	lager_add_msiof_device(spi_bus, ARRAY_SIZE(spi_bus));
}

static const char *lager_boards_compat_dt[] __initdata = {
	"renesas,lager",
	"renesas,lager-reference",
	NULL,
};

DT_MACHINE_START(LAGER_DT, "lager")
	.smp		= smp_ops(r8a7790_smp_ops),
	.init_early	= shmobile_init_delay,
	.init_time	= rcar_gen2_timer_init,
	.init_machine	= lager_add_standard_devices,
	.init_late	= shmobile_init_late,
	.reserve	= rcar_gen2_reserve,
	.restart	= lager_restart,
	.dt_compat	= lager_boards_compat_dt,
MACHINE_END
