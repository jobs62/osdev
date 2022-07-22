include Makefile.inc

.gcc: .gmp .mpfr .mpc .binutils
	make -f gcc.mk install
	touch .gcc

.nasm:
	make -f nasm.mk install
	touch .nasm

.gmp:
	make -f gmp.mk install
	touch .gmp

.mpfr: .gmp
	make -f mpfr.mk install
	touch .mpfr

.mpc: .gmp .mpfr
	make -f mpc.mk install
	touch .mpc

.binutils:
	make -f binutils.mk install
	touch .binutils

toolchain: .gcc .nasm
