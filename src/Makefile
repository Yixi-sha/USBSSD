CONFIG_MODULE_SIG=n
ifneq ($(KERNELRELEASE),)
obj-m := USBSSD.o
USBSSD-objs := main.o request.o allocator.o sub_request.o command.o
else
KDIR :=/lib/modules/$(shell uname -r)/build
all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	rm -f *.ko *.o *.mod.o *.mod.c *.symvers *.order *.mod *.dwo .*.cmd
endif

rebuild : clean all