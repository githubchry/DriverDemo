#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>

extern struct bus_type chrybus;

void chrydev_release(struct device *dev)
{
    printk(KERN_INFO "%s,%s:%d\n", __FILE__, __func__, __LINE__);
}

struct device chrydev = {
    .init_name      = "chrydev",
    .bus            = &chrybus,
    .release        = chrydev_release,  //暂无作用，仅为了消除dmesg警告
};

static int __init busdev_demo_init(void)
{
    int ret = 0;

    // 将device注册到总线中, 通过命令查看效果：ls -l /sys/bus/chrybus/device/chrydev
    ret = device_register(&chrydev);
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


static void __exit busdev_demo_exit(void)
{
    device_unregister(&chrydev);
    printk(KERN_INFO "%s,%s:%d ojbk\n", __FILE__, __func__, __LINE__);
}

module_init(busdev_demo_init);
module_exit(busdev_demo_exit);

MODULE_LICENSE("GPL");














