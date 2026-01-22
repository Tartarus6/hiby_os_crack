#!/bin/bash

#HINT: Run me from project root :)

cd qemu-10.2.0

./configure --python=/usr/bin/python3 \
             --cpu=x86_64 \
             --target-list=mipsel-softmmu \
             --enable-debug \
             --enable-kvm \
             --prefix=/usr/local \
             --disable-docs

make clean

make -j$(nproc)
sudo make install
