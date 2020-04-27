#include <linux/init.h>
#include <linux/module.h>
#include <linux/input.h>

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>     //IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING

#define NAME        "input_keyirq_demo"


//声明全局的设备对象
struct input_dev *simple_input_dev = NULL;
int irqno = -1;
struct gpio_desc *gpio0a7_desc;


//从已经在dts自定义好的节点里面获取irqno
/*
/ {
	chry_gpio0a7_node {
		compatible = "chry_gpio0a7";
		chry-gpios = <&gpio0 7 GPIO_ACTIVE_HIGH>;
	};
};
*/
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
    if (!gpio_is_valid(gpio))
    {
        printk(KERN_ERR "%s,%s:%d of_get_named_gpio failed\n", __FILE__, __func__, __LINE__);
        goto exit;
    }

    printk(KERN_INFO "%s,%s:%d ojbk! gpio %d\n", __FILE__, __func__, __LINE__, gpio);
exit:

    return gpio_to_desc(gpio);
}

static 	irqreturn_t input_key_irq_handler(int irqno, void *devid)
{
    // 读取数据寄存器，获取gpio当前电平

    uint32_t val = gpiod_get_value(gpio0a7_desc);
    
    //上报数据 input_event
    //参数1：当前的input device上报数据
    //参数2：上报数据的类型 EV_KEY,EV_ABS
    //参数3：具体的数据是什么：KEY_POWER
    //参数4：值是什么
    if (val)
    {
        //按下
        printk(KERN_ERR "%s,%s:%d key down, irqno： %d\n", __FILE__, __func__, __LINE__, irqno);
        
        input_event(simple_input_dev, EV_KEY, KEY_POWER, 1);
        input_sync(simple_input_dev);   //上报数据结束
    }
    else
    {
        //松开
        printk(KERN_ERR "%s,%s:%d key up, irqno： %d\n", __FILE__, __func__, __LINE__, irqno);
        input_event(simple_input_dev, EV_KEY, KEY_POWER, 0);
        input_sync(simple_input_dev);   //上报数据结束
    }

    return IRQ_HANDLED;
}

static int __init simple_input_init(void)
{
    int ret = 0;

    //分配一个input device对象
    simple_input_dev = input_allocate_device();
    if (NULL == simple_input_dev)
    {
        printk(KERN_ERR "%s,%s:%d input_allocate_device failed\n", __FILE__, __func__, __LINE__);
        ret = -ENOMEM;
        goto err0;
    }

    // 表示当前设备能够产生按键数据
    __set_bit(EV_KEY, simple_input_dev->evbit);
    // 表示当前设备能够产生电源键
    __set_bit(KEY_POWER, simple_input_dev->keybit);

    ret = input_register_device(simple_input_dev);
    if(0 != ret)
    {
        printk(KERN_ERR "%s,%s:%d input_register_device failed %d \n", __FILE__, __func__, __LINE__, ret);
        goto err1;
    }

    // 驱动硬件
    // 获取gpiod并设置为输入模式
    gpio0a7_desc = get_gpio_desc_from_dts_node();
    ret = gpiod_direction_output(gpio0a7_desc, 1); 
    if(ret != 0)
    {
        printk(KERN_ERR "%s,%s:%d gpiod_direction_output[1] failed %d \n", __FILE__, __func__, __LINE__, ret);
        goto err2;
    }

    // 申请按键中断
    irqno = gpiod_to_irq(gpio0a7_desc);
    ret = request_irq(irqno, input_key_irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, NAME, NULL);
    if(ret != 0)
    {
        printk(KERN_ERR "%s,%s:%d request_irq failed %d \n", __FILE__, __func__, __LINE__, ret);
        goto err3;
    }

    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);

    return 0;
    
//err4:
    free_irq(irqno, NULL);  
err3:
    gpiod_put(gpio0a7_desc);
err2:
    input_unregister_device(simple_input_dev);
err1:
    input_free_device(simple_input_dev);
err0:
    return ret;
}


static void __exit simple_input_exit(void)
{
    //释放中断
    free_irq(irqno, NULL);  
    input_unregister_device(simple_input_dev);
    input_free_device(simple_input_dev);
    
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(simple_input_init);
module_exit(simple_input_exit);

MODULE_LICENSE("GPL");


