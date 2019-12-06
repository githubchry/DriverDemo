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
ls -l /sys/class/chryclass/hello
ls -l /dev/hello
sudo cat /dev/chrdev_demo 

卸载模块
sudo rmmod chrdev_demo

查看内核打印
dmesg

清理编译文件
make clean