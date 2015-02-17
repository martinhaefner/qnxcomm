obj-m += qnxcomm.o
qnxcomm-objs := channel.o connection.o driver_data.o \
	internal_msgsend.o process_entry.o qnxcomm_driver.o \
	proc.o connection_table.o 


all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
