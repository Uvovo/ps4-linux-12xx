/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) Armandas Kvietkus <armandas.kvietkus@proton.me>
 */
#ifndef _PS4_BUZZER_H
#define _PS4_BUZZER_H

/**
 * PS4 Aeolia Buzzer ICC Constants
 *
 * The buzzer shares the indicator ICC channel with the LED subsystem.
 * The EMC distinguishes buzzer from LED by payload length:
 *   LED:    35-byte payload starting with 0x03
 *   Buzzer:  1-byte payload (trigger value)
 *
 * Reverse-engineered from Orbis kernel module 12.02.elf
 * (icc_indicator_set_buzzer function).
 */

#define PS4_BUZZER_ICC_MAJOR	0x09
#define PS4_BUZZER_ICC_MINOR	0x20
#define PS4_BUZZER_PAYLOAD_LEN	1

#endif /* _PS4_BUZZER_H */
