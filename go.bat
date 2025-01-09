call build.bat

@echo off
if %errorlevel% NEQ 0 (
GOTO END_BUGGY
)

call run.bat

GOTO END_OK
:END_BUGGY
@echo ***********************************************************************
@echo                                ERROR
@echo ***********************************************************************
GOTO FINAL
:END_OK
