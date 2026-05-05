// SPDX-License-Identifier: GPL-2.0-only

#include <generated/compile.h>
#include <generated/utsrelease.h>
#include <linux/proc_ns.h>
#include <linux/refcount.h>
#include <linux/uts.h>
#include <linux/utsname.h>

struct uts_namespace init_uts_ns = {
	.ns = NS_COMMON_INIT(init_uts_ns),
	.name = {
		.sysname	= UTS_SYSNAME,
		.nodename	= UTS_NODENAME,
		.release	= UTS_RELEASE,
		.version	= UTS_VERSION,
		.machine	= UTS_MACHINE,
		.domainname	= UTS_DOMAINNAME,
	},
	.user_ns = &init_user_ns,
};

/*
 * Keep Strawberry banner content manual on purpose.
 * i mean why not. not like it causes any perf loss while in use lol :3 .
 */
const char linux_banner[] =
	"Linux version " UTS_RELEASE " (" LINUX_COMPILE_BY "@"
	LINUX_COMPILE_HOST ") (" LINUX_COMPILER ") " UTS_VERSION "\n"
	"\n"
	"  _________ __                       ___.\n"
	" /   _____//  |_____________ __  _  _\\_ |__   __________________ ___.__.\n"
	" \\_____  \\\\   __\\_  __ \\__  \\\\ \\/ \\/ /| __ \\_/ __ \\_  __ \\_  __ <   |  |\n"
	" /        \\|  |  |  | \\// __ \\\\     / | \\_\\ \\  ___/|  | \\/|  | \\/\\___  |\n"
	"/_______  /|__|  |__|  (____  /\\/\\_/  |___  /\\___  >__|   |__|   / ____|\n"
	"        \\/                  \\/            \\/     \\/              \\/\n"
	"\n"
	"  Strawberry PS4 Linux\n"
	"  Platform  : Sony PlayStation 4\n"
	"  Focus     : AEOLIA / BELIZE\n"
	"  Release   : " UTS_RELEASE "\n"
	"  Builder   : " LINUX_COMPILE_BY "@" LINUX_COMPILE_HOST "\n"
	"  Toolchain : " LINUX_COMPILER "\n"
	"  Build     : " UTS_VERSION "\n"
	"\n";
