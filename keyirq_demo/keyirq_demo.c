#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>     // msleep_interruptible
#include <linux/uaccess.h>      // copy_from_user()
#include <asm/io.h>     // ioremap 头文件
#include <linux/slab.h>     //kmalloc

#include <linux/of_irq.h>     //irq
#include <linux/of_gpio.h>
#include <linux/interrupt.h>     //IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING
#include <linux/sched.h>     //
#include <linux/poll.h>     //poll_wait

#define BASEMINOR   0
#define COUNT       1
#define CLASS       "chry"
#define NAME        "keyirq_demo"

/*
# 获取寄存器地址
寄存器地址 = 基地址 + 偏移量

1.基地址获取
打开TRM文档的第2章第1节
address mapping可以看到地址分布图 

找到里面的GPIO0, 基地址为FF75_0000

2.偏移量
打开TRM文档的第49章第4节 该章描述GPIO相关
找到 GPIO_SWPORTA_DR 得到偏移为0x0000
找到 GPIO_SWPORTA_DDR 得到偏移为0x0004

# 寄存器地址位设置
GPIO_SWPORTA_DDR寄存器控制GPIO的输入输出方向，32位，分别对应ABCD四组共32个引脚，0表示输入1表示输出
GPIO_SWPORTA_DR寄存器是GPIO的数据位，32位，分别对应ABCD四组共32个引脚，如果方向是输出，置1表示输出高电平
*/
//GPIO Operational Base
#define RK3288_GPIO0_BASE       ((uint32_t)0xFF750000) 
//GPIO DATA direction REGISTER 
#define RK3288_GPIO0_DDR_BASE       ((uint32_t)RK3288_GPIO0_BASE+0x0004)   
#define RK3288_GPIO0A7_DDR_BIT      7
//GPIO DATA REGISTER
#define RK3288_GPIO0_DR_BASE        ((uint32_t)RK3288_GPIO0_BASE+0x0000)     
#define RK3288_GPIO0A7_DR_BIT       7   //[7:0]表示GPIOn_A[7:0]     [15:8]表示GPIOn_B[7:0]   [23:16]表示GPIOn_C[7:0]   [31:24]表示GPIOn_D[7:0] 
//GPIO external port register
#define RK3288_GPIO0_EXT_BASE        ((uint32_t)RK3288_GPIO0_BASE+0x0050)     
#define RK3288_GPIO0A7_EXT_BIT      7   //[7:0]表示GPIOn_A[7:0]     [15:8]表示GPIOn_B[7:0]   [23:16]表示GPIOn_C[7:0]   [31:24]表示GPIOn_D[7:0] 

#define USE_BIT_OP_MODE 0   //写了两种操作方式，bit与或方式和val读写方式

#define KEY_ENTER   28
//设计描述按键数据的对象 直接照抄输入子系统
struct key_event {
    int key;    //键值：W A S D ESC ENTER RESET....在input.h里面定义了所有的键值，也可以自定义
    int val;    //状态：1按下 0松开
};

//设计设备对象
struct keyirq_demo_desc {
    struct cdev cdev;
    dev_t devno;  
    struct class *cls;
    struct device *dev;
    struct resource *res;
    volatile uint32_t *gpio0_ddr_vreg;  //方向寄存器 内存映射后的地址
    volatile uint32_t *gpio0_dr_vreg;   //数据寄存器 内存映射后的地址
    volatile uint32_t *gpio0_ext_vreg;  //输入寄存器 内存映射后的地址   输入模式时，从该寄存器获取当前外部输入状态

    int irqno;
    struct key_event event;
    wait_queue_head_t wq_head;   //等待队列头
    int key_state;  //表示是否有按键数据到来，以唤醒阻塞休眠的标志位
};

//声明全局的设备对象
struct keyirq_demo_desc *keyirq_demo_dev = NULL;

static int keyirq_demo_open(struct inode *inode, struct file *flip);
static int keyirq_demo_release(struct inode *inode, struct file *flip);
static ssize_t keyirq_demo_read(struct file *flip, char __user *buf, size_t len, loff_t *pos);
static ssize_t keyirq_demo_write(struct file *flip, const char __user *buf, size_t len, loff_t *pos);
static unsigned int keyirq_demo_poll(struct file *flip, struct poll_table_struct *pts);

struct file_operations fops = {
    .owner      = THIS_MODULE,          //拥有该结构的模块的指针，一般为THIS_MODULES 这个成员用来在它的操作还在被使用时阻止模块被卸载
    .open       = keyirq_demo_open,     
    .release    = keyirq_demo_release,
    .read       = keyirq_demo_read,
    .write      = keyirq_demo_write,
    .poll      = keyirq_demo_poll,
};

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

//从已经在dts自定义好的节点里面获取irqno
static int get_irqno_from_dts_node(void)
{
    int irqno = -1;
    int gpio = -1;
    //1.获取到设备树中的节点
    struct device_node *np = of_find_node_by_path("/chry_gpio0a7_node");
    if (NULL == np)
    {
        printk(KERN_ERR "%s,%s:%d find node failed\n", __FILE__, __func__, __LINE__);
        goto exit;
    }

    //2.通过节点获取gpio号码
    gpio = of_get_named_gpio(np, "chry-gpios", 0);
    if (!gpio_is_valid(gpio))
    {
        printk(KERN_ERR "%s,%s:%d of_get_named_gpio failed\n", __FILE__, __func__, __LINE__);
        goto exit;
    }

    //3.通过gpio号码获取中断号码
    irqno = __gpio_to_irq(gpio);
    //irqno = irq_of_parse_and_map(np, 0);    //第二个参数0表示第0个中断，因为自定义的设备树节点只定义了一个中断
    if (0 > irqno)
    {
        printk(KERN_ERR "%s,%s:%d __gpio_to_irq failed\n", __FILE__, __func__, __LINE__);
        goto exit;
    }

    printk(KERN_INFO "%s,%s:%d ojbk! irqno： %d\n", __FILE__, __func__, __LINE__, irqno);
exit:
    return irqno;
}

static 	irqreturn_t key_irq_handler(int irqno, void *devid)
{
    // 读取数据寄存器，获取gpio当前电平

    uint32_t val = *keyirq_demo_dev->gpio0_ext_vreg & ((uint32_t)1 << RK3288_GPIO0A7_EXT_BIT);
    //uint32_t val = readl(keyirq_demo_dev->gpio0_ext_vreg) & (1 << RK3288_GPIO0A7_DDR_BIT);
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

static int __init keyirq_demo_init(void)
{
    int ret = 0;
    
    //为全局的设备对象申请空间
    //GFP_KERNEL:如果当前空间不够用,该函数会一直阻塞
    keyirq_demo_dev = kmalloc(sizeof(struct keyirq_demo_desc), GFP_KERNEL);
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
    if((ret = IS_ERR(keyirq_demo_dev->cls)))
	{
        printk(KERN_ERR "%s,%s:%d failed %d \n", __FILE__, __func__, __LINE__, ret);
        goto err3;
	}

    // 5.在chry class下创建节点, 相当于mknod /dev/keyirq_demo 通过命令查看效果：ls -l /sys/class/chry/keyirq_demo /dev/keyirq_demo
    keyirq_demo_dev->dev = device_create(keyirq_demo_dev->cls, NULL, keyirq_demo_dev->devno, NULL, NAME); 
    if((ret = IS_ERR(keyirq_demo_dev->dev)))
	{
        printk(KERN_ERR "%s,%s:%d failed %d \n", __FILE__, __func__, __LINE__, ret);
        goto err4;
	}
    //-------------------------------------------------------------------------
    //硬件初始化：地址映射或者申请中断号
    //-------------------------------------------------------------------------
    // 6.映射虚拟寄存器地址 uint32_t是4字节  控制输入输出方向
    keyirq_demo_dev->gpio0_ddr_vreg = (uint32_t *)ioremap(RK3288_GPIO0_DDR_BASE, 4); 
    if(NULL == keyirq_demo_dev->gpio0_ddr_vreg)
    {
        printk(KERN_ERR "%s,%s:%d\n\tioremap[%p, 0x%x] failed\n", 
            __FILE__, __func__, __LINE__,  keyirq_demo_dev->gpio0_ddr_vreg, RK3288_GPIO0_DDR_BASE);
        ret = -1;
        goto err5;
    }

    // 7.映射虚拟寄存器地址 uint32_t是4字节  数据寄存器
    keyirq_demo_dev->gpio0_dr_vreg  = (uint32_t *)ioremap(RK3288_GPIO0_DR_BASE, 4); 
    if(NULL == keyirq_demo_dev->gpio0_dr_vreg)
    {
        printk(KERN_ERR "%s,%s:%d\n\tioremap[%p, 0x%x] failed\n", 
            __FILE__, __func__, __LINE__, keyirq_demo_dev->gpio0_dr_vreg, RK3288_GPIO0_DR_BASE);
        ret = -1;
        goto err6;
    }

    // 8.映射虚拟寄存器地址 uint32_t是4字节  外部输入寄存器
    keyirq_demo_dev->gpio0_ext_vreg  = (uint32_t *)ioremap(RK3288_GPIO0_EXT_BASE, 4); 
    if(NULL == keyirq_demo_dev->gpio0_ext_vreg)
    {
        printk(KERN_ERR "%s,%s:%d\n\tioremap[%p, 0x%x] failed\n", 
            __FILE__, __func__, __LINE__, keyirq_demo_dev->gpio0_ext_vreg, RK3288_GPIO0_EXT_BASE);
        ret = -1;
        goto err7;
    }

    //-------------------------------------------------------------------------
    // 设置gpio输入输出方向为输入
    *keyirq_demo_dev->gpio0_ddr_vreg &= ~((uint32_t)1 << RK3288_GPIO0A7_DDR_BIT); 

    keyirq_demo_dev->irqno = get_irqno_from_dts_node();
    //申请中断
    /*
    参数1： irq 	设备对应的中断号
    参数2： handler 	中断的处理函数 
        typedef irqreturn_t (*irq_handler_t)(int, void *);
    参数3：flags 	触发方式
        #define IRQF_TRIGGER_NONE	0x00000000  //内部控制器触发中断的时候的标志
        #define IRQF_TRIGGER_RISING	0x00000001 //上升沿
        #define IRQF_TRIGGER_FALLING    0x00000002 //下降沿
        #define IRQF_TRIGGER_HIGH	0x00000004  // 高点平
        #define IRQF_TRIGGER_LOW	0x00000008 //低电平触发
    参数4：name 	中断的描述，自定义,主要是给用户查看的
        /proc/interrupts
    参数5：dev 	传递给参数2中函数指针的值
    返回值： 正确为0，错误非0
    */
    ret = request_irq(keyirq_demo_dev->irqno, key_irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, NAME, NULL);
    if(ret != 0)
    {
        printk(KERN_ERR "%s,%s:%d request_irq failed %d \n", __FILE__, __func__, __LINE__, ret);
        goto err8;
    }

    //初始化等待队列头
    init_waitqueue_head(&keyirq_demo_dev->wq_head);
    keyirq_demo_dev->key_state = 0;

    printk(KERN_ERR "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
    return 0;

//err9:
    free_irq(keyirq_demo_dev->irqno, NULL);  
err8:
    iounmap(keyirq_demo_dev->gpio0_ext_vreg);
err7:
    iounmap(keyirq_demo_dev->gpio0_dr_vreg);
err6:
    iounmap(keyirq_demo_dev->gpio0_ddr_vreg);
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


static void __exit keyirq_demo_exit(void)
{
    //释放中断
    free_irq(keyirq_demo_dev->irqno, NULL);  
    // 释放映射
    iounmap(keyirq_demo_dev->gpio0_ext_vreg);
    iounmap(keyirq_demo_dev->gpio0_ddr_vreg);
    iounmap(keyirq_demo_dev->gpio0_dr_vreg);

    device_destroy(keyirq_demo_dev->cls, keyirq_demo_dev->devno);
    class_destroy(keyirq_demo_dev->cls);
    cdev_del(&keyirq_demo_dev->cdev);
    unregister_chrdev_region(keyirq_demo_dev->devno, COUNT);
    kfree(keyirq_demo_dev);

    printk(KERN_ERR "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(keyirq_demo_init);
module_exit(keyirq_demo_exit);

MODULE_LICENSE("GPL");

