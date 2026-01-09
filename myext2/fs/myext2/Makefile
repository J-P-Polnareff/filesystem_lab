obj-m := myext2.o
myext2-y := super.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$$PWD modules

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean
	rm -f *.o *.ko *.mod.c *.mod.o *.symvers *.order
