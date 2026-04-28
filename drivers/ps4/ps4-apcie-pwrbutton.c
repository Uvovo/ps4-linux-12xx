#include <linux/errno.h>
#include <linux/export.h>
#include <linux/mutex.h>
#include <asm/ps4.h>

#include "aeolia.h"

/* Prototype declarations */
void icc_pwrbutton_trigger(struct apcie_dev *sc, int state);
int icc_pwrbutton_init(struct apcie_dev *sc);
void icc_pwrbutton_remove(struct apcie_dev *sc);
/* ---------------------- */ //this file just felt very empty at the top hence the comments

static DEFINE_MUTEX(ps4_display_reprobe_lock);
static ps4_display_reprobe_cb_t ps4_display_reprobe_cb;
static void *ps4_display_reprobe_data;

int ps4_display_reprobe_register(ps4_display_reprobe_cb_t cb, void *data)
{
	if (!cb)
		return -EINVAL;

	mutex_lock(&ps4_display_reprobe_lock);
	ps4_display_reprobe_cb = cb;
	ps4_display_reprobe_data = data;
	mutex_unlock(&ps4_display_reprobe_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(ps4_display_reprobe_register);

void ps4_display_reprobe_unregister(ps4_display_reprobe_cb_t cb, void *data)
{
	mutex_lock(&ps4_display_reprobe_lock);
	if (ps4_display_reprobe_cb == cb &&
	    ps4_display_reprobe_data == data) {
		ps4_display_reprobe_cb = NULL;
		ps4_display_reprobe_data = NULL;
	}
	mutex_unlock(&ps4_display_reprobe_lock);
}
EXPORT_SYMBOL_GPL(ps4_display_reprobe_unregister);

void ps4_display_reprobe_request(void)
{
	mutex_lock(&ps4_display_reprobe_lock);
	if (ps4_display_reprobe_cb)
		ps4_display_reprobe_cb(ps4_display_reprobe_data);
	mutex_unlock(&ps4_display_reprobe_lock);
}

void icc_pwrbutton_trigger(struct apcie_dev *sc, int state)
{
	/*
	 * Repurpose the PS4 power button as a display reprobe trigger.
	 * Do not emit KEY_POWER because userspace may treat it as
	 * shutdown/suspend/poweroff.
	 */
	if (!state)
		ps4_display_reprobe_request();
}

int icc_pwrbutton_init(struct apcie_dev *sc)
{
	int ret = 0;
	u16 button;

	// enable power button notifications
	button = 0x100;
	ret = apcie_icc_cmd(8, 1, &button, sizeof(button), NULL, 0);
	if (ret < 0) {
		sc_info("%s: Failed to enable power notifications (%d)\n",
			__func__, ret);
	}

	// enable reset button notifications (?)
	button = 0x102;
	ret = apcie_icc_cmd(8, 1, &button, sizeof(button), NULL, 0);
	if (ret < 0) {
		sc_info("%s: Failed to enable reset notifications (%d)\n",
		        __func__, ret);
	}

	return 0;
}

void icc_pwrbutton_remove(struct apcie_dev *sc)
{
	sc->icc.pwrbutton_dev = NULL;
}
