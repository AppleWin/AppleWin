/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2014, Tom Charlesworth, Michael Pohoreski

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

#pragma once

class FrameBase;

void New_DOSProDOS_Disk( const char * pTitle, const std::string & pathname, const size_t nDiskSize,
	const bool bIsDOS33, const bool bNewDiskCopyBitsyBoot, const bool bNewDiskCopyBitsyBye, const bool bNewDiskCopyBASIC, const bool bNewDiskCopyProDOS,
	FrameBase *pFrame );                                                      // file will be overwritten
void New_Blank_Disk( const char * pTitle, const std::string & pathname, 
	const size_t nDiskSize, const bool bIsHardDisk, FrameBase *pFrame );      // file will be overwritten
void Format_ProDOS_Disk( const std::string & pathname, FrameBase *pFrame);  // file must exist
void Format_DOS33_Disk( const std::string & pathname, FrameBase *pFrame);   // file must exist
