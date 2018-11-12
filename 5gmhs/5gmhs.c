
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/gpio.h>

#include "5gmhs.h"

MODULE_LICENSE("GPL");



#define WNC_AB_GPIO			"wnc_ab_gpio"

//static struct resource res;
static int hwver_major = 0;
static int hwver_minor = 0;
struct mhs_platform_data *pdata = NULL;

static int hwver_open(struct inode* inode, struct file* filp);
static int hwver_release(struct inode* inode, struct file* filp);
static ssize_t hwver_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos);
//static ssize_t hwver_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos);

struct pinctrl *pinctrl;
struct pinctrl_state *ssc_qup;


static int mhs_proc_show(struct seq_file *m, void *v) {
  seq_printf(m, "%d\n",pdata->val);
  return 0;
}

static int mhs_proc_open(struct inode *inode, struct  file *file) {
  return single_open(file, mhs_proc_show, NULL);
}

static const struct file_operations proc_fops = {
	.owner	 = THIS_MODULE,
	.open = mhs_proc_open,
	.read    = seq_read,
	 .release = single_release,
};
/*create /proc/hw_ver*/
static void mhs_create_proc(void) {
	struct proc_dir_entry* entry;

	entry = proc_create(HWVER_DEVICE_PROC_NAME, 0, NULL, &proc_fops);
	if(!entry)
	{
		printk(KERN_DEBUG "\n:Could not create /proc/hw_ver");
 }								
}

static int hwver_open(struct inode* inode, struct file* filp) {
	//struct mhs_platform_data* dev;        

	//dev = container_of(inode->i_cdev, struct mhs_platform_data, dev);
	filp->private_data = pdata;
			
	return 0;
}

static int hwver_release(struct inode* inode, struct file* filp) {
	return 0;
}


static ssize_t hwver_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos) {
	ssize_t err = 0;
	struct mhs_platform_data* dev = filp->private_data;        

	pr_info("hwver_read\n");

	if(down_interruptible(&(dev->sem))) {
		return -ERESTARTSYS;
	}

	if(count < sizeof(dev->val)) {
		goto out;
	}        

	if(copy_to_user(buf, &(dev->val), sizeof(dev->val))) {
		err = -EFAULT;
		goto out;
	}

	err = sizeof(dev->val);

out:
	up(&(dev->sem));
	return err;
}

#if 0
static ssize_t hwver_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos) {
	struct mhs_platform_data* dev = filp->private_data;
	ssize_t err = 0;        

	if(down_interruptible(&(dev->sem))) {
		return -ERESTARTSYS;        
	}        

	if(count != sizeof(dev->val)) {
		goto out;        
	}        

	if(copy_from_user(&(dev->val), buf, count)) {
		err = -EFAULT;
		goto out;
	}

	err = sizeof(dev->val);

out:
	up(&(dev->sem));
	return err;
}
#endif


static struct file_operations hwver_fops = {
	.owner = THIS_MODULE,
	.open = hwver_open,
	.release = hwver_release,
	.read = hwver_read,
	//.write = hwver_write, 
};

static int  __hwver_setup_dev(struct mhs_platform_data* dev) {
	int err;
	dev_t devno = MKDEV(hwver_major, hwver_minor);

	pr_info("__hwver_setup_dev\n");

	cdev_init(&(dev->dev), &hwver_fops);
	dev->dev.owner = THIS_MODULE;
	dev->dev.ops = &hwver_fops;        

	err = cdev_add(&(dev->dev),devno, 1);
	if(err) {
		pr_info("cdev_add fail\n");
		return err;
	}        
	
	//mutex_init(&(dev->sem));
	sema_init(&(dev->sem),1);
	

	return 0;
}

static int mhs_platform_device_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	//struct device_node *np = dev->of_node;
	
	dev_t dev_node = 0;
	int err = -1;
	int ret = 0;
	pr_info("mhs_platform_device_probe 1105\n");

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		goto out;

	// for test
	pdata->val = 3 ;

	err = alloc_chrdev_region(&dev_node, 0, 1, HWVER_DEVICE_NODE_NAME);
	if(err < 0) {
		printk(KERN_ALERT"Failed to alloc char dev region.\n");
		goto fail;
	}

	hwver_major = MAJOR(dev_node);
	hwver_minor = MINOR(dev_node);         

	err = __hwver_setup_dev(pdata);
	if(err) {
		printk(KERN_ALERT"Failed to setup dev: %d.\n", err);
		goto cleanup;
	}        

	
	pr_info("hwver_major=%d, hwver_minor=%d\n",hwver_major,hwver_minor);


	mhs_create_proc();

	pr_info("Succedded to initialize mhs device.\n");


	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(pinctrl)) 
	{
		ret = PTR_ERR(pinctrl);
		pr_info("Failed to get pinctrl, err = %d\n", ret);
		goto out;
	}

	pr_info("Succedded devm_pinctrl_get.\n");

	ssc_qup = pinctrl_lookup_state(pinctrl, WNC_AB_GPIO );
	if (IS_ERR_OR_NULL(ssc_qup)) 
	{
		ret = PTR_ERR(ssc_qup);
		pr_info("Failed to get ssc_qup active state, err = %d\n",ret);
		goto out;
	}

	pr_info("Succedded pinctrl_lookup_state.\n");

	if (!IS_ERR_OR_NULL(ssc_qup)) 
	{
		ret = pinctrl_select_state(pinctrl,ssc_qup);
		if (ret) 
		{
			pr_info("Failed to select ssc_qup active state, err = %d\n",ret);
			goto out;
		}
	}
	
	pr_info("Succedded pinctrl_select_state.\n");
	
#if 0
	/* Play with our custom poperty. */
	if (of_property_read_u32(np, "mhs-asdf", &asdf) ) {
		dev_err(dev, "of_property_read_u32\n");
		return -EINVAL;
	}
	if (asdf != 0x12345678) {
		dev_err(dev, "asdf = %llx\n", (unsigned long long)asdf);
		return -EINVAL;
	}
#endif


	return 0;
fail:
cleanup:
out:
	return -EINVAL;	
}

static int mhs_platform_device_remove(struct platform_device *pdev)
{
	pr_info("mhs_platform_device_remove\n");
	dev_info(&pdev->dev, "remove\n");

	return 0;
}

static const struct of_device_id of_mhs_platform_device_match[] = {
	{ .compatible = "mhs_platform_device", },
	{},
};

MODULE_DEVICE_TABLE(of, of_mhs_platform_device_match);

static struct platform_driver mhs_plaform_driver = {
	.probe      = mhs_platform_device_probe,
	.remove	    = mhs_platform_device_remove,
	.driver     = {
		.name   = "mhs_platform_device",
		.of_match_table = of_mhs_platform_device_match,
		.owner = THIS_MODULE,
	},
};

static int mhs_platform_device_init(void)
{
	pr_info("mhs_platform_device_init\n");
	return platform_driver_register(&mhs_plaform_driver);
}

static void mhs_platform_device_exit(void)
{
	pr_info("mhs_platform_device_exit\n");
	platform_driver_unregister(&mhs_plaform_driver);
}

module_init(mhs_platform_device_init)
module_exit(mhs_platform_device_exit)
