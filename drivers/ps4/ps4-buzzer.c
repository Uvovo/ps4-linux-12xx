// SPDX-License-Identifier: GPL-2.0-only
/*
 * PS4 Aeolia Sysfs Buzzer Driver
 *
 * Copyright (C) Armandas Kvietkus <armandas.kvietkus@proton.me>
 *
 * Triggers the front-panel piezo buzzer via the Aeolia EMC ICC
 * indicator channel.  Reverse-engineered from Orbis kernel 12.02.elf.
 *
 * Sysfs interface:
 *   /sys/devices/platform/ps4-buzzer/beep
 *     Write "1" to trigger a beep.
 *     Write "0" to stop (if supported by EMC firmware).
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include "aeolia.h"
#include "ps4-buzzer.h"

static DEFINE_MUTEX(ps4_buzzer_lock);

static int ps4_buzzer_send(u8 value)
{
	u8 payload = value;
	u8 reply[16];
	int ret;

	mutex_lock(&ps4_buzzer_lock);
	ret = apcie_icc_cmd(PS4_BUZZER_ICC_MAJOR, PS4_BUZZER_ICC_MINOR,
			    &payload, PS4_BUZZER_PAYLOAD_LEN,
			    reply, sizeof(reply));
	mutex_unlock(&ps4_buzzer_lock);

	return ret;
}

static ssize_t beep_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	u8 val;
	int ret;

	ret = kstrtou8(buf, 0, &val);
	if (ret)
		return ret;

	ret = ps4_buzzer_send(val);
	if (ret)
		return ret;

	return count;
}

static DEVICE_ATTR_WO(beep);

static struct attribute *ps4_buzzer_attrs[] = {
	&dev_attr_beep.attr,
	NULL,
};

ATTRIBUTE_GROUPS(ps4_buzzer);

static int ps4_buzzer_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "PS4 buzzer driver ready.\n");
	return 0;
}

static struct platform_driver ps4_buzzer_driver = {
	.probe  = ps4_buzzer_probe,
	.driver = {
		.name		= "ps4-buzzer",
		.dev_groups	= ps4_buzzer_groups,
	},
};

static struct platform_device *ps4_buzzer_pdev;

static int __init ps4_buzzer_init(void)
{
	int ret;

	ret = platform_driver_register(&ps4_buzzer_driver);
	if (ret)
		return ret;

	ps4_buzzer_pdev = platform_device_register_simple("ps4-buzzer", -1,
							  NULL, 0);
	if (IS_ERR(ps4_buzzer_pdev)) {
		ret = PTR_ERR(ps4_buzzer_pdev);
		platform_driver_unregister(&ps4_buzzer_driver);
		ps4_buzzer_pdev = NULL;
		return ret;
	}

	return 0;
}

static void __exit ps4_buzzer_exit(void)
{
	if (ps4_buzzer_pdev)
		platform_device_unregister(ps4_buzzer_pdev);

	platform_driver_unregister(&ps4_buzzer_driver);
}

module_init(ps4_buzzer_init);
module_exit(ps4_buzzer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Armandas Kvietkus <armandas.kvietkus@proton.me>");
MODULE_DESCRIPTION("PS4 Aeolia front panel buzzer driver");
MODULE_ALIAS("platform:ps4-buzzer");
