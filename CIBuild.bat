rem Commands to build AppleWin on travis

setlocal

choco install visualstudio2019community
choco install visualstudio2019-workload-nativedesktop

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsMSBuildCmd.bat"

MSBuild.exe /p:Configuration=Release AppleWinExpress2019.sln
