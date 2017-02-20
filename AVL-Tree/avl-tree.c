#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

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

struct Node
{
	int key;
	int height;
	struct Node* left;
	struct Node* right;
};

static struct Node* makeNode(int key)
{
	struct Node* node = (struct Node*) vmalloc(sizeof(struct Node));
	node->key = key;
	node->height = 1;
	node->left = node->right = NULL;
	return node;
}

static int maxInt(int a, int b)
{
    return a > b ? a : b;
}

static int height(struct Node* node)
{
    return node ? node->height : 0;
}

static int balanceFactor(struct Node* node)
{
    return height(node->right) - height(node->left);
}

static void fixHeight(struct Node* node)
{
    int h1 = height(node->left);
    int h2 = height(node->right);
    node->height = maxInt(h1, h2) + 1;
}

static struct Node* rotateRight(struct Node* p)
{
    struct Node* q = p->left;
    p->left = q->right;
    q->right = p;
    fixHeight(p);
    fixHeight(q);
    return q;
}

static struct Node* rotateLeft(struct Node* q)
{
    struct Node* p = q->right;
    q->right = p->left;
    p->left = q;
    fixHeight(q);
    fixHeight(p);
    return p;
}

static struct Node* balance(struct Node* p)
{
    fixHeight(p);
    if (balanceFactor(p) == 2) {
        if (balanceFactor(p->right) < 0) {
            p->right = rotateRight(p->right);
        }
        return rotateLeft(p);
    }
    if (balanceFactor(p) == -2) {
        if (balanceFactor(p->left) > 0) {
            p->left = rotateLeft(p->left);
        }
        return rotateRight(p);
    }
    return p;
}

static struct Node* insert(struct Node* root, int key)
{
    if (!root) {
        return makeNode(key);
    }

    if (key < root->key) {
        root->left = insert(root->left, key);
    } else {
        root->right = insert(root->right, key);
    }

    return balance(root);
}

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

static int digitCount(int value)
{
    int result = 0;

    while (value != 0) {
        result++;
        value /= 10;
    }

    return result;
}

static void int2str(char* str, int value)
{
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

static struct Node* makeTree(int a[], int size)
{
    struct Node* tree = NULL;
    int i;

    for (i = 0; i < size; i++) {
        tree = insert(tree, a[i]);
    }

    return tree;
}

static void rootLeftRight(char* result, struct Node* root)
{
	char buf[MAXSTR];

    if (root) {
    	strncpy(result, strcat(result, "[{"), MAXSTR);
    	int2str(buf, root->key);
    	strncpy(result, strcat(result, buf), MAXSTR);
    	strncpy(result, strcat(result, ","), MAXSTR);
    	int2str(buf, root->height);
    	strncpy(result, strcat(result, buf), MAXSTR);
    	strncpy(result, strcat(result, "},"), MAXSTR);
    	rootLeftRight(result, root->left);
    	strncpy(result, strcat(result, ","), MAXSTR);
    	rootLeftRight(result, root->right);
    	strncpy(result, strcat(result, "]"), MAXSTR);
    } else {
    	strncpy(result, strcat(result, "[]"), MAXSTR);
    }
}

static void process(void)
{
	int value;

	char* token = msg;
	char* end = msg;

	int a[MAXSTR];
	size_t size = 0;

	struct Node* tree;

	while (token != NULL) {
		strsep(&end, " ");
		value = str2int(token);
		a[size++] = value;
		token = end;
	}

	tree = makeTree(a, size);

	msg[0] = '\0';
	rootLeftRight(msg, tree);
	strncpy(msg, strcat(msg, "\n"), MAXSTR);;
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

