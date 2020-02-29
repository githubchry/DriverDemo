#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>     //kmalloc
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>  //struct platform_device_id
#include <linux/uaccess.h>      // copy_from_user()
#include <linux/delay.h>    //msleep_interruptible

#define BASEMINOR   0
#define COUNT       1
#define CLASS       "chry"
#define NAME        "gpio_demo"

//设计设备对象
struct platform_gpio_desc {
    struct cdev cdev;
    dev_t devno;  
    struct class *cls;
    struct device *dev;
    struct resource *res;
    volatile uint32_t *vreg_ddr;
    volatile uint32_t *vreg_dr;
    uint32_t vreg_ddr_bit;
    uint32_t vreg_dr_bit;
};

//声明全局的设备对象
struct platform_gpio_desc *gpio_demo_dev = NULL;


static int platform_gpiodrv_probe(struct platform_device *pdev);
static int platform_gpiodrv_remove(struct platform_device *pdev);

//设置支持的设备对象name和id 
//id是用来区分如果设备name相同的时候进一步比较(通过在后面添加一个数字来代表不同的设备，因为有时候有这种需求) 
static const struct platform_device_id gpiodev_id_table[] = {
	{ "rk3288_gpiodev", 0x3288 },
	{ "hi3559_gpiodev", 0x3559 },
};

struct platform_driver platform_gpiodrv = {
    .probe      = platform_gpiodrv_probe,
    .remove     = platform_gpiodrv_remove,
    .id_table   = gpiodev_id_table,
    .driver = {
        .name           = "gpiodrv",     //ls /sys/bus/platform/devices/gpiodrv  在不制定id_table的情况下名称可以用作匹配平台驱动
    }
};

static int gpio_demo_open(struct inode *inode, struct file *flip);
static int gpio_demo_release(struct inode *inode, struct file *flip);
static ssize_t gpio_demo_read(struct file *flip, char __user *buf, size_t len, loff_t *pos);
static ssize_t gpio_demo_write(struct file *flip, const char __user *buf, size_t len, loff_t *pos);

static struct file_operations fops = {
    .owner      = THIS_MODULE,          //拥有该结构的模块的指针，一般为THIS_MODULES 这个成员用来在它的操作还在被使用时阻止模块被卸载
    .open       = gpio_demo_open,     
    .release    = gpio_demo_release,
    .read       = gpio_demo_read,     
    .write      = gpio_demo_write,
};

static int platform_gpiodrv_probe(struct platform_device *pdev)
{
    int ret = 0;
    //ls /sys/bus/platform/drivers/gpiodrv/pdev_name
    printk(KERN_INFO "%s,%s:%d\n", __FILE__, __func__, __LINE__);
    //拿到寄存器地址，然后ioremap映射成虚拟地址，然后操作。。。。

    //GFP_KERNEL:如果当前空间不够用,该函数会一直阻塞
    gpio_demo_dev = kmalloc(sizeof(struct platform_gpio_desc), GFP_KERNEL);
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
    if(IS_ERR(gpio_demo_dev->cls))
	{
        printk(KERN_ERR "%s,%s:%d failed %d \n", __FILE__, __func__, __LINE__, ret);
        ret = PTR_ERR(gpio_demo_dev->cls);
        goto err3;
	}

    // 5.在chry class下创建节点, 相当于mknod /dev/gpio_demo 通过命令查看效果：ls -l /sys/class/chry/gpio_demo /dev/gpio_demo
    gpio_demo_dev->dev = device_create(gpio_demo_dev->cls, NULL, gpio_demo_dev->devno, NULL, NAME); 
    if(IS_ERR(gpio_demo_dev->dev))
	{
        printk(KERN_ERR "%s,%s:%d failed %d \n", __FILE__, __func__, __LINE__, ret);
        ret = PTR_ERR(gpio_demo_dev->dev);
        goto err4;
	}
    //============至此设备节点已经创建好，以上都是通用性代码，下面根据驱动需求干活===============
    // 6.获取匹配到的设备的资源信息 
    //platform_get_resource()参数1：从哪个设备获取资源；参数2：获取的资源类型；参数3：获取同类型资源的第几个(从0开始，注意不是获取对应的下标的资源)
    gpio_demo_dev->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if(NULL == gpio_demo_dev->res)
    {
        printk(KERN_ERR "%s,%s:%d\n\tplatform_get_resource failed\n",  __FILE__, __func__, __LINE__);
        ret = -1;
        goto err5;
    }
    // 7.映射虚拟寄存器地址 输入输出方向寄存器
    gpio_demo_dev->vreg_ddr = (uint32_t *)ioremap(gpio_demo_dev->res->start, resource_size(gpio_demo_dev->res)); 
    if(NULL == gpio_demo_dev->vreg_ddr)
    {
        printk(KERN_ERR "%s,%s:%d\n\tioremap ddr[0x%llx, %lld] failed\n", 
            __FILE__, __func__, __LINE__,  gpio_demo_dev->res->start, resource_size(gpio_demo_dev->res));
        ret = -1;
        goto err5;
    }

    gpio_demo_dev->res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
    if(NULL == gpio_demo_dev->res)
    {
        printk(KERN_ERR "%s,%s:%d\n\tplatform_get_resource failed\n",  __FILE__, __func__, __LINE__);
        ret = -1;
        goto err6;
    }
    // 7.映射虚拟寄存器地址 数据寄存器
    gpio_demo_dev->vreg_dr = (uint32_t *)ioremap(gpio_demo_dev->res->start, resource_size(gpio_demo_dev->res)); 
    if(NULL == gpio_demo_dev->vreg_dr)
    {
        printk(KERN_ERR "%s,%s:%d\n\tioremap dr[0x%llx, %lld] failed\n", 
            __FILE__, __func__, __LINE__,  gpio_demo_dev->res->start, resource_size(gpio_demo_dev->res));
        ret = -1;
        goto err6;
    }

    //获取中断资源platform_get_irq效果等同于platform_get_resource(pdev, IORESOURCE_IRQ, n);但是前者返回值是int中断号
    //这里没用到irq仅仅作演示 设备驱动那边填的是地址位
    //int irqno = gpio_demo_dev->vreg_ddr_bit;
    gpio_demo_dev->vreg_ddr_bit = gpio_demo_dev->vreg_ddr_bit;
    gpio_demo_dev->vreg_dr_bit = gpio_demo_dev->vreg_dr_bit;

    // 8.硬件初始化 默认先配置ddr寄存器为输出 dr寄存器输出低电平
    // 设置gpio输入输出方向为输出
    *gpio_demo_dev->vreg_ddr |= 1 << gpio_demo_dev->vreg_ddr_bit;
    // 设置gpio输出高电平
    *gpio_demo_dev->vreg_dr |= 1 << gpio_demo_dev->vreg_dr_bit;
    msleep_interruptible(500);
    // 设置gpio输出低电平
    *gpio_demo_dev->vreg_dr  &= ~((uint32_t)1 << gpio_demo_dev->vreg_dr_bit); 

/*
    //也可以用readl/writel 不作演示
    // 设置gpio输入输出方向为输出
    val = readl(gpio0_ddr_vreg);
    val |= 1 << RK3288_GPIO0A7_DDR_BIT;
    writel(val, gpio0_ddr_vreg);
   
    // 设置gpio输出高电平
    val = readl(gpio0_dr_vreg);
    val |= 1 << RK3288_GPIO0A7_DDR_BIT;
    writel(val, gpio0_dr_vreg);   

    msleep_interruptible(500);

    // 设置gpio输出低电平
    val = readl(gpio0_dr_vreg);
    val &= ~(1 << RK3288_GPIO0A7_DDR_BIT);
    writel(val, gpio0_dr_vreg);  
*/

    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);

    return 0;
    
//err7:
    iounmap(gpio_demo_dev->vreg_dr);
err6:
    iounmap(gpio_demo_dev->vreg_ddr);
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

static int platform_gpiodrv_remove(struct platform_device *pdev)
{
    // 设置gpio输出低电平
    *gpio_demo_dev->vreg_dr  &= ~((uint32_t)1 << gpio_demo_dev->vreg_dr_bit); 
    
    iounmap(gpio_demo_dev->vreg_dr);
    iounmap(gpio_demo_dev->vreg_ddr);
    device_destroy(gpio_demo_dev->cls, gpio_demo_dev->devno);
    class_destroy(gpio_demo_dev->cls);
    cdev_del(&gpio_demo_dev->cdev);
    unregister_chrdev_region(gpio_demo_dev->devno, COUNT);
    kfree(gpio_demo_dev);

    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
    return 0;
}

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
    uint32_t ddr = *gpio_demo_dev->vreg_ddr & ((uint32_t)1 << gpio_demo_dev->vreg_ddr_bit);
    uint32_t dr = *gpio_demo_dev->vreg_dr & ((uint32_t)1 << gpio_demo_dev->vreg_dr_bit);

    printk(KERN_INFO "%s,%s:%d pos:%lld\n", __FILE__, __func__, __LINE__, *pos);   

    if(*pos >= 3)
    {
        return 0;
    }

    if(len < 2)
    {
        return -ENOMEM;
    }

    ddr ? (data[0] = '1') : (data[0] = '0');
    dr ? (data[1] = '1') : (data[1] = '0');

    data[2] = '\n';     //换行符 cat的时候打印好看一点

    // 将内核空间的数据拷贝到用户空间 成功返回0，失败返回没有拷贝成功的数据字节数
    ret = copy_to_user(buf, data, 3);   
    if(0 != ret)
	{
        printk(KERN_ERR "%s,%s:%d %ld\n", __FILE__, __func__, __LINE__, ret);
		return -EFAULT;	
	}
    *pos += 3;

    //gpio0_ddr_vreg
    printk(KERN_INFO "%s,%s:%d=> %c%c\n", __FILE__, __func__, __LINE__, data[0], data[1]);
    return 3;
}

//向设备发送数据 规则：写入“0”设置成输出模式并置低电平，写入“1”设置成输出模式并置高电平，写入“2”设置成输入模式
static ssize_t gpio_demo_write(struct file *flip, const char __user *buf, size_t len, loff_t *pos)
{   
    char data[2];   //存放用户空间传进来的数据

    //这里不能直接打印用户空间传进来的数据
    printk(KERN_INFO "%s,%s:%d len:%ld\n", __FILE__, __func__, __LINE__, len);   

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
        *gpio_demo_dev->vreg_ddr |= 1 << gpio_demo_dev->vreg_ddr_bit;
        // 设置gpio输出低电平
        *gpio_demo_dev->vreg_dr  &= ~((uint32_t)1 << gpio_demo_dev->vreg_dr_bit); 
        break;
    case '1':
        // 设置gpio输入输出方向为输出
        *gpio_demo_dev->vreg_ddr |= 1 << gpio_demo_dev->vreg_ddr_bit;
        // 设置gpio输出高电平
        *gpio_demo_dev->vreg_dr |= 1 << gpio_demo_dev->vreg_dr_bit;
        break;
    case '2':
        // 设置gpio输入输出方向为输入
        *gpio_demo_dev->vreg_ddr &= ~((uint32_t)1 << gpio_demo_dev->vreg_ddr_bit);
        break;
    default:
        return -EINVAL;
        break;
    }
    printk(KERN_INFO "%s,%s:%d len:%ld\n", __FILE__, __func__, __LINE__, len);
    return len;     //使用echo 1 > /dev/gpio_demo, 如果返回0 echo就会一直重试 所以要么返回小于0的错误码 要么返回实际值
}



static int __init platform_gpiodrv_init(void)
{
    int ret = 0;

    // 将platform driver注册到总线中, 通过命令查看效果：ls -l /sys/bus/platform/devices/gpiodrv
    ret = platform_driver_register(&platform_gpiodrv);
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


static void __exit platform_gpiodrv_exit(void)
{
    platform_driver_unregister(&platform_gpiodrv);
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(platform_gpiodrv_init);
module_exit(platform_gpiodrv_exit);

MODULE_LICENSE("GPL");


