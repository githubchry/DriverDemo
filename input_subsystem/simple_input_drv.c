#include <linux/init.h>
#include <linux/module.h>
#include<linux/input.h>

//声明全局的设备对象
struct input_dev *simple_input_dev = NULL;

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

    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);

    return 0;
    
err1:
    input_free_device(simple_input_dev);
err0:
    return ret;
}


static void __exit simple_input_exit(void)
{
    input_unregister_device(simple_input_dev);
    input_free_device(simple_input_dev);
    
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(simple_input_init);
module_exit(simple_input_exit);

MODULE_LICENSE("GPL");


