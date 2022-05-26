# UEFI-GPT-image-creator
GPT Disk Image Creator for UEFI Development, with EFI System Partition and FAT32 Filesystem

This is a self-contained C program to build a valid GPT disk image file, with FAT32 filesystem containing /EFI/BOOT/BOOTX64.EFI directories and file.
Purpose is to aid in UEFI development speed, and reduce dependencies on other programs to mount a disk, create a FAT32 filesystem, and move files over.

Generated disk image files have been tested on both qemu and hardware (Dell XPS13 7390) after writing to a usb drive.

Verified GPT status of output images with gdisk (gdisk64 on windows) and qemu with OVMF.

If not given an image name, a default 'test.img' file is created.

Currently limited to 256MB in size, with a 40MB EFI system partition, and a 215 MB empty Basic Data partition for use with other developments.

Usage:
`write_gpt [image_name]`

NOTE: This assumes you already have a valid BOOTX64.EFI file in the current directory to use. The program will error if not.
qemu.bat is included as an example, change the drive and bios names as needed, as well as any other parms.

To build:
`build`
