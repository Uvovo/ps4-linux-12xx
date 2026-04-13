#ifndef _AEOLIA_H
#define _AEOLIA_H

#include <linux/io.h>
#include <linux/pci.h>
#include <linux/i2c.h>
#include "aeolia-baikal.h"

enum ps4_sb_model {
	PS4_SB_MODEL_UNKNOWN = 0,
	PS4_SB_MODEL_AEOLIA,
	PS4_SB_MODEL_BELIZE,
	PS4_SB_MODEL_BAIKAL,
};

struct ps4_sb_desc {
	enum ps4_sb_model model;
	const char *name;
	bool uses_bpcie;
	bool xhci_has_ahci;
	bool xhci_has_middle_host;
};

enum aeolia_func_id {
	AEOLIA_FUNC_ID_ACPI = 0,
	AEOLIA_FUNC_ID_GBE,
	AEOLIA_FUNC_ID_AHCI,
	AEOLIA_FUNC_ID_SDHCI,
	AEOLIA_FUNC_ID_PCIE,
	AEOLIA_FUNC_ID_DMAC,
	AEOLIA_FUNC_ID_MEM,
	AEOLIA_FUNC_ID_XHCI,

	AEOLIA_NUM_FUNCS
};

/* MSI registers for up to 31, but only 23 known. */
#define APCIE_NUM_SUBFUNC		23

/* Sub-functions, aka MSI vectors */
enum apcie_subfunc {
	APCIE_SUBFUNC_GLUE	= 0,
	APCIE_SUBFUNC_ICC	= 3,
	APCIE_SUBFUNC_HPET	= 5,
	APCIE_SUBFUNC_SFLASH	= 11,
	APCIE_SUBFUNC_RTC	= 13,
	APCIE_SUBFUNC_UART0	= 19,
	APCIE_SUBFUNC_UART1	= 20,
	APCIE_SUBFUNC_TWSI	= 21,

	APCIE_NUM_SUBFUNCS	= 23
};

#define APCIE_NR_UARTS 2

/* Relative to BAR2 */
#define APCIE_RGN_RTC_BASE		0x0
#define APCIE_RGN_RTC_SIZE		0x1000

#define APCIE_RGN_CHIPID_BASE		0x1000
#define APCIE_RGN_CHIPID_SIZE		0x1000

#define APCIE_REG_CHIPID_0		0x1104
#define APCIE_REG_CHIPID_1		0x1108
#define APCIE_REG_CHIPREV		0x110c

/* Relative to BAR4 */
#define APCIE_RGN_UART_BASE		0x140000
#define APCIE_RGN_UART_SIZE		0x1000

#define APCIE_RGN_PCIE_BASE		0x1c8000
#define APCIE_RGN_PCIE_SIZE		0x1000

#define APCIE_RGN_ICC_BASE		0x184000
#define APCIE_RGN_ICC_SIZE		0x1000

#define APCIE_REG_BAR(x)		(APCIE_RGN_PCIE_BASE + (x))
#define APCIE_REG_BAR_MASK(func, bar)	APCIE_REG_BAR(((func) * 0x30) + \
						((bar) << 3))
#define APCIE_REG_BAR_ADDR(func, bar)	APCIE_REG_BAR(((func) * 0x30) + \
						((bar) << 3) + 0x4)

#define APCIE_REG_MSI(x)		(APCIE_RGN_PCIE_BASE + 0x400 + (x))
#define APCIE_REG_MSI_CONTROL		APCIE_REG_MSI(0x0)
#define APCIE_REG_MSI_MASK(func)	APCIE_REG_MSI(0x4c + ((func) << 2))
#define APCIE_REG_MSI_DATA_HI(func)	APCIE_REG_MSI(0x8c + ((func) << 2))
#define APCIE_REG_MSI_ADDR(func)	APCIE_REG_MSI(0xac + ((func) << 2))
/* This register has non-uniform structure per function, dealt with in code */
#define APCIE_REG_MSI_DATA_LO(off)	APCIE_REG_MSI(0x100 + (off))

/* Not sure what the two individual bits do */
#define APCIE_REG_MSI_CONTROL_ENABLE	0x05

/* Enable for the entire function, 4 is special */
#define APCIE_REG_MSI_MASK_FUNC		0x01000000
#define APCIE_REG_MSI_MASK_FUNC4	0x80000000

#define APCIE_REG_ICC(x)		(APCIE_RGN_ICC_BASE + (x))
#define APCIE_REG_ICC_DOORBELL		APCIE_REG_ICC(0x804)
#define APCIE_REG_ICC_STATUS		APCIE_REG_ICC(0x814)
#define APCIE_REG_ICC_IRQ_MASK		APCIE_REG_ICC(0x824)

/* Apply to both DOORBELL and STATUS */
#define APCIE_ICC_SEND			0x01
#define APCIE_ICC_ACK			0x02

/* Relative to func6 BAR5 */
#define APCIE_SPM_ICC_BASE		0x2c000
#define APCIE_SPM_ICC_SIZE		0x1000

/* Boot params passed from southbridge */
#define APCIE_SPM_BP_BASE		0x2f000
#define APCIE_SPM_BP_SIZE		0x20

#define APCIE_SPM_ICC_REQUEST		0x0
#define APCIE_SPM_ICC_REPLY		0x800

struct apcie_dev {
	struct pci_dev *pdev;
	struct irq_domain *irqdomain;
	void __iomem *bar0;
	void __iomem *bar2;
	void __iomem *bar4;

	int nvec;
	int8_t irq_map[100];
	int serial_line[2];
	struct ps4_icc_dev icc;
};

#define sc_err(...) dev_info(&sc->pdev->dev, __VA_ARGS__)
#define sc_warn(...) dev_info(&sc->pdev->dev, __VA_ARGS__)
#define sc_notice(...) dev_info(&sc->pdev->dev, __VA_ARGS__)
#define sc_info(...) dev_info(&sc->pdev->dev, __VA_ARGS__)
#define sc_dbg(...) dev_info(&sc->pdev->dev, __VA_ARGS__)

static inline int apcie_irqnum(struct apcie_dev *sc, int index)
{
	if (sc->nvec > 1) {
		return sc->pdev->irq + index;
	} else {
		return sc->pdev->irq;
	}
}

int apcie_icc_cmd(u8 major, u16 minor, const void *data, u16 length,
	    void *reply, u16 reply_length);

static inline const struct ps4_sb_desc *ps4_sb_desc_by_device(u16 device)
{
	static const struct ps4_sb_desc aeolia = {
		.model = PS4_SB_MODEL_AEOLIA,
		.name = "Aeolia",
		.xhci_has_middle_host = true,
	};
	static const struct ps4_sb_desc belize = {
		.model = PS4_SB_MODEL_BELIZE,
		.name = "Belize",
		.xhci_has_ahci = true,
	};
	static const struct ps4_sb_desc baikal = {
		.model = PS4_SB_MODEL_BAIKAL,
		.name = "Baikal",
		.uses_bpcie = true,
		.xhci_has_ahci = true,
	};

	switch (device) {
	case PCI_DEVICE_ID_SONY_AEOLIA_ACPI:
	case PCI_DEVICE_ID_SONY_AEOLIA_GBE:
	case PCI_DEVICE_ID_SONY_AEOLIA_AHCI:
	case PCI_DEVICE_ID_SONY_AEOLIA_SDHCI:
	case PCI_DEVICE_ID_SONY_AEOLIA_PCIE:
	case PCI_DEVICE_ID_SONY_AEOLIA_DMAC:
	case PCI_DEVICE_ID_SONY_AEOLIA_MEM:
	case PCI_DEVICE_ID_SONY_AEOLIA_XHCI:
		return &aeolia;
	case PCI_DEVICE_ID_SONY_BELIZE_ACPI:
	case PCI_DEVICE_ID_SONY_BELIZE_GBE:
	case PCI_DEVICE_ID_SONY_BELIZE_AHCI:
	case PCI_DEVICE_ID_SONY_BELIZE_SDHCI:
	case PCI_DEVICE_ID_SONY_BELIZE_PCIE:
	case PCI_DEVICE_ID_SONY_BELIZE_DMAC:
	case PCI_DEVICE_ID_SONY_BELIZE_MEM:
	case PCI_DEVICE_ID_SONY_BELIZE_XHCI:
		return &belize;
	case PCI_DEVICE_ID_SONY_BAIKAL_ACPI:
	case PCI_DEVICE_ID_SONY_BAIKAL_GBE:
	case PCI_DEVICE_ID_SONY_BAIKAL_AHCI:
	case PCI_DEVICE_ID_SONY_BAIKAL_SDHCI:
	case PCI_DEVICE_ID_SONY_BAIKAL_PCIE:
	case PCI_DEVICE_ID_SONY_BAIKAL_DMAC:
	case PCI_DEVICE_ID_SONY_BAIKAL_MEM:
	case PCI_DEVICE_ID_SONY_BAIKAL_XHCI:
		return &baikal;
	default:
		return NULL;
	}
}

static inline enum ps4_sb_model ps4_sb_model_by_device(u16 device)
{
	const struct ps4_sb_desc *desc = ps4_sb_desc_by_device(device);

	return desc ? desc->model : PS4_SB_MODEL_UNKNOWN;
}

static inline bool ps4_sb_uses_bpcie(u16 device)
{
	const struct ps4_sb_desc *desc = ps4_sb_desc_by_device(device);

	return desc && desc->uses_bpcie;
}

#endif
