obj-m += hostcc-module.o
hostcc-module-y = hostcc-local-response.o hostcc-signals.o hostcc-network-response.o hostcc-logging.o hostcc.o
PWD := $(CURDIR)
all:	
		python3 generate_system_params.py
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean		
