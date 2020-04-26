PLAT?=rk3288

# 模块名
NAME = chrybus_demo

ifneq ($(KERNELRELEASE),)

obj-m := $(NAME).o
obj-m += chrydrv_demo.o
obj-m += chrydev_demo.o
obj-m += platform_gpiodev.o
obj-m += platform_gpiodrv.o

else

ifeq ($(PLAT), rk3288)
KERNELDIR:=/home/chry/codes/firmware/rk3288/linux/kernel
else ifeq ($(PLAT), hi3559)
KERNELDIR:=/home/chry/code/hi3559a/Hi3559AV100R001C02SPC020/01.software/board/Hi3559AV100_SDK_V2.0.2.0/osdrv/opensource/kernel/linux-4.9.y_multi-core
else
KERNELDIR:=/lib/modules/$(shell uname -r)/build
endif

PWD:=$(shell pwd)

module:
	make -C $(KERNELDIR) M=$(PWD) modules
	cp *.ko /srv/mnt/02rk3288

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
# 这两个也就是测试用一下 以后编译完直接把模块上传上去 根本用不到这个
load:
	sudo insmod $(NAME).ko

unload:
	sudo rmmod $(NAME)

endif
