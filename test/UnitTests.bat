@REM %1 = <Debug|Release>
@IF "%~1" == "" GOTO help

@ECHO Performing unit-test: TestCPU6502
.\%1\TestCPU6502.exe
@IF errorlevel 1 GOTO failed

@ECHO Performing unit-test: TestDebugger
.\%1\TestDebugger.exe
@if errorlevel 1 GOTO failed

@GOTO end

:failed
@ECHO Unit-test failed
EXIT /B 1

:help
@ECHO %0 "<Debug|Release>"

:end