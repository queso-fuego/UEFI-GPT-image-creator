# To enable running scripts, run powershell as administrator and run the command
# Set-ExecutionPolicy RemoteSigned

# To run this script correctly, ensure powershell (or windows terminal, etc.) is running
#   as administrator

# Set image name from optional input arg
$image = $args[0]
if ($image -eq $null) { 
	$image = 'test.vhd' 
} else {
	$image = (Get-Item $image).Name	# Remove path if tab-completed, to be able to construct full path below
} 

# Mount VHD
Mount-DiskImage $PSScriptRoot\$image

# Assign drive letter to EFI System Partition on VHD
Get-Disk | Where FriendlyName -eq 'Msft Virtual Disk' | 
	Get-Partition | Where Type -eq 'System' | Set-Partition -NewDriveLetter X

# Mess with the mounted ESP if you want, come back and press enter when done
Pause

# Remove drive letter
Get-Disk | Where FriendlyName -eq 'Msft Virtual Disk' | 
	Get-Partition | Where Type -eq 'System' | Remove-PartitionAccessPath -AccessPath X:\

# Unmount VHD
Dismount-DiskImage $PSScriptRoot\$image

