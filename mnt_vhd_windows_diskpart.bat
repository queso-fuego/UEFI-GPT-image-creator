@echo off

:: NOTE: Image must be a .vhd file, with VHD footer.
::   Use "./write_gpt.exe -v" or "./write_gpt.exe --vhd" to create a VHD file

:: NOTE: Unfortunately there isn't a good way to determine the 
::   total number of volumes at runtime, or the right ones to use. 
::   Run diskpart manually at least once to get the
::   correct volume numbers for your setup, and note that you will 
::   have to change the volume numbers after adding or removing
::   any disks or partitions on your system. Be careful.

set IMAGE=%1
set MOUNT_SCRIPT=mnt_vhd_windows_diskpart.txt
set UNMOUNT_SCRIPT=unmnt_vhd_windows_diskpart.txt

:: Default image name if called with no arguments
if "%IMAGE%"=="" (
    set IMAGE=test.vhd
)

:: Create diskpart mount script 
:: Select virtual hard disk .vhd image
echo select vdisk file="%~dp0%IMAGE%" > %MOUNT_SCRIPT%

:: Attach vhd
echo attach vdisk >> %MOUNT_SCRIPT%

:: Since the volumes are in random order when scripted, set 
::   ids on both and mount both to be sure
echo select volume 4 >> %MOUNT_SCRIPT%
echo set id=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7 >> %MOUNT_SCRIPT%
echo assign letter=x >> %MOUNT_SCRIPT%

echo select volume 5 >> %MOUNT_SCRIPT%
echo set id=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7 >> %MOUNT_SCRIPT%
echo assign letter=y >> %MOUNT_SCRIPT%

:: Create diskpart unmount script 
echo select vdisk file="%~dp0%IMAGE%" > %UNMOUNT_SCRIPT%

:: Since the volume are in random order when scripted, remove both
echo select volume 4 >> %UNMOUNT_SCRIPT%
echo remove >> %UNMOUNT_SCRIPT%
echo select volume 5 >> %UNMOUNT_SCRIPT%
echo remove >> %UNMOUNT_SCRIPT%
echo detach vdisk >> %UNMOUNT_SCRIPT%

:: Run mount script
diskpart /s %~dp0%MOUNT_SCRIPT%

:: Remove mount script when done
:: del %MOUNT_SCRIPT%
