TARGET = tafi

obj-m := $(TARGET).o

tafi-objs := tafi_core.o tafi_fb.o tafi_chardev.o tafi_bus.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean