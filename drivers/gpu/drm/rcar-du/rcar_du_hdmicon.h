/*
 * drivers/gpu/drm/rcar-du/rcar_du_hdmicon.h
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
 *
 * This file is based on the drivers/gpu/drm/rcar-du/rcar_du_lvdscon.h
 *
 * Copyright (C) 2013 Renesas Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __RCAR_DU_HDMI_H__
#define __RCAR_DU_HDMI_H__

struct rcar_du_device;
struct rcar_du_encoder;

int rcar_du_hdmi_connector_init(struct rcar_du_device *rcdu,
			       struct rcar_du_encoder *renc);

#endif /* __RCAR_DU_HDMI_H__ */
