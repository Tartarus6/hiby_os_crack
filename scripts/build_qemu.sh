#!/bin/bash

#HINT: Run me from project root :)

cd /qemu-v8.1.1
mkdir build
cd build

../configure --target-list=mipsel-softmmu \
             --enable-debug \
             --enable-kvm \
             --prefix=/usr/local

make -j$(nproc)
sudo make install
