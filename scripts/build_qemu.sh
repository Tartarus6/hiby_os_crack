#!/bin/bash

#HINT: Run me from project root :)

cd /qemu
mkdir build
cd build

../configure --target-list=mipsel-softmmu \
             --enable-debug \
             --enable-kvm \
             --prefix=/usr/local

make -j$(nproc)
sudo make install
