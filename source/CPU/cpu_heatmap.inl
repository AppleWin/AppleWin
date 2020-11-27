/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2020, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: 6502/65C02 emulation
 *
 * Author: Various
 */

 /****************************************************************************
*
*  RAM ACCESS MACROS (built-in debugger mode)
*
***/

inline void Heatmap_R(uint16_t address)
{
	// todo
}

inline void Heatmap_W(uint16_t address)
{
	// todo
}

inline void Heatmap_X(uint16_t address)
{
	// todo
}

inline uint8_t Heatmap_ReadByte(uint16_t addr, int uExecutedCycles)
{
	Heatmap_R(addr);
	return _READ;
}

inline uint8_t Heatmap_ReadByte_With_IO_F8xx(uint16_t addr, int uExecutedCycles)
{
	Heatmap_R(addr);
	return _READ_WITH_IO_F8xx;
}

inline void Heatmap_WriteByte(uint16_t addr, uint16_t value, int uExecutedCycles)
{
	Heatmap_W(addr);
	_WRITE(value);
}

inline void Heatmap_WriteByte_With_IO_F8xx(uint16_t addr, uint16_t value, int uExecutedCycles)
{
	Heatmap_W(addr);
	_WRITE_WITH_IO_F8xx(value);
}
