/*
 *  Quanta IX9 platform driver
 *
 *
 *  Copyright (C) 2019 Quanta Computer inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/proc_fs.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/input-polldev.h>
#include <linux/rfkill.h>
#include <linux/slab.h>
#include <linux/platform_data/pca954x.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,12,0)
#include <linux/platform_data/pca953x.h>
#else
#include <linux/i2c/pca953x.h>
#endif
#define MUX_INFO(bus, deselect) \
	{.adap_id = bus, .deselect_on_exit = deselect}

static struct pca954x_platform_mode pca9546_1_modes[] = {
	MUX_INFO(0x10, 1),
	MUX_INFO(0x11, 1),
	MUX_INFO(0x12, 1),
	MUX_INFO(0x13, 1),
};

static struct pca954x_platform_data pca9546_1_data = {
	.modes 		= pca9546_1_modes,
	.num_modes 	= 4,
};

static struct pca954x_platform_mode pca9548_1_modes[] = {
	MUX_INFO(0x14, 1),
	MUX_INFO(0x15, 1),
	MUX_INFO(0x16, 1),
	MUX_INFO(0x17, 1),
};

static struct pca954x_platform_data pca9548_1_data = {
	.modes 		= pca9548_1_modes,
	.num_modes 	= 4,
};

static struct pca954x_platform_mode pca9548sfp1_modes[] = {
	MUX_INFO(0x20, 1),
	MUX_INFO(0x21, 1),
	MUX_INFO(0x22, 1),
	MUX_INFO(0x23, 1),
	MUX_INFO(0x24, 1),
	MUX_INFO(0x25, 1),
	MUX_INFO(0x26, 1),
	MUX_INFO(0x27, 1),
};

static struct pca954x_platform_data pca9548sfp1_data = {
	.modes 		= pca9548sfp1_modes,
	.num_modes 	= 8,
};

static struct pca954x_platform_mode pca9548sfp2_modes[] = {
	MUX_INFO(0x28, 1),
	MUX_INFO(0x29, 1),
	MUX_INFO(0x2a, 1),
	MUX_INFO(0x2b, 1),
	MUX_INFO(0x2c, 1),
	MUX_INFO(0x2d, 1),
	MUX_INFO(0x2e, 1),
	MUX_INFO(0x2f, 1),
};

static struct pca954x_platform_data pca9548sfp2_data = {
	.modes 		= pca9548sfp2_modes,
	.num_modes 	= 8,
};

static struct pca954x_platform_mode pca9548sfp3_modes[] = {
	MUX_INFO(0x30, 1),
	MUX_INFO(0x31, 1),
	MUX_INFO(0x32, 1),
	MUX_INFO(0x33, 1),
	MUX_INFO(0x34, 1),
	MUX_INFO(0x35, 1),
	MUX_INFO(0x36, 1),
	MUX_INFO(0x37, 1),
};

static struct pca954x_platform_data pca9548sfp3_data = {
	.modes 		= pca9548sfp3_modes,
	.num_modes 	= 8,
};

static struct pca954x_platform_mode pca9548sfp4_modes[] = {
	MUX_INFO(0x38, 1),
	MUX_INFO(0x39, 1),
	MUX_INFO(0x3a, 1),
	MUX_INFO(0x3b, 1),
	MUX_INFO(0x3c, 1),
	MUX_INFO(0x3d, 1),
	MUX_INFO(0x3e, 1),
	MUX_INFO(0x3f, 1),
};

static struct pca954x_platform_data pca9548sfp4_data = {
	.modes 		= pca9548sfp4_modes,
	.num_modes 	= 8,
};

static struct pca953x_platform_data tca9539_1_data = {
	.gpio_base = 0x10,
};
//CPU Linking Board at CPU's I2C Bus
static struct pca953x_platform_data pca9555_CPU_data = {
	.gpio_base = 0x20,
};

static struct i2c_board_info ix9_i2c_devices[] = {
	{
		I2C_BOARD_INFO("pca9546", 0x72),
		.platform_data = &pca9546_1_data,	// 0 pca9546_1
	},
	{
		I2C_BOARD_INFO("pca9548", 0x77),
		.platform_data = &pca9548_1_data,	// 1 pca9548_1
	},
	{
		I2C_BOARD_INFO("tca9539", 0x74),
		.platform_data = &tca9539_1_data,	// 2 Board ID and QSFP-DD PW EN/PG
	},
	{
		I2C_BOARD_INFO("24c02", 0x54),		// 3 MB_BOARDINFO_EEPROM
	},
	{
		I2C_BOARD_INFO("pca9548", 0x73),
		.platform_data = &pca9548sfp1_data,	// 4 0x77 ch0 pca9548 #1
	},
	{
		I2C_BOARD_INFO("pca9548", 0x73),
		.platform_data = &pca9548sfp2_data,	// 5 0x77 ch1 pca9548 #2
	},
	{
		I2C_BOARD_INFO("pca9548", 0x73),
		.platform_data = &pca9548sfp3_data,	// 6 0x77 ch2 pca9548 #3
	},
	{
		I2C_BOARD_INFO("pca9548", 0x73),
		.platform_data = &pca9548sfp4_data,	// 7 0x77 ch3 pca9548 #4
	},
	{
		I2C_BOARD_INFO("CPLD-QSFPDD", 0x38),	// 8 0x72 ch0 CPLD-IO #2, #3
	},
	{
		I2C_BOARD_INFO("CPLDLED_IX9", 0x39),	// 9 0x72 ch1 CPLD-LED #4, #5
	},
	{
		I2C_BOARD_INFO("optoe1", 0x50),		// 10 eeprom for loopback module
	},
	{
		I2C_BOARD_INFO("pca9555", 0x22),	// 11 CPU Linking Board at CPU's I2C Bus
		.platform_data = &pca9555_CPU_data,
	},
};

static struct platform_driver ix9_platform_driver = {
	.driver = {
		.name = "qci-ix9",
		.owner = THIS_MODULE,
	},
};

static struct platform_device *ix9_device;

static int __init ix9_platform_init(void)
{
	struct i2c_client *client;
	struct i2c_adapter *adapter;
	int ret, i;

	ret = platform_driver_register(&ix9_platform_driver);
	if (ret < 0)
		return ret;

	/* Register platform stuff */
	ix9_device = platform_device_alloc("qci-ix9", -1);
	if (!ix9_device) {
		ret = -ENOMEM;
		goto fail_platform_driver;
	}

	ret = platform_device_add(ix9_device);
	if (ret)
		goto fail_platform_device;

	adapter = i2c_get_adapter(0);
	client = i2c_new_device(adapter, &ix9_i2c_devices[0]);		// pca9546_1 - Address: 0x72
	client = i2c_new_device(adapter, &ix9_i2c_devices[1]);		// pca9548_1 - Address: 0x77
	client = i2c_new_device(adapter, &ix9_i2c_devices[11]);		// CPU Linking Board at CPU's I2C Bus - Address: 0x22
	i2c_put_adapter(adapter);

	adapter = i2c_get_adapter(0x10);
	client = i2c_new_device(adapter, &ix9_i2c_devices[8]);		// CPLD-IO #2 - Address: 0x38
	client = i2c_new_device(adapter, &ix9_i2c_devices[9]);		// CPLD-LED #4 - Address: 0x39
	i2c_put_adapter(adapter);

	adapter = i2c_get_adapter(0x11);
	client = i2c_new_device(adapter, &ix9_i2c_devices[8]);		// CPLD-IO #3 - Address: 0x38
	client = i2c_new_device(adapter, &ix9_i2c_devices[9]);		// CPLD-LED #5 - Address: 0x39
	i2c_put_adapter(adapter);

	adapter = i2c_get_adapter(0x12);
	client = i2c_new_device(adapter, &ix9_i2c_devices[3]);		// MB_BOARDINFO_EEPROM - Address: 0x54
	i2c_put_adapter(adapter);

	adapter = i2c_get_adapter(0x13);
	client = i2c_new_device(adapter, &ix9_i2c_devices[2]);		// tca9539_1 Board ID and QSFP-DD PW EN/PG - Address: 0x74
	i2c_put_adapter(adapter);

	adapter = i2c_get_adapter(0x14);
	client = i2c_new_device(adapter, &ix9_i2c_devices[4]);		// pca9548 #1 QSFPDD - Address: 0x73
	i2c_put_adapter(adapter);

	adapter = i2c_get_adapter(0x15);
	client = i2c_new_device(adapter, &ix9_i2c_devices[5]);		// pca9548 #2 QSFPDD - Address: 0x73
	i2c_put_adapter(adapter);

	adapter = i2c_get_adapter(0x16);
	client = i2c_new_device(adapter, &ix9_i2c_devices[6]);		// pca9548 #3 QSFPDD - Address: 0x73
	i2c_put_adapter(adapter);

	adapter = i2c_get_adapter(0x17);
	client = i2c_new_device(adapter, &ix9_i2c_devices[7]);		// pca9548 #4 QSFPDD - Address: 0x73
	i2c_put_adapter(adapter);

	for(i = 0x20; i < 0x40; i++)
	{
		adapter = i2c_get_adapter(i);
		client = i2c_new_device(adapter, &ix9_i2c_devices[10]);		// eeprom for loopback module - Address: 0x50
		i2c_put_adapter(adapter);
	}

	return 0;

fail_platform_device:
	platform_device_put(ix9_device);

fail_platform_driver:
	platform_driver_unregister(&ix9_platform_driver);
	return ret;
}

static void __exit ix9_platform_exit(void)
{
	platform_device_unregister(ix9_device);
	platform_driver_unregister(&ix9_platform_driver);
}

module_init(ix9_platform_init);
module_exit(ix9_platform_exit);

MODULE_AUTHOR("Jonathan Tsai <jonathan.tsai@quantatw.com>");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("Quanta IX9 Platform Driver");
MODULE_LICENSE("GPL");
