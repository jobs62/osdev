TARGET=i686-pc-myos

AS=nasm
CC=${TARGET}-gcc
LD=${TARGET}-ld
OBJCOPY=${TARGET}-objcopy

CFLAGS= -O2 -g
CFLAGS+= -std=gnu99 -Wall -Wextra -Werror=discarded-qualifiers
CFLAGS+= -fstrength-reduce -fomit-frame-pointer -finline-functions -ffreestanding -fno-builtin
ASFLAGS= -f elf32

TARGET=i686-pc-myos
export PREFIX=${HOME}/opt/x-tools
export PATH := ${PREFIX}/bin:${PATH}
