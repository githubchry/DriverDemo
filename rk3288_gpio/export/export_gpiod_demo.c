#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>     // msleep_interruptible
#include <linux/uaccess.h>  // copy_from_user()
#include <linux/slab.h>     //kmalloc
#include <linux/delay.h>     // msleep_interruptible
#include <linux/gpio/consumer.h>

#include <linux/of.h>
#include <linux/of_gpio.h>
#define BASEMINOR   0
#define COUNT       1
#define CLASS       "chry"
#define NAME        "gpio0a7_demo"

#define RK3288_GPIO0_A7  7

//设计设备对象
struct gpio_demo_desc {
    struct cdev cdev;
    dev_t devno;  
    struct class *cls;
    struct device *dev;
	struct gpio_desc *gpiod;
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

    printk(KERN_INFO "%s,%s:%d pos:%lld\n", __FILE__, __func__, __LINE__, *pos);   

    if(*pos >= 3)
    {
        return 0;
    }

    if(len < 2)
    {
        return -ENOMEM;
    }

    data[0] = '0' + gpiod_get_value(gpio_demo_dev->gpiod);
    //这里使用了最新的gpiod的接口获取当前gpio的方向
    data[1] = '0' + gpiod_get_direction(gpio_demo_dev->gpiod);
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
    printk(KERN_INFO "%s,%s:%d=> %c,%c\n", __FILE__, __func__, __LINE__, data[0], data[1]);
    return 3;
}

//向设备发送数据 规则：写入“0”设置成输出模式并置低电平，写入“1”设置成输出模式并置高电平，写入“2”设置成输入模式
static ssize_t gpio_demo_write(struct file *flip, const char __user *buf, size_t len, loff_t *pos)
{   
    char data[2];   //存放用户空间传进来的数据
    int ret = 0;

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
        ret = gpiod_direction_output(gpio_demo_dev->gpiod, 0); 
        break;
    case '1':
        ret = gpiod_direction_output(gpio_demo_dev->gpiod, 1); 
        break;
    case '2':
        // 设置gpio输入输出方向为输入
        ret = gpiod_direction_input(gpio_demo_dev->gpiod); 
        break;
    default:
        return -EINVAL;
        break;
    }
    printk(KERN_INFO "%s,%s:%d ret:%d, len:%d\n", __FILE__, __func__, __LINE__, ret, len);
    return len;     //使用echo 1 > /dev/gpio_demo, 如果返回0 echo就会一直重试 所以要么返回小于0的错误码 要么返回实际值
}
//*
//从已经在dts自定义好的节点里面获取irqno
static struct gpio_desc * get_gpio_desc_from_dts_node(void)
{
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
    if (0 > gpio)
    {
        printk(KERN_ERR "%s,%s:%d of_get_named_gpio_flags failed\n", __FILE__, __func__, __LINE__);
        goto exit;
    }

    printk(KERN_INFO "%s,%s:%d ojbk! gpio %d\n", __FILE__, __func__, __LINE__, gpio);
exit:

    return gpio_to_desc(gpio);
}
//*/
static int __init gpio_demo_init(void)
{
    int ret = 0;

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

    //-------------------------------------------------------------------------
    //chry => chry-gpios 从设备树获取chry-gpios
    gpio_demo_dev->gpiod = get_gpio_desc_from_dts_node();
    if (IS_ERR(gpio_demo_dev->gpiod))
	{
        printk(KERN_ERR "%s,%s:%d devm_gpiod_get failed \n", __FILE__, __func__, __LINE__);
        goto err5;
	}

    /* 
    8. 设置gpio7的输入输出方向为输出, 同时输出1(高电平)
    echo out > direction + echo 0 > value = echo low > direction
    echo out > direction + echo 1 > value = echo high > direction
    gpio_set_value()与gpio_direction_output()有什么区别？
    如果使用该GPIO时，不会动态的切换输入输出，建议在开始时就设置好GPIO 输出方向，后面拉高拉低时使用gpio_set_value()接口，而不建议使用gpio_direction_output(), 
    因为gpio_direction_output接口里面有mutex锁，对中断上下文调用会有错误异常，且相比 gpio_set_value，gpio_direction_output 所做事情更多，浪费。
    */
    ret = gpiod_direction_output(gpio_demo_dev->gpiod, 1); 
    if(ret != 0)
    {
        printk(KERN_ERR "%s,%s:%d gpiod_direction_output[1] failed %d \n", __FILE__, __func__, __LINE__, ret);
        goto err6;
    }
    // 延时500ms, 根据官方资料[https://www.kernel.org/doc/Documentation/timers/timers-howto.txt], 大于20ms的延时需要用msleep_interruptible
    msleep_interruptible(500);

    // 设置为低电平   echo 0 > value
    gpiod_set_value(gpio_demo_dev->gpiod, 0); 
    msleep_interruptible(500);

    // 设置为高电平   echo 1 > value
    gpiod_set_value(gpio_demo_dev->gpiod, 1); 

    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
    return 0;

err6:
    gpiod_put(gpio_demo_dev->gpiod);
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
    gpiod_set_value(gpio_demo_dev->gpiod, 0); 
    
    gpiod_put(gpio_demo_dev->gpiod);
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

