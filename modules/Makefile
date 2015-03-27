default: ppid_map
KERNELDIR ?= /lib/modules/`uname -r`/build
PWD = `pwd`

ifeq ($(KERNELRELEASE),)

.PHONY: clean.
clean:
	rm -rf *.o *.~ *.ko *.mod.c

ppid_map:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

else
	obj-m:= ppid_map.o
endif
