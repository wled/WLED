@echo off
SETLOCAL
SET FWPATH=c:\path\to\your\WLED\build_output\firmware
GOTO ESPS

:UPDATEONE
IF NOT EXIST %FWPATH%\%2 GOTO SKIP
	ping -w 1000 -n 1 %1 | find "TTL=" || GOTO SKIP
	ECHO Updating %1
	curl -s -F "update=@%FWPATH%/%2" %1/update >nul
:SKIP
GOTO:EOF

:ESPS
call :UPDATEONE 192.168.x.x firmware.bin
call :UPDATEONE ....
