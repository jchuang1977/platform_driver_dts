#ifndef _5GMHS_H_
#define _5GMHS_H_

#include <linux/cdev.h>
#include <linux/semaphore.h>

#define HWVER_DEVICE_NODE_NAME  "hw_ver"
#define HWVER_DEVICE_FILE_NAME  "hw_ver"
#define HWVER_DEVICE_PROC_NAME	"hw_ver"

struct mhs_platform_data {
	int val;
	struct semaphore sem;
	struct cdev dev;
	struct gpio_desc *hw_bit0_gpio;
	struct gpio_desc *hw_bit1_gpio;
	struct gpio_desc *hw_bit2_gpio;
	struct gpio_desc *hw_bit3_gpio;
	struct gpio_desc *hw_bit4_gpio;
};

#endif
