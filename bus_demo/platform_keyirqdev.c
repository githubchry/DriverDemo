#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>


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
#define RK3288_GPIO0_DDR_REG       ((uint32_t)RK3288_GPIO0_BASE+0x0004)   
#define RK3288_GPIO0A7_DDR_BIT      7   //[7:0]表示GPIOn_A[7:0]     [15:8]表示GPIOn_B[7:0]   [23:16]表示GPIOn_C[7:0]   [31:24]表示GPIOn_D[7:0] 

//GPIO external port register
#define RK3288_GPIO0_EXT_REG        ((uint32_t)RK3288_GPIO0_BASE+0x0050)     
#define RK3288_GPIO0A7_EXT_BIT      7   //[7:0]表示GPIOn_A[7:0]     [15:8]表示GPIOn_B[7:0]   [23:16]表示GPIOn_C[7:0]   [31:24]表示GPIOn_D[7:0] 

// GPIO0A7对应的中断号，rk3288里面估计无法通过datasheet查到，这个85是从之前学习中断的时候通过dts获取keyirq号，然后通过keyirq号获取irq号得到的
#define RK3288_GPIO0A7_IRQ        82

//仅仅为了消除rmmod时的dmesg警告
static void platform_keyirqdev_release(struct device * dev)
{
    printk(KERN_INFO "%s,%s:%d\n", __FILE__, __func__, __LINE__);
    return;
}

//平台总线还是比较少用，毕竟这个操作的是最原始的ioremap操作，而驱动开发更多的是从dts获取资源
//同时，平台总线一般用在相同系列的soc更换的情景，比如瑞芯微rk3288 => rk3399，海思的hi3516 => hi3559，硬说要rk3559 => hi3559是不可取的
static struct resource keyirq_res[] = {
    // 寄存器地址
    DEFINE_RES_NAMED(RK3288_GPIO0_DDR_REG, 4, NULL, IORESOURCE_MEM),
    DEFINE_RES_NAMED(RK3288_GPIO0_EXT_REG, 4, NULL, IORESOURCE_MEM),
    // 寄存器地址偏移位
    DEFINE_RES_NAMED(RK3288_GPIO0A7_DDR_BIT, 1, NULL, IORESOURCE_REG),
    DEFINE_RES_NAMED(RK3288_GPIO0A7_EXT_BIT, 1, NULL, IORESOURCE_REG),
    //中断资源
    DEFINE_RES_NAMED(RK3288_GPIO0A7_IRQ, 1, NULL, IORESOURCE_IRQ)
};

struct platform_device platform_keyirqdev = {
    .name           = "rk3288_keyirqdev",     //名称用作匹配平台驱动
    .id             = -1,
    .num_resources  = ARRAY_SIZE(keyirq_res),
    .resource       = keyirq_res,
    .dev = {
        .release = platform_keyirqdev_release,
    }
};

static int __init platform_keyirqdev_init(void)
{
    int ret = 0;

    // 将platform device注册到总线中, 通过命令查看效果：ls -l /sys/bus/platform/devices/rk3288_keyirq 
    ret = platform_device_register(&platform_keyirqdev);
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


static void __exit platform_keyirqdev_exit(void)
{
    platform_device_unregister(&platform_keyirqdev);
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(platform_keyirqdev_init);
module_exit(platform_keyirqdev_exit);

MODULE_LICENSE("GPL");














