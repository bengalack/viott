@echo Make sure MSXDOS is present in the dska-folder
@REM C:\tools\openmsx20\openmsx.exe -machine Sanyo_phc-70FD -ext ram1MB -ext msxdos2 -diska dska/ -script openmsx.tcl
@REM C:\tools\openmsx20\openmsx.exe -machine Sanyo_phc-70FD2 -ext ram1MB -ext msxdos2 -diska dska/ -script openmsx.tcl
@REM C:\tools\openmsx20\openmsx.exe -machine Philips_NMS_8255 -ext ram1MB -ext msxdos2 -diska dska/ -script openmsx.tcl
@REM C:\tools\openmsx20\openmsx.exe -machine Panasonic_FS-A1WX -ext ram1MB -ext msxdos2 -diska dska/ -script openmsx.tcl
C:\tools\openmsx20\openmsx.exe -machine Panasonic_FS-A1WSX -ext ram1MB -ext msxdos2 -diska dska/ -script openmsx.tcl
@REM C:\tools\openmsx20\openmsx.exe -machine Sony_HB-F1XD -ext ram1MB -ext msxdos2 -diska dska/ -script openmsx.tcl

@REM C:\tools\openmsx20\openmsx.exe -machine Panasonic_FS-A1ST -ext ram1MB -ext msxdos2 -diska dska/ -script openmsx.tcl

@REM Just for testing that MSX1 does not work
@REM C:\tools\openmsx20\openmsx.exe -machine Spectravideo_SVI-738 -ext ram1MB -ext msxdos2 -diska dska/ -script openmsx.tcl