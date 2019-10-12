@REM ACME only ever returns 0!
acme.exe hddrvr.a65
@IF %ERRORLEVEL% NEQ 0 goto error

copy hddrvr.bin ..\..\resource
@goto end

:error
@echo "ACME failed"

:end
