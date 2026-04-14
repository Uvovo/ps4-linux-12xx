/*
 * xhci-aeoliat.c - xHCI host controller driver for Aeolia (Sony PS4)
 *
 * Borrows code from xhci-pci.c, hcd-pci.c, and xhci-plat.c.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/acpi.h>
#include <asm/ps4.h>
#include "xhci-aeolia.h"
#include "../../ps4/aeolia.h"

#include "xhci.h"

static const char hcd_name[] = "xhci_aeolia";

static struct hc_driver __read_mostly xhci_aeolia_hc_driver;

#define NR_DEVICES 3

struct aeolia_xhci {
	struct ata_host *host;
	int nr_irqs;
	struct usb_hcd *hcd[NR_DEVICES];
};

static struct pci_dev *ps4_xhci_get_glue_device(struct pci_dev *pdev)
{
	unsigned int glue_devfn = (pdev->devfn & ~7) | AEOLIA_FUNC_ID_PCIE;

	return pci_get_slot(pdev->bus, glue_devfn);
}

/*
 * Prefer the glue function so Baikal model detection stays anchored to the
 * southbridge glue device instead of whichever child function probed first.
 */
static const struct ps4_sb_desc *ps4_xhci_desc(struct pci_dev *pdev)
{
	const struct ps4_sb_desc *desc = NULL;
	struct pci_dev *glue_pdev = ps4_xhci_get_glue_device(pdev);

	if (glue_pdev) {
		desc = ps4_sb_desc_by_device(glue_pdev->device);
		pci_dev_put(glue_pdev);
	}

	return desc ? desc : ps4_sb_desc_by_device(pdev->device);
}

static inline enum ps4_sb_model ps4_xhci_model(struct pci_dev *pdev)
{
	const struct ps4_sb_desc *desc = ps4_xhci_desc(pdev);

	return desc ? desc->model : PS4_SB_MODEL_UNKNOWN;
}

static inline bool ps4_xhci_has_ahci(struct pci_dev *pdev)
{
	const struct ps4_sb_desc *desc = ps4_xhci_desc(pdev);

	return desc && desc->xhci_has_ahci;
}

static inline bool ps4_xhci_has_middle_host(struct pci_dev *pdev)
{
	const struct ps4_sb_desc *desc = ps4_xhci_desc(pdev);

	return desc && desc->xhci_has_middle_host;
}

static int xhci_aeolia_setup(struct usb_hcd *hcd);

static const struct xhci_driver_overrides xhci_aeolia_overrides __initconst = {
	.extra_priv_size = sizeof(struct xhci_hcd),
	.reset = xhci_aeolia_setup,
};

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
	return xhci_gen_setup(hcd, xhci_aeolia_quirks);
}

static int xhci_aeolia_probe_one(struct pci_dev *dev, int index)
{
	int retval;
	struct aeolia_xhci *axhci = pci_get_drvdata(dev);
	struct hc_driver *driver = &xhci_aeolia_hc_driver;
	struct usb_hcd *hcd;
	struct xhci_hcd *xhci;
	int irq = (axhci->nr_irqs > 1) ? (dev->irq + index) : dev->irq;

	// ok...adding this printk appears to have introduced a delay that fixed
	// bringup of the middle host controller, so w/e for now...

	printk("xhci_aeolia_probe_one %d, controller is %x\n", index, dev->device);

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

#define DRV_VERSION	"3.0"
#define DRV_NAME	"ahci"
static const struct ata_port_info ahci_port_info = {
	AHCI_HFLAGS	(AHCI_HFLAG_31BIT_ONLY),
	.flags		= AHCI_FLAG_COMMON,
	.pio_mask	= ATA_PIO4,
	.udma_mask	= ATA_UDMA6,
	.port_ops	= &ahci_ops,
};

static struct scsi_host_template ahci_sht = {
	AHCI_SHT(DRV_NAME),
};

static bool bus_master;
static int ahci_init_one(struct pci_dev *pdev)
{
	struct f_resource* r_mem;
	struct ahci_controller* ctlr;
	struct aeolia_xhci *axhci = pci_get_drvdata(pdev);
	struct ata_port_info pi = ahci_port_info;
	const struct ata_port_info *ppi[] = { &pi, NULL };
	struct ahci_host_priv *hpriv;
      struct ata_host *host;
	int n_ports, i, rc;
	int ahci_pci_bar = 2;
	resource_size_t		rsrc_start;
	resource_size_t		rsrc_len;

	dev_dbg(&pdev->dev, "ENTER \n");

	WARN_ON((int)ATA_MAX_QUEUE > AHCI_MAX_CMDS);

	ata_print_version_once(&pdev->dev, DRV_VERSION);

	/*
	rc = pcim_iomap_regions_request_all(pdev, 1 << ahci_pci_bar, DRV_NAME);
	if (rc == -EBUSY)
		pcim_pin_device(pdev);
	if (rc)
		return rc;
	*/

	hpriv = devm_kzalloc(&pdev->dev, sizeof(*hpriv), GFP_KERNEL);
	//hpriv = kzalloc(sizeof(*hpriv), GFP_KERNEL);
	if (!hpriv)
		return -ENOMEM;
	hpriv->flags |= (unsigned long)pi.private_data;

	//hpriv->mmio = pcim_iomap_table(pdev)[ahci_pci_bar];
	rsrc_start = pci_resource_start(pdev, ahci_pci_bar);
	rsrc_len = pci_resource_len(pdev, ahci_pci_bar);
	//if (!request_mem_region(rsrc_start, rsrc_len, "xhci-ahci.mem")) {
	if (!devm_request_mem_region(&pdev->dev, rsrc_start, rsrc_len, "xhci-ahci.mem")) {
		dev_dbg(&pdev->dev, "controller already in use\n");
		rc = -EBUSY;
		goto put_hpriv;
	}


	hpriv->mmio = pci_ioremap_bar(pdev, ahci_pci_bar);
	if (hpriv->mmio == NULL) {
		dev_dbg(&pdev->dev, "error mapping memory\n");
		rc = -EFAULT;
		goto release_mem_region;
	}

	r_mem = kzalloc(sizeof(*r_mem), GFP_KERNEL);
	if (r_mem) {
		r_mem->r_bustag = 1;//mem
		r_mem->r_bushandle = hpriv->mmio;

			ctlr = kzalloc(sizeof(*ctlr), GFP_KERNEL);
			if (ctlr) {
				ctlr->r_mem = r_mem;
				ctlr->dev_id = ps4_xhci_model(pdev) ==
					       PS4_SB_MODEL_BAIKAL ?
					       0x90d9104d : 0x90ca104d;
				ctlr->trace_len = 6;
				bpcie_sata_phy_init(&pdev->dev, ctlr);
			kfree(ctlr);
		}
		kfree(r_mem);
	}
	device_wakeup_enable(&pdev->dev);

	/* save initial config */
	ahci_save_initial_config(&pdev->dev, hpriv);

	/* prepare host */
	if (hpriv->cap & HOST_CAP_NCQ) {
		pi.flags |= ATA_FLAG_NCQ;
		/*
		 * Auto-activate optimization is supposed to be
		 * supported on all AHCI controllers indicating NCQ
		 * capability, but it seems to be broken on some
		 * chipsets including NVIDIAs.
		 */
		if (!(hpriv->flags & AHCI_HFLAG_NO_FPDMA_AA))
			pi.flags |= ATA_FLAG_FPDMA_AA;

		/*
		 * All AHCI controllers should be forward-compatible
		 * with the new auxiliary field. This code should be
		 * conditionalized if any buggy AHCI controllers are
		 * encountered.
		 */
		pi.flags |= ATA_FLAG_FPDMA_AUX;
	}

	if (hpriv->cap & HOST_CAP_PMP)
		pi.flags |= ATA_FLAG_PMP;

	ahci_set_em_messages(hpriv, &pi);

	/* CAP.NP sometimes indicate the index of the last enabled
	 * port, at other times, that of the last possible port, so
	 * determining the maximum port number requires looking at
	 * both CAP.NP and port_map.
	 */
	n_ports = max(ahci_nr_ports(hpriv->cap), fls(hpriv->port_map));

	host = ata_host_alloc_pinfo(&pdev->dev, ppi, n_ports);
	if (!host) {
		rc = -ENOMEM;
		goto unmap_registers;
	}
	axhci->host = host;
	pci_set_drvdata(pdev, axhci);

	host->private_data = hpriv;

	{
		int index = 1;
		int irq = (axhci->nr_irqs > 1) ? (pdev->irq + index) : pdev->irq;
		hpriv->irq = irq;
	}

	if (!(hpriv->cap & HOST_CAP_SSS) || ahci_ignore_sss)
		host->flags |= ATA_HOST_PARALLEL_SCAN;
	else
		dev_info(&pdev->dev, "SSS flag set, parallel bus scan disabled\n");

	if (pi.flags & ATA_FLAG_EM)
		ahci_reset_em(host);

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];

		ata_port_pbar_desc(ap, ahci_pci_bar, -1, "abar");
		ata_port_pbar_desc(ap, ahci_pci_bar,
				   0x100 + ap->port_no * 0x80, "port");

		/* set enclosure management message type */
		if (ap->flags & ATA_FLAG_EM)
			ap->em_message_type = hpriv->em_msg_type;


		/* disabled/not-implemented port */
		if (!(hpriv->port_map & (1 << i)))
			ap->ops = &ata_dummy_port_ops;
	}

	rc = ahci_reset_controller(host);
	dev_dbg(&pdev->dev, "ahci_reset_controller returned %d\n", rc);
	if (rc)
		goto release_host;

	ahci_init_controller(host);
	ahci_print_info(host, "ATA");

	if (!bus_master) {
		pci_set_master(pdev);
		bus_master = true;
	}

	rc = ahci_host_activate(host, &ahci_sht);
	dev_dbg(&pdev->dev, "ahci_host_activate returned %d\n", rc);
	if (rc) {
		goto host_deactivate;
	}

	pm_runtime_put_noidle(&pdev->dev);
	return 0;

	host_deactivate:
	release_host:
	unmap_registers:
		iounmap(hpriv->mmio);
	release_mem_region:
		devm_release_mem_region(&pdev->dev, rsrc_start, rsrc_len);
	put_hpriv:
		//kfree(hpriv);
		devm_kfree(&pdev->dev, hpriv);
		dev_err(&pdev->dev, "init %s fail, %d\n",
				pci_name(pdev), rc);
	return rc;
}

static void ahci_remove_one(struct pci_dev *pdev)
{
	pm_runtime_get_noresume(&pdev->dev);

	struct aeolia_xhci *axhci = pci_get_drvdata(pdev);
	if (axhci && axhci->host) {
		ata_host_detach(axhci->host);
		struct ahci_host_priv *hpriv = axhci->host->private_data;
		if (hpriv) {
			iounmap(hpriv->mmio);
		}
		axhci->host = NULL;
	}
}

static int xhci_aeolia_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int idx;
	int retval;
	int southbridge_status;
	int full_status;
	struct aeolia_xhci *axhci;
	const struct ps4_sb_desc *desc;
	struct pci_dev *glue_pdev;

	desc = ps4_xhci_desc(dev);
	glue_pdev = ps4_xhci_get_glue_device(dev);
	southbridge_status = apcie_irq_domain_status();
	full_status = apcie_status();
	dev_info(&dev->dev,
		 "xhci IRQ setup: child_device=%#06x model=%s glue=%s/%#06x irq_ready=%d full_ready=%d\n",
		 dev->device, desc ? desc->name : "unknown",
		 glue_pdev ? pci_name(glue_pdev) : "<none>",
		 glue_pdev ? glue_pdev->device : 0,
		 southbridge_status, full_status);
	if (glue_pdev)
		pci_dev_put(glue_pdev);

	if (southbridge_status == 0) {
		dev_info(&dev->dev,
			 "xhci IRQ setup: deferring until southbridge is ready\n");
		return -EPROBE_DEFER;
	}

	if (pci_enable_device(dev) < 0)
		return -ENODEV;

	axhci = devm_kzalloc(&dev->dev, sizeof(*axhci), GFP_KERNEL);
	if (!axhci) {
		retval = -ENOMEM;
		goto disable_device;
	}
	pci_set_drvdata(dev, axhci);

	dev_info(&dev->dev,
		 "xhci IRQ setup: requesting %d vectors via apcie_assign_irqs\n",
		 NR_DEVICES);
	axhci->nr_irqs = retval = apcie_assign_irqs(dev, NR_DEVICES);
	dev_info(&dev->dev,
		 "xhci IRQ setup: apcie_assign_irqs returned %d irq=%d\n",
		 retval, retval > 0 ? dev->irq : 0);
	if (retval < 0) {
		goto free_axhci;
	}

	if (!ps4_xhci_has_ahci(dev)) {
		pci_set_master(dev);
	}

	if (dma_set_mask_and_coherent(&dev->dev, DMA_BIT_MASK(31))) {
		return -ENODEV;
	}

	if (ps4_xhci_has_ahci(dev)) {
		retval = ahci_init_one(dev);
		dev_dbg(&dev->dev, "ahci_init_one returned %d", retval);
		if (!bus_master) {
			pci_set_master(dev);
			bus_master = true;
		}
	}

	for (idx = 0; idx < NR_DEVICES; idx++) {
		if (!ps4_xhci_has_middle_host(dev) && idx == 1) {
			continue;
		}
		retval = xhci_aeolia_probe_one(dev, idx);
		if (retval)
			goto remove_hcds;
	}

	return 0;

remove_hcds:
	while (idx--)
		xhci_aeolia_remove_one(dev, idx);
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
		if (!ps4_xhci_has_middle_host(dev)) {
			if (idx != 1)
				xhci_aeolia_remove_one(dev, idx);
			else if (ps4_xhci_has_ahci(dev))
				ahci_remove_one(dev);
		} else {
			xhci_aeolia_remove_one(dev, idx);
		}
	}

	apcie_free_irqs(dev->irq, axhci->nr_irqs);

	// TODO (ps4patches): Belize, remove in ahci commit
	kfree(axhci);

	pci_disable_device(dev);
}

static void xhci_hcd_pci_shutdown(struct pci_dev *dev){

	if (!ps4_xhci_has_ahci(dev)) {
		usb_hcd_pci_shutdown(dev);
		return;
	}

	struct aeolia_xhci *axhci;
	struct usb_hcd	   *hcd;
	int idx;

	axhci = pci_get_drvdata(dev);
	if (!axhci)
		return;

	for (idx = 0; idx < NR_DEVICES; idx++) {
		if (!ps4_xhci_has_middle_host(dev)) {
			if (idx != 1) {
				hcd = axhci->hcd[idx];
				if (hcd) {
					if (test_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags) && hcd->driver->shutdown) {
						hcd->driver->shutdown(hcd);
						if (usb_hcd_is_primary_hcd(hcd) && hcd->irq > 0)
							free_irq(hcd->irq, hcd);
					}
				}
			}
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
		if (!ps4_xhci_has_middle_host(pdev) && idx == 1)
			continue;
		xhci = hcd_to_xhci(axhci->hcd[idx]);
		retval = xhci_suspend(xhci, device_may_wakeup(dev));
		if (retval < 0)
			goto resume;
	}
	return 0;

resume:
	while (idx--) {
		xhci = hcd_to_xhci(axhci->hcd[idx]);
		xhci_resume(xhci, 0);
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
		if (!ps4_xhci_has_middle_host(pdev) && idx == 1)
			continue;
		xhci = hcd_to_xhci(axhci->hcd[idx]);
		retval = xhci_resume(xhci, 0);
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
	/* suspend and resume implemented later */
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
