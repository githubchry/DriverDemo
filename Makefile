PLAT?=X86

ifneq ($(KERNELRELEASE),)

obj-m := chrdev_demo.o

else

ifeq ($(PLAT),arm)
KERNELDIR:=/home/chry/code/rk3288/linux-sdk/kernel
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
