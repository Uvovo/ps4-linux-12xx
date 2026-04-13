#ifndef _BAIKAL_H
#define _BAIKAL_H

#include "aeolia.h"

#define bpcie_dev apcie_dev

enum baikal_func_id {
	BAIKAL_FUNC_ID_ACPI = AEOLIA_FUNC_ID_ACPI,
	BAIKAL_FUNC_ID_GBE = AEOLIA_FUNC_ID_GBE,
	BAIKAL_FUNC_ID_AHCI = AEOLIA_FUNC_ID_AHCI,
	BAIKAL_FUNC_ID_SDHCI = AEOLIA_FUNC_ID_SDHCI,
	BAIKAL_FUNC_ID_PCIE = AEOLIA_FUNC_ID_PCIE,
	BAIKAL_FUNC_ID_DMAC = AEOLIA_FUNC_ID_DMAC,
	BAIKAL_FUNC_ID_MEM = AEOLIA_FUNC_ID_MEM,
	BAIKAL_FUNC_ID_XHCI = AEOLIA_FUNC_ID_XHCI,

	BAIKAL_NUM_FUNCS = AEOLIA_NUM_FUNCS,
};

enum bpcie_subfunc {
	BPCIE_SUBFUNC_GLUE	= 0,
	BPCIE_SUBFUNC_ICC	= 3,
	BPCIE_SUBFUNC_SFLASH	= 19,
	BPCIE_SUBFUNC_RTC	= 21,
	BPCIE_SUBFUNC_HPET	= 22,
	BPCIE_SUBFUNC_UART0	= 26,
	BPCIE_SUBFUNC_UART1	= 27,

	BPCIE_SUBFUNC_USB0	= 0,
	BPCIE_SUBFUNC_USB2	= 2,
	BPCIE_SUBFUNC_ACPI	= 1,
	BPCIE_SUBFUNC_SPM	= 1,
	BPCIE_SUBFUNC_DMAC1	= 0,
	BPCIE_SUBFUNC_DMAC2	= 1,

	BPCIE_NUM_SUBFUNCS	= 32
};

enum bpcie_subfuncs_per_func {
	BPCIE_SUBFUNCS_FUNC0 = 2,
	BPCIE_SUBFUNCS_FUNC1 = 1,
	BPCIE_SUBFUNCS_FUNC2 = 1,
	BPCIE_SUBFUNCS_FUNC3 = 1,
	BPCIE_SUBFUNCS_FUNC4 = 31,
	BPCIE_SUBFUNCS_FUNC5 = 2,
	BPCIE_SUBFUNCS_FUNC6 = 3,
	BPCIE_SUBFUNCS_FUNC7 = 3,
};

#define BPCIE_NR_UARTS 2

/* Relative to BAR4 */
#define BPCIE_RGN_CHIPID_BASE		0x4000
#define BPCIE_RGN_CHIPID_SIZE		0x9000

#define BPCIE_REG_CHIPID_0		0xC020
#define BPCIE_REG_CHIPID_1		0xC024
#define BPCIE_REG_CHIPREV		0x4084

/* Relative to BAR2 */
#define BPCIE_HPET_BASE			0x109000
#define BPCIE_HPET_SIZE			0x400

#define BPCIE_RGN_UART_BASE		0x10E000
#define BPCIE_RGN_UART_SIZE		0x1000

#define BPCIE_RGN_ICC_BASE		(0x108000 - 0x800)
#define BPCIE_RGN_ICC_SIZE		0x1000

#define BPCIE_ACK_WRITE			0x110084
#define BPCIE_ACK_READ			0x110088

#define BPCIE_REG_ICC(x)		(BPCIE_RGN_ICC_BASE + (x))
#define BPCIE_REG_ICC_DOORBELL		BPCIE_REG_ICC(0x804)
#define BPCIE_REG_ICC_STATUS		BPCIE_REG_ICC(0x814)
#define BPCIE_REG_ICC_IRQ_MASK		BPCIE_REG_ICC(0x824)

#define BPCIE_ICC_SEND			0x01
#define BPCIE_ICC_ACK			0x02

#define BPCIE_USB_BASE			0x180000

/* Relative to func6 BAR5 */
#define BPCIE_SPM_ICC_BASE		0x2c000
#define BPCIE_SPM_ICC_SIZE		0x1000

#define BPCIE_SPM_BP_BASE		0x2f000
#define BPCIE_SPM_BP_SIZE		0x20

#define BPCIE_SPM_ICC_REQUEST		0x0
#define BPCIE_SPM_ICC_REPLY		0x800

static inline int bpcie_irqnum(struct bpcie_dev *sc, int index)
{
	return apcie_irqnum(sc, index);
}

int bpcie_assign_irqs(struct pci_dev *dev, int nvec);
void bpcie_free_irqs(unsigned int virq, unsigned int nr_irqs);
int bpcie_status(void);
int bpcie_icc_cmd(u8 major, u16 minor, const void *data, u16 length,
		  void *reply, u16 reply_length);
int bpcie_uart_init(struct bpcie_dev *sc);
void bpcie_uart_remove(struct bpcie_dev *sc);
int bpcie_icc_init(struct bpcie_dev *sc);
void bpcie_icc_remove(struct bpcie_dev *sc);
#ifdef CONFIG_PM
void bpcie_uart_suspend(struct bpcie_dev *sc, pm_message_t state);
void bpcie_icc_suspend(struct bpcie_dev *sc, pm_message_t state);
void bpcie_uart_resume(struct bpcie_dev *sc);
void bpcie_icc_resume(struct bpcie_dev *sc);
#endif

#endif
