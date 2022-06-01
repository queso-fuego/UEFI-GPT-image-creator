# UEFI-GPT-image-creator
GPT Disk Image Creator for UEFI Development, with EFI System Partition and FAT32 Filesystem

This is a self-contained C program to build a valid GPT disk image file, with FAT32 filesystem containing `/EFI/BOOT/` directory, and optional `BOOTX64.EFI` file.
Purpose is to aid in UEFI development speed, and reduce dependencies on other programs to mount a disk, create a FAT32 filesystem, and move files over.

- Generated disk image files have been tested on both qemu and hardware (Dell XPS13 7390) after writing to a usb drive.

- Verified GPT status of output images with gdisk (gdisk64 on windows) and qemu with OVMF.

- Program Tested on Ubuntu, FreeBSD, and Windows 10

The image contains an EFI System Partition with a default size of 40MB, and an empty Basic Data Partition with a default size of 215MB for use with other developments.
The size of both can be changed with command line parameters, see **Usage** section below.

If the file `BOOTX64.EFI` is in the current directory when `write_gpt` is ran, that file will be added to the `/EFI/BOOT/` folder in the ESP.
If that file is not found, `/EFI/BOOT/` will be empty in the created image.

A valid OVMF file for qemu is included as `bios64.bin`. Use it with qemu as `-bios bios64.bin`.

`qemu.bat`/`qemu.sh` is included as an example, change the drive and bios names as needed, as well as any other parms.

## Build
Windows: `build` \
Linux/BSD: `./build.sh` 

## Usage
### Basic:
Windows: `write_gpt [image_name]` \
Linux/BSD: `./write_gpt [image_name]` 

This will write a new image file with the default name `test.img`.

### Expanded:
```console
write_gpt [-h --help] [image_name [-ue --update-efi file_name] [-ud --update-data file_name] [-ad --add-data file_name]
          [-es --efi-size efi_size] [-ds --data-size data_size]]
-h --help:         Print this message
image_name:        Name of output GPT disk image file
-ue --update-efi:  Update/overwrite file_name in the /EFI/BOOT/ directory of the EFI system partition
-ud --update-data: Update/overwrite file_name in the basic data partition
-ad --add-data:    Add file_name to the basic data partition in the created image file
-es --efi-size:    Set the size of the EFI System Partition in MB; Minimum size of 32 for FAT32
-ds --data-size:   Set the size of the Basic Data Partition in MB; Minimum size of 0 for empty/no data partition
```

Multi-dash options are aliases for the single dash options i.e. -ue is the same as --update-efi. \
-ad or --add-data will only add `file_name` to a *new* image file. Existing files should use -ud or --update-data instead. \
-ad can not be used with -ue/-ud. \
If no image name is provided, a default `test.img` file is created. 

## Example
![Example screenshot](./example_6-01-22.png "Showing an example of running a generated image in qemu.")
