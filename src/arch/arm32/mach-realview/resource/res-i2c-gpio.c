/*
 * resource/res-i2c-gpio.c
 *
 * Copyright(c) 2007-2013 jianjun jiang <jerryjianjun@gmail.com>
 * official site: http://xboot.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <xboot.h>
#include <bus/i2c-gpio.h>
#include <realview/reg-gpio.h>

static struct i2c_gpio_data_t i2c_gpio_datas[] = {
	[0] = {
		.sda_pin			= REALVIEW_GPIO2(0),
		.scl_pin			= REALVIEW_GPIO2(1),
		.udelay				= 50,
		.timeout			= 100,
		.sda_is_open_drain	= 0,
		.scl_is_open_drain	= 0,
		.scl_is_output_only	= 0,
	},
	[1] = {
		.sda_pin			= REALVIEW_GPIO2(2),
		.scl_pin			= REALVIEW_GPIO2(3),
		.udelay				= 50,
		.timeout			= 100,
		.sda_is_open_drain	= 0,
		.scl_is_open_drain	= 0,
		.scl_is_output_only	= 0,
	},
};

static struct resource_t res_i2c_gpios[] = {
	{
		.name		= "i2c-gpio",
		.id			= 0,
		.data		= &i2c_gpio_datas[0],
	}, {
		.name		= "i2c-gpio",
		.id			= 1,
		.data		= &i2c_gpio_datas[1],
	},
};

static __init void resource_i2c_gpio_init(void)
{
	u32_t i;

	for(i = 0; i < ARRAY_SIZE(res_i2c_gpios); i++)
	{
		if(register_resource(&res_i2c_gpios[i]))
			LOG("Register resource '%s.%d'", res_i2c_gpios[i].name, res_i2c_gpios[i].id);
		else
			LOG("Failed to register resource '%s.%d'", res_i2c_gpios[i].name, res_i2c_gpios[i].id);
	}
}

static __exit void resource_i2c_gpio_exit(void)
{
	u32_t i;

	for(i = 0; i < ARRAY_SIZE(res_i2c_gpios); i++)
	{
		if(unregister_resource(&res_i2c_gpios[i]))
			LOG("Unregister resource '%s.%d'", res_i2c_gpios[i].name, res_i2c_gpios[i].id);
		else
			LOG("Failed to unregister resource '%s.%d'", res_i2c_gpios[i].name, res_i2c_gpios[i].id);
	}
}

resource_initcall(resource_i2c_gpio_init);
resource_exitcall(resource_i2c_gpio_exit);