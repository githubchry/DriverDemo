#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>  //struct platform_device_id



int platform_gpiodrv_probe(struct platform_device *pdev)
{
    //ls /sys/bus/platform/drivers/gpiodrv/pdev_name
    printk(KERN_INFO "%s,%s:%d\n", __FILE__, __func__, __LINE__);
    //拿到寄存器地址，然后ioremap映射成虚拟地址，然后操作。。。。
    
    return 0;
}

int platform_gpiodrv_remove(struct platform_device *pdev)
{
    printk(KERN_INFO "%s,%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}

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














