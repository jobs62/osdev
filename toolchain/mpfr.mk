include Makefile.inc

mpfr-4.1.0.tar.gz:
	wget https://ftp.gnu.org/gnu/mpfr/$@

mpfr-4.1.0/configure: mpfr-4.1.0.tar.gz
	tar -xvf $^

build/mpfr/Makefile: mpfr-4.1.0/configure
	mkdir -p build/mpfr && cd build/mpfr && ../../mpfr-4.1.0/configure --prefix=${PREFIX} --with-gmp-include=${PREFIX}/include --with-gmp-lib=${PREFIX}/lib

all: build/mpfr/Makefile
	cd build/mpfr && make all

install: build/mpfr/Makefile
	cd build/mpfr && make install
