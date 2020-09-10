/* Centec TsingMa Memory Controller Driver
 *
 * Author: lius <lius@centecnetworks.com>
 *
 * Copyright 2002-2019, Centec Networks (Suzhou) Co., Ltd.
 *
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>

struct ctc5236_mc {
	struct device *dev;
	void __iomem *base;
	int irq;

};

/* DDR interrupt enable register */
#define DDR_ERR_INT_EN	0xF0

/* DDR interrupt status register */
#define DDR_ERR_INT_STATUS	0xF4

/* over top-bound info register*/
#define DDR_ERR_INT_OVER_TOPBOUND_L	0xF8
#define DDR_ERR_INT_OVER_TOPBOUND_H	0xFC

#define DDR_PORT0_ERR_INT_STATUS		0x1
#define DDR_PORT1_ERR_INT_STATUS		0x2
#define DDR_PORT2_ERR_INT_STATUS		0x3
#define DDR_PORT3_ERR_INT_STATUS		0x4
#define DDR_ERR_ECC_INT_STATUS		0x10000
#define DDR_ERR_WR_PORT_REC_UNDERFLOW		0x20000
#define DDR_ERR_WR_PORT_REC_OVERFLOW		0x40000
#define DDR_ERR_RD_PORT_REC_UNDERFLOW		0x80000
#define DDR_ERR_RD_PORT_REC_OVERFLOW		0x100000

#define DDR_PORT0_STATUS	0xB0
#define DDR_PORT1_STATUS	0xB4
#define DDR_PORT2_STATUS	0xB8
#define DDR_PORT3_STATUS	0xBC

#define DDR_ERR_OVER_TOPBOUND	0x20000
#define DDR_ERR_WCMDQ_OVER		0x40000
#define DDR_ERR_WCMDQ_UNDER		0x80000
#define DDR_ERR_WDATAQ_OVER		0x100000
#define DDR_ERR_WDATAQ_UNDER	0x200000
#define DDR_ERR_WESPQ_OVER		0x400000
#define DDR_ERR_WESPQ_UNDER		0x800000
#define DDR_ERR_WINFOQ_OVER		0x1000000
#define DDR_ERR_WINFOQ_UNDER		0x2000000
#define DDR_ERR_RCMDQ_OVER		0x4000000
#define DDR_ERR_RCMDQ_UNDER		0x8000000
#define DDR_ERR_RDATAQ_OVER		0x10000000
#define DDR_ERR_RDATAQ_UNDER		0x20000000
#define DDR_ERR_RESPQ_OVER		0x40000000
#define DDR_ERR_RESPQ_UNDER		0x80000000

#define DDR_PORT0_BASE 0xB0
#define DDR_PORT1_BASE 0xB4
#define DDR_PORT2_BASE 0xB8
#define DDR_PORT3_BASE 0xBc

#define DDR_PORT0	0
#define DDR_PORT1	1
#define DDR_PORT2	2
#define DDR_PORT3	3

static int port_err_status(int status, int port, void *dev_id)
{
	int id = port;
	unsigned long temp = 0;
	unsigned int addr_h = 0;
	unsigned int addr_l = 0;
	struct ctc5236_mc *mci = dev_id;

	/*the  reason of port interrupt  */
	if (status & DDR_ERR_OVER_TOPBOUND) {
		/* get low 32-bit address */
		addr_l = readl(mci->base + DDR_ERR_INT_OVER_TOPBOUND_L);
		/* get high 2-bit address */
		addr_h = readl(mci->base + DDR_ERR_INT_OVER_TOPBOUND_H);
		temp = (addr_l | (((unsigned long)((addr_h >> 12) & 0x3)) << 32)
		    );

		pr_emerg("ERROR:port%d is out of top-bound range!\n"
			 "The error address is 0x%p\n",
			 id, (void *)temp);
	}

	if (status & DDR_ERR_WCMDQ_OVER)
		pr_err("ERROR:port%d write command queue is overflow!\n", id);

	if (status & DDR_ERR_WCMDQ_UNDER)
		pr_err("ERROR:port%d write command queue is underflow!\n", id);

	if (status & DDR_ERR_WDATAQ_OVER)
		pr_err("ERROR:port%d write data queue is overflow!\n", id);

	if (status & DDR_ERR_WDATAQ_UNDER)
		pr_err("ERROR:port%d write data queue is underflow!\n", id);

	if (status & DDR_ERR_WESPQ_OVER)
		pr_err("ERROR:port%d write response queue is overflow!\n", id);

	if (status & DDR_ERR_WESPQ_UNDER)
		pr_err("ERROR:port%d write response queue is underflow!\n", id);

	if (status & DDR_ERR_WINFOQ_OVER)
		pr_err("ERROR:port%d write info queue is overflow!\n", id);

	if (status & DDR_ERR_WINFOQ_UNDER)
		pr_err("ERROR:port%d write info queue is underflow!\n", id);

	if (status & DDR_ERR_RCMDQ_OVER)
		pr_err("ERROR:port%d read command queue is overflow!\n", id);

	if (status & DDR_ERR_RCMDQ_UNDER)
		pr_err("ERROR:port%d read command queue is underflow!\n", id);

	if (status & DDR_ERR_RDATAQ_OVER)
		pr_err("ERROR:port%d read data queue is overflow!\n", id);

	if (status & DDR_ERR_RDATAQ_UNDER)
		pr_err("ERROR:port%d read data queue is underflow!\n", id);

	if (status & DDR_ERR_RESPQ_OVER)
		pr_err("ERROR:port%d read response queue is overflow!\n", id);

	if (status & DDR_ERR_RESPQ_UNDER)
		pr_err("ERROR:port%d read response queue is underflow!\n", id);

	return 1;
}

static irqreturn_t ctc_mc_err_handler(int irq, void *dev_id)
{
	struct ctc5236_mc *mci = dev_id;
	unsigned int status;
	unsigned int ret = 0;

	/* get interrupt status */
	status = readl(mci->base + DDR_ERR_INT_STATUS);

	if (status & DDR_PORT0_ERR_INT_STATUS) {
		ret = readl(mci->base + DDR_PORT0_BASE);
		port_err_status(ret, DDR_PORT0, mci);
	}

	if (status & DDR_PORT1_ERR_INT_STATUS) {
		ret = readl(mci->base + DDR_PORT1_BASE);
		port_err_status(ret, DDR_PORT1, mci);
	}

	if (status & DDR_PORT2_ERR_INT_STATUS) {
		ret = readl(mci->base + DDR_PORT2_BASE);
		port_err_status(ret, DDR_PORT2, mci);
	}

	if (status & DDR_PORT3_ERR_INT_STATUS) {
		ret = readl(mci->base + DDR_PORT3_BASE);
		port_err_status(ret, DDR_PORT3, mci);
	}

	if (status & DDR_ERR_ECC_INT_STATUS)
		pr_err("ERROR:The ecc more than 1-bit error !\n");

	if (status & DDR_ERR_WR_PORT_REC_UNDERFLOW)
		pr_err("ERROR:MPARB wr_port_rec FIFO is underflow!\n");

	if (status & DDR_ERR_WR_PORT_REC_OVERFLOW)
		pr_err("ERROR:MPARB wr_port_rec FIFO is overflow!\n");

	if (status & DDR_ERR_RD_PORT_REC_UNDERFLOW)
		pr_err("ERROR:MPARB rd_port_rec FIFO is underflow!\n");

	if (status & DDR_ERR_RD_PORT_REC_OVERFLOW)
		pr_err("ERROR:MPARB rd_port_rec FIFO is underflow!\n");

	/* disable DDR interrupt */
	writel(0x0, mci->base + DDR_ERR_INT_EN);

	return IRQ_HANDLED;
}

static const struct of_device_id ctc5236_ddr_ctrl_of_match[] = {
	{
	 .compatible = "ctc,ctc5236-ddr-ctrl",
	 },
	{},
};

MODULE_DEVICE_TABLE(of, ctc5236_ddr_ctrl_of_match);

static int ctc5236_mc_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;
	struct ctc5236_mc *mci;
	int ret;

	id = of_match_device(ctc5236_ddr_ctrl_of_match, &pdev->dev);
	if (!id)
		return -ENODEV;

	mci = kzalloc(sizeof(*mci), GFP_KERNEL);
	if (!mci)
		return -ENODEV;

	mci->base = devm_ioremap_resource(&pdev->dev, pdev->resource);
	if (IS_ERR(mci->base))
		return PTR_ERR(mci->base);

	mci->irq = platform_get_irq(pdev, 0);
	ret =
	    devm_request_irq(&pdev->dev, mci->irq, ctc_mc_err_handler, 0,
			     dev_name(&pdev->dev), mci);
	if (ret < 0) {
		dev_err(&pdev->dev, "Unable to request irq %d\n", mci->irq);
		goto err;
	}

	return 0;

err:
	dev_err(&pdev->dev, "Probe Failed!\n");
	return ret;
}

static int ctc5236_mc_remove(struct platform_device *pdev)
{
	struct ctc5236_mc *mci = platform_get_drvdata(pdev);

	devm_iounmap(&pdev->dev, mci->base);
	devm_free_irq(&pdev->dev, mci->irq, mci);

	kfree(mci);
	return 0;
}

static struct platform_driver ctc5236_mc_driver = {
	.probe = ctc5236_mc_probe,
	.remove = ctc5236_mc_remove,
	.driver = {
		   .name = "ctc5236_mc",
		   .of_match_table = ctc5236_ddr_ctrl_of_match,
		   },
};

static int __init ctc5236_mc_init(void)
{
	return platform_driver_register(&ctc5236_mc_driver);
}

static void __exit ctc5236_mc_exit(void)
{
	platform_driver_unregister(&ctc5236_mc_driver);
}

module_init(ctc5236_mc_init);
module_exit(ctc5236_mc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Centec Network");
MODULE_DESCRIPTION("Centec TsingMa memory controller driver");
