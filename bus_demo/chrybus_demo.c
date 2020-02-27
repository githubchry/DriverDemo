#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>     //kmalloc
#include <linux/device.h>
#include <linux/uaccess.h>      // copy_from_user()



struct bus_type chrybus = {
    .name      = "chrybus",
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














