#include <linux/init.h>
#include <linux/module.h>
#include <linux/input.h>

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>     //IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING

#define NAME        "input_keyirq_demo"
#define KEY_NUM     10

/*
从已经在dts自定义好的节点里面获取irqno
/ {
	chry_multi_key_node {
		compatible = "chry_multi_key";
		key_irq@0 {
			key_name = "key_gpio0a7";
			key_code = <12>;
			gpio = <&gpio0 7 GPIO_ACTIVE_HIGH>;	
		};
		
		key_irq@1 {
			key_name = "key_gpio0b1";
			key_code = <8>;
			gpio = <&gpio0 9 GPIO_ACTIVE_HIGH>;
		};
	};
};
*/

//设计一个对象，描述dts上的key_irq节点
struct key_desc {
    int irqno;
    int key_code;
    const char *key_name;
    struct gpio_desc *gpiod;
    struct device_node *cnp;    //随时获取节点的各个信息
};

struct key_desc all_key[KEY_NUM];

//声明全局的设备对象
struct input_dev *simple_input_dev = NULL;

static int get_all_child_from_dts_node(void)
{
    int idx = 0;
    struct device_node *np = NULL;
    struct device_node *cnp = NULL;
    struct device_node *prev = NULL;
     
    //1.获取到设备树中的/chry_multi_key_node节点
    np = of_find_node_by_path("/chry_multi_key_node");
    if (NULL == np)
    {
        printk(KERN_ERR "%s,%s:%d find node failed\n", __FILE__, __func__, __LINE__);
        return idx;
    }
    
    //2.遍历/chry_multi_key_node节点下的子节点，获取gpio 中断号 键值等信息
    do
    {
        cnp = of_get_next_child(np, prev);
        if (NULL != cnp)
        {
            all_key[idx++].cnp = cnp;
        }
        
        prev = cnp;
    } while (NULL != prev && idx < KEY_NUM);
    

    printk(KERN_INFO "%s,%s:%d ojbk! got %d key node\n", __FILE__, __func__, __LINE__, idx);
    return idx;
}

static 	irqreturn_t input_key_irq_handler(int irqno, void *devid)
{
    // 读取数据寄存器，获取gpio当前电平
    struct key_desc *keyd = (struct key_desc *)devid;
    uint32_t val = gpiod_get_value(keyd->gpiod);
    
    if (val)
    {
        //按下
        printk(KERN_ERR "%s,%s:%d key down, irqno： %d\n", __FILE__, __func__, __LINE__, keyd->irqno);
    }
    else
    {
        //松开
        printk(KERN_ERR "%s,%s:%d key up, irqno： %d\n", __FILE__, __func__, __LINE__, keyd->irqno);
    }
    
    //上报数据 input_event
    //参数1：当前的input device上报数据
    //参数2：上报数据的类型 EV_KEY,EV_ABS
    //参数3：具体的数据是什么：KEY_POWER
    //参数4：值是什么
    input_report_key(simple_input_dev, keyd->key_code, val);
    input_sync(simple_input_dev);   //上报数据结束

    return IRQ_HANDLED;
}

static int __init simple_input_init(void)
{
    int ret = 0;
    int num = 0;
    int idx = 0;

    //分配一个input device对象
    simple_input_dev = input_allocate_device();
    if (NULL == simple_input_dev)
    {
        printk(KERN_ERR "%s,%s:%d input_allocate_device failed\n", __FILE__, __func__, __LINE__);
        ret = -ENOMEM;
        goto err0;
    }

    // 初始化input device对象
#if 1   //非必要信息
    // cat /sys/class/input/eventn/device/name
    simple_input_dev->name = "simple input key";
    // cat /sys/class/input/eventn/device/phys
    simple_input_dev->phys = "gpio0a7";
    // cat /sys/class/input/eventn/device/uniq
    simple_input_dev->uniq = "gpio0a7 for rk3288";
    //描述硬件相关信息，连接方式，厂家，产品，版本 随便填
    simple_input_dev->id.bustype = BUS_HOST;    //比如BUS_I2C、BUS_SPI、gpio用BUS_HOST  cat /sys/class/input/eventn/device/id/bustype
    simple_input_dev->id.vendor  = 0x1234;      //描述厂家  cat /sys/class/input/eventn/device/id/vendor
    simple_input_dev->id.product = 0x8888;      //描述产品  cat /sys/class/input/eventn/device/id/product
    simple_input_dev->id.version = 0x0001;      //描述版本  cat /sys/class/input/eventn/device/id/version
#endif
    // 表示当前设备能够产生按键类型数据
    __set_bit(EV_KEY, simple_input_dev->evbit);     //等价于： simple_input_dev->evbit[BIT_WORD(EV_KEY)] |= BIT_MASK(EV_KEY);
    
    num = get_all_child_from_dts_node();
    for (idx = 0; idx < num; idx++)
    {
        int gpio;
        // 设置当前设备能够产生的键值
        of_property_read_u32(all_key[idx].cnp, "key_code", &all_key[idx].key_code);
        __set_bit(all_key[idx].key_code, simple_input_dev->keybit);

        // 获取gpio并设置方向为输出
        gpio = of_get_named_gpio(all_key[idx].cnp, "gpio", 0);
        all_key[idx].gpiod = gpio_to_desc(gpio);
        ret = gpiod_direction_output(all_key[idx].gpiod, 1); 
        if(ret != 0)
        {
            printk(KERN_ERR "%s,%s:%d gpiod_direction_output[1] failed %d \n", __FILE__, __func__, __LINE__, ret);
            goto err2;
        }

        // 申请按键中断
        of_property_read_string(all_key[idx].cnp, "key_name", &all_key[idx].key_name);
        all_key[idx].irqno = gpiod_to_irq(all_key[idx].gpiod);
        ret = request_irq(all_key[idx].irqno, input_key_irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, all_key[idx].key_name, &all_key[idx]);
        if(ret != 0)
        {
            printk(KERN_ERR "%s,%s:%d request_irq failed %d \n", __FILE__, __func__, __LINE__, ret);
            goto err2;
        }

        printk(KERN_ERR "%s,%s:%d \n key name:%s, code:%d, irqno:%d, gpio:%d\n", 
            __FILE__, __func__, __LINE__, 
            all_key[idx].key_name, all_key[idx].key_code, all_key[idx].irqno, gpio);
    }
    
    ret = input_register_device(simple_input_dev);
    if(0 != ret)
    {
        printk(KERN_ERR "%s,%s:%d input_register_device failed %d \n", __FILE__, __func__, __LINE__, ret);
        goto err1;
    }

    // 驱动硬件
    // 获取gpiod并设置为输入模式

    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);

    return 0;
    
//err3:
    input_unregister_device(simple_input_dev);
err2:
    for (idx--; idx >= 0; idx--)
    {
        free_irq(all_key[idx].irqno, &all_key[idx]);  
    }
err1:
    input_free_device(simple_input_dev);
err0:
    return ret;
}


static void __exit simple_input_exit(void)
{
    int idx = 2;

    //idx = get_all_child_from_dts_node();
    for (idx--; idx >= 0; idx--)
    {
        free_irq(all_key[idx].irqno, &all_key[idx]);  
    }
    
    input_unregister_device(simple_input_dev);

    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
    input_free_device(simple_input_dev);

    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(simple_input_init);
module_exit(simple_input_exit);

MODULE_LICENSE("GPL");


