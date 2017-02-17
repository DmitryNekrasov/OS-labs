#include <linux/kernel.h>
#include <linux/module.h>

#define MODULE_NAME "AVL tree module"
static const char* mod_name = MODULE_NAME;

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dmitry");
MODULE_DESCRIPTION(MODULE_NAME);

#define DEBUG 1
#if DEBUG
#define log(S) { printk("[%s] %s\n", mod_name, S); }
#else
#define log(S) ;
#endif

static int __init my_init(void) {
	log("module's been loaded");
	return 0;
}

static void __exit my_exit(void) {
	log("module's been unloaded");
}

module_init(my_init);
module_exit(my_exit);
