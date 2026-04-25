/* SPDX-License-Identifier: GPL-2.0-only */
/**
 * ps4-led.h — Public header for the PS4 Aeolia front panel LED driver
 *
 * Copyright (C) rmux <armandas.kvietkus@proton.me>
 */

#ifndef _PS4_LED_H
#define _PS4_LED_H

/* ============================================================
 * ICC Command Identifiers — LED / Indicator
 * ============================================================
 * Used with apcie_icc_cmd() to drive the front panel LED.
 *
 * PS4_LED_ICC_MAJOR:  ICC major number for the indicator device.
 *                     Registered on Orbis as "icc indicator device".
 * PS4_LED_ICC_MINOR:  Minor command number for setting LED state.
 * PS4_LED_PAYLOAD_LEN: Fixed byte length of every LED payload.
 *                      All led_* arrays in ps4-led.c are this size.
 */
#define PS4_LED_ICC_MAJOR       0x09
#define PS4_LED_ICC_MINOR       0x20
#define PS4_LED_PAYLOAD_LEN     35

#endif /* _PS4_LED_H */
