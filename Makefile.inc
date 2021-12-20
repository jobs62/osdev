AS=nasm
CC=gcc
LD=ld

CFLAGS= -m32 -O2 -g
CFLAGS+= -std=gnu99 -Wall -Wextra
CFLAGS+= -fstrength-reduce -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin

LDFLAGS= -f elf32