#!/bin/sh

# stop already exist process
for pid in $(pgrep shairport); do
    if [ $pid != $$ ]; then
        kill $pid
    fi
done
exit 0

