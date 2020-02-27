#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>     //kmalloc
#include <linux/device.h>
#include <linux/uaccess.h>      // copy_from_user()

#define BASEMINOR   0
#define COUNT       3
#define CLASS       "chry"
#define NAME        "chrdev_demo"
#define KNBUFLEN 32

extern int chry_add(int a, int b);
extern int chry_sub(int a, int b);

struct chrdev_demo_desc
{
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

    /*
    内核中定义的struct class结构体，顾名思义，一个struct class结构体类型变量对应一个类，或者说设备类型，
    内核同时提供了class_create(…)函数，可以用它来创建一个类，
    这个类存放于sysfs下面(/sys/class/类名)，一旦创建好了这个类，
    再调用 device_create(…)函数来在/dev目录下创建相应的设备节点。
    这样，加载模块的时候，用户空间中的udev会自动响应 device_create()函数，去/sys/class/下寻找对应的类从而创建设备节点。
    */

    struct class *cls;
    struct device *dev;

    char knbuf[KNBUFLEN];
};

struct chrdev_demo_desc *chrdev_demo_dev = NULL;

/*
用户空间使用 open() 函数打开一个字符设备 fd = open("/dev/hello",O_RDWR) 
这一函数会调用两个数据结构 struct inode{...}与struct file{...} ，二者均在虚拟文件系统VFS处
-------------------------------------------------------------------------------
file 文件结构体
这里的file与用户空间程序中的FILE指针是不同的，用户空间FILE是定义在C库中，从来不会出现在内核中。
而struct file，却是内核当中的数据结构，因此，它也不会出现在用户层程序中。
file结构体指示一个已经打开的文件（设备对应于设备文件），其实系统中的每个打开的文件在内核空间都有一个相应的struct file结构体，
它由内核在打开文件时创建，并传递给在文件上进行操作的任何函数，直至文件被关闭。如果文件被关闭，内核就会释放相应的数据结构。
在内核源码中，struct file要么表示为file，或者为filp(意指“file pointer”), 注意区分一点，file指的是struct file本身，而filp是指向这个结构体的指针。
-------------------------------------------------------------------------------
VFS inode 包含文件访问权限、属主、组、大小、生成时间、访问时间、最后修改时间等信息。它是Linux 管理文件系统的最基本单位，也是文件系统连接任何子目录、文件的桥梁。
主要关注inode结构体的以下两个成员：
dev_t i_rdev;           表示设备文件的结点，这个域实际上包含了设备号。
struct cdev *i_cdev;    当inode结点指向一个字符设备文件时，此域为一个指向inode结构的指针。
*/
int chrdev_demo_open(struct inode *inode, struct file *flip)
{
    printk(KERN_INFO "%s,%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}
// 执行命令：sudo cat /dev/chrdev_demo 可以从dmesg看到open和release被调用
int chrdev_demo_release(struct inode *inode, struct file *flip)
{
    printk(KERN_INFO "%s,%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}

//从设备中同步读取数据 把内核空间的knbuf拷贝到用户空间  pos表示偏移量，指示从文件的哪里开始读取
static ssize_t chrdev_demo_read(struct file *flip, char __user *buf, size_t len, loff_t *pos)
{
    ssize_t ret = 0; 

    printk(KERN_INFO "%s,%s:%d len:%ld, pos:%lld\n", __FILE__, __func__, __LINE__, len, *pos);   

    if(*pos >= KNBUFLEN)
    {
        return 0;
    }

    if(len < KNBUFLEN)
    {
        //return -ENOMEM;
    }

    ret = len > KNBUFLEN ? KNBUFLEN : len;
    // 将内核空间的数据拷贝到用户空间 成功返回0，失败返回没有拷贝成功的数据字节数 
    ret = copy_to_user(buf, chrdev_demo_dev->knbuf, ret);   
    if(0 != ret)
	{
        printk(KERN_ERR "%s,%s:%d %ld\n", __FILE__, __func__, __LINE__, ret);
		return -EFAULT;	
	}
    *pos += len;
    // 理论上可以直接将buf的地址指向到knbuf  但不建议这么做 危害很大 比如传入buf为null 内核就爆炸了 
    //copy_to_user里面会对异常情况做判断处理

    return len;
}

//向设备发送数据
static ssize_t chrdev_demo_write(struct file *flip, const char __user *buf, size_t len, loff_t *pos)
{   
    ssize_t ret = 0;
    //这里不能直接打印用户空间传进来的数据
    printk(KERN_INFO "%s,%s:%d len:%ld\n", __FILE__, __func__, __LINE__, len);   

    if(len < 1/*|| len > KNBUFLEN*/)    
    {
        return -EINVAL;
    }

    ret = len > KNBUFLEN ? KNBUFLEN : len;
    // 将用户空间的数据拷贝到内核空间 不能在内核空间直接访问用户空间的数据，比如打印判断等操作 访问权限问题会导致内核崩溃
    if(copy_from_user(chrdev_demo_dev->knbuf, buf, ret))
	{
		return -EFAULT;	
	}
    
    printk(KERN_INFO "%s,%s:%d => %s\n", __FILE__, __func__, __LINE__, chrdev_demo_dev->knbuf);

    return ret;  
}

/*
各成员解析：https://blog.csdn.net/zqixiao_09/article/details/50850475
系统调用通过设备文件的主设备号找到相应的设备驱动程序，然后读取这个数据结构相应的函数指针，
接着把控制权交给该函数，这是Linux的设备驱动程序工作的基本原理。

在fs.h中有定义了一个全局的extern const struct file_operations def_chr_fops;
具体的实现在char_dev.c
const struct file_operations def_chr_fops = {
	.open = chrdev_open,
	.llseek = noop_llseek,
};
在inode.c中的init_special_inode()里面引用：inode->i_fop = &def_chr_fops;

当应用层调用open打开一个字符设备的时候, 最终是通过调用到def_chr_fops.open, 而def_chr_fops.open = chrdev_open
chrdev_open()所做的事情可以概括如下:
1. 根据设备号(inode->i_rdev), 在字符设备驱动模型中查找对应的驱动程序, 这通过kobj_lookup() 来实现, kobj_lookup()会返回对应驱动程序cdev的kobject.
2. 设置inode->i_cdev , 指向找到的cdev.
3. 将inode添加到cdev->list 的链表中.
4. 使用cdev的ops 设置file对象的f_op
5. 如果ops中定义了open方法,则调用该open方法
6. 返回
执行完 chrdev_open()之后,file对象的f_op指向cdev的ops,因而之后对设备进行的read, write等操作,就会执行cdev的相应操作。
*/
struct file_operations fops = {
    .owner      = THIS_MODULE,          //拥有该结构的模块的指针，一般为THIS_MODULES 这个成员用来在它的操作还在被使用时阻止模块被卸载
    .open       = chrdev_demo_open,     
    .release    = chrdev_demo_release,
    .read       = chrdev_demo_read,     
    .write      = chrdev_demo_write,
};
/*
全局数组 chrdevs 包含了255(CHRDEV_MAJOR_HASH_SIZE 的值)个 struct char_device_struct的元素，每一个对应一个相应的主设备号。
所有的字符设备都在 chrdevs 数组中, 通过__register_chrdev_region() 可从中注册设备号，如果要和cdev关联起来,还要调用cdev_add()。
*/
static int __init chrdev_demo_init(void)
{
    int ret = 0;
    //GFP_KERNEL:如果当前空间不够用,该函数会一直阻塞
    chrdev_demo_dev = kmalloc(sizeof(struct chrdev_demo_desc), GFP_KERNEL);
    if (NULL == chrdev_demo_dev)
    {
        printk(KERN_ERR "%s,%s:%d kmalloc failed\n", __FILE__, __func__, __LINE__);
        ret = -ENOMEM;
        goto err0;
    }
    
    // 1.模块加载函数通过 register_chrdev_region 或 alloc_chrdev_region 来静态或者动态获取设备号;
    // 老版本内核中使用register_chrdev接口, 但是会造成资源浪费，新版内核被弃用.
    // 三个函数下面其实都是对__register_chrdev_region() 的调用
    // 通过命令查看效果：cat /proc/devices | grep chrdev_demo
    ret = alloc_chrdev_region(&chrdev_demo_dev->devno, BASEMINOR, COUNT, NAME);
    if(ret < 0)
    {
        printk(KERN_ERR "%s,%s:%d failed %d \n", __FILE__, __func__, __LINE__, ret);
        goto err1;
    }
    printk(KERN_INFO "%s,%s:%d> major = %d\n", __FILE__, __func__, __LINE__, MAJOR(chrdev_demo_dev->devno));
    
    // 2. 通过 cdev_init 建立cdev与 file_operations之间的连接
    cdev_init(&chrdev_demo_dev->cdev, &fops);

    // 3. 通过 cdev_add 向系统添加一个cdev以完成注册;
    ret = cdev_add(&chrdev_demo_dev->cdev, chrdev_demo_dev->devno, COUNT);
    if (ret < 0)
    {
        printk(KERN_ERR "%s,%s:%d failed %d \n", __FILE__, __func__, __LINE__, ret);
        goto err2;
    }
    
    // 4.新建chry class 通过命令查看效果：ls -l /sys/class/chry
    chrdev_demo_dev->cls = class_create(THIS_MODULE, CLASS);
    if(IS_ERR(chrdev_demo_dev->cls))
	{
        printk(KERN_ERR "%s,%s:%d failed %d \n", __FILE__, __func__, __LINE__, ret);
        ret = PTR_ERR(chrdev_demo_dev->cls);
        goto err3;
	}

    // 5.在chry class下创建节点, 相当于mknod /dev/chrdev_demo 通过命令查看效果：ls -l /sys/class/chry/chrdev_demo /dev/chrdev_demo
    chrdev_demo_dev->dev = device_create(chrdev_demo_dev->cls, NULL, chrdev_demo_dev->devno, NULL, NAME); 
    if(IS_ERR(chrdev_demo_dev->dev))
	{
        printk(KERN_ERR "%s,%s:%d failed %d \n", __FILE__, __func__, __LINE__, ret);
        ret = PTR_ERR(chrdev_demo_dev->dev);
        goto err4;
	}
    
    // 测试调用另一个模块的接口 
    printk(KERN_INFO "chry_add(123,45) = %d, chry_sub(123,45) = %d\n", 
        chry_add(123, 45), chry_sub(123, 45));
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);

    return 0;
    
//err5:
    device_destroy(chrdev_demo_dev->cls, chrdev_demo_dev->devno);
err4:
    class_destroy(chrdev_demo_dev->cls);
err3:
    cdev_del(&chrdev_demo_dev->cdev);
err2:
    unregister_chrdev_region(chrdev_demo_dev->devno, COUNT);
err1:
    kfree(chrdev_demo_dev);
err0:
    return ret;
}


static void __exit chrdev_demo_exit(void)
{
    device_destroy(chrdev_demo_dev->cls, chrdev_demo_dev->devno);
    class_destroy(chrdev_demo_dev->cls);
    cdev_del(&chrdev_demo_dev->cdev);
    unregister_chrdev_region(chrdev_demo_dev->devno, COUNT);
    kfree(chrdev_demo_dev);
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(chrdev_demo_init);
module_exit(chrdev_demo_exit);

MODULE_LICENSE("GPL");














