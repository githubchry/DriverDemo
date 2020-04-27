#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>     //kmalloc
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>  //struct platform_device_id
#include <linux/uaccess.h>      // copy_from_user()
#include <linux/delay.h>    //msleep_interruptible
#include <asm/io.h>     // ioremap 头文件

#include <linux/of_irq.h>     //irq
#include <linux/of_gpio.h>
#include <linux/interrupt.h>     //IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING
#include <linux/sched.h>     //
#include <linux/poll.h>     //poll_wait

#define BASEMINOR   0
#define COUNT       1
#define CLASS       "chry"
#define NAME        "keyirq_demo"


#define KEY_ENTER   28
//设计描述按键数据的对象 直接照抄输入子系统
struct key_event {
    int key;    //键值：W A S D ESC ENTER RESET....在input.h里面定义了所有的键值，也可以自定义
    int val;    //状态：1按下 0松开
};

//设计设备对象
struct platform_keyirq_desc {
    struct cdev cdev;
    dev_t devno;  
    struct class *cls;
    struct device *dev;
    struct resource *res;

    volatile uint32_t *vreg_ddr;
    volatile uint32_t *vreg_ext;
    uint32_t vreg_ddr_bit;
    uint32_t vreg_ext_bit;

    int irqno;
    struct key_event event;
    wait_queue_head_t wq_head;   //等待队列头
    int key_state;  //表示是否有按键数据到来，以唤醒阻塞休眠的标志位
};

//声明全局的设备对象
struct platform_keyirq_desc *keyirq_demo_dev = NULL;


static int platform_keyirqdrv_probe(struct platform_device *pdev);
static int platform_keyirqdrv_remove(struct platform_device *pdev);

//设置支持的设备对象name和id 
//id是用来区分如果设备name相同的时候进一步比较(通过在后面添加一个数字来代表不同的设备，因为有时候有这种需求) 
static const struct platform_device_id keyirqdev_id_table[] = {

    //平台总线还是比较少用，毕竟这个操作的是最原始的ioremap操作，而驱动开发更多的是从dts获取资源
    //同时，平台总线一般用在相同系列的soc更换的情景，比如瑞芯微rk3288 => rk3399，海思的hi3516 => hi3559
    //硬说要rk3559 => hi3559兼容两个系列的平台是不可取的
	{ "rk3288_keyirqdev", 0x3288 },
	{ "hi3559_keyirqdev", 0x3559 },
};

struct platform_driver platform_keyirqdrv = {
    .probe      = platform_keyirqdrv_probe,
    .remove     = platform_keyirqdrv_remove,
    .id_table   = keyirqdev_id_table,
    .driver = {
        .name           = "keyirqdrv",     //ls /sys/bus/platform/devices/keyirqdrv  在不制定id_table的情况下名称可以用作匹配平台驱动
    }
};

static int keyirq_demo_open(struct inode *inode, struct file *flip);
static int keyirq_demo_release(struct inode *inode, struct file *flip);
static ssize_t keyirq_demo_read(struct file *flip, char __user *buf, size_t len, loff_t *pos);
static ssize_t keyirq_demo_write(struct file *flip, const char __user *buf, size_t len, loff_t *pos);
static unsigned int keyirq_demo_poll(struct file *flip, struct poll_table_struct *pts);

static struct file_operations fops = {
    .owner      = THIS_MODULE,          //拥有该结构的模块的指针，一般为THIS_MODULES 这个成员用来在它的操作还在被使用时阻止模块被卸载
    .open       = keyirq_demo_open,     
    .release    = keyirq_demo_release,
    .read       = keyirq_demo_read,     
    .write      = keyirq_demo_write,
    .poll      = keyirq_demo_poll,
};

static 	irqreturn_t key_irq_handler(int irqno, void *devid)
{
    // 读取数据寄存器，获取gpio当前电平

    uint32_t val = *keyirq_demo_dev->vreg_ext & ((uint32_t)1 << keyirq_demo_dev->vreg_ext_bit);
    if (val)
    {
        //按下
        printk(KERN_ERR "%s,%s:%d key down, irqno： %d\n", __FILE__, __func__, __LINE__, irqno);
        keyirq_demo_dev->event.key = KEY_ENTER;
        keyirq_demo_dev->event.val = val;
    }
    else
    {
        //松开
        printk(KERN_ERR "%s,%s:%d key up, irqno： %d\n", __FILE__, __func__, __LINE__, irqno);
        keyirq_demo_dev->event.key = KEY_ENTER;
        keyirq_demo_dev->event.val = val;
    }
    //唤醒休眠，然后设置标志位
    wake_up_interruptible(&keyirq_demo_dev->wq_head);
    keyirq_demo_dev->key_state = 1;

    return IRQ_HANDLED;
}

static int platform_keyirqdrv_probe(struct platform_device *pdev)
{
    int ret = 0;
    //ls /sys/bus/platform/drivers/keyirqdrv/pdev_name
    printk(KERN_INFO "%s,%s:%d\n", __FILE__, __func__, __LINE__);
    //拿到寄存器地址，然后ioremap映射成虚拟地址，然后操作。。。。

    //GFP_KERNEL:如果当前空间不够用,该函数会一直阻塞
    keyirq_demo_dev = kmalloc(sizeof(struct platform_keyirq_desc), GFP_KERNEL);
    if (NULL == keyirq_demo_dev)
    {
        printk(KERN_ERR "%s,%s:%d kmalloc failed\n", __FILE__, __func__, __LINE__);
        ret = -ENOMEM;
        goto err0;
    }
    
    // 1.模块加载函数通过 register_chrdev_region 或 alloc_chrdev_region 来静态或者动态获取设备号;
    // 老版本内核中使用register_chrdev接口, 但是会造成资源浪费，新版内核被弃用.
    // 三个函数下面其实都是对__register_chrdev_region() 的调用
    // 通过命令查看效果：cat /proc/devices | grep keyirq_demo
    ret = alloc_chrdev_region(&keyirq_demo_dev->devno, BASEMINOR, COUNT, NAME);
    if(ret < 0)
    {
        printk(KERN_ERR "%s,%s:%d failed %d \n", __FILE__, __func__, __LINE__, ret);
        goto err1;
    }
    printk(KERN_INFO "%s,%s:%d> major = %d\n", __FILE__, __func__, __LINE__, MAJOR(keyirq_demo_dev->devno));
    
    // 2. 通过 cdev_init 建立cdev与 file_operations之间的连接
    cdev_init(&keyirq_demo_dev->cdev, &fops);

    // 3. 通过 cdev_add 向系统添加一个cdev以完成注册;
    ret = cdev_add(&keyirq_demo_dev->cdev, keyirq_demo_dev->devno, COUNT);
    if (ret < 0)
    {
        printk(KERN_ERR "%s,%s:%d failed %d \n", __FILE__, __func__, __LINE__, ret);
        goto err2;
    }
    
    // 4.新建chry class 通过命令查看效果：ls -l /sys/class/chry
    keyirq_demo_dev->cls = class_create(THIS_MODULE, CLASS);
    if(IS_ERR(keyirq_demo_dev->cls))
	{
        printk(KERN_ERR "%s,%s:%d failed %d \n", __FILE__, __func__, __LINE__, ret);
        ret = PTR_ERR(keyirq_demo_dev->cls);
        goto err3;
	}

    // 5.在chry class下创建节点, 相当于mknod /dev/keyirq_demo 通过命令查看效果：ls -l /sys/class/chry/keyirq_demo /dev/keyirq_demo
    keyirq_demo_dev->dev = device_create(keyirq_demo_dev->cls, NULL, keyirq_demo_dev->devno, NULL, NAME); 
    if(IS_ERR(keyirq_demo_dev->dev))
	{
        printk(KERN_ERR "%s,%s:%d failed %d \n", __FILE__, __func__, __LINE__, ret);
        ret = PTR_ERR(keyirq_demo_dev->dev);
        goto err4;
	}
    //============至此设备节点已经创建好，以上都是通用性代码，下面根据驱动需求干活===============
    // 6.1 获取物理寄存器地址 方向寄存器
    //platform_get_resource()参数1：从哪个设备获取资源；参数2：获取的资源类型；参数3：获取同类型资源的第几个(从0开始，注意不是获取对应的下标的资源)
    keyirq_demo_dev->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if(NULL == keyirq_demo_dev->res)
    {
        printk(KERN_ERR "%s,%s:%d\n\tplatform_get_resource failed\n",  __FILE__, __func__, __LINE__);
        ret = -1;
        goto err5;
    }
    // 6.2 映射虚拟寄存器地址 方向寄存器
    keyirq_demo_dev->vreg_ddr = (uint32_t *)ioremap(keyirq_demo_dev->res->start, resource_size(keyirq_demo_dev->res)); 
    if(NULL == keyirq_demo_dev->vreg_ddr)
    {
        printk(KERN_ERR "%s,%s:%d\n\tioremap ddr[0x%x, %d] failed\n", 
            __FILE__, __func__, __LINE__,  keyirq_demo_dev->res->start, resource_size(keyirq_demo_dev->res));
        ret = -1;
        goto err5;
    }

    // 7.1 获取物理寄存器地址 输入数据寄存器
    keyirq_demo_dev->res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
    if(NULL == keyirq_demo_dev->res)
    {
        printk(KERN_ERR "%s,%s:%d\n\tplatform_get_resource failed\n",  __FILE__, __func__, __LINE__);
        ret = -1;
        goto err6;
    }
    // 7.2 映射虚拟寄存器地址 输入数据寄存器
    keyirq_demo_dev->vreg_ext = (uint32_t *)ioremap(keyirq_demo_dev->res->start, resource_size(keyirq_demo_dev->res)); 
    if(NULL == keyirq_demo_dev->vreg_ddr)
    {
        printk(KERN_ERR "%s,%s:%d\n\tioremap ddr[0x%x, %d] failed\n", 
            __FILE__, __func__, __LINE__,  keyirq_demo_dev->res->start, resource_size(keyirq_demo_dev->res));
        ret = -1;
        goto err6;
    }

    // 8 获取方向寄存器地址对应的位
    keyirq_demo_dev->res = platform_get_resource(pdev, IORESOURCE_REG, 0);
    if(NULL == keyirq_demo_dev->res)
    {
        printk(KERN_ERR "%s,%s:%d\n\tplatform_get_resource failed\n",  __FILE__, __func__, __LINE__);
        ret = -1;
        goto err7;
    }
    keyirq_demo_dev->vreg_ddr_bit = keyirq_demo_dev->res->start;
    
    // 9 获取输入数据寄存器地址对应的位
    keyirq_demo_dev->res = platform_get_resource(pdev, IORESOURCE_REG, 1);
    if(NULL == keyirq_demo_dev->res)
    {
        printk(KERN_ERR "%s,%s:%d\n\tplatform_get_resource failed\n",  __FILE__, __func__, __LINE__);
        ret = -1;
        goto err7;
    }
    keyirq_demo_dev->vreg_ext_bit = keyirq_demo_dev->res->start;
    
    // 设置gpio输入输出方向为输入
    *keyirq_demo_dev->vreg_ddr &= ~((uint32_t)1 << keyirq_demo_dev->vreg_ddr_bit); 

    // 10.获取中断号  资源platform_get_irq效果等同于platform_get_resource(pdev, IORESOURCE_IRQ, n);但是前者返回值是int中断号
    keyirq_demo_dev->irqno = platform_get_irq(pdev, 0);
    //申请中断
    ret = request_irq(keyirq_demo_dev->irqno, key_irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, NAME, NULL);
    if(ret != 0)
    {
        printk(KERN_ERR "%s,%s:%d request_irq failed %d \n", __FILE__, __func__, __LINE__, ret);
        goto err7;
    }

    //初始化等待队列头
    init_waitqueue_head(&keyirq_demo_dev->wq_head);
    keyirq_demo_dev->key_state = 0;

    printk(KERN_ERR "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);

    return 0;
  
//err8:
    free_irq(keyirq_demo_dev->irqno, NULL);  
err7:
    iounmap(keyirq_demo_dev->vreg_ext);
err6:
    iounmap(keyirq_demo_dev->vreg_ddr);
err5:
    device_destroy(keyirq_demo_dev->cls, keyirq_demo_dev->devno);
err4:
    class_destroy(keyirq_demo_dev->cls);
err3:
    cdev_del(&keyirq_demo_dev->cdev);
err2:
    unregister_chrdev_region(keyirq_demo_dev->devno, COUNT);
err1:
    kfree(keyirq_demo_dev);
err0:
    return ret;
}

static int platform_keyirqdrv_remove(struct platform_device *pdev)
{
    free_irq(keyirq_demo_dev->irqno, NULL); 
    iounmap(keyirq_demo_dev->vreg_ext);
    iounmap(keyirq_demo_dev->vreg_ddr);
    device_destroy(keyirq_demo_dev->cls, keyirq_demo_dev->devno);
    class_destroy(keyirq_demo_dev->cls);
    cdev_del(&keyirq_demo_dev->cdev);
    unregister_chrdev_region(keyirq_demo_dev->devno, COUNT);
    kfree(keyirq_demo_dev);

    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
    return 0;
}

static int keyirq_demo_open(struct inode *inode, struct file *flip)
{
    printk(KERN_INFO "%s,%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}

// 执行命令：sudo cat /dev/keyirq_demo 可以从dmesg看到open、read、release被调用
// 执行命令：sudo sh -c "echo 1 > /dev/keyirq_demo" 可以从dmesg看到open、write、release被调用
static int keyirq_demo_release(struct inode *inode, struct file *flip)
{
    printk(KERN_INFO "%s,%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}

//使用阻塞方式获取key的状态
static ssize_t keyirq_demo_read(struct file *flip, char __user *buf, size_t len, loff_t *pos)
{
    ssize_t ret = 0; 
    //如果当前是非阻塞模式，并且没有数据，立马返回一个出错码
    if(flip->f_flags & O_NONBLOCK && !keyirq_demo_dev->key_state)
		return -EAGAIN;

    printk(KERN_INFO "%s,%s:%d pos:%lld\n", __FILE__, __func__, __LINE__, *pos);   

    //阻塞等待（直到keyirq_demo_dev->key_state = 1）
    wait_event_interruptible(keyirq_demo_dev->wq_head, keyirq_demo_dev->key_state); //不需要传地址，内部实现会取址

    if(len < sizeof(struct key_event))
    {
        return -EINVAL;
    }

    // 将内核空间的数据拷贝到用户空间 成功返回0，失败返回没有拷贝成功的数据字节数
    ret = copy_to_user(buf, &keyirq_demo_dev->event, sizeof(struct key_event));   
    if(0 != ret)
	{
        printk(KERN_ERR "%s,%s:%d %d\n", __FILE__, __func__, __LINE__, ret);
		return -EFAULT;	
	}

    //传递按键信息给用户空间后，把数据清空
    memset(&keyirq_demo_dev->event, 0, sizeof(struct key_event));
    keyirq_demo_dev->key_state = 0;
    return sizeof(struct key_event);
}

//向设备发送数据 规则：写入“0”设置成输出模式并置低电平，写入“1”设置成输出模式并置高电平，写入“2”设置成输入模式
static ssize_t keyirq_demo_write(struct file *flip, const char __user *buf, size_t len, loff_t *pos)
{   
    return 0;
}


static int __init platform_keyirqdrv_init(void)
{
    int ret = 0;

    // 将platform driver注册到总线中, 通过命令查看效果：ls -l /sys/bus/platform/devices/keyirqdrv
    ret = platform_driver_register(&platform_keyirqdrv);
    if (ret < 0)
    {
        printk(KERN_ERR "%s,%s:%d device_register failed\n", __FILE__, __func__, __LINE__);
        goto err0;
    }


    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);

    return 0;
    
err0:
    return ret;
}


static unsigned int keyirq_demo_poll(struct file *flip, struct poll_table_struct *pts)
{
    unsigned int mask = 0;

    //调用poll_wait，将当前的等待队列注册到系统中
    poll_wait(flip, &keyirq_demo_dev->wq_head, pts);
    
    if(!keyirq_demo_dev->key_state)
    {
        //无数据时返回0
        mask = 0;
    }
    else
    {
        //有数据时返回POLLIN
        mask |= POLL_IN;
    }

    printk(KERN_INFO "%s,%s:%d mask:%d\n", __FILE__, __func__, __LINE__, mask);   

    return mask;
}

static void __exit platform_keyirqdrv_exit(void)
{
    platform_driver_unregister(&platform_keyirqdrv);
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(platform_keyirqdrv_init);
module_exit(platform_keyirqdrv_exit);

MODULE_LICENSE("GPL");


