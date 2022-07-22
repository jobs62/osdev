include Makefile.inc

mpc-1.2.1.tar.gz:
	wget https://ftp.gnu.org/gnu/mpc/$@

mpc-1.2.1/configure: mpc-1.2.1.tar.gz
	tar -xvf $^

build/mpc/Makefile: mpc-1.2.1/configure
	mkdir -p build/mpc && cd build/mpc && ../../mpc-1.2.1/configure --prefix=${PREFIX} --with-gmp-include=${PREFIX}/include --with-gmp-lib=${PREFIX}/lib

all: build/mpc/Makefile
	cd build/mpc && make all

install: build/mpc/Makefile
	cd build/mpc && make install
