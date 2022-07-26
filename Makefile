include Makefile.inc

SUBDIR += kernel
SUBDIR += userspace

all: ${SUBDIR}

$(SUBDIR): force_look
	make -C $@

force_look:
	true

disk.img: userspace
	dd if=/dev/zero of=$@ bs=1M count=100
	/usr/sbin/mkfs.fat --mbr=y $@
	mkdir diskmount
	sudo mount -o loop $@ diskmount
	sudo cp userspace/init diskmount/init
	sudo umount $@
	rmdir diskmount

run: kernel disk.img
	qemu-system-i386 -kernel kernel/myos.bin -no-shutdown -no-reboot -drive file=disk.img,format=raw,if=virtio -S -gdb tcp::1234

clean:
	for d in $(SUBDIR); do make -C $$d clean; done

mrproper:
	for d in $(SUBDIR); do make -C $$d mrproper; done
	rm -f disk.img
