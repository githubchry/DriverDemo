#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>


/*
# 获取寄存器地址
寄存器地址 = 基地址 + 偏移量

1.基地址获取
打开TRM文档的第2章第1节d
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
#define RK3288_GPIO0_DDR_BASE       ((uint32_t)RK3288_GPIO0_BASE+0x04)   
#define RK3288_GPIO0A7_DDR_BIT      7
//GPIO DATA REGISTER
#define RK3288_GPIO0_DR_BASE        ((uint32_t)RK3288_GPIO0_BASE+0x00)     
#define RK3288_GPIO0A7_DR_BIT       7   //[7:0]表示GPIOn_A[7:0]     [15:8]表示GPIOn_B[7:0]   [23:16]表示GPIOn_C[7:0]   [31:24]表示GPIOn_D[7:0] 

//仅仅为了消除rmmod时的dmesg警告
static void platform_gpiodev_release(struct device * dev)
{
    printk(KERN_INFO "%s,%s:%d\n", __FILE__, __func__, __LINE__);
    return;
}

static struct resource gpio_res[] = {
    [0] = {
        .start  = RK3288_GPIO0_DDR_BASE,
        .end    = RK3288_GPIO0_DDR_BASE + 4 - 1,    //只用到DR和DDR两个地址, 每个地址占4字节, 从0开始算所以要-1
        .flags  = IORESOURCE_MEM,
    },
    [1] = {
        .start  = RK3288_GPIO0_DR_BASE,
        .end    = RK3288_GPIO0_DR_BASE + 4 - 1,    //只用到DR和DDR两个地址, 每个地址占4字节, 从0开始算所以要-1
        .flags  = IORESOURCE_MEM,
    },
    //中断资源 没用上，仅作演示
    //中断是没用上，但我把要操作地址位用中断资源表示并传过去，这不是规范的做法！别学！
    [2] = {
        .start  = RK3288_GPIO0A7_DDR_BIT,  // IRQ_EINT(4),
        .end    = RK3288_GPIO0A7_DDR_BIT,  //IRQ_EINT(4),      //中断没有连续的概念 所以开始和结束一样
        .flags  = IORESOURCE_IRQ,
    },
    [3] = {
        .start  = RK3288_GPIO0A7_DR_BIT,  // IRQ_EINT(4),
        .end    = RK3288_GPIO0A7_DR_BIT,  //IRQ_EINT(4),      //中断没有连续的概念 所以开始和结束一样
        .flags  = IORESOURCE_IRQ,
    }
};

struct platform_device platform_gpiodev = {
    .name           = "rk3288_gpiodev",     //名称用作匹配平台驱动
    .id             = -1,
    .num_resources  = ARRAY_SIZE(gpio_res),
    .resource       = gpio_res,
    .dev = {
        .release = platform_gpiodev_release,
    }
};

static int __init platform_gpiodev_init(void)
{
    int ret = 0;

    // 将platform device注册到总线中, 通过命令查看效果：ls -l /sys/bus/platform/devices/rk3288_gpio 
    ret = platform_device_register(&platform_gpiodev);
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


static void __exit platform_gpiodev_exit(void)
{
    platform_device_unregister(&platform_gpiodev);
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(platform_gpiodev_init);
module_exit(platform_gpiodev_exit);

MODULE_LICENSE("GPL");














