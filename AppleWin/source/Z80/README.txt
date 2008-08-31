                           ******* Z80Em *******
                        Portable Zilog Z80 Emulator
                         Version 1.2 (04-09-1997)
                                     
                Copyright (C) Marcel de Kogel 1996,1997
                              

This package contains a portable Z80 emulator. It includes two engines: One
written in pure C, which can be used on just about every 32+ bit system, and
one optimised for GCC/x86 (DJGPP, Linux, FreeBSD). It can be used to emulate
systems with multiple Z80s and systems with several different CPUs
You can use in your own applications, as long as proper credit is given,
no profit is made and I am notified. If you want to use this code for
commercial purposes, e.g. using it in commercial projects, selling it, etc.,
please contact me

To use the code, check the system dependent part of Z80.h, write your own
memory and I/O functions, which are declared in Z80IO.h, write your own
Interrupt(), Z80_Patch(), Z80_Reti() and Z80_Retn() functions, initialise
the Z80_IPeriod and Z80_IRQ variables, call Z80_Reset() to reset all registers
to their initial values or call Z80_SetRegs() to set all of them to pre-loaded
ones, and finally call Z80() or repeatedly call Z80_Execute(), the latter
option should be used in multi-processor emulations

For updates and examples on how to use this engine, check my homepage at
http://www.komkon.org/~dekogel/

History
-------
1.2   04-09-97   Fixed some more bugs
1.1   13-02-97   Fixed several bugs and compatibility problems
1.0   31-01-97   Initial release

Please send your comments and bug reports to
m.dekogel@student.utwente.nl
