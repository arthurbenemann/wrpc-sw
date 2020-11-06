// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@gmail.com>
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include "white-rabbit.h"

static int wr_probe(struct platform_device *pdev)
{
	return 0;
}
static int wr_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct platform_device_id wr_id[] = {
	{
		.name = "white-rabbit",
		.driver_data = WR_VER,
	},
	{},
	/* TODO we should support different version */
};

static struct platform_driver wr_dev_drv = {
	.driver =
        {
		.name = KBUILD_MODNAME,
        },
	.probe = wr_probe,
	.remove = wr_remove,
	.id_table = wr_id,
};

module_platform_driver(wr_dev_drv);


MODULE_AUTHOR("Federico Vaga");
MODULE_DESCRIPTION("White Rabbit Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION);
MODULE_DEVICE_TABLE(platform, wr_id);

ADDITIONAL_VERSIONS;
