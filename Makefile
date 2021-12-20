AS=nasm
CC=gcc
LD=ld

C_SRC= kernel.c gdt.c interrupt.c tss.c pci.c ata.c fat.c vmm.c pmm.c stdlib.c liballoc.c liballoc_hook.c virtio_blk.c bdev.c mbr.c
ASM_SRC= kernel.asm interrupt.asm

CFLAGS= -m32 -O2 -g
CFLAGS+= -std=gnu99 -Wall -Wextra
CFLAGS+= -fstrength-reduce -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin

LDFLAGS= -f elf32

C_OBJ= $(C_SRC:.c=.o)
ASM_OBJ= $(ASM_SRC:.asm=.oa)

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
	qemu-system-i386 -monitor stdio -kernel kernel/myos.bin -no-shutdown -no-reboot -drive file=disk.img,if=virtio

clean:
	make -C userspace clean
	make -C kernel clean

mrproper: clean
	make -C userspace mrproper
	make -C kernel mrproper
	rm -f disk.img
