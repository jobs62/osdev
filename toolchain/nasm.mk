include Makefile.inc

nasm-2.15.05.tar.gz:
	wget https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/nasm-2.15.05.tar.gz

nasm-2.15.05/configure: nasm-2.15.05.tar.gz
	tar -xvf $^

build/nasm/Makefile: nasm-2.15.05/configure
	mkdir -p build/nasm && cd build/nasm && ../../nasm-2.15.05/configure --with-sysroot=${PREFIX} --prefix=${PREFIX}

all: build/nasm/Makefile
	cd build/nasm && make all

install: build/nasm/Makefile
	cd build/nasm && make all install
