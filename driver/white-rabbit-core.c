// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@gmail.com>
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/platform_data/wb-uart.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/version.h>
#include "white-rabbit.h"


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

#define WB_UART_RES_N 2
#define WB_UART_MEM_START 0x0
#define WB_UART_MEM_END 0x0
#define WB_IRQ_UART 0
static int wr_uart_register(struct wr_dev *wr)
{
	struct platform_device *pdev;
	struct wb_uart_platform_data pdata;
	struct resource res[WB_UART_RES_N];
	struct resource *wr_res;
	int ret;

        ret = wr_endianess(wr);
	if (ret < 0)
		return -1;
	wr_res = platform_get_resource(wr->pdev, IORESOURCE_MEM, 0);
	res[0].name = "wr-uart-mem";
	res[0].flags = IORESOURCE_MEM;
	res[0].start = wr_res->start + WB_UART_MEM_START;
	res[0].end = wr_res->start + WB_UART_MEM_END;
	wr_res = platform_get_resource(wr->pdev, IORESOURCE_IRQ, 0);
	res[1].name = "wr-uart-irq";
	res[1].flags = IORESOURCE_IRQ;
	res[1].start = wr_res->start + WB_IRQ_UART;
	res[1].end = 0;
	pdata.big_endian = ret;
	pdev = platform_device_register_resndata(&wr->pdev->dev,
						 "wb-uart",
						 PLATFORM_DEVID_AUTO,
						 res, ARRAY_SIZE(res),
						 &pdata, sizeof(pdata));
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	wr->uart_pdev = pdev;

        return 0;
}

static void wr_uart_unregister(struct wr_dev *wr)
{
	if (wr->uart_pdev) {
		platform_device_unregister(wr->uart_pdev);
		wr->uart_pdev = NULL;
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

	res = platform_get_resource(pdev, IORESOURCE_MEM, WR_MEM);
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

	err = wr_uart_register(wr);
	if (err)
		goto err_uart;

	platform_set_drvdata(pdev, wr);
        return 0;

err_uart:
err_endianess:
	devm_kfree(&pdev->dev, wr);
	return err;
}
static int wr_remove(struct platform_device *pdev)
{
	struct wr_dev *wr = platform_get_drvdata(pdev);

        wr_uart_unregister(wr);

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
