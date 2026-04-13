#ifndef _AEOLIA_BAIKAL_H
#define _AEOLIA_BAIKAL_H

#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/pci.h>

#define ICC_REPLY 0x4000
#define ICC_EVENT 0x8000

#define ICC_MAGIC 0x42
#define ICC_EVENT_MAGIC 0x24

struct icc_message_hdr {
	u8 magic;
	u8 major;
	u16 minor;
	u16 unknown;
	u16 cookie;
	u16 length;
	u16 checksum;
} __packed;

#define ICC_HDR_SIZE sizeof(struct icc_message_hdr)
#define ICC_MIN_SIZE 0x20
#define ICC_MAX_SIZE 0x7f0
#define ICC_MIN_PAYLOAD (ICC_MIN_SIZE - ICC_HDR_SIZE)
#define ICC_MAX_PAYLOAD (ICC_MAX_SIZE - ICC_HDR_SIZE)

#define BUF_FULL 0x7f0
#define BUF_EMPTY 0x7f4
#define HDR(x) (offsetof(struct icc_message_hdr, x))

#define ICC_MAJOR 'I'

struct icc_cmd {
	u8 major;
	u16 minor;
	void __user *data;
	u16 length;
	void __user *reply;
	u16 reply_length;
};

#define ICC_IOCTL_CMD _IOWR(ICC_MAJOR, 1, struct icc_cmd)

struct ps4_icc_dev {
	phys_addr_t spm_base;
	void __iomem *spm;

	spinlock_t reply_lock;
	bool reply_pending;

	struct icc_message_hdr request;
	struct icc_message_hdr reply;
	u16 reply_extra_checksum;
	void *reply_buffer;
	int reply_length;
	wait_queue_head_t wq;

	struct i2c_adapter i2c;
	struct input_dev *pwrbutton_dev;
};

#endif
