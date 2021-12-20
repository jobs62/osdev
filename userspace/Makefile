include Makefile.inc

C_SRC= init.c
ASM_SRC= start.asm

C_OBJ= $(C_SRC:.c=.o)
ASM_OBJ= $(ASM_SRC:.asm=.oa)

all: init

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

%.oa: %.asm
	${AS} ${LDFLAGS} $< -o $@

init: $(C_OBJ) $(ASM_OBJ) link.ld
	${LD} -m elf_i386 -T link.ld -o init $(C_OBJ) $(ASM_OBJ)
	objcopy --only-keep-debug $@ $@.sym
	objcopy --strip-debug $@

clean:
	rm -f ${C_OBJ} ${ASM_OBJ}

mrproper:
	rm -f init
