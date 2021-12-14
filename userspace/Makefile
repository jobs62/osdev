AS=nasm
CC=gcc
LD=ld

C_SRC= init.c
ASM_SRC= start.asm

CFLAGS= -m32 -O2 -g
CFLAGS+= -std=gnu99 -Wall -Wextra
CFLAGS+= -fstrength-reduce -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin

LDFLAGS= -f elf32

C_OBJ= $(C_SRC:.c=.o)
ASM_OBJ= $(ASM_SRC:.asm=.oa)

all: init

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

%.oa: %.asm
	${AS} ${LDFLAGS} $< -o $@

init: $(C_OBJ) $(ASM_OBJ) link.ld
	${LD} -m elf_i386 -T link.ld -o init $(C_OBJ) $(ASM_OBJ)

clean:
	rm -f ${C_OBJ} ${ASM_OBJ}
