AS=nasm
CC=i386-elf-gcc
LD=i386-elf-ld

C_SRC= kernel.c gdt.c interrupt.c tss.c pci.c ata.c fat.c vmm.c pmm.c stdlib.c liballoc.c liballoc_hook.c virtio_blk.c bdev.c
ASM_SRC= kernel.asm interrupt.asm

CFLAGS= -m32 -O2 -g
CFLAGS+= -std=gnu99 -Wall -Wextra
CFLAGS+= -fstrength-reduce -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin

LDFLAGS= -f elf32

C_OBJ= $(C_SRC:.c=.o)
ASM_OBJ= $(ASM_SRC:.asm=.oa)

all: myos.bin

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

%.oa: %.asm
	${AS} ${LDFLAGS} $< -o $@

myos.bin: $(C_OBJ) $(ASM_OBJ) link.ld
	${LD} -m elf_i386 -T link.ld -o myos.bin $(C_OBJ) $(ASM_OBJ)

myos.iso: myos.bin
	mkdir -p isodir/boot/grub
	cp myos.bin isodir/boot/myos.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o myos.iso isodir

run: myos.bin
	qemu-system-i386 -monitor stdio -kernel myos.bin -no-shutdown -no-reboot -drive file=disk.img,if=virtio

clean:
	rm -rf isodir
	rm -f ${C_OBJ} ${ASM_OBJ}

mrproper: clean
	rm -f myos.iso myos.bin
