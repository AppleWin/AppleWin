@REM Toolchain
@REM * MSVC     (cl.exe)
@REM * Acme     (ache.exe)

del                 bootsector.bin
acme.exe            bootsector.a
cl.exe bin_to_c.cpp /nologo
       bin_to_c.exe bootsector.bin
