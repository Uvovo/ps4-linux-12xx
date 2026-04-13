#define DEBUG

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/irqdesc.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/pci.h>
#include <asm/apic.h>
#include <asm/irqdomain.h>
#include <asm/msi.h>
#include <asm/ps4.h>

#include "baikal.h"

/* #define QEMU_HACK_NO_IOMMU */

static const int subfuncs_per_func[BAIKAL_NUM_FUNCS] = {
	BPCIE_SUBFUNCS_FUNC0,
	BPCIE_SUBFUNCS_FUNC1,
	BPCIE_SUBFUNCS_FUNC2,
	BPCIE_SUBFUNCS_FUNC3,
	BPCIE_SUBFUNCS_FUNC4,
	BPCIE_SUBFUNCS_FUNC5,
	BPCIE_SUBFUNCS_FUNC6,
	BPCIE_SUBFUNCS_FUNC7,
};

static struct msi_domain_info bpcie_msi_domain_info;

bool bpcie_initialized;

static inline u32 glue_read32(struct bpcie_dev *sc, u32 offset)
{
	return ioread32(sc->bar2 + offset);
}

static inline void glue_write32(struct bpcie_dev *sc, u32 offset, u32 value)
{
	iowrite32(value, sc->bar2 + offset);
}

static inline u8 bpcie_hwirq_subfunc(irq_hw_number_t hwirq)
{
	return hwirq & 0x1f;
}

static struct pci_dev *bpcie_get_glue_device(struct pci_dev *dev)
{
	unsigned int devfn = (dev->devfn & ~7) | BAIKAL_FUNC_ID_PCIE;

	return pci_get_slot(dev->bus, devfn);
}

static void bpcie_msi_write_msg(struct irq_data *data, struct msi_msg *msg)
{
	if (!msg->address_lo)
		return;

	pci_write_msi_msg(data->irq, msg);
}

static void bpcie_msi_unmask(struct irq_data *data)
{
	pci_msi_unmask_irq(data);
}

static void bpcie_msi_mask(struct irq_data *data)
{
	pci_msi_mask_irq(data);
}

static void bpcie_irq_msi_compose_msg(struct irq_data *data,
				      struct msi_msg *msg)
{
	struct irq_cfg *cfg = irqd_cfg(data);

	__irq_msi_compose_msg(cfg, msg, false);
}

static void bpcie_handle_edge_irq(struct irq_desc *desc)
{
	struct bpcie_dev *sc = irq_desc_get_handler_data(desc);
	irq_hw_number_t hwirq = desc->irq_data.hwirq;
	irq_hw_number_t base_hwirq = hwirq & ~0x1fULL;
	u32 func = (hwirq >> 5) & 7;
	unsigned int vector_to_write;
	unsigned int mask;
	unsigned int shift;
	unsigned int subfunc_mask;
	unsigned long flags;
	u32 vector_read;
	int i;

	if (!sc) {
		handle_edge_irq(desc);
		return;
	}

	switch (func) {
	case BAIKAL_FUNC_ID_PCIE:
		vector_to_write = 2;
		mask = GENMASK(30, 0);
		shift = 0;
		break;
	case BAIKAL_FUNC_ID_DMAC:
		vector_to_write = 3;
		mask = GENMASK(1, 0);
		shift = 0;
		break;
	case BAIKAL_FUNC_ID_XHCI:
		vector_to_write = 3;
		mask = GENMASK(2, 0);
		shift = 16;
		break;
	default:
		handle_edge_irq(desc);
		return;
	}

	raw_spin_lock_irqsave(&desc->lock, flags);
	glue_write32(sc, BPCIE_ACK_WRITE, vector_to_write);
	vector_read = glue_read32(sc, BPCIE_ACK_READ);
	raw_spin_unlock_irqrestore(&desc->lock, flags);

	subfunc_mask = mask & ~(vector_read >> shift);
	for (i = 0; i < subfuncs_per_func[func]; i++) {
		struct irq_desc *subdesc;

		if (!(subfunc_mask & BIT(i)))
			continue;

		subdesc = irq_to_desc(irq_find_mapping(desc->irq_data.domain,
						       base_hwirq + i));
		if (subdesc)
			handle_edge_irq(subdesc);
	}
}

static void bpcie_msi_set_desc(msi_alloc_info_t *arg, struct msi_desc *desc)
{
	struct pci_dev *dev = msi_desc_to_pci_dev(desc);
	struct pci_dev *glue_pdev;
	struct bpcie_dev *sc;

	arg->desc = desc;
	arg->type = X86_IRQ_ALLOC_TYPE_PCI_MSI;
	arg->hwirq = PCI_FUNC(dev->devfn) << 5;

	glue_pdev = bpcie_get_glue_device(dev);
	sc = glue_pdev ? pci_get_drvdata(glue_pdev) : NULL;
	arg->devid = glue_pdev ? pci_dev_id(glue_pdev) : pci_dev_id(dev);
	arg->data = sc;

#ifndef QEMU_HACK_NO_IOMMU
	if (!(bpcie_msi_domain_info.flags & MSI_FLAG_MULTI_PCI_MSI))
		arg->hwirq |= 0x1f;
	else
#endif
		arg->hwirq |= desc->msi_index;

	if (glue_pdev)
		pci_dev_put(glue_pdev);
}

static struct irq_chip bpcie_msi_controller = {
	.name = "Baikal-MSI",
	.irq_unmask = bpcie_msi_unmask,
	.irq_mask = bpcie_msi_mask,
	.irq_ack = irq_chip_ack_parent,
	.irq_set_affinity = msi_domain_set_affinity,
	.irq_retrigger = irq_chip_retrigger_hierarchy,
	.irq_compose_msi_msg = bpcie_irq_msi_compose_msg,
	.irq_write_msi_msg = bpcie_msi_write_msg,
	.flags = IRQCHIP_SKIP_SET_WAKE | IRQCHIP_AFFINITY_PRE_STARTUP,
};

static struct msi_domain_ops bpcie_msi_domain_ops = {
	.set_desc = bpcie_msi_set_desc,
};

static struct msi_domain_info bpcie_msi_domain_info = {
	.flags = MSI_FLAG_USE_DEF_DOM_OPS | MSI_FLAG_USE_DEF_CHIP_OPS,
	.ops = &bpcie_msi_domain_ops,
	.chip = &bpcie_msi_controller,
	.handler = bpcie_handle_edge_irq,
	.handler_name = "edge",
};

static struct irq_domain *bpcie_create_irq_domain(struct bpcie_dev *sc)
{
	struct irq_domain *domain, *parent;
	struct fwnode_handle *fn;
	struct irq_fwspec fwspec;

	if (!x86_vector_domain)
		return NULL;

	bpcie_msi_domain_info.chip_data = sc;

	fn = irq_domain_alloc_named_id_fwnode(bpcie_msi_controller.name,
					      pci_dev_id(sc->pdev));
	if (!fn)
		return NULL;

	fwspec.fwnode = fn;
	fwspec.param_count = 1;
	fwspec.param[0] = pci_dev_id(sc->pdev);

	parent = irq_find_matching_fwspec(&fwspec, DOMAIN_BUS_ANY);
	if (!parent)
		parent = x86_vector_domain;
	else if (parent != x86_vector_domain)
		bpcie_msi_domain_info.flags |= MSI_FLAG_MULTI_PCI_MSI;

	domain = msi_create_irq_domain(fn, &bpcie_msi_domain_info, parent);
	if (!domain)
		irq_domain_free_fwnode(fn);

	return domain;
}

static int bpcie_is_compatible_device(struct pci_dev *dev)
{
	return dev && dev->vendor == PCI_VENDOR_ID_SONY &&
	       dev->device == PCI_DEVICE_ID_SONY_BAIKAL_PCIE;
}

static void bpcie_set_child_domains(struct bpcie_dev *sc, struct irq_domain *domain)
{
	int func;

	for (func = 0; func < BAIKAL_NUM_FUNCS; func++) {
		struct pci_dev *pdev = pci_get_slot(sc->pdev->bus,
						    (sc->pdev->devfn & ~7) | func);

		if (!pdev)
			continue;
		dev_set_msi_domain(&pdev->dev, domain);
		pci_dev_put(pdev);
	}
}

int bpcie_assign_irqs(struct pci_dev *dev, int nvec)
{
	struct pci_dev *glue_pdev;
	int ret;

	glue_pdev = bpcie_get_glue_device(dev);
	if (!bpcie_is_compatible_device(glue_pdev)) {
		dev_err(&dev->dev, "bpcie: this is not a Baikal device\n");
		ret = -ENODEV;
		goto out_put;
	}
	if (!pci_get_drvdata(glue_pdev)) {
		dev_err(&dev->dev, "bpcie: not ready yet, cannot assign IRQs\n");
		ret = -ENODEV;
		goto out_put;
	}

#ifndef QEMU_HACK_NO_IOMMU
	if (!(bpcie_msi_domain_info.flags & MSI_FLAG_MULTI_PCI_MSI))
		nvec = 1;
#endif

	ret = pci_alloc_irq_vectors(dev, 1, nvec, PCI_IRQ_MSI);
	if (ret > 0)
		dev->irq = pci_irq_vector(dev, 0);

out_put:
	if (glue_pdev)
		pci_dev_put(glue_pdev);
	return ret;
}

void bpcie_free_irqs(unsigned int virq, unsigned int nr_irqs)
{
	struct irq_data *data = irq_get_irq_data(virq);
	struct msi_desc *desc;
	struct pci_dev *pdev;

	if (!data) {
		irq_domain_free_irqs(virq, nr_irqs);
		return;
	}

	desc = irq_data_get_msi_desc(data);
	if (!desc) {
		irq_domain_free_irqs(virq, nr_irqs);
		return;
	}

	pdev = msi_desc_to_pci_dev(desc);
	if (!pdev)
		return;

	pci_free_irq_vectors(pdev);
}

int bpcie_status(void)
{
	return bpcie_initialized;
}

static void bpcie_glue_remove(struct bpcie_dev *sc);

static int bpcie_glue_init(struct bpcie_dev *sc)
{
	sc_info("bpcie glue probe\n");

	if (!request_mem_region(pci_resource_start(sc->pdev, 2),
				pci_resource_len(sc->pdev, 2),
				"bpcie.glue")) {
		sc_err("Failed to request pcie region\n");
		return -EBUSY;
	}

	if (!request_mem_region(pci_resource_start(sc->pdev, 4),
				pci_resource_len(sc->pdev, 4),
				"bpcie.chipid")) {
		sc_err("Failed to request chipid region\n");
		release_mem_region(pci_resource_start(sc->pdev, 2),
				   pci_resource_len(sc->pdev, 2));
		return -EBUSY;
	}

	sc_info("Baikal chip revision: %08x:%08x:%08x\n",
		ioread32(sc->bar4 + BPCIE_REG_CHIPID_0),
		ioread32(sc->bar4 + BPCIE_REG_CHIPID_1),
		ioread32(sc->bar4 + BPCIE_REG_CHIPREV));

	sc->irqdomain = bpcie_create_irq_domain(sc);
	if (!sc->irqdomain) {
		sc_err("Failed to create IRQ domain\n");
		bpcie_glue_remove(sc);
		return -EIO;
	}

	bpcie_set_child_domains(sc, sc->irqdomain);

	sc->nvec = pci_alloc_irq_vectors(sc->pdev, BPCIE_SUBFUNC_ICC + 1,
					 BPCIE_NUM_SUBFUNCS, PCI_IRQ_MSI);
	if (sc->nvec <= 0) {
		sc_err("Failed to assign IRQs\n");
		bpcie_glue_remove(sc);
		return sc->nvec ? sc->nvec : -EIO;
	}
	sc->pdev->irq = pci_irq_vector(sc->pdev, 0);

	return 0;
}

static void bpcie_glue_remove(struct bpcie_dev *sc)
{
	sc_info("bpcie glue remove\n");

	if (sc->nvec > 0) {
		pci_free_irq_vectors(sc->pdev);
		sc->nvec = 0;
	}

	if (sc->irqdomain) {
		bpcie_set_child_domains(sc, NULL);
		irq_domain_remove(sc->irqdomain);
		sc->irqdomain = NULL;
	}

	release_mem_region(pci_resource_start(sc->pdev, 4),
			   pci_resource_len(sc->pdev, 4));
	release_mem_region(pci_resource_start(sc->pdev, 2),
			   pci_resource_len(sc->pdev, 2));
}

#ifdef CONFIG_PM
static int bpcie_glue_suspend(struct bpcie_dev *sc, pm_message_t state)
{
	return 0;
}

static int bpcie_glue_resume(struct bpcie_dev *sc)
{
	return 0;
}
#endif

int bpcie_uart_init(struct bpcie_dev *sc);
int bpcie_icc_init(struct bpcie_dev *sc);
void bpcie_uart_remove(struct bpcie_dev *sc);
void bpcie_icc_remove(struct bpcie_dev *sc);
#ifdef CONFIG_PM
void bpcie_uart_suspend(struct bpcie_dev *sc, pm_message_t state);
void bpcie_icc_suspend(struct bpcie_dev *sc, pm_message_t state);
void bpcie_uart_resume(struct bpcie_dev *sc);
void bpcie_icc_resume(struct bpcie_dev *sc);
#endif

static int bpcie_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	struct bpcie_dev *sc;
	int ret;

	ret = pci_enable_device(dev);
	if (ret)
		return ret;

	sc = kzalloc(sizeof(*sc), GFP_KERNEL);
	if (!sc) {
		ret = -ENOMEM;
		goto disable_dev;
	}

	sc->pdev = dev;
	memset(sc->irq_map, -1, sizeof(sc->irq_map));
	pci_set_drvdata(dev, sc);

	sc->bar0 = pci_ioremap_bar(dev, 0);
	sc->bar2 = pci_ioremap_bar(dev, 2);
	sc->bar4 = pci_ioremap_bar(dev, 4);
	if (!sc->bar0 || !sc->bar2 || !sc->bar4) {
		sc_err("failed to map some BARs, bailing out\n");
		ret = -EIO;
		goto free_bars;
	}

	ret = bpcie_glue_init(sc);
	if (ret < 0)
		goto free_bars;
	ret = bpcie_uart_init(sc);
	if (ret < 0)
		goto remove_glue;
	ret = bpcie_icc_init(sc);
	if (ret < 0)
		goto remove_uart;

	bpcie_initialized = true;
	return 0;

remove_uart:
	bpcie_uart_remove(sc);
remove_glue:
	bpcie_glue_remove(sc);
free_bars:
	if (sc->bar0)
		iounmap(sc->bar0);
	if (sc->bar2)
		iounmap(sc->bar2);
	if (sc->bar4)
		iounmap(sc->bar4);
	kfree(sc);
disable_dev:
	pci_disable_device(dev);
	return ret;
}

static void bpcie_remove(struct pci_dev *dev)
{
	struct bpcie_dev *sc = pci_get_drvdata(dev);

	bpcie_initialized = false;
	bpcie_icc_remove(sc);
	bpcie_uart_remove(sc);
	bpcie_glue_remove(sc);

	if (sc->bar0)
		iounmap(sc->bar0);
	if (sc->bar2)
		iounmap(sc->bar2);
	if (sc->bar4)
		iounmap(sc->bar4);
	kfree(sc);
	pci_disable_device(dev);
}

#ifdef CONFIG_PM
static int bpcie_suspend(struct pci_dev *dev, pm_message_t state)
{
	struct bpcie_dev *sc = pci_get_drvdata(dev);

	bpcie_icc_suspend(sc, state);
	bpcie_uart_suspend(sc, state);
	bpcie_glue_suspend(sc, state);
	return 0;
}

static int bpcie_resume(struct pci_dev *dev)
{
	struct bpcie_dev *sc = pci_get_drvdata(dev);

	bpcie_icc_resume(sc);
	bpcie_glue_resume(sc);
	bpcie_uart_resume(sc);
	return 0;
}
#endif

static const struct pci_device_id bpcie_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_SONY, PCI_DEVICE_ID_SONY_BAIKAL_PCIE), },
	{ }
};
MODULE_DEVICE_TABLE(pci, bpcie_pci_tbl);

static struct pci_driver bpcie_driver = {
	.name = "baikal_pcie",
	.id_table = bpcie_pci_tbl,
	.probe = bpcie_probe,
	.remove = bpcie_remove,
#ifdef CONFIG_PM
	.suspend = bpcie_suspend,
	.resume = bpcie_resume,
#endif
};
module_pci_driver(bpcie_driver);
