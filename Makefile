# obj-m += watchpoint_module.o

# all:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

# clean:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

# CONFIG_MODULE_FORCE_UNLOAD=y

# # debug build:
# # "CFLAGS was changed ... Fix it to use EXTRA_CFLAGS."
# override EXTRA_CFLAGS+=-g -O0 

# obj-m += testhrarr.o
# #testhrarr-objs  := testhrarr.o

# all:
# 	@echo EXTRA_CFLAGS = $(EXTRA_CFLAGS)
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

# clean:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

obj-m += hello_kernel_module.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules CC=x86_64-linux-gnu-gcc-11

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean