#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>


#define BASEMINOR   0
#define COUNT       3
#define NAME        "chrdev_demo"

dev_t devno;
struct cdev *cdevp = NULL;

int chrdev_demo_open(struct inode *inode, struct file *flip)
{
    printk(KERN_INFO "%s,%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}

int chrdev_demo_release(struct inode *inode, struct file *flip)
{
    printk(KERN_INFO "%s,%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}

struct file_operations fops = {
    .owner      = THIS_MODULE,
    .open       = chrdev_demo_open,
    .release    = chrdev_demo_release,
};


static int __init chrdev_demo_init(void)
{
    int ret = 0;
    // 0. alloc dev_t devno
    ret = alloc_chrdev_region(&devno, BASEMINOR, COUNT, NAME);
    if(ret < 0)
    {
        printk(KERN_ERR "%s,%s:%d failed\n", __FILE__, __func__, __LINE__);
        goto err1;
    }
    printk(KERN_INFO "%s,%s:%d> major = %d\n", __FILE__, __func__, __LINE__, MAJOR(devno));

    // 1.alloc cdev
    cdevp = cdev_alloc();
    if (NULL == cdevp)
    {
        printk(KERN_ERR "%s,%s:%d failed\n", __FILE__, __func__, __LINE__);
        ret = -ENOMEM;  //ENOMEN一般表示没有内存空间的意思 这里加了个负号
        goto err2;
    }
    
    // 2.cdev init  
    cdev_init(cdevp, &fops);

    // 3.cdev add
    ret = cdev_add(cdevp, devno, COUNT);
    if (ret < 0)
    {
        printk(KERN_ERR "%s,%s:%d failed\n", __FILE__, __func__, __LINE__);
        goto err3;
    }
    
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
    return 0;
err3:
    kfree(cdevp);
err2:
    unregister_chrdev_region(devno, COUNT);
err1:
    return ret;
}


static void __exit chrdev_demo_exit(void)
{
    cdev_del(cdevp);
    unregister_chrdev_region(devno, COUNT);
    kfree(cdevp);

    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(chrdev_demo_init);
module_exit(chrdev_demo_exit);

MODULE_LICENSE("GPL");














