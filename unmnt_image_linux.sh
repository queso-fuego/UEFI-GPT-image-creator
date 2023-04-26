#!/bin/sh

# Unmount partition 1
doas umount /mnt/p1

# Disconnect qemu nbd
doas qemu-nbd -d /dev/nbd0 

# Remove mount point for partition 1 
doas rm -rf /mnt/p1

# Disconnect client to nbd device
doas nbd-client -d /dev/nbd0

# Remove nbd module
doas modprobe -r nbd
