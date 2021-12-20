all: kernel/myos.bin disk.img

kernel/myos.bin: 
	make -C kernel myos.bin

userspace/init:
	make -C userspace all

disk.img: userspace/init
	dd if=/dev/zero of=$@ bs=1M count=100
	/usr/sbin/mkfs.fat --mbr=y $@
	mkdir diskmount
	sudo mount -o loop $@ diskmount
	sudo cp userspace/init diskmount/init
	sudo umount $@
	rmdir diskmount

run: kernel/myos.bin disk.img
	qemu-system-i386 -monitor stdio -kernel kernel/myos.bin -no-shutdown -no-reboot -drive file=disk.img,format=raw,if=virtio

clean:
	make -C userspace clean
	make -C kernel clean

mrproper: clean
	make -C userspace mrproper
	make -C kernel mrproper
	rm -f disk.img
