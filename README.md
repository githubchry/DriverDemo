# driver demo
driver demo

https://blog.csdn.net/zqixiao_09/article/details/50839042

编译
make

清空内核打印
sudo dmesg -C

加载模块
sudo insmod chrdev_demo.ko

查看申请到的设备名，设备号
cat /proc/devices   
ls -l /sys/class/chry/chrdev_demo/
ls -l /dev/chrdev_demo
cat /dev/chrdev_demo 

卸载模块
sudo rmmod chrdev_demo

查看内核打印
dmesg

清理编译文件
make clean

# 环境变量

RK3288环境变量
export ARCH=arm
export CROSS_COMPILE=/home/chry/code/rk3288/linux-sdk/prebuilts/gcc/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
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



### ko与ko之间的相互调用

#### 全局符号表 EXPORT_SYMBOL

每个驱动都会提供一些接口供外部调用，当内核中的设备A的驱动接口想要让其他设备驱动去操作的时候，首先要做的就是把设备A的接口导出到全局符号表([EXPORT_SYMBOL](https://blog.csdn.net/qq_37858386/article/details/78444168))，导出之后才能让其他模块调用这些接口。

类似用户空间的so动态库





