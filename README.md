# UEFI-GPT-image-creator
GPT Disk Image Creator for UEFI Development, including EFI System Partition (ESP) with FAT32 Filesystem and Basic Data Partition

This is a self-contained C program to build a valid GPT disk image file, with FAT32 filesystem containing `/EFI/BOOT/` directories, and optional `BOOTX64.EFI` file.
Its purpose is to aid in UEFI development, and reduce dependencies on other programs to mount a disk, create a FAT32 filesystem, and move files into an image.

- Generated disk image files have been tested on both qemu and hardware (Dell XPS13 7390) after writing to a usb drive.

- Verified GPT status of output images with gdisk/sgdisk (gdisk64 on windows) and qemu with OVMF.

The generated image contains an EFI System Partition with a minimum size of ~33MiB for a sector size of 512 bytes, and an empty Basic Data Partition with a default size of 50MiB.
The data partition can be used to hold a file such as an OS or kernel binary.

The size of both partitions can be changed with command line parameters, see **Usage** section below.

If the file `BOOTX64.EFI` is in the current directory when `write_gpt` is ran, that file will be added to the `/EFI/BOOT/` directory in the ESP.
If that file is not found, `/EFI/BOOT/` will be empty in the created image.

A `DSKIMG.INF` file will be created containing the size of the generated image, and added to the `/EFI/BOOT/` directory.

If adding a file to the data partition with `-ad <file>` or `--add-data-file <file>`, a `FILENAME.INF` file will be created containing the size of the file and lba (sector) of the data partition.
This file will be added to the `/EFI/BOOT/` directory in the generated image. The purpose of this is to e.g. find a kernel file for an OS more easily within an EFI application.

A valid OVMF file for qemu is included as `bios64.bin`. Use it with qemu as `-bios bios64.bin`.

`qemu.bat`/`qemu.sh` is included as an example, change the drive, bios, and any other parms as needed.

## Dependencies
C compiler with support for C17 or higher (or minor changes will be needed), for UTF-16 string literals/u"" strings

## Build
Windows: `build` or `make`
Linux/BSD: `./build.sh` or `make`

## Usage
### Basic:
Windows: `write_gpt.exe`
Linux/BSD: `./write_gpt`

This will create a new image file with the default name `test.img`.

### Expanded:
```console
write_gpt [options]

options:
-h  --help             Print this help text
-es --esp-size         Set the size of the EFI System Partition in MB
-ds --data-size        Set the size of the Basic Data Partition in MiB; Minimum 
                       size is 1MiB 
-ls --lba-size         Set the lba (sector) size in bytes; This is considered
                       experimental, as I lack tools for proper testing.
                       Valid sizes: 512/1024/2048/4096 
-i  --image-name       Set the image name. Default name is 'test.img'
-ae --add-esp-file     Add a local file to the generated EFI System Partition.
                       File path must be qualified and quoted, name length
                       must be <= 8 characters, and file must be under root
                       ('/'), '/EFI/', or '/EFI/BOOT/'
                       example: -ae '/EFI/file1.txt' will add local file
                       './file1.txt' as '/EFI/FILE1.TXT' in the ESP.
-ad --add-data-file    Add a file to the basic data partition, and create a
                       <FILENAME.INF> file under '/EFI/BOOT/' in the ESP.
-v  --vhd              Create a fixed vhd footer, and add it to the end of the
                       disk image. The image name will have a .vhd suffix.);
```

-ae/--add-esp-file and -ad/--add-data will add `file_name` to the *new* image file each time. They do not update an existing image.
If no image name is provided, a default name `test.img` will be used.

## Example
![Example screenshot](./example_6-01-22.png "Showing an example of running a generated image in qemu.")
