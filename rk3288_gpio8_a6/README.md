```
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
见源码。


```
