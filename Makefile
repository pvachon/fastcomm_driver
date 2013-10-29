obj-m			+= fastcomm.o

KERNEL_SRC=/lib/modules/$(shell uname -r)

all:
	make -C $(KERNEL_SRC)/build M=$(PWD) modules

clean:
	make -C $(KERNEL_SRC)/build M=$(PWD) clean

