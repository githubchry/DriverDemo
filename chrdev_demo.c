#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>


#define BASEMINOR   0
#define COUNT       3
#define NAME        "chrdev_demo"

/*
在Linux内核中：
    使用cdev结构体来描述字符设备;
    通过其成员dev_t来定义设备号（分为主、次设备号）以确定字符设备的唯一性;
    通过其成员file_operations来定义字符设备驱动提供给VFS的接口函数，如常见的open()、read()、write()等;

struct cdev { 
	struct kobject kobj;                  //内嵌的内核对象.
	struct module *owner;                 //该字符设备所在的内核模块的对象指针.
	const struct file_operations *ops;    //该结构描述了字符设备所能实现的方法，是极为关键的一个结构体.
	struct list_head list;                //用来将已经向内核注册的所有字符设备形成链表.
	dev_t dev;                            //字符设备的设备号，由主设备号和次设备号构成.
	unsigned int count;                   //隶属于同一主设备号的次设备号的个数.
};
*/

struct cdev cdev;
dev_t devno;  

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
    // 1.模块加载函数通过 register_chrdev_region 或 alloc_chrdev_region 来静态或者动态获取设备号;
    ret = alloc_chrdev_region(&devno, BASEMINOR, COUNT, NAME);
    if(ret < 0)
    {
        printk(KERN_ERR "%s,%s:%d failed\n", __FILE__, __func__, __LINE__);
        goto err1;
    }
    printk(KERN_INFO "%s,%s:%d> major = %d\n", __FILE__, __func__, __LINE__, MAJOR(devno));
    
    // 2. 通过 cdev_init 建立cdev与 file_operations之间的连接
    cdev_init(&cdev, &fops);

    // 3. 通过 cdev_add 向系统添加一个cdev以完成注册;
    ret = cdev_add(&cdev, devno, COUNT);
    if (ret < 0)
    {
        printk(KERN_ERR "%s,%s:%d failed\n", __FILE__, __func__, __LINE__);
        goto err2;
    }
    
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);

    return 0;
err2:
    unregister_chrdev_region(devno, COUNT);
err1:
    return ret;
}


static void __exit chrdev_demo_exit(void)
{
    cdev_del(&cdev);
    unregister_chrdev_region(devno, COUNT);
    
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(chrdev_demo_init);
module_exit(chrdev_demo_exit);

MODULE_LICENSE("GPL");














