# driverdemo
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



RK3288环境变量
export ARCH=arm
export CROSS_COMPILE=/home/chry/code/rk3288/linux-sdk/prebuilts/gcc/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
make PLAT=rk3288

Hi3559A环境变量
export ARCH=arm64
export CROSS_COMPILE=aarch64-himix100-linux-
make PLAT=hi3559


