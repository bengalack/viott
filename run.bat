@echo Make sure MSXDOS is present in the dska-folder
@REM C:\tools\openmsx20\openmsx.exe -machine Sanyo_phc-70FD -diska dska/ -command "debug set_watchpoint read_io 0x2E" -command "set throttle off" -command "after time 7 {set throttle on}"
@REM C:\tools\openmsx20\openmsx.exe -machine Sanyo_phc-70FD2 -diska dska/ -command "debug set_watchpoint read_io 0x2E" -command "set throttle off" -command "after time 7 {set throttle on}"
@REM C:\tools\openmsx20\openmsx.exe -machine Philips_NMS_8255 -diska dska/ -command "debug set_watchpoint read_io 0x2E" -command "set throttle off" -command "after time 7 {set throttle on}"
@REM C:\tools\openmsx20\openmsx.exe -machine Panasonic_FS-A1WX -diska dska/ -command "debug set_watchpoint read_io 0x2E" -command "set throttle off" -command "after time 7 {set throttle on}"
@REM C:\tools\openmsx20\openmsx.exe -machine Panasonic_FS-A1WSX -diska dska/ -command "debug set_watchpoint read_io 0x2E" -command "set throttle off" -command "after time 7 {set throttle on}"
@REM C:\tools\openmsx20\openmsx.exe -machine Sony_HB-F1XD -diska dska/ -command "debug set_watchpoint read_io 0x2E" -command "set throttle off" -command "after time 7 {set throttle on}"

C:\tools\openmsx20\openmsx.exe -machine Panasonic_FS-A1ST -diska dska/ -command "debug set_watchpoint read_io 0x2E" -command "set throttle off" -command "after time 7 {set throttle on}" -script openmsx.tcl

@REM Just for testing that MSX1 does not work
@REM C:\tools\openmsx20\openmsx.exe -machine Spectravideo_SVI-738 -diska dska/ -command "debug set_watchpoint read_io 0x2E" -command "set throttle off" -command "after time 7 {set throttle on}"