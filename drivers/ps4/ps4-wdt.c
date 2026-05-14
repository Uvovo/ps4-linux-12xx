// SPDX-License-Identifier: GPL-2.0-only
/**
 * PS4 Aeolia/Belize Watchdog Timer Driver
 *
 * Copyright (C) rmux <armandas.kvietkus@proton.me>
 *
 * The PS4 southbridge (Aeolia/Belize) supports a hardware watchdog
 * via ICC commands. This driver exposes it through the standard
 * Linux watchdog subsystem.
 *
 * ICC Command:
 *   Major: 0x04 (System)
 *   Minor: 0x01 (Reset/Power control)
 *
 * Usage:
 *   echo 1 > /dev/watchdog0  # Start watchdog with default timeout
 *   echo V > /dev/watchdog0  # Magic close to stop watchdog
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>
#include <linux/io.h>
#include "aeolia.h"

#define PS4_WDT_DEFAULT_TIMEOUT	30
#define PS4_WDT_MIN_TIMEOUT	5
#define PS4_WDT_MAX_TIMEOUT	255

static int timeout = PS4_WDT_DEFAULT_TIMEOUT;
module_param(timeout, int, 0444);
MODULE_PARM_DESC(timeout, "Watchdog timeout in seconds (default: 30)");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0444);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default: "
		 __MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

struct ps4_wdt_priv {
	struct watchdog_device wdd;
	bool active;
};

static int ps4_wdt_ping(struct watchdog_device *wdd)
{
	u8 cmd[] = { 0, 0, 2, 0, 1, 0 };
	u8 reply[0x20];
	int ret;

	ret = apcie_icc_cmd(4, 1, cmd, sizeof(cmd), reply, sizeof(reply));
	if (ret < 0)
		return ret;

	return 0;
}

static int ps4_wdt_start(struct watchdog_device *wdd)
{
	struct ps4_wdt_priv *priv = watchdog_get_drvdata(wdd);

	priv->active = true;

	return ps4_wdt_ping(wdd);
}

static int ps4_wdt_stop(struct watchdog_device *wdd)
{
	struct ps4_wdt_priv *priv = watchdog_get_drvdata(wdd);

	priv->active = false;

	return 0;
}

static unsigned int ps4_wdt_get_timeleft(struct watchdog_device *wdd)
{
	return timeout;
}

static const struct watchdog_info ps4_wdt_info = {
	.identity = "PS4 Aeolia Watchdog",
	.options = WDIOF_KEEPALIVEPING | WDIOF_SETTIMEOUT |
		   WDIOF_MAGICCLOSE,
};

static const struct watchdog_ops ps4_wdt_ops = {
	.owner = THIS_MODULE,
	.start = ps4_wdt_start,
	.stop = ps4_wdt_stop,
	.ping = ps4_wdt_ping,
	.get_timeleft = ps4_wdt_get_timeleft,
};

static int ps4_wdt_probe(struct platform_device *pdev)
{
	struct ps4_wdt_priv *priv;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->wdd.info = &ps4_wdt_info;
	priv->wdd.ops = &ps4_wdt_ops;
	priv->wdd.timeout = timeout;
	priv->wdd.min_timeout = PS4_WDT_MIN_TIMEOUT;
	priv->wdd.max_timeout = PS4_WDT_MAX_TIMEOUT;
	priv->wdd.parent = &pdev->dev;

	watchdog_set_drvdata(&priv->wdd, priv);
	watchdog_set_nowayout(&priv->wdd, nowayout);

	ret = devm_watchdog_register_device(&pdev->dev, &priv->wdd);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, priv);

	dev_info(&pdev->dev, "PS4 watchdog timer registered (timeout=%ds)\n",
		 timeout);

	return 0;
}

static void ps4_wdt_remove(struct platform_device *pdev)
{
}

static struct platform_driver ps4_wdt_driver = {
	.probe = ps4_wdt_probe,
	.remove = ps4_wdt_remove,
	.driver = {
		.name = "ps4-wdt",
	},
};

static struct platform_device *ps4_wdt_pdev;

static int __init ps4_wdt_init(void)
{
	int ret;

	ret = platform_driver_register(&ps4_wdt_driver);
	if (ret) {
		pr_err("ps4-wdt: failed to register driver: %d\n", ret);
		return ret;
	}

	ps4_wdt_pdev = platform_device_register_simple("ps4-wdt", -1, NULL, 0);
	if (IS_ERR(ps4_wdt_pdev)) {
		ret = PTR_ERR(ps4_wdt_pdev);
		pr_err("ps4-wdt: failed to register device: %d\n", ret);
		platform_driver_unregister(&ps4_wdt_driver);
		ps4_wdt_pdev = NULL;
		return ret;
	}

	return 0;
}

static void __exit ps4_wdt_exit(void)
{
	if (ps4_wdt_pdev)
		platform_device_unregister(ps4_wdt_pdev);
	platform_driver_unregister(&ps4_wdt_driver);
}

module_init(ps4_wdt_init);
module_exit(ps4_wdt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("rmux <armandas.kvietkus@proton.me>");
MODULE_DESCRIPTION("PS4 Aeolia/Belize watchdog timer driver");
MODULE_ALIAS("platform:ps4-wdt");