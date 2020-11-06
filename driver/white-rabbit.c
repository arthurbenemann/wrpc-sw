// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@gmail.com>
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/version.h>
#include "white-rabbit.h"

#define WR_SIGNATURE 0x0
#define WR_SIGNATURE_OFFSET 0x0

struct wr_memory_ops {
	u32 (*read)(void *addr);
	void (*write)(u32 value, void *addr);
};

struct wr_dev {
	struct platform_device *pdev;
	void __iomem *base;
	struct wr_memory_ops memops;
};

static int wr_endianess(struct wr_dev *wr)
{
	uint32_t signature;

	signature = ioread32(wr->base + WR_SIGNATURE_OFFSET);
	if (signature == WR_SIGNATURE)
		return 0;
	signature = ioread32be(wr->base + WR_SIGNATURE_OFFSET);
	if (signature == WR_SIGNATURE)
		return 1;

	return -1;
}

static int wr_memops_detect(struct wr_dev *wr)
{
	switch (wr_endianess(wr)) {
	case 0:
		wr->memops.read = ioread32;
		wr->memops.write = iowrite32;
		return 0;
	case 1:
		wr->memops.read = ioread32be;
		wr->memops.write = iowrite32be;
		return 0;
	default:
		dev_err(&wr->pdev->dev, "Invalid endianess\n");
		return -EINVAL;
	}
}

static int wr_probe(struct platform_device *pdev)
{
	struct wr_dev *wr;
	struct resource *res;
	int err;

        wr = devm_kzalloc(&pdev->dev, sizeof(*wr), GFP_KERNEL);
	if (!wr)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
#if KERNEL_VERSION(3, 9, 0) > LINUX_VERSION_CODE
	wr->base = devm_request_and_ioremap(&pdev->dev, res);
	if (!wr->base)
		return -EADDRNOTAVAIL;
#else
	wr->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(wr->base))
		return PTR_ERR(wr->base);
#endif

	err = wr_memops_detect(wr);
	if (err)
		goto err_endianess;

	return 0;

err_endianess:
	devm_kfree(&pdev->dev, wr);
	return err;
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
