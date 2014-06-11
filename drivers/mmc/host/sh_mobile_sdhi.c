/*
 * SuperH Mobile SDHI
 *
 * Copyright (C) 2014 Renesas Electronics Corporation
 * Copyright (C) 2009 Magnus Damm
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Based on "Compaq ASIC3 support":
 *
 * Copyright 2001 Compaq Computer Corporation.
 * Copyright 2004-2005 Phil Blundell
 * Copyright 2007-2008 OpenedHand Ltd.
 *
 * Authors: Phil Blundell <pb@handhelds.org>,
 *	    Samuel Ortiz <sameo@openedhand.com>
 *
 */

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sh_mobile_sdhi.h>
#include <linux/mfd/tmio.h>
#include <linux/sh_dma.h>
#include <linux/delay.h>

#include "tmio_mmc.h"

/* SDHI host controller version */
#define SDHI_VERSION_CB0D	0xCB0D
#define SDHI_VERSION_490C	0x490C

#define EXT_ACC           0xe4
#define SD_DMACR(x)       ((x) ? 0x192 : 0xe6)

/* Maximum number DMA transfer size */
#define SH_MOBILE_SDHI_DMA_XMIT_SZ_MAX	6

/* SDHI host controller type */
enum {
	SH_MOBILE_SDHI_VER_490C = 0,
	SH_MOBILE_SDHI_VER_CB0D,
	SH_MOBILE_SDHI_VER_MAX, /* SDHI max */
};

/* SD buffer access size */
enum {
	SH_MOBILE_SDHI_EXT_ACC_16BIT = 0,
	SH_MOBILE_SDHI_EXT_ACC_32BIT,
	SH_MOBILE_SDHI_EXT_ACC_MAX, /* EXT_ACC access size max */
};

/* SD buffer access size for EXT_ACC */
static unsigned short sh_acc_size[][SH_MOBILE_SDHI_EXT_ACC_MAX] = {
	/* { 16bit, 32bit, }, */
	{ 0x0000, 0x0001, },	/* SH_MOBILE_SDHI_VER_490C */
	{ 0x0001, 0x0000, },	/* SH_MOBILE_SDHI_VER_CB0D */
};

struct sh_mobile_sdhi_of_data {
	unsigned long tmio_flags;
	unsigned long capabilities;
	unsigned long capabilities2;
	dma_addr_t dma_rx_offset;
};

static const struct sh_mobile_sdhi_of_data sh_mobile_sdhi_of_cfg[] = {
	{
		.tmio_flags = TMIO_MMC_HAS_IDLE_WAIT,
	},
};

static const struct sh_mobile_sdhi_of_data of_rcar_gen1_compatible = {
	.tmio_flags	= TMIO_MMC_HAS_IDLE_WAIT | TMIO_MMC_WRPROTECT_DISABLE,
	.capabilities	= MMC_CAP_SD_HIGHSPEED | MMC_CAP_SDIO_IRQ,
};

static const struct sh_mobile_sdhi_of_data of_rcar_gen2_compatible = {
	.tmio_flags	= TMIO_MMC_CLK_NO_SLEEP | TMIO_MMC_HAS_IDLE_WAIT |
			  TMIO_MMC_SDIO_STATUS_QUIRK,
	.capabilities	= MMC_CAP_SD_HIGHSPEED | MMC_CAP_SDIO_IRQ,
	.capabilities2	= MMC_CAP2_NO_2BLKS_READ,
	.dma_rx_offset	= 0x2000,
};

static const struct of_device_id sh_mobile_sdhi_of_match[] = {
	{ .compatible = "renesas,sdhi-shmobile" },
	{ .compatible = "renesas,sdhi-sh7372" },
	{ .compatible = "renesas,sdhi-sh73a0", .data = &sh_mobile_sdhi_of_cfg[0], },
	{ .compatible = "renesas,sdhi-r8a73a4", .data = &sh_mobile_sdhi_of_cfg[0], },
	{ .compatible = "renesas,sdhi-r8a7740", .data = &sh_mobile_sdhi_of_cfg[0], },
	{ .compatible = "renesas,sdhi-r8a7778", .data = &of_rcar_gen1_compatible, },
	{ .compatible = "renesas,sdhi-r8a7779", .data = &of_rcar_gen1_compatible, },
	{ .compatible = "renesas,sdhi-r8a7790", .data = &of_rcar_gen2_compatible, },
	{ .compatible = "renesas,sdhi-r8a7791", .data = &of_rcar_gen2_compatible, },
	{ .compatible = "renesas,sdhi-r8a7794", .data = &of_rcar_gen2_compatible, },
	{},
};
MODULE_DEVICE_TABLE(of, sh_mobile_sdhi_of_match);

/* transfer size for SD_DMACR */
static int sh_dma_size[][SH_MOBILE_SDHI_DMA_XMIT_SZ_MAX] = {
	/* { 1byte, 2byte, 4byte, 8byte, 16byte, 32byte, }, */
	{ -EINVAL, 0x0000, 0x0000, -EINVAL, 0x5000, 0xa000, },	/* VER_490C */
	{ -EINVAL, 0x0000, 0x0000, -EINVAL, 0x0001, 0x0004, },	/* VER_CB0D */
};

struct sh_mobile_sdhi {
	struct clk *clk;
	struct tmio_mmc_data mmc_data;
	struct tmio_mmc_dma dma_priv;
};

static int sh_mobile_sdhi_clk_enable(struct platform_device *pdev, unsigned int *f)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);
	struct tmio_mmc_host *host = mmc_priv(mmc);
	struct sh_mobile_sdhi *priv = container_of(host->pdata, struct sh_mobile_sdhi, mmc_data);
	int ret = clk_prepare_enable(priv->clk);
	if (ret < 0)
		return ret;

	*f = clk_get_rate(priv->clk);
	return 0;
}

static void sh_mobile_sdhi_clk_disable(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);
	struct tmio_mmc_host *host = mmc_priv(mmc);
	struct sh_mobile_sdhi *priv = container_of(host->pdata, struct sh_mobile_sdhi, mmc_data);
	clk_disable_unprepare(priv->clk);
}

static int sh_mobile_sdhi_wait_idle(struct tmio_mmc_host *host)
{
	int timeout = 1000;

	while (--timeout && !(sd_ctrl_read16(host, CTL_STATUS2) & (1 << 13)))
		udelay(1);

	if (!timeout) {
		dev_warn(host->pdata->dev, "timeout waiting for SD bus idle\n");
		return -EBUSY;
	}

	return 0;
}

static int sh_mobile_sdhi_write16_hook(struct tmio_mmc_host *host, int addr)
{
	switch (addr)
	{
	case CTL_SD_CMD:
	case CTL_STOP_INTERNAL_ACTION:
	case CTL_XFER_BLK_COUNT:
	case CTL_SD_CARD_CLK_CTL:
	case CTL_SD_XFER_LEN:
	case CTL_SD_MEM_CARD_OPT:
	case CTL_TRANSACTION_CTL:
	case CTL_DMA_ENABLE:
	case EXT_ACC:
		return sh_mobile_sdhi_wait_idle(host);
	}

	return 0;
}

#define SH_MOBILE_SDHI_DISABLE_AUTO_CMD12	0x4000

static void sh_mobile_sdhi_disable_auto_cmd12(int *val)
{
	*val |= SH_MOBILE_SDHI_DISABLE_AUTO_CMD12;

	return;
}

static void sh_mobile_sdhi_cd_wakeup(const struct platform_device *pdev)
{
	mmc_detect_change(platform_get_drvdata(pdev), msecs_to_jiffies(100));
}

static int sh_mobile_sdhi_get_xmit_size(unsigned int type, int shift)
{
	int dma_size;

	if (type >= SH_MOBILE_SDHI_VER_MAX)
		return -EINVAL;
	if (shift >= SH_MOBILE_SDHI_DMA_XMIT_SZ_MAX)
		return -EINVAL;

	dma_size = sh_dma_size[type][shift];
	return dma_size;
}

static const struct sh_mobile_sdhi_ops sdhi_ops = {
	.cd_wakeup = sh_mobile_sdhi_cd_wakeup,
};

static void sh_mobile_sdhi_enable_sdbuf_acc32(struct tmio_mmc_host *host,
								int enable)
{
	unsigned short acc_size;
	unsigned int type;
	u16 ver;

	ver = sd_ctrl_read16(host, CTL_VERSION);
	if (ver == SDHI_VERSION_CB0D)
		type = SH_MOBILE_SDHI_VER_CB0D;
	else if (ver == SDHI_VERSION_490C)
		type = SH_MOBILE_SDHI_VER_490C;
	else {
		dev_err(host->pdata->dev, "Unknown SDHI version\n");
		return;
	}

	acc_size = enable ?
		sh_acc_size[type][SH_MOBILE_SDHI_EXT_ACC_32BIT] :
		sh_acc_size[type][SH_MOBILE_SDHI_EXT_ACC_16BIT];
	sd_ctrl_write16(host, EXT_ACC, acc_size);

}

static int sh_mobile_sdhi_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id =
		of_match_device(sh_mobile_sdhi_of_match, &pdev->dev);
	struct sh_mobile_sdhi *priv;
	struct tmio_mmc_data *mmc_data;
	struct sh_mobile_sdhi_info *p = pdev->dev.platform_data;
	const struct device_node *np = pdev->dev.of_node;
	struct tmio_mmc_host *host;
	struct resource *res;
	int irq, ret, i = 0;
	bool multiplexed_isr = true;
	struct tmio_mmc_dma *dma_priv;
	u16 ver;
	int dma_xmit_sz;
	unsigned int type;
	int base, dma_size;
	int shift = 1; /* 2byte alignment */

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;

	priv = devm_kzalloc(&pdev->dev, sizeof(struct sh_mobile_sdhi), GFP_KERNEL);
	if (priv == NULL) {
		dev_err(&pdev->dev, "kzalloc failed\n");
		return -ENOMEM;
	}

	mmc_data = &priv->mmc_data;
	dma_priv = &priv->dma_priv;

	if (p) {
		if (p->init) {
			ret = p->init(pdev, &sdhi_ops);
			if (ret)
				return ret;
		}
	}

	priv->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(priv->clk)) {
		ret = PTR_ERR(priv->clk);
		dev_err(&pdev->dev, "cannot get clock: %d\n", ret);
		goto eclkget;
	}

	mmc_data->clk_enable = sh_mobile_sdhi_clk_enable;
	mmc_data->clk_disable = sh_mobile_sdhi_clk_disable;
	mmc_data->capabilities = MMC_CAP_MMC_HIGHSPEED;
	mmc_data->write16_hook = sh_mobile_sdhi_write16_hook;
	mmc_data->disable_auto_cmd12 = sh_mobile_sdhi_disable_auto_cmd12;
	if (p) {
		mmc_data->flags = p->tmio_flags;
		mmc_data->ocr_mask = p->tmio_ocr_mask;
		mmc_data->capabilities |= p->tmio_caps;
		mmc_data->capabilities2 |= p->tmio_caps2;
		mmc_data->cd_gpio = p->cd_gpio;
		dma_priv->dma_rx_offset = p->dma_rx_offset;

		if (p->dma_slave_tx > 0 && p->dma_slave_rx > 0) {
			/*
			 * Yes, we have to provide slave IDs twice to TMIO:
			 * once as a filter parameter and once for channel
			 * configuration as an explicit slave ID
			 */
			dma_priv->chan_priv_tx = (void *)p->dma_slave_tx;
			dma_priv->chan_priv_rx = (void *)p->dma_slave_rx;
			dma_priv->slave_id_tx = p->dma_slave_tx;
			dma_priv->slave_id_rx = p->dma_slave_rx;
			mmc_data->enable_sdbuf_acc32 =
				sh_mobile_sdhi_enable_sdbuf_acc32;
		}
	}

	if (np && !of_property_read_u32(np, "dma-xmit-sz", &dma_xmit_sz)) {
		if (dma_xmit_sz) {
			base = dma_xmit_sz;
			for (shift = 0; base > 1; shift++)
				base >>= 1;
		}
	}

	dma_priv->alignment_shift = shift;
	dma_priv->filter = shdma_chan_filter;

	mmc_data->dma = dma_priv;

	/*
	 * All SDHI blocks support 2-byte and larger block sizes in 4-bit
	 * bus width mode.
	 */
	mmc_data->flags |= TMIO_MMC_BLKSZ_2BYTES;

	/*
	 * All SDHI blocks support SDIO IRQ signalling.
	 */
	mmc_data->flags |= TMIO_MMC_SDIO_IRQ;

	if (of_id && of_id->data) {
		const struct sh_mobile_sdhi_of_data *of_data = of_id->data;
		mmc_data->flags |= of_data->tmio_flags;
		mmc_data->capabilities |= of_data->capabilities;
		mmc_data->capabilities2 |= of_data->capabilities2;
		dma_priv->dma_rx_offset = of_data->dma_rx_offset;
	}

	/* SD control register space size is 0x100, 0x200 for bus_shift=1 */
	mmc_data->bus_shift = resource_size(res) >> 9;

	ret = tmio_mmc_host_probe(&host, pdev, mmc_data);
	if (ret < 0)
		goto eprobe;

	/*
	 * FIXME:
	 * this Workaround can be more clever method
	 */
	sh_mobile_sdhi_enable_sdbuf_acc32(host, false);

	ver = sd_ctrl_read16(host, CTL_VERSION);

	/* Some controllers check the ILL_FUNC bit. */
	if (ver == SDHI_VERSION_490C)
		mmc_data->flags |= TMIO_MMC_CHECK_ILL_FUNC;

	/* Set DMA xmit size */
	if (p && p->dma_slave_tx > 0 && p->dma_slave_rx > 0) {
		if (ver == SDHI_VERSION_CB0D)
			type = SH_MOBILE_SDHI_VER_CB0D;
		else
			type = SH_MOBILE_SDHI_VER_490C;

		dma_size = sh_mobile_sdhi_get_xmit_size(type,
					priv->dma_priv.alignment_shift);
		if (dma_size < 0) {
			ret = dma_size;
			goto eprobe;
		}
		sd_ctrl_write16(host, SD_DMACR(type), dma_size);
	}

	/*
	 * Allow one or more specific (named) ISRs or
	 * one or more multiplexed (un-named) ISRs.
	 */

	irq = platform_get_irq_byname(pdev, SH_MOBILE_SDHI_IRQ_CARD_DETECT);
	if (irq >= 0) {
		multiplexed_isr = false;
		ret = devm_request_irq(&pdev->dev, irq, tmio_mmc_card_detect_irq, 0,
				  dev_name(&pdev->dev), host);
		if (ret)
			goto eirq;
	}

	irq = platform_get_irq_byname(pdev, SH_MOBILE_SDHI_IRQ_SDIO);
	if (irq >= 0) {
		multiplexed_isr = false;
		ret = devm_request_irq(&pdev->dev, irq, tmio_mmc_sdio_irq, 0,
				  dev_name(&pdev->dev), host);
		if (ret)
			goto eirq;
	}

	irq = platform_get_irq_byname(pdev, SH_MOBILE_SDHI_IRQ_SDCARD);
	if (irq >= 0) {
		multiplexed_isr = false;
		ret = devm_request_irq(&pdev->dev, irq, tmio_mmc_sdcard_irq, 0,
				  dev_name(&pdev->dev), host);
		if (ret)
			goto eirq;
	} else if (!multiplexed_isr) {
		dev_err(&pdev->dev,
			"Principal SD-card IRQ is missing among named interrupts\n");
		ret = irq;
		goto eirq;
	}

	if (multiplexed_isr) {
		while (1) {
			irq = platform_get_irq(pdev, i);
			if (irq < 0)
				break;
			i++;
			ret = devm_request_irq(&pdev->dev, irq, tmio_mmc_irq, 0,
					  dev_name(&pdev->dev), host);
			if (ret)
				goto eirq;
		}

		/* There must be at least one IRQ source */
		if (!i) {
			ret = irq;
			goto eirq;
		}
	}

	dev_info(&pdev->dev, "%s base at 0x%08lx clock rate %u MHz\n",
		 mmc_hostname(host->mmc), (unsigned long)
		 (platform_get_resource(pdev, IORESOURCE_MEM, 0)->start),
		 host->mmc->f_max / 1000000);

	return ret;

eirq:
	tmio_mmc_host_remove(host);
eprobe:
eclkget:
	if (p && p->cleanup)
		p->cleanup(pdev);
	return ret;
}

static int sh_mobile_sdhi_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);
	struct tmio_mmc_host *host = mmc_priv(mmc);
	struct sh_mobile_sdhi_info *p = pdev->dev.platform_data;

	tmio_mmc_host_remove(host);

	if (p && p->cleanup)
		p->cleanup(pdev);

	return 0;
}

static const struct dev_pm_ops tmio_mmc_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tmio_mmc_host_suspend, tmio_mmc_host_resume)
	SET_RUNTIME_PM_OPS(tmio_mmc_host_runtime_suspend,
			tmio_mmc_host_runtime_resume,
			NULL)
};

static struct platform_driver sh_mobile_sdhi_driver = {
	.driver		= {
		.name	= "sh_mobile_sdhi",
		.owner	= THIS_MODULE,
		.pm	= &tmio_mmc_dev_pm_ops,
		.of_match_table = sh_mobile_sdhi_of_match,
	},
	.probe		= sh_mobile_sdhi_probe,
	.remove		= sh_mobile_sdhi_remove,
};

module_platform_driver(sh_mobile_sdhi_driver);

MODULE_DESCRIPTION("SuperH Mobile SDHI driver");
MODULE_AUTHOR("Magnus Damm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sh_mobile_sdhi");
