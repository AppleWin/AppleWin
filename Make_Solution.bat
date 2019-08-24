@echo off

set version=%1
if "%1"=="" (
    set version=vs2019
)

set geniepath=%~dp0genie
pushd %geniepath%
genie.exe %version%
popd
if ERRORLEVEL 1 goto FAIL

echo Visual Studio solution created: %geniepath%\AppleWin.sln
exit /b

:FAIL
echo.
echo The GENie tool failed to run. Please download the genie.exe file
echo from https://github.com/bkaradzic/GENie and place it in the genie
echo subdirectory (or somewhere in your path).
