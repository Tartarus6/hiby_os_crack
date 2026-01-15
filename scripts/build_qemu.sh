#!/bin/bash

#HINT: Run me from project root :)

cd qemu

make clean

../configure --python=/usr/bin/python3 \
             --cpu=x86_64 \
             --target-list=mipsel-softmmu \
             --enable-debug \
             --enable-kvm \
             --prefix=/usr/local \
             --disable-docs

make -j$(nproc)
sudo make install
