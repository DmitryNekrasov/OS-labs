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

struct Channel
{
	spinlock_t read_lock;
	spinlock_t write_lock;
	char buffer[256];
};

struct Channel* c1;
struct Channel* c2;
struct Channel* c3;
struct Channel* c4;
struct Channel* c5;
struct Channel* c6;

static int step_count = 5;

void put(struct Channel* channel, char* src, size_t len)
{
	spin_lock(&(channel->write_lock));
	memcpy(channel->buffer, src, len);
	spin_unlock(&(channel->read_lock));
}

void get(struct Channel* channel, char* dst)
{
	int len;

	spin_lock(&(channel->read_lock));
	len = channel->buffer[0];
	memcpy(dst, channel->buffer, len);
	spin_unlock(&(channel->write_lock));
}

unsigned int generateRandom(int to)
{
	unsigned int rand;
	get_random_bytes(&rand, sizeof(rand));
	return rand % to;
}

static void intToVerdict(int value, char* verdict)
{
	switch (value) {
	case 1:
		strcpy(verdict, "AC");
		break;

	case 2:
		strcpy(verdict, "WA");
		break;

	case 3:
		strcpy(verdict, "TL");
		break;

	case 4:
		strcpy(verdict, "ML");
		break;

	default:
		strcpy(verdict, "RE");
	}
}

static int team(void* data)
{
	char buffer[256];
	int len = 20;
	int rand;
	int i;

	for (i = 0; i < step_count; i++) {

		buffer[0] = len;

		rand = generateRandom(2);
		if (rand == 1) {
			buffer[1] = 1;
			buffer[2] = 'A' + generateRandom(25);
		} else {
			buffer[1] = 0;
		}

		put(c1, buffer, len);

		if (buffer[1] == 0) {
			printk("Team check monitor\n");
		} else {
			printk("Team submit Problem %c\n", buffer[2]);
		}

		get(c2, buffer);

		printk("Team get monitor\n");
	}

	return 0;
}

static int clientApp(void* data)
{
	char buffer[256];
	int len = 20;
	int i;

	for (i = 0; i < step_count; i++) {

		get(c1, buffer);

		printk("Client App get query\n");

		if (buffer[1] == 1) {
			buffer[1] = 0;
			printk("Client App send problem %c\n", buffer[2]);
			put(c4, buffer, len);
		}

		buffer[0] = len;
		buffer[1] = 1;

		put(c5, buffer, len);
		printk("Client App requested monitor\n");

		get(c3, buffer);
		printk("Client App get monitor\n");

		printk("Client App send monitor to team\n");
		put(c2, buffer, len);
	}

	return 0;
}

static int monitor(void* data)
{
	char buffer[256];
	int len = 20;
	char verdict[5];
	int i;

	for (i = 0; i < step_count; i++) {
		get(c5, buffer);

		if (buffer[1] == 0) {
			intToVerdict(buffer[3], verdict);
			printk("Monitor get verdict %s\n", verdict);
		} else {
			buffer[0] = len;
			put(c3, buffer, len);
			printk("Monitor returned to Client App\n");
		}
	}

	return 0;
}

static int controller(void* data)
{
	char buffer[256];
	int len = 20;
	char verdict[5];
	int i;

	for (i = 0; i < step_count; i++) {
		get(c4, buffer);

		if (buffer[1] == 0) {
			printk("Controller get Problem %c\n", buffer[2]);
			printk("Controller send to Testing System Problem %c", buffer[2]);
			put(c6, buffer, len);
		} else {
			intToVerdict(buffer[3], verdict);
			printk("Controller get verdict %s on Problem %c", verdict, buffer[2]);
			buffer[1] = 0;
			put(c5, buffer, len);
		}
	}

	return 0;
}

static int testingSystem(void* data)
{
	char buffer[256];
	char verdict[5];
	int len = 20;
	int i;

	for (i = 0; i < step_count; i++) {
		get(c6, buffer);

		printk("Testing System get Problem %c\n", buffer[2]);

		buffer[3] = generateRandom(5);
		buffer[0] = len;
		buffer[1] = 1;

		put(c4, buffer, len);

		intToVerdict(buffer[3], verdict);
		printk("Testing System return verdict %s on Problem %c", verdict, buffer[2]);
	}
}

struct Process* team_process;
struct Process* client_app_process;
struct Process* monitor_process;
struct Process* controller_process;
struct Process* testing_system_process;

static void process(void)
{
	if (msg[0] == 'q') {
		kthread_stop(team_process->descriptor);
		kthread_stop(client_app_process->descriptor);
		kthread_stop(monitor_process->descriptor);
		kthread_stop(controller_process->descriptor);
		kthread_stop(testing_system_process->descriptor);
		return;
	}

	int team_data = 1;
	int client_app_data = 2;
	int monitor_data = 3;
	int controller_data = 4;
	int testing_system_data = 5;

	char team_name[20] = "team_name";
	char client_app_name[20] = "client_app_name";
	char monitor_name[20] = "monitor_name";
	char controller_name[20] = "controller_name";
	char testing_system_name[20] = "testing_system_name";

	c1 = (struct Channel*) vmalloc(sizeof(struct Channel));
	c2 = (struct Channel*) vmalloc(sizeof(struct Channel));
	c3 = (struct Channel*) vmalloc(sizeof(struct Channel));
	c4 = (struct Channel*) vmalloc(sizeof(struct Channel));
	c5 = (struct Channel*) vmalloc(sizeof(struct Channel));
	c6 = (struct Channel*) vmalloc(sizeof(struct Channel));

	spin_lock_init(&(c1->read_lock));
	spin_lock_init(&(c1->write_lock));
	spin_lock(&(c1->read_lock));

	spin_lock_init(&(c2->read_lock));
	spin_lock_init(&(c2->write_lock));
	spin_lock(&(c2->read_lock));

	spin_lock_init(&(c3->read_lock));
	spin_lock_init(&(c3->write_lock));
	spin_lock(&(c3->read_lock));

	spin_lock_init(&(c4->read_lock));
	spin_lock_init(&(c4->write_lock));
	spin_lock(&(c4->read_lock));

	spin_lock_init(&(c5->read_lock));
	spin_lock_init(&(c5->write_lock));
	spin_lock(&(c5->read_lock));

	spin_lock_init(&(c6->read_lock));
	spin_lock_init(&(c6->write_lock));
	spin_lock(&(c6->read_lock));

	team_process = (struct Process*) vmalloc(sizeof(struct Process));
	team_process->descriptor = kthread_run(&team, (void*) team_data, team_name);
	memcpy(team_process->keystring, team_name, sizeof(team_name));

	client_app_process = (struct Process*) vmalloc(sizeof(struct Process));
	client_app_process->descriptor = kthread_run(&clientApp, (void*) client_app_data, client_app_name);
	memcpy(client_app_process->keystring, client_app_name, sizeof(client_app_name));

	monitor_process = (struct Process*) vmalloc(sizeof(struct Process));
	monitor_process->descriptor = kthread_run(&monitor, (void*) monitor_data, monitor_name);
	memcpy(monitor_process->keystring, monitor_name, sizeof(monitor_name));

	controller_process = (struct Process*) vmalloc(sizeof(struct Process));
	controller_process->descriptor = kthread_run(&controller, (void*) controller_data, controller_name);
	memcpy(controller_process->keystring, controller_name, sizeof(controller_name));

	testing_system_process = (struct Process*) vmalloc(sizeof(struct Process));
	testing_system_process->descriptor = kthread_run(&testingSystem, (void*) testing_system_data, testing_system_name);
	memcpy(testing_system_process->keystring, testing_system_name, sizeof(testing_system_name));
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
