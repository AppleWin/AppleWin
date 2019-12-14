/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2019, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

/* Description: Card Manager
 *
 * Author: Various
 *
 */

#include "StdAfx.h"

#include "AppleWin.h"
#include "CardManager.h"
#include "Disk.h"

bool CardManager::Disk2IsConditionForFullSpeed(void)
{
	for (UINT i = 0; i < NUM_SLOTS; i++)
	{
		if (g_Slot[i] == CT_Disk2)
		{
			if (i == 5 && sg_pDisk2CardSlot5->IsConditionForFullSpeed())
				return true;
			if (i == 6 && sg_Disk2Card.IsConditionForFullSpeed())
				return true;
		}
	}

	return false;
}

void CardManager::Disk2UpdateDriveState(UINT cycles)
{
	if (g_Slot[5] == CT_Disk2)
		sg_pDisk2CardSlot5->UpdateDriveState(cycles);
	if (g_Slot[6] == CT_Disk2)
		sg_Disk2Card.UpdateDriveState(cycles);
}

void CardManager::Disk2Reset(bool powerCycle /*=false*/)
{
	if (g_Slot[5] == CT_Disk2)
		sg_pDisk2CardSlot5->Reset(powerCycle);
	if (g_Slot[6] == CT_Disk2)
		sg_Disk2Card.Reset(powerCycle);
}

void CardManager::Disk2SetEnhanceDisk(bool enhanceDisk)
{
	if (g_Slot[5] == CT_Disk2)
		sg_pDisk2CardSlot5->SetEnhanceDisk(enhanceDisk);
	if (g_Slot[6] == CT_Disk2)
		sg_Disk2Card.SetEnhanceDisk(enhanceDisk);
}
