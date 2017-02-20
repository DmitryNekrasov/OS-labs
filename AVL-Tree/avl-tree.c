#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#define DEV_MAJOR 75
#define DEV_MINOR 0
#define DEV_NAME "avl_tree"

#define MAXSTR 1000

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dmitry");
MODULE_DESCRIPTION("Character device driver AVL tree");

static int dev_open(struct inode*, struct file*);
static int dev_rls(struct inode*, struct file*);
static ssize_t dev_read(struct file*, char*, size_t, loff_t*);
static ssize_t dev_write(struct file*, const char*, size_t, loff_t*);

static struct file_operations fops =
{
	.read = dev_read,
	.open = dev_open,
	.write = dev_write,
	.release = dev_rls
};

static char msg[MAXSTR] = { 0 };
static int times = 0;

static int str2int(char* str) {
    int len = strlen(str);
    int result = 0;
    int rank = 1;
    int digit;
    int i;

    for (i = len - 1; i >= 0; i--) {
        digit = str[i] - '0';
        result += digit * rank;
        rank *= 10;
    }

    return result;
}

static int digitCount(int value) {
    int result = 0;

    while (value != 0) {
        result++;
        value /= 10;
    }

    return result;
}

static void int2str(char* str, int value) {
    int len = digitCount(value);
    int i;
    int d;

    str[len] = '\0';

    for (i = len - 1; i >= 0; i--) {
        d = value % 10;
        str[i] = d + '0';
        value /= 10;
    }
}

static void process(void)
{
	int sum = 0;
	int value;

	char* token = msg;
	char* end = msg;

	while (token != NULL) {
		strsep(&end, " ");
		value = str2int(token);
		sum += value;
		token = end;
	}

	int2str(msg, sum);
}

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

static int dev_open(struct inode* my_inode, struct file* my_file)
{
	times++;
	printk(KERN_ALERT "Device opened %d times\n", times);

	return 0;
}

static int dev_rls(struct inode* my_inode, struct file* my_file)
{
	printk(KERN_ALERT "Device for AVL tree closed\n");
	return 0;
}

static ssize_t dev_read(struct file* my_file, char* buff, size_t len, loff_t* off)
{
	int msg_len = strlen(msg);
	int size = len >= msg_len ? msg_len : len;

	if (*off >= msg_len) {
		return 0;
	}

	if (copy_to_user(buff, msg, size)) {
		return -EFAULT;
	}

	*off += size;

	return len;
}

static ssize_t dev_write(struct file* my_file, const char* buff, size_t len, loff_t* off)
{
	unsigned long ret;
	printk(KERN_INFO "dev write\n");

	if (len > sizeof(msg) - 1) {
		return -EINVAL;
	}

	ret = copy_from_user(msg, buff, len);
	if (ret) {
		return -EFAULT;
	}

	msg[len - 1] = '\0';
	printk(KERN_INFO "msg = \"%s\"", msg);
	process();
	return len;
}

