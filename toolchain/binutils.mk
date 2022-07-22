include Makefile.inc

binutils-2.38.tar.gz:
	wget https://gnu.askapache.com/binutils/binutils-2.38.tar.gz

binutils-2.38/configure: binutils-2.38.tar.gz
	tar -vxf $^

build/binutils/Makefile: binutils-2.38/configure
	mkdir -p build/binutils && cd build/binutils && ../../binutils-2.38/configure --disable-nls --disable-werror --prefix="${PREFIX}" --target=${TARGET} --with-sysroot
	

all: build/binutils/Makefile
	cd build/binutils && make all

install: build/binutils/Makefile
	cd build/binutils && make all install

