include Makefile.inc

C_SRC= kernel.c gdt.c interrupt.c tss.c pci.c fat.c vmm.c pmm.c stdlib.c liballoc.c liballoc_hook.c virtio_blk.c bdev.c mbr.c
ASM_SRC= kernel.asm interrupt.asm

C_OBJ= $(C_SRC:.c=.o)
ASM_OBJ= $(ASM_SRC:.asm=.oa)

all: myos.bin

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

%.oa: %.asm
	${AS} ${ASFLAGS} $< -o $@

myos.bin: $(C_OBJ) $(ASM_OBJ) link.ld
	${LD} -m elf_i386 -T link.ld -o myos.bin $(C_OBJ) $(ASM_OBJ)
	${OBJCOPY} --only-keep-debug $@ $@.sym
	${OBJCOPY} --strip-debug $@

clean:
	rm -f ${C_OBJ} ${ASM_OBJ}

mrproper: clean
	rm -f myos.iso myos.bin myos.bin.sym disk.img
