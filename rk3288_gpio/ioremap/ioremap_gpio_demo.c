#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>     // msleep_interruptible
#include <linux/uaccess.h>      // copy_from_user()
#include <asm/io.h>     // ioremap 头文件
#include <linux/slab.h>     //kmalloc

#define BASEMINOR   0
#define COUNT       1
#define CLASS       "chry"
#define NAME        "gpio_demo"

/*
# 获取寄存器地址
寄存器地址 = 基地址 + 偏移量

1.基地址获取
打开TRM文档的第2章第1节
address mapping可以看到地址分布图 

找到里面的GPIO0, 基地址为FF75_0000

2.偏移量
打开TRM文档的第49章第4节 该章描述GPIO相关 仔细看每一行，不要漏掉任何一行，操你妈的英文废物
找到 GPIO_SWPORTA_DR 得到偏移为0x0000
找到 GPIO_SWPORTA_DDR 得到偏移为0x0004
找到 GPIO_EXT_PORTA 得到偏移为0x0050

# 寄存器地址位设置
GPIO_SWPORTA_DDR寄存器控制GPIO的输入输出方向，32位，分别对应ABCD四组共32个引脚，0表示输入1表示输出
GPIO_SWPORTA_DR寄存器是GPIO输出模式的数据位，32位，分别对应ABCD四组共32个引脚，如果方向是输出，置1表示输出高电平
GPIO_EXT_PORTA寄存器是GPIO输入模式的数据位，32位，分别对应ABCD四组共32个引脚，如果方向是输出，置1表示输出高电平

默认输入模式上拉
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

//设计设备对象
struct gpio_demo_desc {
    struct cdev cdev;
    dev_t devno;  
    struct class *cls;
    struct device *dev;
    struct resource *res;
    volatile uint32_t *gpio0_ddr_vreg;  //方向寄存器 内存映射后的地址
    volatile uint32_t *gpio0_dr_vreg;   //数据寄存器 内存映射后的地址
    volatile uint32_t *gpio0_ext_vreg;  //输入寄存器 内存映射后的地址   输入模式时，从该寄存器获取当前外部输入状态
};

//声明全局的设备对象
struct gpio_demo_desc *gpio_demo_dev = NULL;

static int gpio_demo_open(struct inode *inode, struct file *flip);
static int gpio_demo_release(struct inode *inode, struct file *flip);
static ssize_t gpio_demo_read(struct file *flip, char __user *buf, size_t len, loff_t *pos);
static ssize_t gpio_demo_write(struct file *flip, const char __user *buf, size_t len, loff_t *pos);

struct file_operations fops = {
    .owner      = THIS_MODULE,          //拥有该结构的模块的指针，一般为THIS_MODULES 这个成员用来在它的操作还在被使用时阻止模块被卸载
    .open       = gpio_demo_open,     
    .release    = gpio_demo_release,
    .read       = gpio_demo_read,     
    .write      = gpio_demo_write,
};

static int gpio_demo_open(struct inode *inode, struct file *flip)
{
    printk(KERN_INFO "%s,%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}

// 执行命令：sudo cat /dev/gpio_demo 可以从dmesg看到open、read、release被调用
// 执行命令：sudo sh -c "echo 1 > /dev/gpio_demo" 可以从dmesg看到open、write、release被调用
static int gpio_demo_release(struct inode *inode, struct file *flip)
{
    printk(KERN_INFO "%s,%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}

//从设备中同步读取数据 返回当前gpio0a6对应的输入输出方向和数据  pos指示从文件的哪里开始读取
static ssize_t gpio_demo_read(struct file *flip, char __user *buf, size_t len, loff_t *pos)
{
    char data[4] = {0};   //返回到用户空间的数据
    ssize_t ret = 0; 
    uint32_t ddr = *gpio_demo_dev->gpio0_ddr_vreg & ((uint32_t)1 << RK3288_GPIO0A7_DDR_BIT);
    uint32_t dr = *gpio_demo_dev->gpio0_dr_vreg & ((uint32_t)1 << RK3288_GPIO0A7_DR_BIT);
    uint32_t ext = *gpio_demo_dev->gpio0_ext_vreg & ((uint32_t)1 << RK3288_GPIO0A7_EXT_BIT);

    printk(KERN_INFO "%s,%s:%d pos:%lld\n", __FILE__, __func__, __LINE__, *pos);   

    if(*pos >= 3)
    {
        return 0;
    }

    if(len < 2)
    {
        return -ENOMEM;
    }

    //printk(KERN_ERR "%s,%s:%d gpiod_get_direction ddr[%d], dr[%d]\n", __FILE__, __func__, __LINE__, gpiod_get_direction(gpio_to_desc(7)), gpiod_get_value(gpio_to_desc(7)));  //#include <asm/gpio.h>     // GPIO头文件
    //printk(KERN_ERR "%s,%s:%d gpiod_get_direction ddr[%x], dr[%x], ext[%x]\n", __FILE__, __func__, __LINE__, ddr, dr, ext);
    //printk(KERN_ERR "%s,%s:%d gpiod_get_direction ddr[%x], dr[%x], ext[%x]\n", __FILE__, __func__, __LINE__, *gpio_demo_dev->gpio0_ddr_vreg, *gpio_demo_dev->gpio0_dr_vreg, *gpio_demo_dev->gpio0_ext_vreg);

    if (0 == ddr)
    {
        //输入模式
        data[0] = '1';
        ext ? (data[1] = '1') : (data[1] = '0');
    }
    else
    {
        // 输出模式
        data[0] = '0';
        dr ? (data[1] = '1') : (data[1] = '0');
    }

    data[2] = '\n';     //换行符 cat的时候打印好看一点

    // 将内核空间的数据拷贝到用户空间 成功返回0，失败返回没有拷贝成功的数据字节数
    ret = copy_to_user(buf, data, 3);   
    if(0 != ret)
	{
        printk(KERN_ERR "%s,%s:%d %d\n", __FILE__, __func__, __LINE__, ret);
		return -EFAULT;	
	}
    *pos += 3;

    //gpio_demo_dev->gpio0_ddr_vreg
    printk(KERN_ERR "%s,%s:%d=> %c%c\n", __FILE__, __func__, __LINE__, data[0], data[1]);
    return 3;
}

//向设备发送数据 规则：写入“0”设置成输出模式并置低电平，写入“1”设置成输出模式并置高电平，写入“2”设置成输入模式
static ssize_t gpio_demo_write(struct file *flip, const char __user *buf, size_t len, loff_t *pos)
{   
    char data[2];   //存放用户空间传进来的数据

    //这里不能直接打印用户空间传进来的数据
    printk(KERN_INFO "%s,%s:%d len:%d\n", __FILE__, __func__, __LINE__, len);   

    if(len < 1 || len > 2)    
    {
        return -EINVAL;
    }

    // 将用户空间的数据拷贝到内核空间 不能在内核空间直接访问用户空间的数据，比如打印判断等操作 访问权限问题会导致内核崩溃
    if(copy_from_user(data, buf, len))
	{
		return -EFAULT;	
	}

    if (len == 2 && data[1] != '\n')     //允许加\n
    {
        return -EINVAL;
    }
    
    printk(KERN_INFO "%s,%s:%d => %c\n", __FILE__, __func__, __LINE__, data[0]);

    switch (data[0])
    {
    case '0':
        // 设置gpio输入输出方向为输出
        *gpio_demo_dev->gpio0_ddr_vreg |= 1 << RK3288_GPIO0A7_DDR_BIT;
        // 设置gpio输出低电平
        *gpio_demo_dev->gpio0_dr_vreg &= ~((uint32_t)1 << RK3288_GPIO0A7_DR_BIT); 
        break;
    case '1':
        // 设置gpio输入输出方向为输出
        *gpio_demo_dev->gpio0_ddr_vreg |= 1 << RK3288_GPIO0A7_DDR_BIT;
        // 设置gpio输出高电平
        *gpio_demo_dev->gpio0_dr_vreg |= 1 << RK3288_GPIO0A7_DR_BIT;
        break;
    case '2':
        // 设置gpio输入输出方向为输入
        *gpio_demo_dev->gpio0_ddr_vreg &= ~((uint32_t)1 << RK3288_GPIO0A7_DDR_BIT); 
        break;
    default:
        return -EINVAL;
        break;
    }
    printk(KERN_INFO "%s,%s:%d len:%d\n", __FILE__, __func__, __LINE__, len);
    return len;     //使用echo 1 > /dev/gpio_demo, 如果返回0 echo就会一直重试 所以要么返回小于0的错误码 要么返回实际值
}

static int __init gpio_demo_init(void)
{
    int ret = 0;
#if !USE_BIT_OP_MODE
    uint32_t val = 0;
#endif

    //GFP_KERNEL:如果当前空间不够用,该函数会一直阻塞
    gpio_demo_dev = kmalloc(sizeof(struct gpio_demo_desc), GFP_KERNEL);
    if (NULL == gpio_demo_dev)
    {
        printk(KERN_ERR "%s,%s:%d kmalloc failed\n", __FILE__, __func__, __LINE__);
        ret = -ENOMEM;
        goto err0;
    }

    // 1.模块加载函数通过 register_chrdev_region 或 alloc_chrdev_region 来静态或者动态获取设备号;
    // 老版本内核中使用register_chrdev接口, 但是会造成资源浪费，新版内核被弃用.
    // 三个函数下面其实都是对__register_chrdev_region() 的调用
    // 通过命令查看效果：cat /proc/devices | grep gpio_demo
    ret = alloc_chrdev_region(&gpio_demo_dev->devno, BASEMINOR, COUNT, NAME);
    if(ret < 0)
    {
        printk(KERN_ERR "%s,%s:%d failed %d \n", __FILE__, __func__, __LINE__, ret);
        goto err1;
    }
    printk(KERN_INFO "%s,%s:%d> major = %d\n", __FILE__, __func__, __LINE__, MAJOR(gpio_demo_dev->devno));
    
    // 2. 通过 cdev_init 建立cdev与 file_operations之间的连接
    cdev_init(&gpio_demo_dev->cdev, &fops);

    // 3. 通过 cdev_add 向系统添加一个cdev以完成注册;
    ret = cdev_add(&gpio_demo_dev->cdev, gpio_demo_dev->devno, COUNT);
    if (ret < 0)
    {
        printk(KERN_ERR "%s,%s:%d failed %d \n", __FILE__, __func__, __LINE__, ret);
        goto err2;
    }
    
    // 4.新建chry class 通过命令查看效果：ls -l /sys/class/chry
    gpio_demo_dev->cls = class_create(THIS_MODULE, CLASS);
    if((ret = IS_ERR(gpio_demo_dev->cls)))
	{
        printk(KERN_ERR "%s,%s:%d failed %d \n", __FILE__, __func__, __LINE__, ret);
        goto err3;
	}

    // 5.在chry class下创建节点, 相当于mknod /dev/gpio_demo 通过命令查看效果：ls -l /sys/class/chry/gpio_demo /dev/gpio_demo
    gpio_demo_dev->dev = device_create(gpio_demo_dev->cls, NULL, gpio_demo_dev->devno, NULL, NAME); 
    if((ret = IS_ERR(gpio_demo_dev->dev)))
	{
        printk(KERN_ERR "%s,%s:%d failed %d \n", __FILE__, __func__, __LINE__, ret);
        goto err4;
	}

    // 6.映射虚拟寄存器地址 uint32_t是4字节  控制输入输出方向
    gpio_demo_dev->gpio0_ddr_vreg = (uint32_t *)ioremap(RK3288_GPIO0_DDR_BASE, 4); 
    if(NULL == gpio_demo_dev->gpio0_ddr_vreg)
    {
        printk(KERN_ERR "%s,%s:%d\n\tioremap[%p, 0x%x] failed\n", 
            __FILE__, __func__, __LINE__,  gpio_demo_dev->gpio0_ddr_vreg, RK3288_GPIO0_DDR_BASE);
        ret = -1;
        goto err5;
    }

    // 7.映射虚拟寄存器地址 uint32_t是4字节  数据寄存器
    gpio_demo_dev->gpio0_dr_vreg  = (uint32_t *)ioremap(RK3288_GPIO0_DR_BASE, 4); 
    if(NULL == gpio_demo_dev->gpio0_dr_vreg)
    {
        printk(KERN_ERR "%s,%s:%d\n\tioremap[%p, 0x%x] failed\n", 
            __FILE__, __func__, __LINE__, gpio_demo_dev->gpio0_dr_vreg, RK3288_GPIO0_DR_BASE);
        ret = -1;
        goto err6;
    }

    // 8.映射虚拟寄存器地址 uint32_t是4字节  外部输入寄存器
    gpio_demo_dev->gpio0_ext_vreg  = (uint32_t *)ioremap(RK3288_GPIO0_EXT_BASE, 4); 
    if(NULL == gpio_demo_dev->gpio0_ext_vreg)
    {
        printk(KERN_ERR "%s,%s:%d\n\tioremap[%p, 0x%x] failed\n", 
            __FILE__, __func__, __LINE__, gpio_demo_dev->gpio0_ext_vreg, RK3288_GPIO0_EXT_BASE);
        ret = -1;
        goto err7;
    }

#if USE_BIT_OP_MODE
    // 设置gpio输入输出方向为输出
    *gpio_demo_dev->gpio0_ddr_vreg |= 1 << RK3288_GPIO0A7_DDR_BIT;
    // 设置gpio输出高电平
    *gpio_demo_dev->gpio0_dr_vreg |= 1 << RK3288_GPIO0A7_DR_BIT;

    msleep_interruptible(500);

    // 设置gpio输出低电平
    *gpio_demo_dev->gpio0_dr_vreg &= ~((uint32_t)1 << RK3288_GPIO0A7_DR_BIT); 

#else
    // 设置gpio输入输出方向为输出
    val = readl(gpio_demo_dev->gpio0_ddr_vreg);
    val |= 1 << RK3288_GPIO0A7_DDR_BIT;
    writel(val, gpio_demo_dev->gpio0_ddr_vreg);
   
    // 设置gpio输出高电平
    val = readl(gpio_demo_dev->gpio0_dr_vreg);
    val |= 1 << RK3288_GPIO0A7_DR_BIT;
    writel(val, gpio_demo_dev->gpio0_dr_vreg);   

    msleep_interruptible(500);

    // 设置gpio输出低电平
    val = readl(gpio_demo_dev->gpio0_dr_vreg);
    val &= ~(1 << RK3288_GPIO0A7_DR_BIT);
    writel(val, gpio_demo_dev->gpio0_dr_vreg);  
#endif

    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
    return 0;

//err8:
    iounmap(gpio_demo_dev->gpio0_ext_vreg);
err7:
    iounmap(gpio_demo_dev->gpio0_dr_vreg);
err6:
    iounmap(gpio_demo_dev->gpio0_ddr_vreg);
err5:
    device_destroy(gpio_demo_dev->cls, gpio_demo_dev->devno);
err4:
    class_destroy(gpio_demo_dev->cls);
err3:
    cdev_del(&gpio_demo_dev->cdev);
err2:
    unregister_chrdev_region(gpio_demo_dev->devno, COUNT);
err1:
    kfree(gpio_demo_dev);
err0:
    return ret;
}


static void __exit gpio_demo_exit(void)
{
#if USE_BIT_OP_MODE
    // 设置gpio输出低电平
    *gpio_demo_dev->gpio0_dr_vreg &= ~((uint32_t)1 << RK3288_GPIO0A7_DR_BIT); 
#else
    uint32_t val;
    // 设置gpio输出低电平
    val = readl(gpio_demo_dev->gpio0_dr_vreg);
    val &= ~(1 << RK3288_GPIO0A7_DR_BIT);
    writel(val, gpio_demo_dev->gpio0_dr_vreg);   
#endif
    // 释放映射
    iounmap(gpio_demo_dev->gpio0_ext_vreg);
    iounmap(gpio_demo_dev->gpio0_ddr_vreg);
    iounmap(gpio_demo_dev->gpio0_dr_vreg);

    device_destroy(gpio_demo_dev->cls, gpio_demo_dev->devno);
    class_destroy(gpio_demo_dev->cls);
    cdev_del(&gpio_demo_dev->cdev);
    unregister_chrdev_region(gpio_demo_dev->devno, COUNT);
    kfree(gpio_demo_dev);

    
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(gpio_demo_init);
module_exit(gpio_demo_exit);

MODULE_LICENSE("GPL");

