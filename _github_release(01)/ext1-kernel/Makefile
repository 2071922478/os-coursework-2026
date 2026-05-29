# Makefile for oslab_chrdev kernel module
# Requires: linux-headers-$(uname -r)

obj-m := oslab_chrdev.o
SRCDIR := src
KDIR   := /lib/modules/$(shell uname -r)/build

.PHONY: all clean load unload test

all:
	cp $(SRCDIR)/oslab_chrdev.c .
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	rm -f oslab_chrdev.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f oslab_chrdev.c

load: all
	sudo insmod oslab_chrdev.ko
	@echo "Module loaded. Devices:"
	@ls -l /dev/oslab_chrdev 2>/dev/null || echo "  (check dmesg)"

unload:
	sudo rmmod oslab_chrdev
	@echo "Module unloaded."

test: load
	@echo ""
	@echo "===== Test 1: Write to device ====="
	@echo "Hello from OS Lab 2026!" | sudo tee /dev/oslab_chrdev
	@echo ""
	@echo "===== Test 2: Read from device ====="
	@sudo cat /dev/oslab_chrdev
	@echo ""
	@echo "===== Test 3: /proc info ====="
	@cat /proc/oslab_info
	@echo ""
	@echo "===== Test 4: Write more data ====="
	@echo "ABCDEFGHIJ" | sudo tee /dev/oslab_chrdev
	@echo "0123456789" | sudo tee /dev/oslab_chrdev
	@sudo cat /dev/oslab_chrdev
	@cat /proc/oslab_info
	@echo ""
	@echo "===== Cleaning up ====="
	@$(MAKE) unload
