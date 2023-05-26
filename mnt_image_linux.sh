#!/bin/sh

# NOTE: Replace 'doas' with 'sudo' as needed
# NOTE: This assumes you have 'nbd' and 'nbd-client' packages installed.
#   If qemu-nbd is not installed, then you may also need 'qemu-utils' 

IMAGE="$1"
if [ -z "$IMAGE" ]; then
    IMAGE="test.img"
fi

# Add network block device module
doas modprobe nbd

# Create mount point for partition 1 (EFI System Partition)
doas mkdir -p /mnt/p1

# Connect client to nbd device
doas nbd-client -c /dev/nbd0

# Use qemu to connect to nbd, creating partitions for each partition
# on the image, these will be e.g. nbd0p1, nbd0p2, ...
# in this case nbd0p1 should be the EFI System Partition
doas qemu-nbd -c /dev/nbd0 "$IMAGE"

# Wait a few secs to ensure nbd0 partitions are made
sleep 1

# Mount nbd partition 1 (ESP) as read/write to mount point
doas mount -t vfat -o rw /dev/nbd0p1 /mnt/p1
