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

#define DEV_MAJOR 105
#define DEV_MINOR 0
#define DEV_NAME "integral"

#define MAXSTR 100

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dmitry");
MODULE_DESCRIPTION("Trapezoidal rule for approximating the definite integral");

#define IDENT "for_thread_%d"
#define NUM 4

static struct completion finished[NUM];

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

struct Data
{
	int thread_num;
	int from;
	int to;
	int step;
	long long result;
};

static int str2int(char* str)
{
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

static struct Data* makeData(int thread_num, int from, int to, int step)
{
	struct Data* data = (struct Data*) vmalloc(sizeof(struct Data));
	data->thread_num = thread_num;
	data->from = from;
	data->to = to;
	data->step = step;
	data->result = 0;
	return data;
}

int f(int x) {
	return x * x;
}

int threadFunction(void* voidData)
{
	struct Data* data = (struct Data*) voidData;

	long long result = 0;
	int i;

	for (i = data->from; i < data->to; i += data->step) {
		result += (long long) (f(i) + f(i + data->step)) * data->step;
	}

	data->result = result;

	printk("Thread #%d, from %d to %d\n", data->thread_num, data->from, data->to);

	complete(finished + data->thread_num);

	return 0;
}



static void process(void)
{
	char* token = msg;
	char* end = msg;

	int from, to;
	int step = 1;
	int interval_size;
	int a[NUM + 1];

	struct Data* data[NUM];
	struct task_struct* tasks[NUM];

	int i;

	int result = 0;

	long long ts1, ts2;

	strsep(&end, " ");
	from = str2int(token);
	token = end;
	strsep(&end, " ");
	to = str2int(token);

	printk("from = %d, to = %d\n", from, to);

	interval_size = (to - from) / NUM;
	a[0] = from;
	for (i = 1; i <= NUM; i++) {
		a[i] = a[i - 1] + interval_size;
	}

	for (i = 0; i < NUM; i++) {
		data[i] = makeData(i, a[i], a[i + 1], step);
	}

	for (i = 0; i < NUM; i++) {
		init_completion(finished + i);
	}

	ts1 = jiffies;

	for (i = 0; i < NUM; i++) {
		tasks[i] = kthread_run(&threadFunction, (void*) data[i], IDENT, i);
	}

	for (i = 0; i < NUM; i++) {
		wait_for_completion(finished + i);
	}

	for (i = 0; i < NUM; i++) {
		result += data[i]->result;
	}
	result /= 2;

	ts2 = jiffies;

	printk("Nanosecs %lld %lld\n", ts1, ts2);
	printk("result = %d, time = %lld\n", result, ts2 - ts1);
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
	printk(KERN_ALERT "Device for integral closed\n");
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
