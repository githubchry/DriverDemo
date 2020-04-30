# 思想
以LED驱动为例

驱动开发思想：机制
    能做什么？
        亮/灭
应用开发思想：策略
    怎么去做？
        1.闪烁
        2.一直亮
        3.一直灭
        4.呼吸灯
        5.流水灯
        .....
驱动只需要保证能用，控制权实际在应用手里，所以驱动开发除了写驱动还要会写应用，并且要遵循开发原则

本项目代码里面没有遵循上面的思想，是因为懒得另外写应用程序去测试驱动，仅仅为了学习，悉知。

## 学习顺序

=>当前学习进度

- 字符设备驱动demo

- 增加file_operations构成完整驱动
- 使用gpio_xxx接口实现基于引脚号的gpio驱动，简单体验一下功能和流程
- ioremap方式开发gpio驱动，最原始本质的实现必须掌握
- 使用gpiod_xxx接口实现基于描述符的gpio驱动，对比传统方式更方便的做法
- 引入中断功能配合gpio驱动的输入模式实现按键中断
- 完善按键中断驱动，理解进程调度：阻塞，非阻塞，多路复用等借口实现
- 设备驱动总线模型，总线概念学习，框架分层
- 简单分别实现设备、驱动、总线代码，三者组成形成简单字符设备驱动
- 基于前面的中断和gpio代码进行改写，在系统自带的平台总线上，分别实现设备、驱动代码
- 引入输入子系统，在输入子系统上实现按键中断驱动
- => 进一步学习输入子系统，实现能够产生多种按键类型多个键值的输入设备驱动



# 驱动开发规范
## 面向对象编程
用结构体表示一个对象，把所有相关的全局变量（如cdev，devno，cls等）整合成一个结构体
一般这个结构体起名为：name_desc
struct led_desc {
    dev_t devno;  
    ....
}
然后全局声明 
struct led_desc *led_dev = NULL;
接着在init的时候实例化对象(分配空间)
然后初始化对象
最后在exit的时候释放
## 出错处理
使用goto影子式编程做出错处理


# vs code增加内核头文件
.vscode/settings.json
```
    "C_Cpp.default.includePath": [
        "/lib/modules/5.4.0-9-generic/build/include"
    ]
```

# driver demo
driver demo

https://blog.csdn.net/zqixiao_09/article/details/50839042

编译
make

清空内核打印
sudo dmesg -C

加载模块
sudo insmod chry_math.ko
sudo insmod chrdev_demo.ko

查看申请到的设备名，设备号
cat /proc/devices   
ls -l /sys/class/chry/chrdev_demo/
ls -l /dev/chrdev_demo

读设备
cat /dev/chrdev_demo 

写设备
sudo echo 'hello world' > /dev/chrdev_demo
可能会出现权限错误,因为使用 sudo 只是让 echo 命令具有了 root 权限，但是没有让 “>” 和 ">>" 命令也具有 root 权限
正确的方式:
sudo sh -c "echo 'hello world' > /dev/chrdev_demo"
或者
echo 'hello world' | sudo tee /dev/chrdev_demo
注意，tee 命令的 "-a" 选项的作用等同于 ">>" 命令，如果去除该选项，那么 tee 命令的作用就等同于 ">" 命令。


卸载模块
sudo rmmod chrdev_demo
sudo rmmod chry_math

查看内核打印
dmesg

清理编译文件
make clean

# 环境变量

RK3288环境变量
export ARCH=arm
export CROSS_COMPILE=/home/chry/codes/firmware/rk3288/linux/prebuilts/gcc/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
make PLAT=rk3288

Hi3559A环境变量
export ARCH=arm64
export CROSS_COMPILE=aarch64-himix100-linux-
make PLAT=hi3559

# 驱动笔记

### 内核裁剪 

`make menuconfig`

### 编译内核 

先把`zImage`文件替换`/usr/bin/zImage`下并提供权限

`make uImage`

uImage = 64字节头 + zImage

### 设备树

设备树，dts类型的文件，用于描述设备信息

简单理解：一块板子，上面有gpio引脚，屏幕，usb，网口，这些信息都要在设备树上体现出来，编译时会根据设备树文件把对应的驱动编译到内核

相关dts文件在目录`arch/arm/boot/dts/`下

如果不想改默认的dts文件比如aaaa.dts，可以拷贝一份出来命名为bbbb.dts，然后修改Makefile文件，找到aaaa.dtb，修改成bbbb.dtb。

Makefile里面的dtb表示编译dts后生成的二进制文件，Makefile会根据dts名称找到对应的dts文件去编译

#### 示例：移植dm9000网卡驱动

实际是设备树文件的修改

vim bbbb.dts 

添加dm9000设备节点信息

```
srom-cs1@5000000 {
    compatible = "simple-bus";
    #address-cells = <1>;
    #size-cells = <1>;
    reg = <0x5000000 0x1000000>;
    ranges;

    ethernet@5000000 {
        compatible = "davicom,dm9000";
        reg = <0x5000000 0x2 0x5000004 0x2>;
        interrupt-parent = <&gpx0>;
        interrupts = <6 4>;
        davicom,no-eeprom;
        mac-address = [00 0a 2d a6 55 a2];
    };
};
```

编译dts文件make dtsb

配置内核make menuconfig，把新加上去的dm9000驱动选中，顺便打开NFS的支持(用于网络挂载根文件系统)

```
[*] Networking support  --->
	Networking options  --->
		<*> Packet socket
		<*>Unix domain sockets 
		[*] TCP/IP networking
		[*]   IP: kernel level autoconfiguration
Device Drivers  --->
	[*] Network device support  --->
		[*]   Ethernet driver support (NEW)  --->
			<*>   DM9000 support
File systems  --->
	[*] Network File Systems (NEW)  --->
		<*>   NFS client support
		[*]     NFS client support for NFS version 3
		[*]       NFS client support for the NFSv3 ACL protocol extension
		[*]   Root file system on NFS
```

退出保存，重新编译内核make uImage

上电时会发现内核报错: `random: nonblockingpool is initialized`

两种解决方法：

方法一：vim drivers/clk/clk.c

找到

static bool clk_ignore_unused;

修改成

static bool clk_ignore_unused = true ;

方法二：

修改内核启动参数bootargs，追加`clk_ignore_unused`参数，然后保存save




