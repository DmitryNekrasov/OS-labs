#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#define DEV_MAJOR 75
#define DEV_MINOR 0
#define DEV_NAME "avl_tree"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dmitry");
MODULE_DESCRIPTION("Character device driver AVL tree");

static int dev_open(struct inode*, struct file*);
static int dev_rls(struct inode*, struct file*);
static ssize_t dev_read(struct file*, char*, size_t, loff_t*);
static ssize_t dev_write(struct file*, const char*, size_t, loff_t);

static struct file_operations fops =
{
	.read = dev_read,
	.open = dev_open,
	.write = dev_write,
	.release = dev_rls
};

int init_module(void)
{
	int t = register_chrdev(DEV_MAJOR, DEV_NAME, &fops);

	if (t < 0) {
		printk(KERN_ALERT "Device registration failed\n");
	} else {
		printk(KERN_ALERT "Device registered\n");
	}

	return t;
}

void cleanup_module(void)
{
	unregister_chrdev(DEV_MAJOR, DEV_NAME);
}

