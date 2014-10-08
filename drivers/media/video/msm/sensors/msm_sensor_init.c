/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_sensor.h"
#include "msm.h"
#include "msm_sensor_bayer.h"
#ifdef CONFIG_IMX091
#include "imx091.h"
#endif
#ifdef CONFIG_IMX135
#include "imx135.h"
#endif

static struct i2c_driver *sensor_i2c_driver[] = {
	/* back camera */
#ifdef CONFIG_IMX135	
	&imx135_i2c_driver,
#else	
	&imx091_i2c_driver,
#endif	
	/* front camera */
};

static int __init msm_sensor_init_module(void)
{
	int index = 0;
	for (index = 0; index < ARRAY_SIZE(sensor_i2c_driver); index++)
		i2c_add_driver(sensor_i2c_driver[index]);
	return 0;
}

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Sensor driver probe");
MODULE_LICENSE("GPL v2");
