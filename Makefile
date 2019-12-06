PLAT?=x86

ifneq ($(KERNELRELEASE),)

obj-m := chrdev_demo.o

else

ifeq ($(PLAT), rk3288)
KERNELDIR:=/home/chry/code/rk3288/linux-sdk/kernel
else ifeq ($(PLAT), hi3559)
KERNELDIR:=/home/chry/code/hi3559a/Hi3559AV100R001C02SPC020/01.software/board/Hi3559AV100_SDK_V2.0.2.0/osdrv/opensource/kernel/linux-4.9.y_multi-core
else
KERNELDIR:=/lib/modules/$(shell uname -r)/build
endif

PWD:=$(shell pwd)

module:
	make -C $(KERNELDIR) M=$(PWD) modules
	#cp *.ko $(PWD)
.PHONY:clean
clean:
	make -C $(KERNELDIR) M=$(PWD) clean
endif
