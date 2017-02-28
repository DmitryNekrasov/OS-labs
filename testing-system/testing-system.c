#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/unistd.h>
#include <asm/uaccess.h>
#include <linux/time.h>
#include <linux/random.h>
#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/jiffies.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>

#define DEV_MAJOR 125
#define DEV_MINOR 0
#define DEV_NAME "testing_system"

#define MAXSTR 100

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dmitry");
MODULE_DESCRIPTION("ACM ICPC testing system");

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

struct Process
{
	struct task_struct* descriptor;
	char keystring[256];
};

static int team(void* data)
{
	int i;

	for (i = 0; i < 5; i++) {
		printk("team %d\n", i);
		msleep(10 * i);
	}

	return 0;
}

static int clientApp(void* data)
{
	int i;

	for (i = 0; i < 5; i++) {
		printk("clientApp %d\n", i);
		msleep(10 * i);
	}

	return 0;
}

static void process(void)
{
	struct Process* teamProcess;
	struct Process* clientAppProcess;

	int teamData = 1;
	int clientAppData = 2;

	char teamName[10] = "team_name";
	char clientAppName[20] = "client_app_name";

	teamProcess = (struct Process*) vmalloc(sizeof(struct Process));
	teamProcess->descriptor = kthread_run(&team, (void*) teamData, teamName);
	memcpy(teamProcess->keystring, teamName, sizeof(teamName));

	clientAppProcess = (struct Process*) vmalloc(sizeof(struct Process));
	clientAppProcess->descriptor = kthread_run(&clientApp, (void*) clientAppData, clientAppName);
	memcpy(clientAppProcess->keystring, clientAppName, sizeof(clientAppName));
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
	printk(KERN_ALERT "Device for testing system closed\n");
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
	process();
	return len;
}
