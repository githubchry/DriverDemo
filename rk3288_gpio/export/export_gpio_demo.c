#include <linux/module.h>
#include <linux/delay.h>     // msleep_interruptible
#include <asm/gpio.h>     // GPIO头文件

#define RK3288_GPIO8_A6  254

static int __init gpio_demo_init(void)
{
    int ret = 0;
    //1.导出gpio248 相当于echo 248 > /sys/class/gpio/export 
    ret = gpio_request(RK3288_GPIO8_A6, "gpio8a6");
    if(ret != 0)
    {
        printk(KERN_ERR "%s,%s:%d gpio_request[%d] failed %d \n", __FILE__, __func__, __LINE__, RK3288_GPIO8_A6, ret);
        goto err1;
    }

    /* 
    2. 设置gpio248的输入输出方向为输出, 同时输出1(高电平)
    echo out > direction + echo 0 > value = echo low > direction
    echo out > direction + echo 1 > value = echo high > direction
    gpio_set_value()与gpio_direction_output()有什么区别？
    如果使用该GPIO时，不会动态的切换输入输出，建议在开始时就设置好GPIO 输出方向，后面拉高拉低时使用gpio_set_value()接口，而不建议使用gpio_direction_output(), 
    因为gpio_direction_output接口里面有mutex锁，对中断上下文调用会有错误异常，且相比 gpio_set_value，gpio_direction_output 所做事情更多，浪费。
    */
    ret = gpio_direction_output(RK3288_GPIO8_A6, 1); 
    if(ret != 0)
    {
        printk(KERN_ERR "%s,%s:%d gpio_direction_output[%d] failed %d \n", __FILE__, __func__, __LINE__, RK3288_GPIO8_A6, ret);
        goto err2;
    }
    // 延时500ms, 根据官方资料[https://www.kernel.org/doc/Documentation/timers/timers-howto.txt], 大于20ms的延时需要用msleep_interruptible
    msleep_interruptible(500);

    // 3.设置为低电平   echo 0 > value
    gpio_set_value(RK3288_GPIO8_A6, 0); 
    msleep_interruptible(500);

    // 4.设置为高电平   echo 1 > value
    gpio_set_value(RK3288_GPIO8_A6, 1); 

    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);

    return 0;
//err3:
    gpio_set_value(RK3288_GPIO8_A6, 0); 
err2:
    gpio_free(RK3288_GPIO8_A6);
err1:
    return ret;
}


static void __exit gpio_demo_exit(void)
{
    gpio_set_value(RK3288_GPIO8_A6, 0); 
    gpio_free(RK3288_GPIO8_A6);
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(gpio_demo_init);
module_exit(gpio_demo_exit);

MODULE_LICENSE("GPL");














