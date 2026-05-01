/*
 * xhci-aeoliat.c - xHCI host controller driver for Aeolia (Sony PS4)
 *
 * Borrows code from xhci-pci.c, hcd-pci.c, and xhci-plat.c.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

//#define DEBUG
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/module.h>
#include "xhci-aeolia.h"
#include <asm/ps4.h>

#include "xhci.h"

static struct hc_driver __read_mostly xhci_aeolia_hc_driver;

#define NR_DEVICES 3

struct aeolia_xhci {
	int nr_irqs;
	struct usb_hcd *hcd[NR_DEVICES];
};

static int xhci_aeolia_setup(struct usb_hcd *hcd);

static const struct xhci_driver_overrides xhci_aeolia_overrides __initconst = {
	.reset = xhci_aeolia_setup,
};

struct xhci_aeolia_caps {
	u32 capbase;
	u32 hcs_params1;
	u32 hcs_params2;
	u32 hcc_params;
	u32 page_size;
};

static bool xhci_aeolia_is_baikal(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);

	return pdev->device == PCI_DEVICE_ID_SONY_BAIKAL_XHCI;
}

static void xhci_aeolia_read_caps(struct usb_hcd *hcd,
				  struct xhci_aeolia_caps *caps)
{
	struct xhci_cap_regs __iomem *cap = hcd->regs;
	struct xhci_op_regs __iomem *op;

	memset(caps, 0, sizeof(*caps));
	if (!cap)
		return;

	caps->capbase = readl(&cap->hc_capbase);
	caps->hcs_params1 = readl(&cap->hcs_params1);
	caps->hcs_params2 = readl(&cap->hcs_params2);
	caps->hcc_params = readl(&cap->hcc_params);

	op = hcd->regs + HC_LENGTH(caps->capbase);
	caps->page_size = readl(&op->page_size) & XHCI_PAGE_SIZE_MASK;
}

static bool xhci_aeolia_caps_suspicious(const struct xhci_aeolia_caps *caps)
{
	if (!caps->capbase || caps->capbase == ~0u)
		return true;
	if (!caps->hcs_params1 || caps->hcs_params1 == ~0u)
		return true;
	if (!caps->hcc_params || caps->hcc_params == ~0u)
		return true;
	if (!caps->page_size || caps->page_size == XHCI_PAGE_SIZE_MASK)
		return true;
	if (HCS_MAX_PORTS(caps->hcs_params1) > 16)
		return true;
	if (HCS_MAX_INTRS(caps->hcs_params1) > 16)
		return true;
	if (HCS_MAX_SCRATCHPAD(caps->hcs_params2) > 32)
		return true;

	return false;
}

static void xhci_aeolia_log_caps(struct device *dev, const char *tag,
				 const struct xhci_aeolia_caps *caps)
{
	dev_err(dev,
		"%s capbase=%08x hcs1=%08x hcs2=%08x hcc=%08x pagesz=%x ports=%u intrs=%u scratchpad=%u\n",
		tag, caps->capbase, caps->hcs_params1, caps->hcs_params2,
		caps->hcc_params, caps->page_size,
		HCS_MAX_PORTS(caps->hcs_params1),
		HCS_MAX_INTRS(caps->hcs_params1),
		HCS_MAX_SCRATCHPAD(caps->hcs_params2));
}

static void xhci_aeolia_quirks(struct device *dev, struct xhci_hcd *xhci)
{
	/*
	 * Do not try to enable MSIs, we provide the MSIs ourselves
	 * Do not touch DMA mask, we need a custom one
	 */
	xhci->quirks |= XHCI_PLAT | XHCI_PLAT_DMA;
}

/* called during probe() after chip reset completes */
static int xhci_aeolia_setup(struct usb_hcd *hcd)
{
	struct device *dev = hcd->self.controller;
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	struct xhci_aeolia_caps caps;
	int retval;

	xhci->imod_interval = 40000;

	if (xhci_aeolia_is_baikal(dev)) {
		xhci_aeolia_read_caps(hcd, &caps);
		if (xhci_aeolia_caps_suspicious(&caps)) {
			dev_warn(dev,
				 "Baikal xHCI caps not ready, waiting 20ms before setup\n");
			msleep(20);
		}
	}

	retval = xhci_gen_setup(hcd, xhci_aeolia_quirks);
	if (!retval)
		return 0;

	if (retval == -ENOMEM && xhci_aeolia_is_baikal(dev)) {
		xhci_aeolia_read_caps(hcd, &caps);
		if (xhci_aeolia_caps_suspicious(&caps)) {
			dev_warn(dev,
				 "Baikal xHCI setup saw unstable caps, retrying: capbase=%08x hcs1=%08x hcs2=%08x hcc=%08x pagesz=%x ports=%u intrs=%u scratchpad=%u\n",
				 caps.capbase, caps.hcs_params1, caps.hcs_params2,
				 caps.hcc_params, caps.page_size,
				 HCS_MAX_PORTS(caps.hcs_params1),
				 HCS_MAX_INTRS(caps.hcs_params1),
				 HCS_MAX_SCRATCHPAD(caps.hcs_params2));
			msleep(20);
			retval = xhci_gen_setup(hcd, xhci_aeolia_quirks);
			if (!retval)
				return 0;
		}
	}

	xhci_aeolia_read_caps(hcd, &caps);
	xhci_aeolia_log_caps(dev, "xHCI setup failed", &caps);
	return retval;
}

static bool xhci_aeolia_has_middle_host(struct pci_dev *dev)
{
	return dev->device == PCI_DEVICE_ID_SONY_AEOLIA_XHCI;
}

static bool xhci_aeolia_skip_index(struct pci_dev *dev, int index)
{
	return !xhci_aeolia_has_middle_host(dev) && index == 1;
}

static int xhci_aeolia_irqnum(struct aeolia_xhci *axhci,
			      struct pci_dev *dev, int index)
{
	/*
	 * Belize/Baikal still use subfunction slots 0/1/2 even though slot 1 is
	 * not an xHCI controller. Keep the sparse numbering so controller 2
	 * continues to use IRQ slot 2 instead of being incorrectly collapsed to
	 * slot 1.
	 */
	if (axhci->nr_irqs <= 1 || index >= axhci->nr_irqs)
		return dev->irq;

	return dev->irq + index;
}

static int xhci_aeolia_probe_one(struct pci_dev *dev, int index)
{
	int retval;
	struct aeolia_xhci *axhci = pci_get_drvdata(dev);
	struct hc_driver *driver = &xhci_aeolia_hc_driver;
	struct usb_hcd *hcd;
	struct xhci_hcd *xhci;
	int irq = xhci_aeolia_irqnum(axhci, dev, index);

	dev_dbg(&dev->dev, "xhci_aeolia_probe_one %d controller %04x\n",
		index, dev->device);

	hcd = usb_create_hcd(driver, &dev->dev, pci_name(dev));
	pci_set_drvdata(dev, axhci); /* usb_create_hcd clobbers this */
	if (!hcd)
		return -ENOMEM;

	hcd->rsrc_start = pci_resource_start(dev, 2 * index);
	hcd->rsrc_len = pci_resource_len(dev, 2 * index);
	if (!devm_request_mem_region(&dev->dev, hcd->rsrc_start, hcd->rsrc_len,
			driver->description)) {
		dev_dbg(&dev->dev, "controller already in use\n");
		retval = -EBUSY;
		goto put_hcd;
	}
	hcd->regs = pci_ioremap_bar(dev, 2 * index);
	if (hcd->regs == NULL) {
		dev_dbg(&dev->dev, "error mapping memory\n");
		retval = -EFAULT;
		goto release_mem_region;
	}

	device_wakeup_enable(hcd->self.controller);

	xhci = hcd_to_xhci(hcd);
	xhci->main_hcd = hcd;
	xhci->shared_hcd = usb_create_shared_hcd(driver, &dev->dev,
			pci_name(dev), hcd);
	if (!xhci->shared_hcd) {
		retval = -ENOMEM;
		goto unmap_registers;
	}

	retval = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (retval)
		goto put_usb3_hcd;

	retval = usb_add_hcd(xhci->shared_hcd, irq, IRQF_SHARED);
	if (retval)
		goto dealloc_usb2_hcd;

	axhci->hcd[index] = hcd;

	return 0;

dealloc_usb2_hcd:
	usb_remove_hcd(hcd);
put_usb3_hcd:
	usb_put_hcd(xhci->shared_hcd);
unmap_registers:
	iounmap(hcd->regs);
release_mem_region:
	devm_release_mem_region(&dev->dev, hcd->rsrc_start, hcd->rsrc_len);
put_hcd:
	usb_put_hcd(hcd);
	dev_err(&dev->dev, "init %s(%d) fail, %d\n",
			pci_name(dev), index, retval);
	return retval;
}

static void xhci_aeolia_remove_one(struct pci_dev *dev, int index)
{
	struct aeolia_xhci *axhci = pci_get_drvdata(dev);
	struct usb_hcd *hcd = axhci->hcd[index];
	struct xhci_hcd *xhci;

	if (!hcd)
		return;
	xhci = hcd_to_xhci(hcd);

	usb_remove_hcd(xhci->shared_hcd);
	usb_remove_hcd(hcd);
	usb_put_hcd(xhci->shared_hcd);
	iounmap(hcd->regs);
	usb_put_hcd(hcd);

	axhci->hcd[index] = NULL;
}

static int xhci_aeolia_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int idx;
	int retval;
	struct aeolia_xhci *axhci;

	if (apcie_status() == 0)
		return -EPROBE_DEFER;

	if (pci_enable_device(dev) < 0)
		return -ENODEV;

	//axhci = kzalloc(sizeof(*axhci), GFP_KERNEL);
	axhci = devm_kzalloc(&dev->dev, sizeof(*axhci), GFP_KERNEL);
	if (!axhci) {
		retval = -ENOMEM;
		goto disable_device;
	}
	pci_set_drvdata(dev, axhci);

	axhci->nr_irqs = retval = apcie_assign_irqs(dev, NR_DEVICES);
	if (retval < 0) {
		goto free_axhci;
	}

	retval = dma_set_mask(&dev->dev, DMA_BIT_MASK(31));
	if (retval)
		goto free_irqs;
	retval = dma_set_coherent_mask(&dev->dev, DMA_BIT_MASK(31));
	if (retval)
		goto free_irqs;

	pci_set_master(dev);

	for (idx = 0; idx < NR_DEVICES; idx++) {
		if (xhci_aeolia_skip_index(dev, idx))
			continue;
		retval = xhci_aeolia_probe_one(dev, idx);
		if (retval)
			goto remove_hcds;
	}

	return 0;

remove_hcds:
	while (idx--)
		if (!xhci_aeolia_skip_index(dev, idx))
			xhci_aeolia_remove_one(dev, idx);
free_irqs:
	apcie_free_irqs(dev->irq, axhci->nr_irqs);
free_axhci:
	devm_kfree(&dev->dev, axhci);
	pci_set_drvdata(dev, NULL);
disable_device:
	pci_disable_device(dev);
	return retval;
}

static void xhci_aeolia_remove(struct pci_dev *dev)
{
	int idx;
	struct aeolia_xhci *axhci = pci_get_drvdata(dev);

	if (!axhci)
		return;

	for (idx = 0; idx < NR_DEVICES; idx++) {
		if (!xhci_aeolia_skip_index(dev, idx))
			xhci_aeolia_remove_one(dev, idx);
	}

	apcie_free_irqs(dev->irq, axhci->nr_irqs);
	pci_disable_device(dev);
}

static void xhci_hcd_pci_shutdown(struct pci_dev *dev)
{
	struct aeolia_xhci *axhci;
	struct usb_hcd		*hcd;
	int idx;

	axhci = pci_get_drvdata(dev);
	if (!axhci)
		return;

	for (idx = 0; idx < NR_DEVICES; idx++) {
		if (xhci_aeolia_skip_index(dev, idx))
			continue;

		hcd = axhci->hcd[idx];
		if (hcd &&
		    test_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags) &&
		    hcd->driver->shutdown) {
			hcd->driver->shutdown(hcd);
		}
	}
}
 
static const struct pci_device_id pci_ids[] = {
		{ PCI_DEVICE(PCI_VENDOR_ID_SONY, PCI_DEVICE_ID_SONY_AEOLIA_XHCI) },
		{ PCI_DEVICE(PCI_VENDOR_ID_SONY, PCI_DEVICE_ID_SONY_BELIZE_XHCI) },
		{ PCI_DEVICE(PCI_VENDOR_ID_SONY, PCI_DEVICE_ID_SONY_BAIKAL_XHCI) },
	{ /* end: all zeroes */ }
};
MODULE_DEVICE_TABLE(pci, pci_ids);

#ifdef CONFIG_PM_SLEEP
static int xhci_aeolia_suspend(struct device *dev)
{
	int idx;
	struct aeolia_xhci *axhci = dev_get_drvdata(dev);
	struct xhci_hcd	*xhci;
	int retval;
	struct pci_dev		*pdev = to_pci_dev(dev);
	
	for (idx = 0; idx < NR_DEVICES; idx++) {
		if(pdev->device != PCI_DEVICE_ID_SONY_AEOLIA_XHCI && idx == 1)
			continue;
		xhci = hcd_to_xhci(axhci->hcd[idx]);
		retval = xhci_suspend(xhci, device_may_wakeup(dev));
		if (retval < 0)
			goto resume;
	}
	return 0;

resume:
	while (idx--) {
		if (xhci_aeolia_skip_index(pdev, idx) || !axhci->hcd[idx])
			continue;
		xhci = hcd_to_xhci(axhci->hcd[idx]);
		xhci_resume(xhci, false, false);
	}
	return retval;
}

static int xhci_aeolia_resume(struct device *dev)
{
	int idx;
	struct aeolia_xhci *axhci = dev_get_drvdata(dev);
	struct xhci_hcd	*xhci;
	int retval;
	struct pci_dev		*pdev = to_pci_dev(dev);

	for (idx = 0; idx < NR_DEVICES; idx++) {
		if (xhci_aeolia_skip_index(pdev, idx) || !axhci->hcd[idx])
			continue;
		xhci = hcd_to_xhci(axhci->hcd[idx]);
		retval = xhci_resume(xhci, false, false);
		if (retval < 0)
			return retval;
	}
	return 0;
}

static const struct dev_pm_ops xhci_aeolia_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(xhci_aeolia_suspend, xhci_aeolia_resume)
};
#endif

/* pci driver glue; this is a "new style" PCI driver module */
static struct pci_driver xhci_aeolia_driver = {
	.name =		"xhci_aeolia",
	.id_table =	pci_ids,

	.probe =	xhci_aeolia_probe,
	.remove =	xhci_aeolia_remove,
	.shutdown = 	xhci_hcd_pci_shutdown,
#ifdef CONFIG_PM_SLEEP
	.driver = {
		.pm = &xhci_aeolia_pm_ops
	},
#endif
};

static int __init xhci_aeolia_init(void)
{
	xhci_init_driver(&xhci_aeolia_hc_driver, &xhci_aeolia_overrides);
	return pci_register_driver(&xhci_aeolia_driver);
}
module_init(xhci_aeolia_init);

static void __exit xhci_aeolia_exit(void)
{
	pci_unregister_driver(&xhci_aeolia_driver);
}
module_exit(xhci_aeolia_exit);

MODULE_DESCRIPTION("xHCI Aeolia Host Controller Driver");
MODULE_LICENSE("GPL");
