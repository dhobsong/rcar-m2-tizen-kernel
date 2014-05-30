/*
 * rcar_du_lvdsenc.c  --  R-Car Display Unit LVDS Encoder
 *
 * Copyright (C) 2013-2014 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <drm/drm_encoder_slave.h>

#include "rcar_du_drv.h"
#include "rcar_du_encoder.h"
#include "rcar_du_lvdsenc.h"
#include "rcar_lvds_regs.h"

#ifdef R8A779X_ES2_DU_LVDS_CH_DATA_GAP_WORKAROUND
static DEFINE_SPINLOCK(lvdsenc_lock);
#endif

struct rcar_du_lvdsenc {
	struct rcar_du_device *dev;

	unsigned int index;
	void __iomem *mmio;
	struct clk *clock;
	int dpms;

	enum rcar_lvds_input input;
};

static void rcar_lvds_write(struct rcar_du_lvdsenc *lvds, u32 reg, u32 data)
{
	iowrite32(data, lvds->mmio + reg);
}

int rcar_du_lvdsenc_start(struct rcar_du_lvdsenc *lvds,
				 struct rcar_du_crtc *rcrtc)
{
	const struct drm_display_mode *mode = &rcrtc->crtc.mode;
	unsigned int freq = mode->clock;
	u32 lvdcr0;
	u32 lvdhcr;
	u32 pllcr;
	int ret;
#ifdef R8A779X_ES2_DU_LVDS_CH_DATA_GAP_WORKAROUND
	unsigned long flags;
	unsigned int wait_loop, i;
#endif

	if (lvds->dpms == DRM_MODE_DPMS_ON)
		return 0;

	/* software reset release */
	if ((lvds->dev->info->lvds0_crtc) || (lvds->dev->info->lvds1_crtc)) {
		void __iomem *srstclr7_reg;
		u32 srstclr7_lvds = 0;
		if (lvds->dev->info->lvds0_crtc & (0x01 << rcrtc->index))
			srstclr7_lvds |= SRCR7_LVDS0;
		if (lvds->dev->info->lvds1_crtc & (0x01 << rcrtc->index))
			srstclr7_lvds |= SRCR7_LVDS1;
		srstclr7_reg = ioremap_nocache(SRSTCLR7_REGISTER, 0x04);
		writel_relaxed(srstclr7_lvds, srstclr7_reg);
		iounmap(srstclr7_reg);
	}

	ret = clk_prepare_enable(lvds->clock);
	if (ret < 0)
		return ret;

	/* PLL clock configuration */
	if (freq <= 38000)
		pllcr = LVDPLLCR_CEEN | LVDPLLCR_COSEL | LVDPLLCR_PLLDLYCNT_38M;
	else if (freq <= 60000)
		pllcr = LVDPLLCR_CEEN | LVDPLLCR_COSEL | LVDPLLCR_PLLDLYCNT_60M;
	else if (freq <= 121000)
		pllcr = LVDPLLCR_CEEN | LVDPLLCR_COSEL | LVDPLLCR_PLLDLYCNT_121M;
	else
		pllcr = LVDPLLCR_PLLDLYCNT_150M;

	rcar_lvds_write(lvds, LVDPLLCR, pllcr);

	/* Hardcode the channels and control signals routing for now.
	 *
	 * HSYNC -> CTRL0
	 * VSYNC -> CTRL1
	 * DISP  -> CTRL2
	 * 0     -> CTRL3
	 */
	rcar_lvds_write(lvds, LVDCTRCR, LVDCTRCR_CTR3SEL_ZERO |
			LVDCTRCR_CTR2SEL_DISP | LVDCTRCR_CTR1SEL_VSYNC |
			LVDCTRCR_CTR0SEL_HSYNC);

#ifdef R8A7790_ES1_DU_LVDS_LANE_MISCONNECTION_WORKAROUND
	if (rcar_du_needs(lvds->dev, RCAR_DU_QUIRK_LVDS_LANES))
		lvdhcr = LVDCHCR_CHSEL_CH(0, 0) | LVDCHCR_CHSEL_CH(1, 3)
		       | LVDCHCR_CHSEL_CH(2, 2) | LVDCHCR_CHSEL_CH(3, 1);
	else
		lvdhcr = LVDCHCR_CHSEL_CH(0, 0) | LVDCHCR_CHSEL_CH(1, 1)
		       | LVDCHCR_CHSEL_CH(2, 2) | LVDCHCR_CHSEL_CH(3, 3);
#else
	lvdhcr = LVDCHCR_CHSEL_CH(0, 0) | LVDCHCR_CHSEL_CH(1, 1)
	       | LVDCHCR_CHSEL_CH(2, 2) | LVDCHCR_CHSEL_CH(3, 3);
#endif

	rcar_lvds_write(lvds, LVDCHCR, lvdhcr);

	/* Select the input, hardcode mode 0, enable LVDS operation and turn
	 * bias circuitry on.
	 */
	lvdcr0 = LVDCR0_BEN | LVDCR0_LVEN;
	if (rcrtc->index == 2)
		lvdcr0 |= LVDCR0_DUSEL;
	rcar_lvds_write(lvds, LVDCR0, lvdcr0);

#ifdef R8A779X_ES2_DU_LVDS_CH_DATA_GAP_WORKAROUND
	if (rcar_du_needs(rcrtc->group->dev, RCAR_DU_QUIRK_LVDS_CH_DATA_GAP)) {
		/* caluculate waiting loop number */
		if (freq < 61000)
			wait_loop = WAIT_PS_TIME_UNDER_61MHZ /
				 rcrtc->group->dev->info->cpu_clk_time_ps;
		else if (freq <= 121000)
			wait_loop = WAIT_PS_TIME_UPPER_61MHZ /
				 rcrtc->group->dev->info->cpu_clk_time_ps;
		else
			wait_loop = WAIT_PS_TIME_UPPER_121MHZ /
				 rcrtc->group->dev->info->cpu_clk_time_ps;

		/* Turn the PLL on, wait for the startup delay,
			 and turn the output on. */
		lvdcr0 |= LVDCR0_PLLEN;
		rcar_lvds_write(lvds, LVDCR0, lvdcr0);

		usleep_range(100, 150);

		/* 1 loop is four CPU instruction */
		wait_loop = wait_loop / 4;

		lvdcr0 |= LVDCR0_LVRES;

		spin_lock_irqsave(&lvdsenc_lock, flags);

		writel_relaxed(lvdcr0, lvds->mmio + LVDCR0);

		/* Waiting for workaround */
		for (i = 0; i < wait_loop; i++)
			asm("nop");

		/* Turn all the channels on. */
		writel_relaxed(LVDCR1_CHSTBY(3) | LVDCR1_CHSTBY(2) |
			       LVDCR1_CHSTBY(1) | LVDCR1_CHSTBY(0) |
			       LVDCR1_CLKSTBY, lvds->mmio + LVDCR1);

		spin_unlock_irqrestore(&lvdsenc_lock, flags);
	} else {
		/* Turn all the channels on. */
		rcar_lvds_write(lvds, LVDCR1, LVDCR1_CHSTBY(3) |
				 LVDCR1_CHSTBY(2) | LVDCR1_CHSTBY(1) |
				 LVDCR1_CHSTBY(0) | LVDCR1_CLKSTBY);

		/* Turn the PLL on, wait for the startup delay,
			 and turn the output on. */
		lvdcr0 |= LVDCR0_PLLEN;
		rcar_lvds_write(lvds, LVDCR0, lvdcr0);

		usleep_range(100, 150);

		lvdcr0 |= LVDCR0_LVRES;
		rcar_lvds_write(lvds, LVDCR0, lvdcr0);
	}
#else
	/* Turn all the channels on. */
	rcar_lvds_write(lvds, LVDCR1, LVDCR1_CHSTBY(3) | LVDCR1_CHSTBY(2) |
			LVDCR1_CHSTBY(1) | LVDCR1_CHSTBY(0) | LVDCR1_CLKSTBY);

	/* Turn the PLL on, wait for the startup delay, and turn the output
	 * on.
	 */
	lvdcr0 |= LVDCR0_PLLEN;
	rcar_lvds_write(lvds, LVDCR0, lvdcr0);
#endif

	usleep_range(100, 150);

	lvdcr0 |= LVDCR0_LVRES;
	rcar_lvds_write(lvds, LVDCR0, lvdcr0);

	lvds->dpms = DRM_MODE_DPMS_ON;
	return 0;
}

void rcar_du_lvdsenc_stop(struct rcar_du_lvdsenc *lvds)
{
	if (lvds->dpms == DRM_MODE_DPMS_OFF)
		return;

	rcar_lvds_write(lvds, LVDCR0, 0);
	rcar_lvds_write(lvds, LVDCR1, 0);

	clk_disable_unprepare(lvds->clock);

	/* software reset */
	if ((lvds->dev->info->lvds0_crtc) || (lvds->dev->info->lvds1_crtc)) {
		void __iomem *srcr7_reg;
		u32 srcr7_lvds = 0;
		if (lvds->index == 0)
			srcr7_lvds |= SRCR7_LVDS0;
		if (lvds->index == 1)
			srcr7_lvds |= SRCR7_LVDS1;
		srcr7_reg = ioremap_nocache(SRCR7_REGISTER, 0x04);
		writel_relaxed(readl_relaxed(srcr7_reg) |
			       srcr7_lvds, srcr7_reg);
		iounmap(srcr7_reg);
	}

	lvds->dpms = DRM_MODE_DPMS_OFF;
}

int rcar_du_lvdsenc_dpms(struct rcar_du_lvdsenc *lvds,
			 struct drm_crtc *crtc, int mode)
{
	if (mode == DRM_MODE_DPMS_OFF) {
		rcar_du_lvdsenc_stop(lvds);
		return 0;
	} else if (crtc) {
		struct rcar_du_crtc *rcrtc = to_rcar_crtc(crtc);
		return rcar_du_lvdsenc_start(lvds, rcrtc);
	} else
		return -EINVAL;
}

static int rcar_du_lvdsenc_get_resources(struct rcar_du_lvdsenc *lvds,
					 struct platform_device *pdev)
{
	struct resource *mem;
	char name[7];

	sprintf(name, "lvds.%u", lvds->index);

	mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (mem == NULL) {
		dev_err(&pdev->dev, "failed to get memory resource for %s\n",
			name);
		return -EINVAL;
	}

	lvds->mmio = devm_ioremap_resource(&pdev->dev, mem);
	if (lvds->mmio == NULL) {
		dev_err(&pdev->dev, "failed to remap memory resource for %s\n",
			name);
		return -ENOMEM;
	}

	lvds->clock = devm_clk_get(&pdev->dev, name);
	if (IS_ERR(lvds->clock)) {
		dev_err(&pdev->dev, "failed to get clock for %s\n", name);
		return PTR_ERR(lvds->clock);
	}

	return 0;
}

int rcar_du_lvdsenc_init(struct rcar_du_device *rcdu)
{
	struct platform_device *pdev = to_platform_device(rcdu->dev);
	struct rcar_du_lvdsenc *lvds;
	unsigned int i;
	int ret;

	for (i = 0; i < rcdu->info->num_lvds; ++i) {
		lvds = devm_kzalloc(&pdev->dev, sizeof(*lvds), GFP_KERNEL);
		if (lvds == NULL) {
			dev_err(&pdev->dev, "failed to allocate private data\n");
			return -ENOMEM;
		}

		lvds->dev = rcdu;
		lvds->index = i;
		lvds->input = i ? RCAR_LVDS_INPUT_DU1 : RCAR_LVDS_INPUT_DU0;
		lvds->dpms = DRM_MODE_DPMS_OFF;

		ret = rcar_du_lvdsenc_get_resources(lvds, pdev);
		if (ret < 0)
			return ret;

		rcdu->lvds[i] = lvds;
	}

	return 0;
}
