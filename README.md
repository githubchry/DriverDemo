# driverdemo
driver demo

make
sudo dmesg -C
sudo insmod chrdev_demo.ko
dmesg
sudo rmmod chrdev_demo
dmesg
make clean