@REM ACME only ever returns 0!
acme.exe HDC-SmartPort.a65
@IF %ERRORLEVEL% NEQ 0 goto error

copy HDC-SmartPort.bin ..\..\resource
@goto end

:error
@echo "ACME failed"

:end
