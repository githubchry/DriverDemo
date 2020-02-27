#include <linux/init.h>
#include <linux/module.h>

int chry_add(int a, int b)
{
    return a + b;
}

int chry_sub(int a, int b)
{
    return a - b;
}

/**
 * EXPORT_SYMBOL标签内定义的函数或者符号对全部内核代码公开，
 * 不用修改内核代码就可以在内核模块中直接调用，
 * 即使用EXPORT_SYMBOL可以将一个函数以符号的方式导出给其他模块使用。
 * 类似用户空间的so动态库
 * 
 * 使用需要确保两点:
 * 1.被调用模块比调用者先加载insmod
 * 2.卸载rmmod该模块时确保当前没有调用者
*/
EXPORT_SYMBOL(chry_add);
EXPORT_SYMBOL(chry_sub);

MODULE_LICENSE("GPL");
