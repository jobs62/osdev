include Makefile.inc

gcc-12.1.0.tar.gz:
	wget https://gnu.askapache.com/gcc/gcc-12.1.0/gcc-12.1.0.tar.gz

gcc-12.1.0/configure: gcc-12.1.0.tar.gz
	tar -vxf $^
	patch -ruN -p0 < gcc.patch

build/gcc/Makefile: gcc-12.1.0/configure
	mkdir -p build/gcc && cd build/gcc && ../../gcc-12.1.0/configure --without-headers --enable-initfini-array --enable-languages=c,c++ --target=${TARGET} --prefix=${PREFIX} --with-gmp-include=${PREFIX}/include --with-mpc-include=${PREFIX}/include --with-mpfr-include=${PREFIX}/include --with-gmp-lib=${PREFIX}/lib --with-mpc-lib=${PREFIX}/lib --with-mpfr-lib=${PREFIX}/lib --disable-nls --disable-libstdcxx

all: build/gcc/Makefile
	cd build/gcc && make all-gcc all-target-libgcc

install: build/gcc/Makefile
	cd build/gcc && make all-gcc all-target-libgcc install-gcc install-target-libgcc

