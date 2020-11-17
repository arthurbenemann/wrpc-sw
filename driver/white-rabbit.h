// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@gmail.com>
 */

#ifndef __LINUX_WHITE_RABBIT_H
#define __LINUX_WHITE_RABBIT_H

#include <linux/serial_core.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/types.h>

enum wr_versions {
	WR_VER = 0,
};

#define WR_SIGNATURE 0x0
#define WR_SIGNATURE_OFFSET 0x0

#define WR_MEM 0

struct wr_memory_ops {
	u32 (*read)(void *addr);
	void (*write)(u32 value, void *addr);
};

struct wr_dev {
	struct platform_device *pdev;
	void __iomem *base;
	struct wr_memory_ops memops;
	spinlock_t lock;

	struct platform_device *uart_pdev;
};

static inline struct wr_dev *to_wr_dev(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return platform_get_drvdata(pdev);
}

static inline uint32_t wr_ioread(struct wr_dev *wr, void *addr)
{
	return wr->memops.read(addr);
}

static inline void wr_iowrite(struct wr_dev *wr,
			      uint32_t value, void *addr)
{
	return wr->memops.write(value, addr);
}

#endif /* __LINUX_WHITE_RABBIT_H */
