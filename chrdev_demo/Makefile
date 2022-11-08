PLAT?=imx6ull

# make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-

# 模块名
MODNAME = chrdev_demo

ifneq ($(KERNELRELEASE),)

obj-m := $(MODNAME).o

else

ifeq ($(PLAT), rk3288)
KERNELDIR:=/home/chry/codes/firmware/rk3288/linux/kernel
else ifeq ($(PLAT), imx6ull)
KERNELDIR:=/home/chry/imx6ul/kernel-source-4.14.98
else ifeq ($(PLAT), hi3559)
KERNELDIR:=/home/chry/code/hi3559a/Hi3559AV100R001C02SPC020/01.software/board/Hi3559AV100_SDK_V2.0.2.0/osdrv/opensource/kernel/linux-4.9.y_multi-core
else
KERNELDIR:=/lib/modules/$(shell uname -r)/build
endif

PWD:=$(shell pwd)

module:
	make -C $(KERNELDIR) M=$(PWD) modules
	#cp *.ko /home/chry/mnt/rk3288

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
    
# load/unload也就是本机测试用一下 以后编译完直接把模块上传上去 根本用不到这个
load:
	sudo insmod $(NAME).ko

unload:
	sudo rmmod $(NAME)

endif
