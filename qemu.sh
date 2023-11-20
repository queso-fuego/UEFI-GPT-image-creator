#!/bin/sh

# Sendin' Out a TEST O S
qemu-system-x86_64 \
-drive format=raw,file=test.hdd \
-bios bios64.bin \
-m 256M \
-vga std \
-name TESTOS \
-machine q35 \
-usb \
-device usb-mouse \
-rtc base=localtime \
-net none

# For testing other drive physical/logical sizes. Although this did not work for me for lba size > 512
#qemu-system-x86_64 \
#-bios bios64.bin \
#-vga std \
#-boot d -device virtio-scsi-pci,id=scsi1,bus=pci.0 \
#-drive file=test.img,if=none,id=drive-virtio-disk1 \
#-device scsi-hd,bus=scsi1.0,drive=drive-virtio-disk1,id=virtio-scsi-pci,physical_block_size=1024,logical_block_size=1024 \
#-net none

