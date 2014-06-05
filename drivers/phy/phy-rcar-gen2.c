/*
 * Renesas R-Car Gen2 PHY driver
 *
 * Copyright (C) 2014 Renesas Solutions Corp.
 * Copyright (C) 2014 Cogent Embedded, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#define USBHS_LPSTS			0x02
#define USBHS_UGCTRL			0x80
#define USBHS_UGCTRL2			0x84
#define USBHS_UGSTS			0x88

/* Low Power Status register (LPSTS) */
#define USBHS_LPSTS_SUSPM		0x4000

/* USB General control register (UGCTRL) */
#define USBHS_UGCTRL_CONNECT		0x00000004
#define USBHS_UGCTRL_PLLRESET		0x00000001

/* USB General control register 2 (UGCTRL2) */
#define USBHS_UGCTRL2_USB2SEL		0x80000000
#define USBHS_UGCTRL2_USB2SEL_PCI	0x00000000
#define USBHS_UGCTRL2_USB2SEL_USB30	0x80000000
#define USBHS_UGCTRL2_USB0SEL		0x00000030
#define USBHS_UGCTRL2_USB0SEL_PCI	0x00000010
#define USBHS_UGCTRL2_USB0SEL_HS_USB	0x00000030

/* USB General status register (UGSTS) */
#define USBHS_UGSTS_LOCK		0x00000300

#define	NUM_USB_CHANNELS	2

struct rcar_gen2_phy {
	struct phy *phy;
	struct rcar_gen2_phy_driver *drv;
	u32 select_mask;
	u32 select_value;
};

struct rcar_gen2_phy_driver {
	void __iomem *base;
	struct clk *clk;
	spinlock_t lock;
	struct rcar_gen2_phy phys[NUM_USB_CHANNELS][2];
};

static int rcar_gen2_phy_init(struct phy *p)
{
	struct rcar_gen2_phy *phy = phy_get_drvdata(p);
	struct rcar_gen2_phy_driver *drv = phy->drv;
	unsigned long flags;
	u32 ugctrl2;

	clk_prepare_enable(drv->clk);

	spin_lock_irqsave(&drv->lock, flags);
	ugctrl2 = readl(drv->base + USBHS_UGCTRL2);
	ugctrl2 &= ~phy->select_mask;
	ugctrl2 |= phy->select_value;
	writel(ugctrl2, drv->base + USBHS_UGCTRL2);
	spin_unlock_irqrestore(&drv->lock, flags);
	return 0;
}

static int rcar_gen2_phy_exit(struct phy *p)
{
	struct rcar_gen2_phy *phy = phy_get_drvdata(p);

	clk_disable_unprepare(phy->drv->clk);

	return 0;
}

static int rcar_gen2_phy_power_on(struct phy *p)
{
	struct rcar_gen2_phy *phy = phy_get_drvdata(p);
	struct rcar_gen2_phy_driver *drv = phy->drv;
	void __iomem *base = drv->base;
	unsigned long flags;
	u32 value;
	int err = 0, i;

	/* Skip if it's not USBHS */
	if (phy->select_value != USBHS_UGCTRL2_USB0SEL_HS_USB)
		return 0;

	spin_lock_irqsave(&drv->lock, flags);

	/* Power on USBHS PHY */
	value = readl(base + USBHS_UGCTRL);
	value &= ~USBHS_UGCTRL_PLLRESET;
	writel(value, base + USBHS_UGCTRL);

	value = readw(base + USBHS_LPSTS);
	value |= USBHS_LPSTS_SUSPM;
	writew(value, base + USBHS_LPSTS);

	for (i = 0; i < 20; i++) {
		value = readl(base + USBHS_UGSTS);
		if ((value & USBHS_UGSTS_LOCK) == USBHS_UGSTS_LOCK) {
			value = readl(base + USBHS_UGCTRL);
			value |= USBHS_UGCTRL_CONNECT;
			writel(value, base + USBHS_UGCTRL);
			goto out;
		}
		udelay(1);
	}

	/* Timed out waiting for the PLL lock */
	err = -ETIMEDOUT;

out:
	spin_unlock_irqrestore(&drv->lock, flags);

	return err;
}

static int rcar_gen2_phy_power_off(struct phy *p)
{
	struct rcar_gen2_phy *phy = phy_get_drvdata(p);
	struct rcar_gen2_phy_driver *drv = phy->drv;
	void __iomem *base = drv->base;
	unsigned long flags;
	u32 value;

	/* Skip if it's not USBHS */
	if (phy->select_value != USBHS_UGCTRL2_USB0SEL_HS_USB)
		return 0;

	spin_lock_irqsave(&drv->lock, flags);

	/* Power off USBHS PHY */
	value = readl(base + USBHS_UGCTRL);
	value &= ~USBHS_UGCTRL_CONNECT;
	writel(value, base + USBHS_UGCTRL);

	value = readw(base + USBHS_LPSTS);
	value &= ~USBHS_LPSTS_SUSPM;
	writew(value, base + USBHS_LPSTS);

	value = readl(base + USBHS_UGCTRL);
	value |= USBHS_UGCTRL_PLLRESET;
	writel(value, base + USBHS_UGCTRL);

	spin_unlock_irqrestore(&drv->lock, flags);

	return 0;
}

static struct phy_ops rcar_gen2_phy_ops = {
	.init		= rcar_gen2_phy_init,
	.exit		= rcar_gen2_phy_exit,
	.power_on	= rcar_gen2_phy_power_on,
	.power_off	= rcar_gen2_phy_power_off,
	.owner		= THIS_MODULE,
};

static const struct of_device_id rcar_gen2_phy_match_table[] = {
	{ .compatible = "renesas,usb-phy-r8a7790" },
	{ .compatible = "renesas,usb-phy-r8a7791" },
	{ }
};
MODULE_DEVICE_TABLE(of, rcar_gen2_phy_match_table);

static struct phy *rcar_gen2_phy_xlate(struct device *dev,
				       struct of_phandle_args *args)
{
	struct rcar_gen2_phy_driver *drv;

	drv = dev_get_drvdata(dev);
	if (!drv)
		return ERR_PTR(-EINVAL);

	if (args->args[0] >= NUM_USB_CHANNELS || args->args[1] >= 2)
		return ERR_PTR(-ENODEV);

	return drv->phys[args->args[0]][args->args[1]].phy;
}

static int rcar_gen2_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rcar_gen2_phy_driver *drv;
	struct phy_provider *provider;
	struct device_node *np;
	struct resource *res;
	void __iomem *base;
	struct clk *clk;

	if (!dev->of_node) {
		dev_err(dev,
			"This driver is required to be instantiated from device tree\n");
		return -EINVAL;
	}

	clk = devm_clk_get(dev, "usbhs");
	if (IS_ERR(clk)) {
		dev_err(dev, "Can't get USBHS clock\n");
		return PTR_ERR(clk);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	drv = devm_kzalloc(dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	spin_lock_init(&drv->lock);

	drv->clk = clk;
	drv->base = base;

	for_each_child_of_node(dev->of_node, np) {
		struct rcar_gen2_phy *phy;
		u32 values[2];
		int error;

		error = of_property_read_u32_array(np, "renesas,phy-select",
						   values, 2);
		if (error) {
			dev_err(dev, "Failed to read \"renesas,phy-select\" property\n");
			return error;
		}
		if (values[0] >= NUM_USB_CHANNELS || values[1] >= 2) {
			dev_err(dev, "Values out of range in \"renesas,phy-select\" property\n");
			return error;
		}
		phy = &drv->phys[values[0]][values[1]];

		error = of_property_read_u32_array(np, "renesas,ugctrl2-masks",
						   values, 2);
		if (error) {
			dev_err(dev, "Failed to read \"renesas,ugctrl2-masks\" property\n");
			return error;
		}
		phy->select_mask  = values[0];
		phy->select_value = values[1];

		phy->phy = devm_phy_create(dev, &rcar_gen2_phy_ops, NULL);
		if (IS_ERR(phy->phy)) {
			dev_err(dev, "Failed to create PHY\n");
			return PTR_ERR(phy->phy);
		}

		phy->drv = drv;
		phy_set_drvdata(phy->phy, phy);
	}

	provider = devm_of_phy_provider_register(dev, rcar_gen2_phy_xlate);
	if (IS_ERR(provider)) {
		dev_err(dev, "Failed to register PHY provider\n");
		return PTR_ERR(provider);
	}

	dev_set_drvdata(dev, drv);

	return 0;
}

static struct platform_driver rcar_gen2_phy_driver = {
	.driver = {
		.name		= "phy_rcar_gen2",
		.of_match_table	= rcar_gen2_phy_match_table,
	},
	.probe	= rcar_gen2_phy_probe,
};

module_platform_driver(rcar_gen2_phy_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Renesas R-Car Gen2 PHY");
MODULE_AUTHOR("Sergei Shtylyov <sergei.shtylyov@cogentembedded.com>");
