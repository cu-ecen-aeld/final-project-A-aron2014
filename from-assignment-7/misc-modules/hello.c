/*                                                     
 * $Id: hello.c,v 1.5 2004/10/26 03:32:21 corbet Exp $ 
 */                                                    
#include <linux/init.h>
#include <linux/module.h>
MODULE_LICENSE("Dual BSD/GPL");

#define UNAME "A-aron2014"
static int hello_init(void)
{
	printk(KERN_ALERT "Hello, %s\n", UNAME);

	return 0;
}

static void hello_exit(void)
{
	printk(KERN_ALERT "Goodbye, %s\n",UNAME);
}

module_init(hello_init);
module_exit(hello_exit);
