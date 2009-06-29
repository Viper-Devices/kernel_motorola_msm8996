/*
 * mt9v011 -Micron 1/4-Inch VGA Digital Image Sensor
 *
 * Copyright (c) 2009 Mauro Carvalho Chehab (mchehab@redhat.com)
 * This code is placed under the terms of the GNU General Public License v2
 */

#include <linux/i2c.h>
#include <linux/videodev2.h>
#include <linux/delay.h>
#include <media/v4l2-device.h>
#include "mt9v011.h"
#include <media/v4l2-i2c-drv.h>
#include <media/v4l2-chip-ident.h>

MODULE_DESCRIPTION("Micron mt9v011 sensor driver");
MODULE_AUTHOR("Mauro Carvalho Chehab <mchehab@redhat.com>");
MODULE_LICENSE("GPL");


static int debug;
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "Debug level (0-2)");

/* supported controls */
static struct v4l2_queryctrl mt9v011_qctrl[] = {
	{
		.id = V4L2_CID_GAIN,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Gain",
		.minimum = 0,
		.maximum = (1 << 10) - 1,
		.step = 1,
		.default_value = 0x0020,
		.flags = 0,
	}, {
		.id = V4L2_CID_RED_BALANCE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Red Balance",
		.minimum = -1 << 9,
		.maximum = (1 << 9) - 1,
		.step = 1,
		.default_value = 0,
		.flags = 0,
	}, {
		.id = V4L2_CID_BLUE_BALANCE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Blue Balance",
		.minimum = -1 << 9,
		.maximum = (1 << 9) - 1,
		.step = 1,
		.default_value = 0,
		.flags = 0,
	},
};

struct mt9v011 {
	struct v4l2_subdev sd;

	u16 global_gain, red_bal, blue_bal;
};

static inline struct mt9v011 *to_mt9v011(struct v4l2_subdev *sd)
{
	return container_of(sd, struct mt9v011, sd);
}

static int mt9v011_read(struct v4l2_subdev *sd, unsigned char addr)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	__be16 buffer;
	int rc, val;

	if (1 != (rc = i2c_master_send(c, &addr, 1)))
		v4l2_dbg(0, debug, sd,
			 "i2c i/o error: rc == %d (should be 1)\n", rc);

	msleep(10);

	if (2 != (rc = i2c_master_recv(c, (char *)&buffer, 2)))
		v4l2_dbg(0, debug, sd,
			 "i2c i/o error: rc == %d (should be 1)\n", rc);

	val = be16_to_cpu(buffer);

	v4l2_dbg(2, debug, sd, "mt9v011: read 0x%02x = 0x%04x\n", addr, val);

	return val;
}

static void mt9v011_write(struct v4l2_subdev *sd, unsigned char addr,
				 u16 value)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	unsigned char buffer[3];
	int rc;

	buffer[0] = addr;
	buffer[1] = value >> 8;
	buffer[2] = value & 0xff;

	v4l2_dbg(2, debug, sd,
		 "mt9v011: writing 0x%02x 0x%04x\n", buffer[0], value);
	if (3 != (rc = i2c_master_send(c, buffer, 3)))
		v4l2_dbg(0, debug, sd,
			 "i2c i/o error: rc == %d (should be 3)\n", rc);
}


struct i2c_reg_value {
	unsigned char reg;
	u16           value;
};

/*
 * Values used at the original driver
 * Some values are marked as Reserved at the datasheet
 */
static const struct i2c_reg_value mt9v011_init_default[] = {
					/* guessed meaning - as mt9m111 */
		{ R0D_MT9V011_RESET, 0x0001 },
		{ R0D_MT9V011_RESET, 0x0000 },
		{ R01_MT9V011_ROWSTART, 0x0008 },
		{ R02_MT9V011_COLSTART, 0x0014 },
		{ R03_MT9V011_HEIGHT, 0x01e0 },
		{ R04_MT9V011_WIDTH, 0x0280 },
		{ R05_MT9V011_HBLANK, 0x0001 },
		{ R05_MT9V011_HBLANK, 0x0001 },
		{ R0A_MT9V011_CLK_SPEED, 0x0000 },
		{ R05_MT9V011_HBLANK, 0x000a },
		{ 0x30, 0x0005 },
		{ 0x34, 0x0100 },
		{ 0x3d, 0x068f },
		{ 0x40, 0x01e0 },
		{ 0x52, 0x0100 },
		{ 0x58, 0x0038 },	/* Datasheet default 0x0078 */
		{ 0x59, 0x0723 },	/* Datasheet default 0x0703 */
		{ 0x62, 0x041a },	/* Datasheet default 0x0418 */
		{ R09_MT9V011_SHUTTER_WIDTH, 0x0418 },
		{ R20_MT9V011_READ_MODE, 0x1100 },
};

static void set_balance(struct v4l2_subdev *sd)
{
	struct mt9v011 *core = to_mt9v011(sd);
	u16 green1_gain, green2_gain, blue_gain, red_gain;

	green1_gain = core->global_gain;
	green2_gain = core->global_gain;

	blue_gain = core->global_gain +
		    core->global_gain * core->blue_bal / (1 << 9);

	red_gain = core->global_gain +
		   core->global_gain * core->blue_bal / (1 << 9);

	mt9v011_write(sd, R2B_MT9V011_GREEN_1_GAIN, green1_gain);
	mt9v011_write(sd, R2E_MT9V011_GREEN_2_GAIN,  green1_gain);
	mt9v011_write(sd, R2C_MT9V011_BLUE_GAIN, blue_gain);
	mt9v011_write(sd, R2D_MT9V011_RED_GAIN, red_gain);
}

static int mt9v011_reset(struct v4l2_subdev *sd, u32 val)
{
	u16 version;
	int i;

	version = mt9v011_read(sd, R00_MT9V011_CHIP_VERSION);

	if (version != MT9V011_VERSION) {
		v4l2_info(sd, "*** unknown micron chip detected (0x%04x.\n",
			  version);
		return -EINVAL;

	}

	for (i = 0; i < ARRAY_SIZE(mt9v011_init_default); i++)
		mt9v011_write(sd, mt9v011_init_default[i].reg,
			       mt9v011_init_default[i].value);

	set_balance(sd);

	return 0;
};

static int mt9v011_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct mt9v011 *core = to_mt9v011(sd);

	v4l2_dbg(1, debug, sd, "g_ctrl called\n");

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		ctrl->value = core->global_gain;
		return 0;
	case V4L2_CID_RED_BALANCE:
		ctrl->value = core->red_bal;
		return 0;
	case V4L2_CID_BLUE_BALANCE:
		ctrl->value = core->blue_bal;
		return 0;
	}
	return -EINVAL;
}

static int mt9v011_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct mt9v011 *core = to_mt9v011(sd);
	u8 i, n;
	n = ARRAY_SIZE(mt9v011_qctrl);

	for (i = 0; i < n; i++) {
		if (ctrl->id != mt9v011_qctrl[i].id)
			continue;
		if (ctrl->value < mt9v011_qctrl[i].minimum ||
		    ctrl->value > mt9v011_qctrl[i].maximum)
			return -ERANGE;
		v4l2_dbg(1, debug, sd, "s_ctrl: id=%d, value=%d\n",
					ctrl->id, ctrl->value);
		break;
	}

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		core->global_gain = ctrl->value;
		break;
	case V4L2_CID_RED_BALANCE:
		core->red_bal = ctrl->value;
		break;
	case V4L2_CID_BLUE_BALANCE:
		core->blue_bal = ctrl->value;
		break;
	default:
		return -EINVAL;
	}

	set_balance(sd);

	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int mt9v011_g_register(struct v4l2_subdev *sd,
			      struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	reg->val = mt9v011_read(sd, reg->reg & 0xff);
	reg->size = 2;

	return 0;
}

static int mt9v011_s_register(struct v4l2_subdev *sd,
			      struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	mt9v011_write(sd, reg->reg & 0xff, reg->val & 0xffff);

	return 0;
}
#endif

static int mt9v011_g_chip_ident(struct v4l2_subdev *sd,
				struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_MT9V011,
					  MT9V011_VERSION);
}

static const struct v4l2_subdev_core_ops mt9v011_core_ops = {
	.g_ctrl = mt9v011_g_ctrl,
	.s_ctrl = mt9v011_s_ctrl,
	.reset = mt9v011_reset,
	.g_chip_ident = mt9v011_g_chip_ident,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = mt9v011_g_register,
	.s_register = mt9v011_s_register,
#endif
};

static const struct v4l2_subdev_ops mt9v011_ops = {
	.core = &mt9v011_core_ops,
};


/****************************************************************************
			I2C Client & Driver
 ****************************************************************************/

static int mt9v011_probe(struct i2c_client *c,
			 const struct i2c_device_id *id)
{
	struct mt9v011 *core;
	struct v4l2_subdev *sd;

	/* Check if the adapter supports the needed features */
	if (!i2c_check_functionality(c->adapter,
	     I2C_FUNC_SMBUS_READ_BYTE | I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
		return -EIO;

	core = kzalloc(sizeof(struct mt9v011), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	core->global_gain = 0x0024;

	sd = &core->sd;
	v4l2_i2c_subdev_init(sd, c, &mt9v011_ops);
	v4l_info(c, "chip found @ 0x%02x (%s)\n",
		 c->addr << 1, c->adapter->name);

	return 0;
}

static int mt9v011_remove(struct i2c_client *c)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(c);

	v4l2_dbg(1, debug, sd,
		"mt9v011.c: removing mt9v011 adapter on address 0x%x\n",
		c->addr << 1);

	v4l2_device_unregister_subdev(sd);
	kfree(to_mt9v011(sd));
	return 0;
}

/* ----------------------------------------------------------------------- */

static const struct i2c_device_id mt9v011_id[] = {
	{ "mt9v011", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mt9v011_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = "mt9v011",
	.probe = mt9v011_probe,
	.remove = mt9v011_remove,
	.id_table = mt9v011_id,
};
