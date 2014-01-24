/*
 * HID Sensors Driver
 * Copyright (c) 2012, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#ifndef _HID_SENSORS_IDS_H
#define _HID_SENSORS_IDS_H

#define HID_MAX_PHY_DEVICES					0xFF

/* Accel 3D (200073) */
#define HID_USAGE_SENSOR_ACCEL_3D				0x200073
#define HID_USAGE_SENSOR_DATA_ACCELERATION			0x200452
#define HID_USAGE_SENSOR_ACCEL_X_AXIS				0x200453
#define HID_USAGE_SENSOR_ACCEL_Y_AXIS				0x200454
#define HID_USAGE_SENSOR_ACCEL_Z_AXIS				0x200455

/* ALS (200041) */
#define HID_USAGE_SENSOR_ALS					0x200041
#define HID_USAGE_SENSOR_DATA_LIGHT				0x2004d0
#define HID_USAGE_SENSOR_LIGHT_ILLUM				0x2004d1

/* Gyro 3D: (200076) */
#define HID_USAGE_SENSOR_GYRO_3D				0x200076
#define HID_USAGE_SENSOR_DATA_ANGL_VELOCITY			0x200456
#define HID_USAGE_SENSOR_ANGL_VELOCITY_X_AXIS			0x200457
#define HID_USAGE_SENSOR_ANGL_VELOCITY_Y_AXIS			0x200458
#define HID_USAGE_SENSOR_ANGL_VELOCITY_Z_AXIS			0x200459

/* ORIENTATION: Compass 3D: (200083) */
#define HID_USAGE_SENSOR_COMPASS_3D				0x200083
#define HID_USAGE_SENSOR_DATA_ORIENTATION			0x200470
#define HID_USAGE_SENSOR_ORIENT_MAGN_HEADING			0x200471
#define HID_USAGE_SENSOR_ORIENT_MAGN_HEADING_X			0x200472
#define HID_USAGE_SENSOR_ORIENT_MAGN_HEADING_Y			0x200473
#define HID_USAGE_SENSOR_ORIENT_MAGN_HEADING_Z			0x200474

#define HID_USAGE_SENSOR_ORIENT_COMP_MAGN_NORTH			0x200475
#define HID_USAGE_SENSOR_ORIENT_COMP_TRUE_NORTH			0x200476
#define HID_USAGE_SENSOR_ORIENT_MAGN_NORTH			0x200477
#define HID_USAGE_SENSOR_ORIENT_TRUE_NORTH			0x200478

#define HID_USAGE_SENSOR_ORIENT_DISTANCE			0x200479
#define HID_USAGE_SENSOR_ORIENT_DISTANCE_X			0x20047A
#define HID_USAGE_SENSOR_ORIENT_DISTANCE_Y			0x20047B
#define HID_USAGE_SENSOR_ORIENT_DISTANCE_Z			0x20047C
#define HID_USAGE_SENSOR_ORIENT_DISTANCE_OUT_OF_RANGE		0x20047D

/* ORIENTATION: Inclinometer 3D: (200086) */
#define HID_USAGE_SENSOR_INCLINOMETER_3D			0x200086
#define HID_USAGE_SENSOR_ORIENT_TILT				0x20047E
#define HID_USAGE_SENSOR_ORIENT_TILT_X				0x20047F
#define HID_USAGE_SENSOR_ORIENT_TILT_Y				0x200480
#define HID_USAGE_SENSOR_ORIENT_TILT_Z				0x200481

#define HID_USAGE_SENSOR_ORIENT_ROTATION_MATRIX			0x200482
#define HID_USAGE_SENSOR_ORIENT_QUATERNION			0x200483
#define HID_USAGE_SENSOR_ORIENT_MAGN_FLUX			0x200484

#define HID_USAGE_SENSOR_ORIENT_MAGN_FLUX_X_AXIS		0x200485
#define HID_USAGE_SENSOR_ORIENT_MAGN_FLUX_Y_AXIS		0x200486
#define HID_USAGE_SENSOR_ORIENT_MAGN_FLUX_Z_AXIS		0x200487

/* Time (2000a0) */
#define HID_USAGE_SENSOR_TIME					0x2000a0
#define HID_USAGE_SENSOR_TIME_YEAR				0x200521
#define HID_USAGE_SENSOR_TIME_MONTH				0x200522
#define HID_USAGE_SENSOR_TIME_DAY				0x200523
#define HID_USAGE_SENSOR_TIME_HOUR				0x200525
#define HID_USAGE_SENSOR_TIME_MINUTE				0x200526
#define HID_USAGE_SENSOR_TIME_SECOND				0x200527

/* Units */
#define HID_USAGE_SENSOR_UNITS_NOT_SPECIFIED			0x00
#define HID_USAGE_SENSOR_UNITS_LUX				0x01
#define HID_USAGE_SENSOR_UNITS_KELVIN				0x01000100
#define HID_USAGE_SENSOR_UNITS_FAHRENHEIT			0x03000100
#define HID_USAGE_SENSOR_UNITS_PASCAL				0xF1E1
#define HID_USAGE_SENSOR_UNITS_NEWTON				0x11E1
#define HID_USAGE_SENSOR_UNITS_METERS_PER_SECOND		0x11F0
#define HID_USAGE_SENSOR_UNITS_METERS_PER_SEC_SQRD		0x11E0
#define HID_USAGE_SENSOR_UNITS_FARAD				0xE14F2000
#define HID_USAGE_SENSOR_UNITS_AMPERE				0x01001000
#define HID_USAGE_SENSOR_UNITS_WATT				0x21d1
#define HID_USAGE_SENSOR_UNITS_HENRY				0x21E1E000
#define HID_USAGE_SENSOR_UNITS_OHM				0x21D1E000
#define HID_USAGE_SENSOR_UNITS_VOLT				0x21D1F000
#define HID_USAGE_SENSOR_UNITS_HERTZ				0x01F0
#define HID_USAGE_SENSOR_UNITS_DEGREES_PER_SEC_SQRD		0x14E0
#define HID_USAGE_SENSOR_UNITS_RADIANS				0x12
#define HID_USAGE_SENSOR_UNITS_RADIANS_PER_SECOND		0x12F0
#define HID_USAGE_SENSOR_UNITS_RADIANS_PER_SEC_SQRD		0x12E0
#define HID_USAGE_SENSOR_UNITS_SECOND				0x0110
#define HID_USAGE_SENSOR_UNITS_GAUSS				0x01E1F000
#define HID_USAGE_SENSOR_UNITS_GRAM				0x0101
#define HID_USAGE_SENSOR_UNITS_CENTIMETER			0x11
#define HID_USAGE_SENSOR_UNITS_G				0x1A
#define HID_USAGE_SENSOR_UNITS_MILLISECOND			0x19
#define HID_USAGE_SENSOR_UNITS_PERCENT				0x17
#define HID_USAGE_SENSOR_UNITS_DEGREES				0x14
#define HID_USAGE_SENSOR_UNITS_DEGREES_PER_SECOND		0x15

/* Common selectors */
#define HID_USAGE_SENSOR_PROP_REPORT_INTERVAL			0x20030E
#define HID_USAGE_SENSOR_PROP_SENSITIVITY_ABS			0x20030F
#define HID_USAGE_SENSOR_PROP_SENSITIVITY_RANGE_PCT		0x200310
#define HID_USAGE_SENSOR_PROP_SENSITIVITY_REL_PCT		0x200311
#define HID_USAGE_SENSOR_PROP_ACCURACY				0x200312
#define HID_USAGE_SENSOR_PROP_RESOLUTION			0x200313
#define HID_USAGE_SENSOR_PROP_RANGE_MAXIMUM			0x200314
#define HID_USAGE_SENSOR_PROP_RANGE_MINIMUM			0x200315
#define HID_USAGE_SENSOR_PROP_REPORT_STATE			0x200316
#define HID_USAGE_SENSOR_PROY_POWER_STATE			0x200319

/* Per data field properties */
#define HID_USAGE_SENSOR_DATA_MOD_NONE					0x00
#define HID_USAGE_SENSOR_DATA_MOD_CHANGE_SENSITIVITY_ABS		0x1000

/* Power state enumerations */
#define HID_USAGE_SENSOR_PROP_POWER_STATE_UNDEFINED_ENUM	0x200850
#define HID_USAGE_SENSOR_PROP_POWER_STATE_D0_FULL_POWER_ENUM	0x200851
#define HID_USAGE_SENSOR_PROP_POWER_STATE_D1_LOW_POWER_ENUM	0x200852
#define HID_USAGE_SENSOR_PROP_POWER_STATE_D2_STANDBY_WITH_WAKE_ENUM 0x200853
#define HID_USAGE_SENSOR_PROP_POWER_STATE_D3_SLEEP_WITH_WAKE_ENUM 0x200854
#define HID_USAGE_SENSOR_PROP_POWER_STATE_D4_POWER_OFF_ENUM	0x200855

/* Report State enumerations */
#define HID_USAGE_SENSOR_PROP_REPORTING_STATE_NO_EVENTS_ENUM	0x200840
#define HID_USAGE_SENSOR_PROP_REPORTING_STATE_ALL_EVENTS_ENUM	0x200841

#endif
