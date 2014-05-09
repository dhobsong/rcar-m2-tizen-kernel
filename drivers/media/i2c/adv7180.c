/*
 * drivers/media/i2c/adv7180.c
 *
 * Copyright (C) 2011-2014 Renesas Electronics Corporation
 *
 * adv7180.c Analog Devices ADV7180 video decoder driver
 * Copyright (c) 2009 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <media/v4l2-ioctl.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-chip-ident.h>
#include <linux/mutex.h>
#include <media/soc_camera.h>

#define ADV7180_INPUT_CONTROL_REG			0x00
#define ADV7180_INPUT_CONTROL_AD_PAL_BG_NTSC_J_SECAM	0x00
#define ADV7180_INPUT_CONTROL_AD_PAL_BG_NTSC_J_SECAM_PED 0x10
#define ADV7180_INPUT_CONTROL_AD_PAL_N_NTSC_J_SECAM	0x20
#define ADV7180_INPUT_CONTROL_AD_PAL_N_NTSC_M_SECAM	0x30
#define ADV7180_INPUT_CONTROL_NTSC_J			0x40
#define ADV7180_INPUT_CONTROL_NTSC_M			0x50
#define ADV7180_INPUT_CONTROL_PAL60			0x60
#define ADV7180_INPUT_CONTROL_NTSC_443			0x70
#define ADV7180_INPUT_CONTROL_PAL_BG			0x80
#define ADV7180_INPUT_CONTROL_PAL_N			0x90
#define ADV7180_INPUT_CONTROL_PAL_M			0xa0
#define ADV7180_INPUT_CONTROL_PAL_M_PED			0xb0
#define ADV7180_INPUT_CONTROL_PAL_COMB_N		0xc0
#define ADV7180_INPUT_CONTROL_PAL_COMB_N_PED		0xd0
#define ADV7180_INPUT_CONTROL_PAL_SECAM			0xe0
#define ADV7180_INPUT_CONTROL_PAL_SECAM_PED		0xf0
#define ADV7180_INPUT_CONTROL_INSEL_MASK		0x0f

#define ADV7180_EXTENDED_OUTPUT_CONTROL_REG		0x04
#define ADV7180_EXTENDED_OUTPUT_CONTROL_NTSCDIS		0xC5

#define ADV7180_AUTODETECT_ENABLE_REG			0x07
#define ADV7180_AUTODETECT_DEFAULT			0x7f
/* Contrast */
#define ADV7180_CON_REG		0x08	/* Unsigned */
#define ADV7180_CON_MIN		0	/* Gain on luma channel = 0 */
#define ADV7180_CON_DEF		128	/* Gain on luma channel = 1 */
#define ADV7180_CON_MAX		255	/* Gain on luma channel = 2 */
/* Brightness */
#define ADV7180_BRI_REG		0x0a	/* Signed */
#define ADV7180_BRI_MIN		-128	/* -30 IRE */
#define ADV7180_BRI_DEF		0	/* 0 IRE */
#define ADV7180_BRI_MAX		127	/* +30 IRE */
/* Hue */
#define ADV7180_HUE_REG		0x0b	/* Signed, inverted */
#define ADV7180_HUE_MIN		-127	/* -90 degree */
#define ADV7180_HUE_DEF		0	/* 0 degree */
#define ADV7180_HUE_MAX		128	/* +90 degree */
/* Saturation */
#define ADV7180_SD_SAT_CB_REG	0xe3	/* Unsigned */
#define ADV7180_SD_SAT_CR_REG	0xe4	/* Unsigned */
#define ADV7180_SAT_MIN		0	/* Gain on Cb/Cr channel = -42 dB */
#define ADV7180_SAT_DEF		128	/* Gain on Cb/Cr channel = 0 dB */
#define ADV7180_SAT_MAX		255	/* Gain on Cb/Cr channel = +6 dB */

#define ADV7180_ADI_CTRL_REG				0x0e
#define ADV7180_ADI_CTRL_IRQ_SPACE			0x20

/* Power Management */
#define ADV7180_PWR_MAN_REG		0x0f
#define ADV7180_PWR_MAN_ON		0x04
#define ADV7180_PWR_MAN_OFF		0x24
#define ADV7180_PWR_MAN_RES		0x80

#define ADV7180_STATUS1_REG				0x10
#define ADV7180_STATUS1_IN_LOCK		0x01
#define ADV7180_STATUS1_AUTOD_MASK	0x70
#define ADV7180_STATUS1_AUTOD_NTSM_M_J	0x00
#define ADV7180_STATUS1_AUTOD_NTSC_4_43 0x10
#define ADV7180_STATUS1_AUTOD_PAL_M	0x20
#define ADV7180_STATUS1_AUTOD_PAL_60	0x30
#define ADV7180_STATUS1_AUTOD_PAL_B_G	0x40
#define ADV7180_STATUS1_AUTOD_SECAM	0x50
#define ADV7180_STATUS1_AUTOD_PAL_COMB	0x60
#define ADV7180_STATUS1_AUTOD_SECAM_525	0x70
#define ADV7180_STATUS1_COL_KILL	0x80
#define ADV7180_STATUS1_PAL_SIGNAL	(ADV7180_STATUS1_AUTOD_PAL_M | \
					ADV7180_STATUS1_AUTOD_PAL_60 | \
					ADV7180_STATUS1_AUTOD_PAL_B_G | \
					ADV7180_STATUS1_AUTOD_PAL_COMB)

#define ADV7180_IDENT_REG 0x11
#define ADV7180_ID_7180 0x18

#define ADV7180_ICONF1_ADI		0x40
#define ADV7180_ICONF1_ACTIVE_LOW	0x01
#define ADV7180_ICONF1_PSYNC_ONLY	0x10
#define ADV7180_ICONF1_ACTIVE_TO_CLR	0xC0

#define ADV7180_IRQ1_LOCK	0x01
#define ADV7180_IRQ1_UNLOCK	0x02
#define ADV7180_ISR1_ADI	0x42
#define ADV7180_ICR1_ADI	0x43
#define ADV7180_IMR1_ADI	0x44
#define ADV7180_IMR2_ADI	0x48
#define ADV7180_IRQ3_AD_CHANGE	0x08
#define ADV7180_ISR3_ADI	0x4A
#define ADV7180_ICR3_ADI	0x4B
#define ADV7180_IMR3_ADI	0x4C
#define ADV7180_IMR4_ADI	0x50

#define ADV7180_NTSC_V_BIT_END_REG		0xE6
#define ADV7180_NTSC_V_BIT_END_MANUAL_NVEND	0x4F

/* Input image size */
#define ADV7180_MAX_WIDTH	720
#define ADV7180_MAX_HEIGHT	480

struct adv7180_state {
	struct v4l2_ctrl_handler ctrl_hdl;
	struct v4l2_subdev	sd;
	struct work_struct	work;
	struct mutex		mutex; /* mutual excl. when accessing chip */
	int			irq;
	v4l2_std_id		curr_norm;
	bool			autodetect;
	u8			input;
	const struct adv7180_color_format	*cfmt;
};

#define to_adv7180_sd(_ctrl) (&container_of(_ctrl->handler,		\
					    struct adv7180_state,	\
					    ctrl_hdl)->sd)

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

struct adv7180_color_format {
	enum v4l2_mbus_pixelcode code;
	enum v4l2_colorspace colorspace;
};

/*
 * supported color format list
 */
static const struct adv7180_color_format adv7180_cfmts[] = {
	{
		.code		= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
};

static v4l2_std_id adv7180_std_to_v4l2(u8 status1)
{
	/* in case V4L2_IN_ST_NO_SIGNAL */
	if (!(status1 & ADV7180_STATUS1_IN_LOCK))
		return V4L2_STD_UNKNOWN;

	switch (status1 & ADV7180_STATUS1_AUTOD_MASK) {
	case ADV7180_STATUS1_AUTOD_NTSM_M_J:
		return V4L2_STD_NTSC;
	case ADV7180_STATUS1_AUTOD_NTSC_4_43:
		return V4L2_STD_NTSC_443;
	case ADV7180_STATUS1_AUTOD_PAL_M:
		return V4L2_STD_PAL_M;
	case ADV7180_STATUS1_AUTOD_PAL_60:
		return V4L2_STD_PAL_60;
	case ADV7180_STATUS1_AUTOD_PAL_B_G:
		return V4L2_STD_PAL;
	case ADV7180_STATUS1_AUTOD_SECAM:
		return V4L2_STD_SECAM;
	case ADV7180_STATUS1_AUTOD_PAL_COMB:
		return V4L2_STD_PAL_Nc | V4L2_STD_PAL_N;
	case ADV7180_STATUS1_AUTOD_SECAM_525:
		return V4L2_STD_SECAM;
	default:
		return V4L2_STD_UNKNOWN;
	}
}

static int v4l2_std_to_adv7180(v4l2_std_id std)
{
	if (std == V4L2_STD_PAL_60)
		return ADV7180_INPUT_CONTROL_PAL60;
	if (std == V4L2_STD_NTSC_443)
		return ADV7180_INPUT_CONTROL_NTSC_443;
	if (std == V4L2_STD_PAL_N)
		return ADV7180_INPUT_CONTROL_PAL_N;
	if (std == V4L2_STD_PAL_M)
		return ADV7180_INPUT_CONTROL_PAL_M;
	if (std == V4L2_STD_PAL_Nc)
		return ADV7180_INPUT_CONTROL_PAL_COMB_N;

	if (std & V4L2_STD_PAL)
		return ADV7180_INPUT_CONTROL_PAL_BG;
	if (std & V4L2_STD_NTSC)
		return ADV7180_INPUT_CONTROL_NTSC_M;
	if (std & V4L2_STD_SECAM)
		return ADV7180_INPUT_CONTROL_PAL_SECAM;

	return -EINVAL;
}

static u32 adv7180_status_to_v4l2(u8 status1)
{
	if (!(status1 & ADV7180_STATUS1_IN_LOCK))
		return V4L2_IN_ST_NO_SIGNAL;

	return 0;
}

static int __adv7180_status(struct i2c_client *client, u32 *status,
			    v4l2_std_id *std)
{
	int status1 = i2c_smbus_read_byte_data(client, ADV7180_STATUS1_REG);

	if (status1 < 0)
		return status1;

	if (status)
		*status = adv7180_status_to_v4l2(status1);
	if (std)
		*std = adv7180_std_to_v4l2(status1);

	return 0;
}

static inline struct adv7180_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct adv7180_state, sd);
}

static int adv7180_querystd(struct v4l2_subdev *sd, v4l2_std_id *std)
{
	struct adv7180_state *state = to_state(sd);
	int err = mutex_lock_interruptible(&state->mutex);
	if (err)
		return err;

	/* when we are interrupt driven we know the state */
	if (!state->autodetect || state->irq > 0)
		*std = state->curr_norm;
	else
		err = __adv7180_status(v4l2_get_subdevdata(sd), NULL, std);

	mutex_unlock(&state->mutex);
	return err;
}

static int adv7180_s_routing(struct v4l2_subdev *sd, u32 input,
			     u32 output, u32 config)
{
	struct adv7180_state *state = to_state(sd);
	int ret = mutex_lock_interruptible(&state->mutex);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (ret)
		return ret;

	/* We cannot discriminate between LQFP and 40-pin LFCSP, so accept
	 * all inputs and let the card driver take care of validation
	 */
	if ((input & ADV7180_INPUT_CONTROL_INSEL_MASK) != input)
		goto out;

	ret = i2c_smbus_read_byte_data(client, ADV7180_INPUT_CONTROL_REG);

	if (ret < 0)
		goto out;

	ret &= ~ADV7180_INPUT_CONTROL_INSEL_MASK;
	ret = i2c_smbus_write_byte_data(client,
					ADV7180_INPUT_CONTROL_REG, ret | input);
	state->input = input;
out:
	mutex_unlock(&state->mutex);
	return ret;
}

static int adv7180_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	struct adv7180_state *state = to_state(sd);
	int ret = mutex_lock_interruptible(&state->mutex);
	if (ret)
		return ret;

	ret = __adv7180_status(v4l2_get_subdevdata(sd), status, NULL);
	mutex_unlock(&state->mutex);
	return ret;
}

static int adv7180_g_chip_ident(struct v4l2_subdev *sd,
				struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_ADV7180, 0);
}

static int adv7180_s_std(struct v4l2_subdev *sd, v4l2_std_id std)
{
	struct adv7180_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = mutex_lock_interruptible(&state->mutex);
	if (ret)
		return ret;

	/* all standards -> autodetect */
	if (std == V4L2_STD_ALL) {
		ret =
		    i2c_smbus_write_byte_data(client, ADV7180_INPUT_CONTROL_REG,
				ADV7180_INPUT_CONTROL_AD_PAL_BG_NTSC_J_SECAM
					      | state->input);
		if (ret < 0)
			goto out;

		__adv7180_status(client, NULL, &state->curr_norm);
		state->autodetect = true;
	} else {
		ret = v4l2_std_to_adv7180(std);
		if (ret < 0)
			goto out;

		ret = i2c_smbus_write_byte_data(client,
						ADV7180_INPUT_CONTROL_REG,
						ret | state->input);
		if (ret < 0)
			goto out;

		state->curr_norm = std;
		state->autodetect = false;
	}
	ret = 0;
out:
	mutex_unlock(&state->mutex);
	return ret;
}

static int adv7180_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct adv7180_state *state = to_state(sd);

	dev_dbg(&client->dev, "format %d\n", state->cfmt->code);

	return 0;
}

static int adv7180_g_mbus_config(struct v4l2_subdev *sd,
					struct v4l2_mbus_config *cfg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_subdev_desc *ssdd = soc_camera_i2c_to_desc(client);

	cfg->flags = V4L2_MBUS_PCLK_SAMPLE_RISING | V4L2_MBUS_MASTER |
		V4L2_MBUS_VSYNC_ACTIVE_LOW | V4L2_MBUS_HSYNC_ACTIVE_LOW |
		V4L2_MBUS_DATA_ACTIVE_HIGH;
	cfg->type = V4L2_MBUS_BT656;
	cfg->flags = soc_camera_apply_board_flags(ssdd, cfg);

	return 0;
}

/*
 * adv7180_g_crop() - V4L2 decoder i/f handler for g_crop
 * @sd: pointer to standard V4L2 sub-device structure
 * @a: pointer to standard V4L2 cropcap structure
 *
 * Gets current cropping rectangle.
 */
static int adv7180_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left = 0;
	a->c.top = 0;
	/* set current window size */
	a->c.width = ADV7180_MAX_WIDTH;		/* width is fixed value */
	a->c.height = ADV7180_MAX_HEIGHT;	/* heigth is fixed value */
	a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

/*
 * adv7180_cropcap() - V4L2 decoder i/f handler for cropcap
 * @sd: pointer to standard V4L2 sub-device structure
 * @a: pointer to standard V4L2 cropcap structure
 *
 * Gets cropping limits, default cropping rectangle and pixel aspect.
 */
static int adv7180_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left = 0;
	a->bounds.top = 0;
	/* set maximum window size */
	a->bounds.width = ADV7180_MAX_WIDTH;	/* width is fixed value */
	a->bounds.height = ADV7180_MAX_HEIGHT;	/* heigth is fixed value */
	a->defrect = a->bounds;
	a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator = 1;
	a->pixelaspect.denominator = 1;

	return 0;
}

/*
 * adv7180_try_fmt() - V4L2 decoder i/f handler for try_mbus_fmt
 * @sd: pointer to standard V4L2 sub-device structure
 * @mf: pointer to mediabus format structure
 *
 * Negotiate the image capture size and mediabus format.
 * Try a format.
 */
static int adv7180_try_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *mf)
{
	struct adv7180_state *state = to_state(sd);
	int i;

	mf->width = ADV7180_MAX_WIDTH;		/* width is fixed value */
	mf->height = ADV7180_MAX_HEIGHT;	/* heigth is fixed value */

	for (i = 0; i < ARRAY_SIZE(adv7180_cfmts); i++)
		if (mf->code == adv7180_cfmts[i].code)
			break;

	if (i == ARRAY_SIZE(adv7180_cfmts)) {
		/* Unsupported format requested. Propose either */
		if (state->cfmt) {
			/* the current one or */
			mf->colorspace = state->cfmt->colorspace;
			mf->code = state->cfmt->code;
		} else {
			/* the default one */
			mf->colorspace = adv7180_cfmts[0].colorspace;
			mf->code = adv7180_cfmts[0].code;
		}
	} else {
		/* Also return the colorspace */
		mf->colorspace	= adv7180_cfmts[i].colorspace;
	}

	return 0;
}

/*
 * adv7180_enum_fmt() - V4L2 decoder i/f handler for enum_mbus_fmt
 * @sd: pointer to standard V4L2 sub-device structure
 * @index: format index
 * @code: pointer to mediabus format
 *
 * Enumerate supported mediabus formats.
 */
static int adv7180_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(adv7180_cfmts))
		return -EINVAL;

	*code = adv7180_cfmts[index].code;

	return 0;
}

/*
 * adv7180_s_ctrl() - V4L2 decoder i/f handler for s_ctrl
 * @ctrl: pointer to standard V4L2 control structure
 *
 * Set a control in ADV7180 decoder device.
 */
static int adv7180_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = to_adv7180_sd(ctrl);
	struct adv7180_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = mutex_lock_interruptible(&state->mutex);
	int val;

	if (ret)
		return ret;

	val = ctrl->val;
	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		if ((ctrl->val < ADV7180_BRI_MIN) ||
					(ctrl->val > ADV7180_BRI_MAX))
			ret = -ERANGE;
		else
			ret = i2c_smbus_write_byte_data(client,
							ADV7180_BRI_REG,
							val);
		break;
	case V4L2_CID_HUE:
		/*Hue is inverted according to HSL chart */
		if ((ctrl->val < ADV7180_HUE_MIN) ||
					(ctrl->val > ADV7180_HUE_MAX))
			ret = -ERANGE;
		else
			ret = i2c_smbus_write_byte_data(client,
							ADV7180_HUE_REG,
							-val);
		break;
	case V4L2_CID_CONTRAST:
		if ((ctrl->val < ADV7180_CON_MIN) ||
					(ctrl->val > ADV7180_CON_MAX))
			ret = -ERANGE;
		else
			ret = i2c_smbus_write_byte_data(client,
							ADV7180_CON_REG,
							val);
		break;
	case V4L2_CID_SATURATION:
		/*
		 *This could be V4L2_CID_BLUE_BALANCE/V4L2_CID_RED_BALANCE
		 *Let's not confuse the user, everybody understands saturation
		 */
		if ((ctrl->val < ADV7180_SAT_MIN) ||
					(ctrl->val > ADV7180_SAT_MAX))
			ret = -ERANGE;
		else {
			ret = i2c_smbus_write_byte_data(client,
							ADV7180_SD_SAT_CB_REG,
							val);
			if (ret < 0)
				break;

			ret = i2c_smbus_write_byte_data(client,
							ADV7180_SD_SAT_CR_REG,
							val);
		}
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&state->mutex);
	return ret;
}

static const struct v4l2_ctrl_ops adv7180_ctrl_ops = {
	.s_ctrl = adv7180_s_ctrl,
};

/*
 * adv7180_init_controls() - Init controls
 * @state: pointer to private state structure
 *
 * Init ADV7180 supported control handler.
 */
static int adv7180_init_controls(struct adv7180_state *state)
{
	v4l2_ctrl_handler_init(&state->ctrl_hdl, 4);

	v4l2_ctrl_new_std(&state->ctrl_hdl, &adv7180_ctrl_ops,
			  V4L2_CID_BRIGHTNESS, ADV7180_BRI_MIN,
			  ADV7180_BRI_MAX, 1, ADV7180_BRI_DEF);
	v4l2_ctrl_new_std(&state->ctrl_hdl, &adv7180_ctrl_ops,
			  V4L2_CID_CONTRAST, ADV7180_CON_MIN,
			  ADV7180_CON_MAX, 1, ADV7180_CON_DEF);
	v4l2_ctrl_new_std(&state->ctrl_hdl, &adv7180_ctrl_ops,
			  V4L2_CID_SATURATION, ADV7180_SAT_MIN,
			  ADV7180_SAT_MAX, 1, ADV7180_SAT_DEF);
	v4l2_ctrl_new_std(&state->ctrl_hdl, &adv7180_ctrl_ops,
			  V4L2_CID_HUE, ADV7180_HUE_MIN,
			  ADV7180_HUE_MAX, 1, ADV7180_HUE_DEF);
	state->sd.ctrl_handler = &state->ctrl_hdl;
	if (state->ctrl_hdl.error) {
		int err = state->ctrl_hdl.error;

		v4l2_ctrl_handler_free(&state->ctrl_hdl);
		return err;
	}
	v4l2_ctrl_handler_setup(&state->ctrl_hdl);

	return 0;
}

/*
 * adv7180_exit_controls() - Cleanup controls
 * @state: pointer to private state structure
 *
 * Free ADV7180 supported control handler.
 */
static void adv7180_exit_controls(struct adv7180_state *state)
{
	v4l2_ctrl_handler_free(&state->ctrl_hdl);
}

static int adv7180_set_params(struct i2c_client *client, u32 *width,
			     u32 *height,
			     enum v4l2_mbus_pixelcode code)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct adv7180_state *state = to_state(sd);
	int i;
	long status;

	/*
	 * select format
	 */
	for (i = 0; i < ARRAY_SIZE(adv7180_cfmts); i++) {
		if (code == adv7180_cfmts[i].code) {
			state->cfmt = adv7180_cfmts + i;
			break;
		}
	}
	if (i >= ARRAY_SIZE(adv7180_cfmts))
		return -EINVAL;			/* no match format */

	status = i2c_smbus_read_byte_data(client, ADV7180_STATUS1_REG);
	if (status < 0) {
		dev_info(&client->dev, "Not detect any video input signal\n");
	} else {
		if (status & ADV7180_STATUS1_IN_LOCK) {
			if (((status & ADV7180_STATUS1_AUTOD_NTSC_4_43)
				 == ADV7180_STATUS1_AUTOD_NTSC_4_43) ||
				  ((status & ADV7180_STATUS1_AUTOD_MASK)
				  == ADV7180_STATUS1_AUTOD_NTSM_M_J)) {
				dev_info(&client->dev,
				"Detected the NTSC video input signal\n");
			} else if (status & ADV7180_STATUS1_PAL_SIGNAL) {
				dev_info(&client->dev,
				"Detected the PAL video input signal\n");
			}
		} else {
			dev_info(&client->dev,
				"Not detect any video input signal\n");
		}
	}

	/*
	 * set window size
	 */
	*width = ADV7180_MAX_WIDTH;	/* fixed value */
	*height = ADV7180_MAX_HEIGHT;	/* fixed value */

	return 0;
}

/*
 * adv7180_g_fmt() - V4L2 decoder i/f handler for g_mbus_fmt
 * @sd: pointer to standard V4L2 sub-device structure
 * @mf: pointer to mediabus format structure
 *
 * Negotiate the image capture size and mediabus format.
 * Get the data format.
 */
static int adv7180_g_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	struct adv7180_state *state = to_state(sd);

	mf->width	= ADV7180_MAX_WIDTH;	/* fixed value */
	mf->height	= ADV7180_MAX_HEIGHT;	/* fixed value */
	mf->code	= state->cfmt->code;
	mf->colorspace	= state->cfmt->colorspace;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

/*
 * adv7180_s_fmt() - V4L2 decoder i/f handler for s_mbus_fmt
 * @sd: pointer to standard V4L2 sub-device structure
 * @mf: pointer to mediabus format structure
 *
 * Negotiate the image capture size and mediabus format.
 * Try a format.
 */
static int adv7180_s_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct adv7180_state *state = to_state(sd);
	int ret;

	ret = adv7180_set_params(client, &mf->width, &mf->height,
				    mf->code);

	if (!ret)
		mf->colorspace = state->cfmt->colorspace;

	return ret;
}

static const struct v4l2_subdev_video_ops adv7180_video_ops = {
	.querystd = adv7180_querystd,
	.g_input_status = adv7180_g_input_status,
	.s_routing = adv7180_s_routing,
	.s_stream	= adv7180_s_stream,
	.g_mbus_fmt	= adv7180_g_fmt,
	.s_mbus_fmt	= adv7180_s_fmt,
	.try_mbus_fmt	= adv7180_try_fmt,
	.cropcap	= adv7180_cropcap,
	.g_crop		= adv7180_g_crop,
	.enum_mbus_fmt	= adv7180_enum_fmt,
	.g_mbus_config	= adv7180_g_mbus_config,
};

static const struct v4l2_subdev_core_ops adv7180_core_ops = {
	.g_chip_ident = adv7180_g_chip_ident,
	.s_std = adv7180_s_std,
	.queryctrl = v4l2_subdev_queryctrl,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
};

static const struct v4l2_subdev_ops adv7180_ops = {
	.core = &adv7180_core_ops,
	.video = &adv7180_video_ops,
};

static void adv7180_work(struct work_struct *work)
{
	struct adv7180_state *state = container_of(work, struct adv7180_state,
						   work);
	struct i2c_client *client = v4l2_get_subdevdata(&state->sd);
	u8 isr3;

	mutex_lock(&state->mutex);
	i2c_smbus_write_byte_data(client, ADV7180_ADI_CTRL_REG,
				  ADV7180_ADI_CTRL_IRQ_SPACE);
	isr3 = i2c_smbus_read_byte_data(client, ADV7180_ISR3_ADI);
	/* clear */
	i2c_smbus_write_byte_data(client, ADV7180_ICR3_ADI, isr3);
	i2c_smbus_write_byte_data(client, ADV7180_ADI_CTRL_REG, 0);

	if (isr3 & ADV7180_IRQ3_AD_CHANGE && state->autodetect)
		__adv7180_status(client, NULL, &state->curr_norm);
	mutex_unlock(&state->mutex);

	enable_irq(state->irq);
}

static irqreturn_t adv7180_irq(int irq, void *devid)
{
	struct adv7180_state *state = devid;

	schedule_work(&state->work);

	disable_irq_nosync(state->irq);

	return IRQ_HANDLED;
}

/*
 * init_device - Init a ADV7180 device
 * @client: pointer to i2c_client structure
 * @state: pointer to private state structure
 *
 * Initialize the ADV7180 device
 */
static int init_device(struct i2c_client *client, struct adv7180_state *state)
{
	int ret;

	/* Initialize adv7180 */
	/* Enable autodetection */
	if (state->autodetect) {
		ret =
		    i2c_smbus_write_byte_data(client, ADV7180_INPUT_CONTROL_REG,
				ADV7180_INPUT_CONTROL_AD_PAL_BG_NTSC_J_SECAM
					      | state->input);
		if (ret < 0)
			return ret;

		ret =
		    i2c_smbus_write_byte_data(client,
					      ADV7180_AUTODETECT_ENABLE_REG,
					      ADV7180_AUTODETECT_DEFAULT);
		if (ret < 0)
			return ret;
	} else {
		ret = v4l2_std_to_adv7180(state->curr_norm);
		if (ret < 0)
			return ret;

		ret =
		    i2c_smbus_write_byte_data(client, ADV7180_INPUT_CONTROL_REG,
					      ret | state->input);
		if (ret < 0)
			return ret;

	}
	/* ITU-R BT.656-4 compatible */
	ret = i2c_smbus_write_byte_data(client,
			ADV7180_EXTENDED_OUTPUT_CONTROL_REG,
			ADV7180_EXTENDED_OUTPUT_CONTROL_NTSCDIS);
	if (ret < 0)
		return ret;

	/* Manually set V bit end position in NTSC mode */
	ret = i2c_smbus_write_byte_data(client,
					ADV7180_NTSC_V_BIT_END_REG,
					ADV7180_NTSC_V_BIT_END_MANUAL_NVEND);
	if (ret < 0)
		return ret;

	/* read current norm */
	__adv7180_status(client, NULL, &state->curr_norm);

	/* register for interrupts */
	if (state->irq > 0) {
		ret = request_irq(state->irq, adv7180_irq, 0, KBUILD_MODNAME,
				  state);
		if (ret)
			return ret;

		ret = i2c_smbus_write_byte_data(client, ADV7180_ADI_CTRL_REG,
						ADV7180_ADI_CTRL_IRQ_SPACE);
		if (ret < 0)
			return ret;

		/* config the Interrupt pin to be active low */
		ret = i2c_smbus_write_byte_data(client, ADV7180_ICONF1_ADI,
						ADV7180_ICONF1_ACTIVE_LOW |
						ADV7180_ICONF1_PSYNC_ONLY);
		if (ret < 0)
			return ret;

		ret = i2c_smbus_write_byte_data(client, ADV7180_IMR1_ADI, 0);
		if (ret < 0)
			return ret;

		ret = i2c_smbus_write_byte_data(client, ADV7180_IMR2_ADI, 0);
		if (ret < 0)
			return ret;

		/* enable AD change interrupts */
		ret = i2c_smbus_write_byte_data(client, ADV7180_IMR3_ADI,
						ADV7180_IRQ3_AD_CHANGE);
		if (ret < 0)
			return ret;

		ret = i2c_smbus_write_byte_data(client, ADV7180_IMR4_ADI, 0);
		if (ret < 0)
			return ret;

		ret = i2c_smbus_write_byte_data(client, ADV7180_ADI_CTRL_REG,
						0);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/*
 * adv7180_probe - Probe a ADV7180 device
 * @client: pointer to i2c_client structure
 * @id: pointer to i2c_device_id structure
 *
 * Initialize the ADV7180 device
 */
static int adv7180_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct adv7180_state *state;
	struct v4l2_subdev *sd;
	int ret;

	/* Check if the adapter supports the needed features */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	v4l_info(client, "chip found @ 0x%02x (%s)\n",
		 client->addr << 1, client->adapter->name);

	state = kzalloc(sizeof(struct adv7180_state), GFP_KERNEL);
	if (state == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	state->irq = client->irq;
	INIT_WORK(&state->work, adv7180_work);
	mutex_init(&state->mutex);
	state->autodetect = true;
	state->input = 0;
	state->cfmt = &adv7180_cfmts[0];
	sd = &state->sd;
	v4l2_i2c_subdev_init(sd, client, &adv7180_ops);

	ret = adv7180_init_controls(state);
	if (ret)
		goto err_unreg_subdev;
	ret = init_device(client, state);
	if (ret)
		goto err_free_ctrl;
	return 0;

err_free_ctrl:
	adv7180_exit_controls(state);
err_unreg_subdev:
	mutex_destroy(&state->mutex);
	v4l2_device_unregister_subdev(sd);
	kfree(state);
err:
	printk(KERN_ERR KBUILD_MODNAME ": Failed to probe: %d\n", ret);
	return ret;
}

/*
 * adv7180_remove - Remove ADV7180 device support
 * @client: pointer to i2c_client structure
 *
 * Reset the ADV7180 device
 */
static int adv7180_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct adv7180_state *state = to_state(sd);

	if (state->irq > 0) {
		free_irq(client->irq, state);
		if (cancel_work_sync(&state->work)) {
			/*
			 * Work was pending, therefore we need to enable
			 * IRQ here to balance the disable_irq() done in the
			 * interrupt handler.
			 */
			enable_irq(state->irq);
		}
	}

	v4l2_ctrl_handler_free(&state->ctrl_hdl);
	mutex_destroy(&state->mutex);
	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id adv7180_id[] = {
	{KBUILD_MODNAME, 0},
	{},
};

#ifdef CONFIG_PM
/*
 * adv7180_suspend - Suspend ADV7180 device
 * @client: pointer to i2c_client structure
 * @state: power management state
 *
 * Power down the ADV7180 device
 */
static int adv7180_suspend(struct i2c_client *client, pm_message_t state)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, ADV7180_PWR_MAN_REG,
					ADV7180_PWR_MAN_OFF);
	if (ret < 0)
		return ret;
	return 0;
}

/*
 * adv7180_resume - Resume ADV7180 device
 * @client: pointer to i2c_client structure
 *
 * Power on and initialize the ADV7180 device
 */
static int adv7180_resume(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct adv7180_state *state = to_state(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, ADV7180_PWR_MAN_REG,
					ADV7180_PWR_MAN_ON);
	if (ret < 0)
		return ret;
	ret = init_device(client, state);
	if (ret < 0)
		return ret;
	return 0;
}
#endif

MODULE_DEVICE_TABLE(i2c, adv7180_id);

static struct i2c_driver adv7180_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = KBUILD_MODNAME,
		   },
	.probe = adv7180_probe,
	.remove = adv7180_remove,
#ifdef CONFIG_PM
	.suspend = adv7180_suspend,
	.resume = adv7180_resume,
#endif
	.id_table = adv7180_id,
};

module_i2c_driver(adv7180_driver);

MODULE_DESCRIPTION("Analog Devices ADV7180 video decoder driver");
MODULE_AUTHOR("Mocean Laboratories");
MODULE_LICENSE("GPL v2");
