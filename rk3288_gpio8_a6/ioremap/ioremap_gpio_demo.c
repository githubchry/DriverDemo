#include <linux/module.h>
#include <linux/delay.h>     // msleep_interruptible
#include <asm/io.h>     // ioremap 头文件
/*
# 获取寄存器地址
寄存器地址 = 基地址 + 偏移量

1.基地址获取
打开TRM文档的第2章第1节
address mapping可以看到地址分布图 

找到里面的GPIO8, 基地址为FF7F_0000
找到里面的SECURE GRF, 基地址为FF77_0000

2.偏移量
打开TRM文档的第4章第3节 该章描述功能选择相关
搜索GPIO8A  找到GRF_GPIO8A_IOMUX 搜索之 得到偏移为0x0080

打开TRM文档的第49章第4节 该章描述GPIO相关
找到 GPIO_SWPORTA_DR 得到偏移为0x0000
找到 GPIO_SWPORTA_DDR 得到偏移为0x0004

# 寄存器地址位设置
在文档可以看到GRF_GPIO8A_IOMUX这个寄存器是32位的，31:16表示高16位，按照描述,如果修改了15:0其中的位，要在31:16相对的bit置1
13:12这两位控制GPIO8A[6]选择的复用功能  00表示复用为gpio功能  如果要设置这两位，就要在29:28bit置1

GPIO_SWPORTA_DDR寄存器控制GPIO的输入输出方向，32位，分别对应ABCD四组共32个引脚，0表示输入1表示输出
GPIO_SWPORTA_DR寄存器是GPIO的数据位，32位，分别对应ABCD四组共32个引脚，如果方向是输出，置1表示输出高电平
*/


//宏定义RK3288里面GPIO8相关的(功能、方向、数据)寄存器地址(物理地址)

//功能选择 SECURE GRF 配置管脚复用为GPIO功能
#define RK3288_GPIO8_GRF_BASE       ((uint32_t)0xFF770000) 
#define RK3288_GPIO8A_GRF_BASE      ((uint32_t)RK3288_GPIO8_GRF_BASE+0x80)
#define RK3288_GPIO8A6_GRF_LOW_BIT  12
#define RK3288_GPIO8A6_GRF_HIGH_BIT 13

//GPIO Operational Base
#define RK3288_GPIO8_BASE       ((uint32_t)0xFF7F0000) 
//GPIO DATA direction REGISTER 
#define RK3288_GPIO8_DDR_BASE       ((uint32_t)RK3288_GPIO8_BASE+0x04)   
#define RK3288_GPIO8A6_DDR_BIT      6
//GPIO DATA REGISTER
#define RK3288_GPIO8_DR_BASE        ((uint32_t)RK3288_GPIO8_BASE+0x00)     
#define RK3288_GPIO8A6_DR_BIT       6   //[7:0]表示GPIOn_A[7:0]     [15:8]表示GPIOn_B[7:0]   [23:16]表示GPIOn_C[7:0]   [31:24]表示GPIOn_D[7:0] 



// 为了使用内存映射，我们需先定义虚拟寄存器指针变量用来保存内存映射后的地址：
static volatile uint32_t *gpio8_grf_vreg;    //功能
static volatile uint32_t *gpio8_ddr_vreg;    //方向
static volatile uint32_t *gpio8_dr_vreg;     //数据

static int __init gpio_demo_init(void)
{
    int ret = 0;
    uint32_t val;

    // 映射虚拟寄存器地址 uint是4字节
    gpio8_grf_vreg = (uint32_t *)ioremap(RK3288_GPIO8A_GRF_BASE, 4); 
    gpio8_ddr_vreg = (uint32_t *)ioremap(RK3288_GPIO8_DDR_BASE, 4); 
    gpio8_dr_vreg  = (uint32_t *)ioremap(RK3288_GPIO8_DR_BASE, 4); 

    if(false == (gpio8_grf_vreg && gpio8_ddr_vreg && gpio8_dr_vreg))
    {
        printk(KERN_ERR "%s,%s:%d\n\tioremap[%p, %p, %p][0x%x, 0x%x, 0x%x] failed\n", 
            __FILE__, __func__, __LINE__, 
            gpio8_grf_vreg, gpio8_ddr_vreg, gpio8_dr_vreg,
            RK3288_GPIO8A_GRF_BASE, RK3288_GPIO8_DDR_BASE, RK3288_GPIO8_DR_BASE);
        ret = -1;
        goto err1;
    }

    val = readl(gpio8_grf_vreg);
    printk(KERN_ERR "gpio8_grf_vreg modify before: 0x%x\n", val);


    val &= ~((uint32_t)1 << RK3288_GPIO8A6_GRF_LOW_BIT); 
    val &= ~((uint32_t)1 << RK3288_GPIO8A6_GRF_HIGH_BIT); 
    val |= 1 << (RK3288_GPIO8A6_GRF_LOW_BIT + 16); 
    val |= 1 << (RK3288_GPIO8A6_GRF_HIGH_BIT + 16); 

    writel(val, gpio8_grf_vreg);     // *gpio8_grf_vreg |= val;
    printk(KERN_ERR "gpio8_grf_vreg modify: %x\n", val);

    val = readl(gpio8_grf_vreg);
    printk(KERN_ERR "gpio8_grf_vreg Revised: 0x%x\n\n", val);

    /*************************************************************************/
    
    // 设置gpio输入输出方向为输出
    val = readl(gpio8_ddr_vreg);
    printk(KERN_ERR "gpio8_ddr_vreg modify before: 0x%x\n", val);

    val |= 1 << RK3288_GPIO8A6_DDR_BIT;
    writel(val, gpio8_ddr_vreg);    //*gpio8_ddr_vreg |= 1 << RK3288_GPIO8A6_DDR_BIT;
    printk(KERN_ERR "gpio8_ddr_vreg modify: 0x%x\n", val);
   
    val = readl(gpio8_ddr_vreg);
    printk(KERN_ERR "gpio8_ddr_vreg Revised: 0x%x\n\n", val);

    /*************************************************************************/

    // 设置gpio输出高电平
    val = readl(gpio8_dr_vreg);
    printk(KERN_ERR "gpio8_dr_vreg modify before: 0x%x\n", val);

    val |= 1 << RK3288_GPIO8A6_DR_BIT;
    writel(val, gpio8_dr_vreg);   
    printk(KERN_ERR "gpio8_dr_vreg modify: 0x%x\n", val);
   
    val = readl(gpio8_dr_vreg);
    printk(KERN_ERR "gpio8_dr_vreg Revised: 0x%x\n\n", val);

    /*************************************************************************/
    
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
    return 0;

//err2:
    iounmap(gpio8_grf_vreg);
    iounmap(gpio8_ddr_vreg);
    iounmap(gpio8_dr_vreg);
err1:
    return ret;
}


static void __exit gpio_demo_exit(void)
{
    // 设置gpio输出低电平
    *gpio8_dr_vreg &= ~((uint32_t)1 << RK3288_GPIO8A6_DR_BIT); 
    // 释放映射
    iounmap(gpio8_grf_vreg);
    iounmap(gpio8_ddr_vreg);
    iounmap(gpio8_dr_vreg);

    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(gpio_demo_init);
module_exit(gpio_demo_exit);

MODULE_LICENSE("GPL");




