AS=nasm
CC=i686-elf-gcc
LD=i686-elf-ld
OBJCOPY=i686-elf-objcopy

CFLAGS= -O2 -g
CFLAGS+= -std=gnu99 -Wall -Wextra
CFLAGS+= -fstrength-reduce -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin
ASFLAGS= -f elf32

export PREFIX=${HOME}/opt/x-tools
export PATH := ${PREFIX}/bin:${PATH}
