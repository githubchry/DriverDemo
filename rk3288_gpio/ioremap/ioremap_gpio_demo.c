#include <linux/module.h>
#include <linux/delay.h>     // msleep_interruptible
#include <asm/io.h>     // ioremap 头文件
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


//宏定义RK3288里面GPIO0相关的(功能、方向、数据)寄存器地址(物理地址)
//GPIO Operational Base
#define RK3288_GPIO0_BASE       ((uint32_t)0xFF750000) 
//GPIO DATA direction REGISTER 
#define RK3288_GPIO0_DDR_BASE       ((uint32_t)RK3288_GPIO0_BASE+0x04)   
#define RK3288_GPIO0A7_DDR_BIT      7
//GPIO DATA REGISTER
#define RK3288_GPIO0_DR_BASE        ((uint32_t)RK3288_GPIO0_BASE+0x00)     
#define RK3288_GPIO0A7_DR_BIT       7   //[7:0]表示GPIOn_A[7:0]     [15:8]表示GPIOn_B[7:0]   [23:16]表示GPIOn_C[7:0]   [31:24]表示GPIOn_D[7:0] 

#define USE_BIT_OP_MODE 0   //写了两种操作方式，bit与或方式和val读写方式

// 为了使用内存映射，我们需先定义虚拟寄存器指针变量用来保存内存映射后的地址：
static volatile uint32_t *GPIO0_ddr_vreg;    //方向
static volatile uint32_t *GPIO0_dr_vreg;     //数据

static int __init gpio_demo_init(void)
{
    int ret = 0;
#if !USE_BIT_OP_MODE
    uint32_t val = 0;
#endif

    // 映射虚拟寄存器地址 uint是4字节
    GPIO0_ddr_vreg = (uint32_t *)ioremap(RK3288_GPIO0_DDR_BASE, 4); 
    GPIO0_dr_vreg  = (uint32_t *)ioremap(RK3288_GPIO0_DR_BASE, 4); 

    if(false == (GPIO0_ddr_vreg && GPIO0_dr_vreg))
    {
        printk(KERN_ERR "%s,%s:%d\n\tioremap[%p, %p][0x%x, 0x%x] failed\n", 
            __FILE__, __func__, __LINE__, 
            GPIO0_ddr_vreg, GPIO0_dr_vreg,
            RK3288_GPIO0_DDR_BASE, RK3288_GPIO0_DR_BASE);
        ret = -1;
        goto err1;
    }
    /*************************************************************************/
#if USE_BIT_OP_MODE
    // 设置gpio输入输出方向为输出
    *GPIO0_ddr_vreg |= 1 << RK3288_GPIO0A7_DDR_BIT;

    // 设置gpio输出高电平
    *GPIO0_dr_vreg |= 1 << RK3288_GPIO0A7_DR_BIT;
#else
    // 设置gpio输入输出方向为输出
    val = readl(GPIO0_ddr_vreg);
    val |= 1 << RK3288_GPIO0A7_DDR_BIT;
    writel(val, GPIO0_ddr_vreg);
   
    // 设置gpio输出高电平
    val = readl(GPIO0_dr_vreg);
    val |= 1 << RK3288_GPIO0A7_DDR_BIT;
    writel(val, GPIO0_dr_vreg);   
#endif
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
    return 0;

//err2:
    iounmap(GPIO0_ddr_vreg);
    iounmap(GPIO0_dr_vreg);
err1:
    return ret;
}


static void __exit gpio_demo_exit(void)
{
#if USE_BIT_OP_MODE
    // 设置gpio输出低电平
    *GPIO0_dr_vreg &= ~((uint32_t)1 << RK3288_GPIO0A7_DR_BIT); 
#else
    uint32_t val;
    // 设置gpio输出高电平
    val = readl(GPIO0_dr_vreg);
    val &= ~(1 << RK3288_GPIO0A7_DDR_BIT);
    writel(val, GPIO0_dr_vreg);   
#endif
    // 释放映射
    iounmap(GPIO0_ddr_vreg);
    iounmap(GPIO0_dr_vreg);

    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(gpio_demo_init);
module_exit(gpio_demo_exit);

MODULE_LICENSE("GPL");

