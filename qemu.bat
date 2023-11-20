:: Sendin' Out a TEST O S
qemu-system-x86_64 ^
-drive format=raw,unit=0,file=test.vhd ^
-bios bios64.bin ^
-m 256M ^
-display sdl ^
-vga std ^
-name TESTOS ^
-machine q35 ^
-usb ^
-device usb-mouse ^
-rtc base=localtime ^
-net none
