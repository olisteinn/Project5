obj-m += part2module.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	dtc -@ -I dts -O dtb -o part2_overlay.dtbo part2_overlay.dts

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f part2_overlay.dtbo
