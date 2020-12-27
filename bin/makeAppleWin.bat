@IF "%~1" == "" GOTO help
@IF "%APPLEWIN_ROOT%" == "" GOTO help2

@MKDIR "%~1"
@COPY /Y "%APPLEWIN_ROOT%\bin\A2_BASIC.SYM" "%~1"
@COPY /Y "%APPLEWIN_ROOT%\bin\APPLE2E.SYM" "%~1"
@COPY /Y "%APPLEWIN_ROOT%\bin\DELREG.INF" "%~1"
@COPY /Y "%APPLEWIN_ROOT%\bin\DebuggerAutoRun.txt" "%~1"
@COPY /Y "%APPLEWIN_ROOT%\bin\GNU General Public License.txt" "%~1"
@COPY /Y "%APPLEWIN_ROOT%\bin\History.txt" "%~1"
@COPY /Y "%APPLEWIN_ROOT%\bin\MASTER.DSK" "%~1"
@COPY /Y "%APPLEWIN_ROOT%\docs\Debugger_Changelog.txt" "%~1"
@COPY /Y "%APPLEWIN_ROOT%\help\AppleWin.chm" "%~1"
@COPY /Y "%APPLEWIN_ROOT%\Release v141_xp\AppleWin.exe" "%~1"
@COPY /Y "%APPLEWIN_ROOT%\Release v141_xp\HookFilter.dll" "%~1"
CD "%~1"
"C:\Program Files (x86)\7-Zip\7z.exe" a ..\AppleWin"%~1".zip *
@REM Even though LINK has /PDB: outputting to HookFilter.pdb, it ends up being called vc141.pdb! (and remaining in the HookFilter folder)
"C:\Program Files (x86)\7-Zip\7z.exe" a ..\AppleWin"%~1"-PDB.zip "%APPLEWIN_ROOT%\Release v141_xp\AppleWin.pdb" "%APPLEWIN_ROOT%\HookFilter\Release v141_xp\vc141.pdb"
CD ..
@GOTO end

:help
@ECHO %0 "<new version>"
@ECHO EG: %0 1.29.8.0
@GOTO end

:help2
@ECHO APPLEWIN_ROOT env variable is not defined

:end
