#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>

int chrybus_match(struct device *dev, struct device_driver *drv)
{
    //匹配规则自己定，一般以name为依据
    //if (0 == strcmp(drv->name, dev->kobj.name))
    if (0 == strncmp(drv->name, dev->kobj.name, strlen("chry")))
    {
        printk(KERN_INFO "%s,%s:%d match sucess!: [%s, %s]\n", __FILE__, __func__, __LINE__, drv->name, dev->kobj.name);
        return 1;
    }
    else
    {
        printk(KERN_INFO "%s,%s:%d match failed!: [%s, %s]\n", __FILE__, __func__, __LINE__, drv->name, dev->kobj.name);
        return 0;
    }
    
    return 0;
}

struct bus_type chrybus = {
    .name       = "chrybus",
    .match      = chrybus_match,    //如果没有实现match接口，默认匹配成功
};

EXPORT_SYMBOL(chrybus);

static int __init busdrv_demo_init(void)
{
    int ret = 0;

    // 注册总线, 通过命令查看效果：ls -l /sys/bus/chrybus/
    ret = bus_register(&chrybus);
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
    bus_unregister(&chrybus);
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(busdrv_demo_init);
module_exit(busdrv_demo_exit);

MODULE_LICENSE("GPL");














