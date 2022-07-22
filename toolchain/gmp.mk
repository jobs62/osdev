include Makefile.inc

gmp-6.2.1.tar.bz2:
	wget https://ftp.gnu.org/gnu/gmp/$@

gmp-6.2.1/configure: gmp-6.2.1.tar.bz2
	tar -xvf $^

build/gmp/Makefile: gmp-6.2.1/configure
	mkdir -p build/gmp && cd build/gmp && ../../gmp-6.2.1/configure --prefix=${PREFIX}

all: build/gmp/Makefile
	cd build/gmp && make all

install: build/gmp/Makefile
	cd build/gmp && make install
