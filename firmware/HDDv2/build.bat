@REM ACME only ever returns 0!
acme.exe hddrvr-v2.a65
@IF %ERRORLEVEL% NEQ 0 goto error

copy hddrvr-v2.bin ..\..\resource
@goto end

:error
@echo "ACME failed"

:end
