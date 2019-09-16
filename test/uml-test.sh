#!/bin/sh

mkdir -p proc
mount -t proc proc proc

insmod /lib/ubr.ko

/test.sh && halt -f

exit 1
