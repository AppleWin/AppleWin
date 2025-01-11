@REM Toolchain
@REM * MSVC     (cl.exe)
@REM * Merlin32 (merlin32.exe)

del                 bootsector
merlin32.exe -V .   bootsector.s
cl.exe bin_to_c.cpp /nologo
       bin_to_c.exe bootsector
