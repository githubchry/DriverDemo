# 命令行控制rk3288 gpio

参考：
[用文件IO的方式操作GPIO](https://www.cnblogs.com/xiaojianliu/p/9734426.html)  
[Linux 用户态设置GPIO控制](https://www.cnblogs.com/lxyd/p/9671673.html)

linux内核已经自带了一套通用的GPIO驱动，可以在用户态配置GPIO的接口。
RK3288 有 9 组 GPIO bank： GPIO0，GPIO1, …, GPIO8。每组又以 A0-A7, B0-B7, C0-C7, D0-D7 作为编号区分（不是所有 bank 都有全部编号，例如 GPIO5 就只有 B0-B7, C0-C3)。

通过`cat /sys/kernel/debug/gpio`可以查看GPIO占用情况
```
root@localhost:~# cat /sys/kernel/debug/gpio 
GPIOs 0-23, platform/pinctrl, gpio0:
 gpio-5   (                    |GPIO Key Power      ) in  hi    
 gpio-10  (                    |hp_ctl_gpio         ) out lo    
 gpio-11  (                    |vcc28_dvp           ) out hi    
 gpio-12  (                    |vcc_otg_5v          ) out hi    
 gpio-14  (                    |vcc_host_5v         ) out hi    

GPIOs 24-55, platform/pinctrl, gpio1:

GPIOs 56-87, platform/pinctrl, gpio2:

GPIOs 88-119, platform/pinctrl, gpio3:

GPIOs 120-151, platform/pinctrl, gpio4:
 gpio-127 (                    |mdio-reset          ) out hi    
 gpio-139 (                    |bt_default_rts      ) in  hi    
 gpio-146 (                    |bt_default_wake     ) in  hi    
 gpio-148 (                    |reset               ) out lo    
 gpio-149 (                    |bt_default_reset    ) out lo    
 gpio-151 (                    |bt_default_wake_host) in  hi    

GPIOs 152-183, platform/pinctrl, gpio5:

GPIOs 184-215, platform/pinctrl, gpio6:

GPIOs 216-247, platform/pinctrl, gpio7:
 gpio-227 (                    |vcc_sd              ) out hi    
 gpio-231 (                    |?                   ) in  hi    

GPIOs 248-263, platform/pinctrl, gpio8:
 gpio-249 (                    |?                   ) out lo    
 gpio-250 (                    |?                   ) out hi    
```
板子两侧的引脚看到丝印：
左侧
GPIO8:
A7 A6

右侧
GPIO:
B5 A3
A7 B1

这里拿GPIO8 A6做实验，首先算出 gpio number
根据上面的打印`GPIOs 248-263, platform/pinctrl, gpio8`可知：
gpio8的起始引脚为248
A6表示第一组的第六个脚，得出number = 248 + 6 = 254.

附 firefly-rk3288 设备树引用GPIO引脚计算 跟上面对不上 后面再验证
例如: GPIO5_B4
GPIO5 BANK = 5
PIN计算: A=0 B=1 C=2 D=3
例如: B4 PIN = 1 * 8 + 4 = 12
gpionumber = BANK * 32 + PIN
gpionumber为 5 * 32 + 1 * 8 + 4 = 172
设备树中引用为:
gpios = <&(gpio label) PIN GPIO_ACTIVE_LOW>;
gpios = <&gpio5 12 GPIO_ACTIVE_LOW>;
以此类推.


## 1.导出gpio254
``` shell
root@localhost:/sys/class/gpio# echo 254 > export
root@localhost:/sys/class/gpio# ls gpio254
```

## 2.将gpio254设置成输出
``` shell
root@localhost:/sys/class/gpio# cd gpio254
root@localhost:/sys/class/gpio/gpio254# echo out > direction 
root@localhost:/sys/class/gpio/gpio254# cat direction 
out
root@localhost:/sys/class/gpio/gpio254# 
```
## 3.控制gpio254输出高电平
```
root@localhost:/sys/class/gpio/gpio254# cat value 
0
root@localhost:/sys/class/gpio/gpio254# echo 1 > value 
root@localhost:/sys/class/gpio/gpio254# 
```
顺带提一下：
`echo out > direction` + `echo 0 > value` = `echo low > direction`
`echo out > direction` + `echo 1 > value` = `echo high > direction`
## 4.取消导出的gpio254
```
root@localhost:/sys/class/gpio# echo 254 > unexport
```
# 编写驱动代码实现以上四步

[LinuxGPIO中文文档](https://www.cnblogs.com/bforever/p/10528395.html)

## 基于引脚号控制gpio的传统接口，不推荐

在老的内核版本中是通过`of_get_named_gpio(node,"goodix,reset-gpio", 0)`方法去获取GPIO资源的资源号（一个int型的数值），然后再使用gpio_xxxx对GPIO资源进行操作。

见源码`export_gpio_demo.c`，里面没有使用`of_get_named_gpio`获取引脚号，而是直接通过丝印和前面上一节描述的方法推算出引脚号直接使用。



## 基于描述符控制gpiod的传统接口，推荐

在内核3.13之后，引入了新的gpiod_api，该api使用`devm_gpiod_get`去获取GPIO资源，获取到的是一个类型为“struct gpio_desc”的结构体指针，在操作GPIO时使用的是`gpiod_direction_output`等方法。

### 修改dts定义gpio节点

[参考官方的led](http://wiki.t-firefly.com/zh_CN/Firefly-RK3288/driver_led.html)

在rk3288-firefly.dts文件定义节点：

```
/ {
	chry_gpio0a7_node {
		compatible = "chry_gpio0a7";
		gpios = <&gpio0 GPIO_A7 GPIO_ACTIVE_LOW>;
	};
};
```



在`get_gpio_desc_from_dts_node`里面从dts获取：

```c
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
    if (0 > gpio)
    {
        printk(KERN_ERR "%s,%s:%d of_get_named_gpio_flags failed\n", __FILE__, __func__, __LINE__);
        goto exit;
    }

    printk(KERN_INFO "%s,%s:%d ojbk! gpio %d\n", __FILE__, __func__, __LINE__, gpio);
exit:

    return gpio_to_desc(gpio);
}
    
```



见源码export_gpiod_demo.c。