#!/bin/bash

INSTALL_DIR=$PWD/plugin-malloc
git clone -b 5.3.0 --depth=1 https://github.com/jemalloc/jemalloc.git
cd jemalloc
./autogen.sh
./configure --with-jemalloc-prefix=plugin_ --enable-static --disable-shared --disable-initial-exec-tls --prefix=$INSTALL_DIR
make -j $(nproc)
make install
cd ..
rm -rf jemalloc
