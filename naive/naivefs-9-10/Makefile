obj-m := naivefs.o
naivefs-objs := naivefs_main.o naivefs_super.o naivefs_inode.o naivefs_file.o naivefs_dir.o

KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f *.ko *.mod* *.o *.symvers *.order

install:
	sudo insmod naivefs.ko

uninstall:
	sudo rmmod naivefs

test: all install
	sudo dmesg | tail -20

.PHONY: all clean install uninstall test
