@echo off

set UNMOUNT_SCRIPT=unmnt_vhd_windows_diskpart.txt

:: Run unmount script created by "mnt_vhd_windows_diskpart.bat" script
diskpart /s %~dp0%UNMOUNT_SCRIPT%

:: Remove unmount script when done
:: del %UNMOUNT_SCRIPT%

