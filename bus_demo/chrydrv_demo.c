#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>

#include "chrydev_info.h"

extern struct bus_type chrybus;

int chrydrv_probe(struct device *dev)
{
    struct chrydev_platform_data *pdesc = (struct chrydev_platform_data *)dev->platform_data;

    printk(KERN_INFO "%s,%s:%d name:%s, irqno:%d, phyaddr：0x%x\n", __FILE__, __func__, __LINE__, pdesc->name, pdesc->irqno, pdesc->phyaddr);

    //拿到寄存器地址，然后ioremap映射成虚拟地址，然后操作。。。。
    
    return 0;
}

int chrydrv_remove(struct device *dev)
{
    printk(KERN_INFO "%s,%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}
struct device_driver chrydrv = {
    .name       = "chrydrv",
    .bus        = &chrybus,
    .probe      = chrydrv_probe,
    .remove     = chrydrv_remove,
};

static int __init busdrv_demo_init(void)
{
    int ret = 0;

    // 将driver注册到总线中, 通过命令查看效果：ls -l /sys/bus/chrybus/driver/chrydrv
    ret = driver_register(&chrydrv);
    if (ret != 0)
    {
        printk(KERN_ERR "%s,%s:%d bus_register failed\n", __FILE__, __func__, __LINE__);
        goto err0;
    }

    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);

    return 0;
    
err0:
    return ret;
}


static void __exit busdrv_demo_exit(void)
{
    driver_unregister(&chrydrv);
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(busdrv_demo_init);
module_exit(busdrv_demo_exit);

MODULE_LICENSE("GPL");














