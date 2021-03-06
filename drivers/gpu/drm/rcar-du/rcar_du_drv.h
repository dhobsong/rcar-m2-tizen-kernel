/*
 * rcar_du_drv.h  --  R-Car Display Unit DRM driver
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

#ifndef __RCAR_DU_DRV_H__
#define __RCAR_DU_DRV_H__

#include <linux/kernel.h>
#include <linux/platform_data/rcar-du.h>

#include "rcar_du_crtc.h"
#include "rcar_du_group.h"

struct clk;
struct device;
struct drm_device;
struct drm_fbdev_cma;
struct rcar_du_device;
struct rcar_du_lvdsenc;

#define R8A7790_ES1_DU_LVDS_LANE_MISCONNECTION_WORKAROUND
#define R8A779X_ES2_DU_LVDS_CH_DATA_GAP_WORKAROUND

#define RCAR_DU_FEATURE_CRTC_IRQ_CLOCK	(1 << 0)	/* Per-CRTC IRQ and clock */
#define RCAR_DU_FEATURE_DEFR8		(1 << 1)	/* Has DEFR8 register */
#define RCAR_DU_FEATURE_NO_LVDS_INTERFACE	(1 << 2)
#define RCAR_DU_FEATURE_INTERLACE	(1 << 4)
#define RCAR_DU_FEATURE_VSP1_SOURCE	(1 << 5)	/* Has inputs from VSP1 */

#define RCAR_DU_QUIRK_ALIGN_128B	(1 << 0)	/* Align pitches to 128 bytes */
#ifdef R8A7790_ES1_DU_LVDS_LANE_MISCONNECTION_WORKAROUND
#define RCAR_DU_QUIRK_LVDS_LANES	(1 << 1)	/* LVDS lanes 1 and 3 inverted */
#endif
#ifdef R8A779X_ES2_DU_LVDS_CH_DATA_GAP_WORKAROUND
#define RCAR_DU_QUIRK_LVDS_CH_DATA_GAP	(1 << 2)	/* LVDS data gap */
#endif

/*
 * struct rcar_du_output_routing - Output routing specification
 * @possible_crtcs: bitmask of possible CRTCs for the output
 * @encoder_type: DRM type of the internal encoder associated with the output
 *
 * The DU has 5 possible outputs (DPAD0/1, LVDS0/1, TCON). Output routing data
 * specify the valid SoC outputs, which CRTCs can drive the output, and the type
 * of in-SoC encoder for the output.
 */
struct rcar_du_output_routing {
	unsigned int possible_crtcs;
	unsigned int possible_clones;
	unsigned int encoder_type;
};

/*
 * struct rcar_du_device_info - DU model-specific information
 * @features: device features (RCAR_DU_FEATURE_*)
 * @quirks: device quirks (RCAR_DU_QUIRK_*)
 * @num_crtcs: total number of CRTCs
 * @routes: array of CRTC to output routes, indexed by output (RCAR_DU_OUTPUT_*)
 * @num_lvds: number of internal LVDS encoders
 */
struct rcar_du_device_info {
	unsigned int features;
	unsigned int quirks;
	unsigned int num_crtcs;
	struct rcar_du_output_routing routes[RCAR_DU_OUTPUT_MAX];
	unsigned int num_lvds;
	unsigned int drgbs_bit;
	unsigned int max_xres;
	unsigned int max_yres;
	bool interlace;
	unsigned int cpu_clk_time_ps;
	unsigned int lvds0_crtc;
	unsigned int lvds1_crtc;
	unsigned int vspd_crtc;
};

struct rcar_du_device {
	struct device *dev;
	const struct rcar_du_platform_data *pdata;
#if defined(R8A7790_ES1_DU_LVDS_LANE_MISCONNECTION_WORKAROUND) || \
	defined(R8A779X_ES2_DU_LVDS_CH_DATA_GAP_WORKAROUND)
	struct rcar_du_device_info *info;
#else
	const struct rcar_du_device_info *info;
#endif

	void __iomem *mmio;

	struct drm_device *ddev;
	struct drm_fbdev_cma *fbdev;

	struct rcar_du_crtc crtcs[3];
	unsigned int num_crtcs;

	struct rcar_du_group groups[2];

	unsigned int dpad0_source;
	unsigned int vspd1_sink;

	struct rcar_du_lvdsenc *lvds[2];
	unsigned int crtcs_connect_id[3];
};

static inline bool rcar_du_has(struct rcar_du_device *rcdu,
			       unsigned int feature)
{
	return rcdu->info->features & feature;
}

static inline bool rcar_du_needs(struct rcar_du_device *rcdu,
				 unsigned int quirk)
{
	return rcdu->info->quirks & quirk;
}

static inline u32 rcar_du_read(struct rcar_du_device *rcdu, u32 reg)
{
	return ioread32(rcdu->mmio + reg);
}

static inline void rcar_du_write(struct rcar_du_device *rcdu, u32 reg, u32 data)
{
	iowrite32(data, rcdu->mmio + reg);
}

#endif /* __RCAR_DU_DRV_H__ */
