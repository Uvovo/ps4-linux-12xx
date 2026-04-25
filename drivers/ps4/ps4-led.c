// SPDX-License-Identifier: GPL-2.0-only
/**
 * PS4 Aeolia Sysfs LED Driver
 *
 * Copyright (C) rmux <armandas.kvietkus@proton.me>
 *
 * Based on the original LED control work by Saya and PsxItArch.
 */

#include <linux/module.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include "aeolia.h"
#include "ps4-led.h"

/* ============================================================
 * ICC LED Payloads
 * ============================================================
 * Each array encodes a full ICC command payload for:
 *   Major: PS4_LED_ICC_MAJOR (0x09)
 *   Minor: PS4_LED_ICC_MINOR (0x20)
 *   Length: PS4_LED_PAYLOAD_LEN (35 bytes)
 *
 * These define the diode mix and PWM behavior sent to the
 * Aeolia EMC, which drives the front panel LED hardware.
 * Format is a proprietary binary protocol reverse-engineered
 * from hardware observation.
 *
 */

/** led_off: All LED channels disabled. Front panel dark. */
static const u8 led_off[] = {
	0x03, 0x01, 0x00, 0x00, 0x10, 0x01, 0x02, 0x00,
	0x02, 0x01, 0x00, 0x11, 0x01, 0x02, 0x00, 0x02,
	0x01, 0x00, 0x02, 0x03, 0x01, 0x00, 0x04, 0x01,
	0xbf, 0x02, 0x00, 0x05, 0x01, 0xff, 0x02, 0x00,
	0x05, 0x01, 0xff
};

/** led_blue: Solid blue. */
static const u8 led_blue[] = {
	0x03, 0x01, 0x00, 0x00, 0x10, 0x01, 0x02, 0xff,
	0x02, 0x01, 0x00, 0x11, 0x01, 0x02, 0x00, 0x02,
	0x01, 0x00, 0x02, 0x03, 0x01, 0x00, 0x04, 0x01,
	0xbf, 0x02, 0x00, 0x05, 0x01, 0xff, 0x02, 0x00,
	0x05, 0x01, 0xff
};

/** led_white: Solid white. */
static const u8 led_white[] = {
	0x03, 0x01, 0x00, 0x00, 0x10, 0x01, 0x02, 0x00,
	0x02, 0x01, 0x00, 0x11, 0x01, 0x02, 0xff, 0x02,
	0x01, 0x00, 0x02, 0x03, 0x01, 0x00, 0x04, 0x01,
	0xbf, 0x02, 0x00, 0x05, 0x01, 0xff, 0x02, 0x00,
	0x05, 0x01, 0xff
};

/** led_orange: Solid orange. */
static const u8 led_orange[] = {
	0x03, 0x01, 0x00, 0x00, 0x10, 0x01, 0x02, 0x00,
	0x02, 0x01, 0x00, 0x11, 0x01, 0x02, 0x00, 0x02,
	0x01, 0x00, 0x02, 0x03, 0x02, 0xff, 0x02, 0x01,
	0x00, 0x02, 0xff, 0x05, 0x01, 0xff, 0x02, 0xff,
	0x05, 0x01, 0x00
};

/** led_orange_blue: Orange and blue channels simultaneously. */
static const u8 led_orange_blue[] = {
	0x03, 0x01, 0x00, 0x00, 0x10, 0x01, 0x02, 0xff,
	0x02, 0x01, 0x00, 0x11, 0x01, 0x02, 0x00, 0x02,
	0x01, 0x00, 0x02, 0x03, 0x01, 0x00, 0x04, 0x01,
	0xbf, 0x02, 0xff, 0x05, 0x01, 0xff, 0x02, 0x00,
	0x05, 0x01, 0xff
};

/** led_orange_white: Orange and white channels simultaneously. */
static const u8 led_orange_white[] = {
	0x03, 0x01, 0x00, 0x00, 0x10, 0x01, 0x02, 0x00,
	0x02, 0x01, 0x00, 0x11, 0x01, 0x02, 0xff, 0x02,
	0x01, 0x00, 0x02, 0x03, 0x01, 0x00, 0x04, 0x01,
	0xbf, 0x02, 0xff, 0x05, 0x01, 0xff, 0x02, 0x00,
	0x05, 0x01, 0xff
};

/** led_pulsate_orange: Orange channel with PWM pulsation. */
static const u8 led_pulsate_orange[] = {
	0x03, 0x01, 0x00, 0x00, 0x10, 0x01, 0x02, 0x00,
	0x02, 0x01, 0x00, 0x11, 0x01, 0x02, 0x00, 0x02,
	0x01, 0x00, 0x02, 0x03, 0x01, 0xff, 0x04, 0x01,
	0x00, 0x02, 0xff, 0x05, 0x01, 0xff, 0x02, 0xff,
	0x05, 0x01, 0x00
};

/** led_orange_white_blue: All three channels active together. */
static const u8 led_orange_white_blue[] = {
	0x03, 0x01, 0x00, 0x00, 0x10, 0x01, 0x02, 0xff,
	0x02, 0x01, 0x00, 0x11, 0x01, 0x02, 0xff, 0x02,
	0x01, 0x00, 0x02, 0x03, 0x01, 0x00, 0x04, 0x01,
	0xbf, 0x02, 0xff, 0x05, 0x01, 0xff, 0x02, 0x00,
	0x05, 0x01, 0xff
};

/** led_white_blue: White and blue channels simultaneously. */
static const u8 led_white_blue[] = {
	0x03, 0x01, 0x00, 0x00, 0x10, 0x01, 0x02, 0xff,
	0x02, 0x01, 0x00, 0x11, 0x01, 0x02, 0xff, 0x02,
	0x01, 0x00, 0x02, 0x03, 0x01, 0x00, 0x04, 0x01,
	0xbf, 0x02, 0x00, 0x05, 0x01, 0xff, 0x02, 0x00,
	0x05, 0x01, 0xff
};

/** led_violet_blue: Violet-tinted blue (partial blue channel). */
static const u8 led_violet_blue[] = {
	0x03, 0x01, 0x00, 0x00, 0x10, 0x01, 0x02, 0x57,
	0x02, 0x01, 0x00, 0x11, 0x01, 0x02, 0x00, 0x02,
	0x01, 0x00, 0x02, 0x03, 0x02, 0xff, 0x02, 0x01,
	0x00, 0x02, 0xff, 0x05, 0x01, 0xff, 0x02, 0xff,
	0x05, 0x01, 0x00
};

/** led_pink: Pink hue (partial white + orange blend). */
static const u8 led_pink[] = {
	0x03, 0x01, 0x00, 0x00, 0x10, 0x01, 0x02, 0x00,
	0x02, 0x01, 0x00, 0x11, 0x01, 0x02, 0x30, 0x02,
	0x01, 0x00, 0x02, 0x03, 0x02, 0xff, 0x02, 0x01,
	0x00, 0x02, 0xff, 0x05, 0x01, 0xff, 0x02, 0xff,
	0x05, 0x01, 0x00
};

/** led_pink_blue: Pink with a blue tint (partial blue + orange). */
static const u8 led_pink_blue[] = {
	0x03, 0x01, 0x00, 0x00, 0x10, 0x01, 0x02, 0x20,
	0x02, 0x01, 0x00, 0x11, 0x01, 0x02, 0x00, 0x02,
	0x01, 0x00, 0x02, 0x03, 0x02, 0xff, 0x02, 0x01,
	0x00, 0x02, 0xff, 0x05, 0x01, 0xff, 0x02, 0xff,
	0x05, 0x01, 0x00
};

/* ============================================================
 * ps4_led_set - LED class brightness callback
 * ============================================================
 * @led_cdev: The LED class device whose brightness changed.
 * @value:    LED_OFF (0) to turn off, any positive value to enable.
 *
 * Called by the LED subsystem when user-space writes to a
 * /sys/class/leds/ps4:<color>:status/brightness node.
 *
 * Color selection uses strstr() on the led_classdev name. More
 * specific compound names (e.g. "orange_white_blue") are checked
 * before their substrings ("orange", "white", "blue") to avoid
 * false early matches.
 *
 */
static void ps4_led_set(struct led_classdev *led_cdev,
			enum led_brightness value)
{
	const u8 *data = led_off;
	u8 reply[0x30];

	if (value != LED_OFF) {
		if (strstr(led_cdev->name, "orange_white_blue"))
			data = led_orange_white_blue;
		else if (strstr(led_cdev->name, "pulsate_orange"))
			data = led_pulsate_orange;
		else if (strstr(led_cdev->name, "orange_white"))
			data = led_orange_white;
		else if (strstr(led_cdev->name, "orange_blue"))
			data = led_orange_blue;
		else if (strstr(led_cdev->name, "white_blue"))
			data = led_white_blue;
		else if (strstr(led_cdev->name, "violet_blue"))
			data = led_violet_blue;
		else if (strstr(led_cdev->name, "pink_blue"))
			data = led_pink_blue;
		else if (strstr(led_cdev->name, "pink"))
			data = led_pink;
		else if (strstr(led_cdev->name, "orange"))
			data = led_orange;
		else if (strstr(led_cdev->name, "white"))
			data = led_white;
		else if (strstr(led_cdev->name, "blue"))
			data = led_blue;
	}

	apcie_icc_cmd(PS4_LED_ICC_MAJOR, PS4_LED_ICC_MINOR,
		      (void *)data, PS4_LED_PAYLOAD_LEN,
		      reply, sizeof(reply));
}

/* ============================================================
 * LED Class Device Nodes
 * ============================================================
 * One struct led_classdev per color/effect. All share ps4_led_set.
 * Registered via devm_led_classdev_register() in probe().
 * Exposed at /sys/class/leds/ps4:<color>:status/
 */
static struct led_classdev ps4_led_nodes[] = {
	{ .name = "ps4:blue:status",              .brightness_set = ps4_led_set },
	{ .name = "ps4:white:status",             .brightness_set = ps4_led_set },
	{ .name = "ps4:orange:status",            .brightness_set = ps4_led_set },
	{ .name = "ps4:orange_blue:status",       .brightness_set = ps4_led_set },
	{ .name = "ps4:orange_white:status",      .brightness_set = ps4_led_set },
	{ .name = "ps4:pulsate_orange:status",    .brightness_set = ps4_led_set },
	{ .name = "ps4:orange_white_blue:status", .brightness_set = ps4_led_set },
	{ .name = "ps4:white_blue:status",        .brightness_set = ps4_led_set },
	{ .name = "ps4:violet_blue:status",       .brightness_set = ps4_led_set },
	{ .name = "ps4:pink:status",              .brightness_set = ps4_led_set },
	{ .name = "ps4:pink_blue:status",         .brightness_set = ps4_led_set },
};

/* ============================================================
 * ps4_led_probe - Register LED class nodes
 * ============================================================ */
static int ps4_led_probe(struct platform_device *pdev)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(ps4_led_nodes); i++) {
		ret = devm_led_classdev_register(&pdev->dev,
						 &ps4_led_nodes[i]);
		if (ret) {
			dev_err(&pdev->dev,
				"failed to register LED node %d: %d\n",
				i, ret);
			return ret;
		}
	}

	dev_info(&pdev->dev, "PS4 LED driver ready.\n");

	return 0;
}

/* ============================================================
 * Platform Driver Struct
 * ============================================================
 */
static struct platform_driver ps4_led_driver = {
	.probe  = ps4_led_probe,
	.driver = {
		.name = "ps4-led",
	},
};

/* ============================================================
 * Module Init / Exit
 * ============================================================
 * Manual init/exit (not module_platform_driver) because we must
 * register both the driver and the platform_device from within
 * the module.
 *
 * Init : register driver first, then device (probe fires).
 * Exit : unregister device first, then driver.
 */
static struct platform_device *ps4_led_pdev;

/**
 * ps4_led_init - Module entry point.
 *
 * Registers the platform driver, then the platform device.
 * On device registration failure, the driver is unregistered
 * before returning to leave the system in a clean state.
 */
static int __init ps4_led_init(void)
{
	int ret;

	ret = platform_driver_register(&ps4_led_driver);
	if (ret) {
		pr_err("ps4-led: failed to register platform driver: %d\n",
		       ret);
		return ret;
	}

	ps4_led_pdev = platform_device_register_simple("ps4-led", -1,
						       NULL, 0);
	if (IS_ERR(ps4_led_pdev)) {
		ret = PTR_ERR(ps4_led_pdev);
		pr_err("ps4-led: failed to register platform device: %d\n",
		       ret);
		platform_driver_unregister(&ps4_led_driver);
		ps4_led_pdev = NULL;
		return ret;
	}

	return 0;
}

/**
 * ps4_led_exit - Module exit point.
 *
 * Unregisters the platform device first, then the driver.
 */
static void __exit ps4_led_exit(void)
{
	if (ps4_led_pdev)
		platform_device_unregister(ps4_led_pdev);

	platform_driver_unregister(&ps4_led_driver);
}

module_init(ps4_led_init);
module_exit(ps4_led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("rmux <armandas.kvietkus@proton.me>");
MODULE_DESCRIPTION("PS4 Aeolia front panel LED driver");
MODULE_ALIAS("platform:ps4-led");
