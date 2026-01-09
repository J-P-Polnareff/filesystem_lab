obj-m := naivefs.o

KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# 移除-Werror以避免警告被当作错误
ccflags-y := -Wno-error=incompatible-pointer-types

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
