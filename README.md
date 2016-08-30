AppleWin (With 4 disk drives) by Tom (Tommy) Ewers
==================================================

Note to AppleWin maintainers
----------------------------

I'm new at this GIT and GITHUB stuff so please be patient with me.

This "pull" request is not really a true pull request.  This code is nowhere near
correct or complete enough to integrate into the AppleWin master as it is.  I've
created this pull request only to make this code visible to others who might be
interested in an AppleWin with more than 2 disk drives.  I don't intend to improve
on it, but I feel it might be worth sharing.										

Is there a better way of doing this?  Is it better to have a "branch" from AppleWin
that we know will never be pulled into master?  Is it worthwhile (or even desirable)
for AppleWin users to see and download the executable created as a result of this
code? and how best to do that?  In other words, if a user for AppleWin desires an
emulator with 4 disk drives (say they are using ApplePascal), how would they even
know that I've already created an "alpha" version?										

I originally wrote most of this code in Jan/Feb 2012 after Tom Charlesworth sent me
1.20.1.0 (from Berlios) in a zipped file via e-mail.  I used my 4 disk Applewin to
de-compile Wizardry III and then Wizardry I.  I've since moved on to a new
computer (2001 to 2015), new OS (XP to Windows10), new C++ environment
(VCExpress2008 to Visual Studio 2015 Community), new repository (BerliOS to GITHUB),
and obviously there were changes made to AppleWin between 1.20.1.0 and 1.25.0.1.
Plus I'm getting older and dumber.										

Tommy


High Level Design
-----------------

This code changes AppleWin 1.25.0.4+ to support 4 disk drives (instead of 2).
										
The README.md for master says 1.25.0.3 is the latest release, but….
At Github.com for AppleWin/AppleWin, clicking on "releases" says 1.25.0.4 is
the latest release (4/23/2015), newer, yet already over a year old.
Building AppleWin/AppleWin (master, 8/20/2016) says 1.25.0.4.

To not get confused, I've created this AppleWin (4 disks) as version 1.25.0.41.

From Github.com, AppleWin/AppleWin I cloned the code from 8/20/2016(?) and made
changes.  This code "hijacks" slot 5 and introduces Disk3 and Disk4.										

Visually on the right hand side of the display (the Buttons), instead of seeing
2 buttons for Disk1 and Disk2, there are now 4 half height buttons labelled
Disk1, Disk2, Disk3, and Disk4.  These correspond as follows:										
					
  Disk1  S6,D1  $C600
  Disk2  S6,D2
  Disk3  S5,D1  $C500
  Disk4  S5,D2

In addition to displaying the disk name, each of the 4 drives also has its own
"disk light" button showing the status of inactive, reading, writing, etc.										
										
Each of the 4 buttons acts like the original 2 buttons.  You can left-click,
right-click, hover over, and drop a file.										

  Left-click:     Select Disk Image dialog to load a disk image.
  Right-click:    Pop-up message to Eject, Change to Read/Write, Change to
                  Write only, Send to CiderPress.
  Hover over:     Pop-up displaying "filename.dsk" in the drive.
  Drop file:      Using your mouse in Windows, you can drag a "filename.dsk" and
                  drop it on the button to load that disk.

Clicking the "swap" button still swaps Disk1 and Disk2.

Clicking the Configuration button and the "Disk tab" displays a dialog for 4 disks
with 4 comboboxes (my original code was written when these were listboxes, so many
changes were made here from my work 4 years ago).											

When you close AppleWin, it remembers the 4 disk images in the 4 drives so that
when you re-start AppleWin those 4 disks are re-loaded.										

In addtion to using "-d1" and "-d2" to load disk1 and disk2, you can now use
"-d3" and "-d4" to load disk3 and disk4 from the AppleWin.exe command line.

Function keys F1 to F8 refer to 8 buttons.  Since I have inserted
2 buttons in the middle of the list, and there are no more function keys to use,
I've kept the original mappings the same and Disk3 and Disk4 do NOT have any
function keys mapped to them.

The changes I've made are not complete.  If you try using the following features,
undesirable results are likely to occur: 
										
  1. Microsoft CP /M SoftCard (definitely will conflict if you use slot 5)
  2. Mockingboards (in slots 4 & 5)
  3. SAM/DAC (in slot 5)
  4. Save State / Load State

No attempt has been made to prevent you from trying to use those features, but if
you do, you will be shooting yourself in the foot.										
										
It might be desirable to add "-m -no-mb" to the applewin.exe command line for this
AppleWin with 4 disks.  I think in 2012 I thought it might be necessary, but now it
is probably not required.

There are a few known deficiencies in the code.  Sometimes the disk drive status
lights "disappear", but if you minimize and restore the AppleWin window, they re-appear.

Hmmmm.  I had a problem with the disk drive lights, so I put in a lot of FrameDrawDiskLEDS()
without paying much attention to FullScreen mode (because I rarely used it in 2012).  
I think the disappearing lights is fixed, but there are now possible display problems
when in full screen mode.										

The code does not completely mimick how multiple disk controllers work.  On a real
AppleII you can have drives spinning from multiple disk controllers.  In this AppleWin,
only 1 drive will ever be spinning at a time.										
										
There are 2 disk status lights below the buttons that are redundant with the lights
on the Disk1 and Disk2 buttons.
										
The T/S statuses below the buttons only display for Disk1 and Disk2.  An enhancement would
be to display for Disk3 and Disk4.										
										
Another possible enhancement is to indicate on the 4 disk drive buttons their status of
"Write-Protected" (i.e. Read only).										
										
During my testing, I noticed another possible general enhancement to the Applewin Disk									
light.  When a disk is read-only, and you access it, the color "orange" is very close
to the color "red" for the little round button display.  In this 4 disk Applewin.exe, I've changed 
DISKPROT.BMP to "yellow".


Four Disk Drives (low level design)
-----------------------------------

Files added/changed:

	/resource
		Applewin.rc
		Copy of DRIVE1.BMP
		Copy of DRIVE2.BMP
		Copy of DISKPROT.BMP
		DISKPROT.BMP
		DRIVE1.BMP
		DRIVE2.BMP
		DRIVE3.BMP
		DRIVE4.BMP
		resource.h

	/source/Configuration
		PageDisk.cpp
		PropertySheetHelper.cpp
		PropertySheetHelper.h
		
	/source
		Applewin.cpp
		Applewin.h
		Common.h
		Disk.cpp
		Disk.h
		Frame.cpp
		Memory.cpp
		Mockingboard.cpp
		SAM.cpp
		SaveState.cpp


/resource
		
	Saved original DRIVE1.BMP and DRIVE2.BMP as Copy of DRIVE1.BMP and Copy of DRIVE2.BMP.	
	Created DRIVE1.BMP, DRIVE2.BMP, DRIVE3.BMP, and DRIVE4.BMP -- 	
	  these are the "half height" drive pictures.
	Saved original DISKPROT.BMP to "Copy of DISKPROT.BMP" and changed DISKPROT.BMP.	

	Applewin.rc	
		Added BITMAPs for  DRIVE3_BUTTON and DRIVE4_BUTTON.
		Added IDC_COMBO_DISK3 and IDC_COMBO_DISK4 to "Disk" dialog box.
		Changed FILEVERSION and PRODUCTVERSION to 1.25.0.41 (from 1.25.0.4),
		and FileDescription to include "(4 disk)".

		
	resource.h	
		Removed already obsolete definitions:  IDC_DISK1, IDC_DISK2, IDC_EDIT_DISK1, IDC_EDIT_DISK2.
		Inserted IDC_COMBO_DISK3 and IDC_COMBO_DISK4.
		Updated _APS_NEXT_CONTROL_VALUE to account for inserted definitions.

/source/Configuration				
	Config.h			
		Note:  No changes, but "m_Slot[5] and "g_Slot5" should be examined in more detail (?)		
		See also PageSound.cpp, PropertySheetHelper.cpp.		
				
	PageDisk.cpp			
		WM_COMMAND:		
			IDC_COMBO_DISK3 and IDC_COMBO_DISK4 handling added	
				They call HandleDiskCombo().

		WM_INITDIALOG:		
			m_PropertySheetHelper.	
				FillComboBox( … IDC_COMBO_DISK3 …)
				FillComboBox( … IDC_COMBO_DISK4 …)
				
			Insert disk names for DRIVE_3 and DRIVE_4 if they are not blank.	
				SendDlgItemMessage( …IDC_COMBO_DISK3 )
				SendDlgItemMessage( …IDC_COMBO_DISK4 )
				
		HandleDiskCombo()		
			After a disk name is chosen, if it was in one of the other 3, remove it.	
				SendDlgItemMessage( … CB_DELETESTRING…)
			If "Eject" was chosen, driveSelected can now be one of 4:	
				IDC_COMBO_DISK3 and IDC_COMBO_DISK4 possible.
				
		RemovalConfirmation()		
			uCommand can now be IDC_COMBO_DISK3 and IDC_COMBO_DISK4 so check for them.	
	
/source												
	Applewin.cpp											
		Did not change or insert code for restricting Mockingboard, SAM/DAC, or Z80.
		If the user changes to using these, bad things will happen. (see  g_Slot5)  										
												
			Some configurations use Mockingboard, SAM/DAC, or Z80 in slot 5.									
			See Configuration tab "Sound".									
			See Configuration tab, Input and "Microsoft CP/M SoftCard:
			"Unplugged" or "slot4" "slot5"									
				IDC_CPM_CONFIG								
				SS_CARDTYPE								
												
		Inserted in LoadConfiguration (which loads from the registry)										
			Disk_LoadLastDiskImage( DRIVE_3)									
			Disk_LoadLastDiskImage( DRIVE_4)									
												
		WinMain()										
			LPSTR szImageName_drive3 = NULL									
			LPSTR szImageName_drive4 = NULL									
												
		Introduced 2 new Command Line arguments for Disk3 and Disk4:										
			"-d3"									
			"-d4"									
			...and retrieved the disk images to szImageName_drive3 and szImageName_drive4.									
												
		Before entering the "do while(RESTART)" loop:										
			DoDiskInsert(DRIVE_3, szImageName_drive3)									
			DoDiskInsert(DRIVE_4, szImageName_drive4)									
			These initialize the drives, for example, see g_aFloppyDisk[].									
												
	Applewin.h											
		Comment made for g_Slot5.  Need more changes to do this right.										
												
	Common.h											
		Inserted:										
			BTN_DRIVE3									
			BTN_DRIVE4									
		Bumped the numbers for BTN_DRIVESWAP, BTN_FULLSCR, etc.										
												
		Defined 2 more Registry entries under "Preferences":										
			REGVALUE_PREF_LAST_DISK_3  "Last Disk Image 3"									
			REGVALUE_PREF_LAST_DISK_4  "Last Disk Image 4"									
												
	Disk.cpp											
		Disk_LoadLastDiskImage()										
			Allow *pRegKey to be set based on REGVALUE_PREF_LAST_DISK_3 and
			REGVALUE_PREF_LAST_DISK_4.									
			This eventually calls DiskInsert() with the image.									
												
		Disk_SaveLastDiskImage()										
			Call RegSaveString() for REGVALUE_PREF_LAST_DISK_3 or REGVALUE_PREF_LAST_DISK_4.									
												
		DiskControlStepper()										
			This code was changed on 7/3/2015 and I'm not sure if my change from my earlier
			work is needed someplace else.   I had the following in 2012:									
												
				return (address == 0xE0) || (address == 0xD0) ? 0xFF : MemReturnRandomData(1);
				// was hard-coded for slot 6 (PHASEOFF)								
												
		DiskDestroy()										
			RemoveDisk(DRIVE_3);									
			RemoveDisk(DRIVE_4);									
												
		DiskEnable()										
			Stop the other THREE disks that might have been spinning.									
			Note, this is not the way the real hardware works.									
												
		DiskGetLightStatus()										
			Changed call to allow returning status for Light 2 and 3 (Drives in slot 5).									
			Changed code to set *pDisk3Status and *pDisk4Status.									
												
		DiskInsert()										
			If disk was in another drive, call DiskEject().									
			Need to check 3 other drives.									
												
		DiskReadWrite()										
			Originally (2012) I encountered an existing latent bug in AppleWin.  The bug
			in general would not cause problems but the error could be demonstrated.  It
			had something to do with the emulator not really stopping the drive from
			"spinning".  The ++fptr was ALWAYS increasing, even after the drive should
		    have stopped.  With 4 drives, other software (Pascal) would not boot because
			of this!!									
																					
			Insert a test for "(floppymotoron or fptr->spinning)" BEFORE getting the next byte.									
												
		DiskSelectImage()										
			The title for "Select Disk Image" can now have "1", "2", "3", or "4".									
												
		DiskSaveSnapshot()										
			This is "Snapshot" code.  Probably a lot more work needed to get this right.									
				No attempt to try saving DRIVE_3 and DRIVE_4.								
												
		DiskLoadSnapshotDriveUnit()										
			This is "Snapshot" code.  Probably a lot more work needed to get this right.									
				No attempt to allow disk2UnitName to be set for DRIVE_3 and 4.								
												
		DiskLoadSnapshot										
			This is "Snapshot" code.  Probably a lot more work needed to get this right.									
				No attempt to call DiskLoadSnapshotDriveUnit for DRIVE_3 and DRIVE_4.								
												
	Disk.h											
		Add to enum Drive_e										
			DRIVE_3,									
			DRIVE_4,									
				which bumps NUM_DRIVES to 4.								
												
		DiskGetLightStatus() declaration changed to include:										
			*pDisk3Status, *pDisk4Status									
												
	Frame.c											
		There are now 10 buttons on the right (6 are full height, and 4 are 1/2 height), 
		which presents some interesting coding problems.										
																						
		Some calculations were previously made with the idea that all buttons were exactly
		the same height, which isn't true any more.  Also the height in pixels for a
		button is an odd number, so splitting one into 2 equal parts is also challenging.										
																						
		#define BUTTONS 10										
												
		Define g_eStatusDrive3 to hold the disk light status.										
		Define g_eStatusDrive4 to hold the disk light status.										
												
		CreateGdiObjects()										
			Load the bitmaps for DRIVE3_BUTTON and DRIVE4_BUTTON.									
												
		DrawButton()										
			A lot of special calculations needed to determine the top and height for
			buttons, and for drawing the "shadow" when the button is "down".									
												
			Buttons 2, 3, 4, and 5 are the half height buttons (Button 0 (help) and
			Button1 (run) are displayed above these).  Buttons 3 and 5 are one pixel
			taller than 2 and 4.									
																					
			I also changed the alignment of text on the button to "TA_LEFT"
			instead of "TA_CENTER" and squished it a little more left.									
												
		DrawFrameWindow()										
			When drawing the Status Area, account for 4 buttons being 1/2 height.									
												
		FrameDrawDiskLEDS()										
			Define eDrive3Status and eDrive4Status.									
			Adjust DiskGetLightStatus to return all 4 drive statuses.									
			Change calculation for "y" to account for 4 half height buttons.									
			Draw disk lights on all 4 half height buttons.									
												
		FrameDrawDiskStatus()										
			New calculation for "y" to account for 4 half height buttons.									
			"y" is used to position the "1" and "2", "Tx", "Sy", etc.									
												
		DrawStatusArea()										
			New calculation for "y" to account for 4 half height buttons.									
			Draw all 4 buttons (add BTN_DRIVE3 and BTN_DRIVE4), and call
			FrameDrawDiskLEDS() to update 4 lights.									
												
		FrameWndProc()										
			WM_DROPFILES:									
				Calculate 4 separate RECTs.  One for each of the 4 half height buttons.								
				Set iDrive to DRIVE_1, DRIVE_2, DRIVE_3, or DRIVE_4 if the point
				of dropping is on the button.If it wasn't on ANY of the buttons, the
				default is to drop it into DRIVE_1 and boot it.								
				A call to DrawButton() can now be any of the 4 drives.								
				FrameDrawDiskLEDS() to redraw the lights.								
												
				Note:  It is fun to try to drop a file from an Explorer window when in
				FullScreen mode.								
												
				Interesting note about dropping *.dsk images onto the buttons:								
					A filename with ALLUPPERCASE.DSK (including the extension) is
					displayed differently from UPPERCASEFILE.dsk.							
												
												
			WM_KEYDOWN:									
				If a function key was pressed (VK_F1  to VK_F8) adjust buttondown to
				skip over Drive3 and Drive4.
												
				0	HELP		VK_F1						
				1	RUN 		VK_F2						
				2	Disk1		VK_F3						
				3	Disk2		VK_F4						
				4	Disk3							
				5	Disk4							
				6	Swap		VK_F5						
				7	FullScreen	VK_F6						
				8	Debug		VK_F7						
				9	Config		VK_F8						
												
				FrameDrawDiskLEDS() called after DrawButton() to repaint the 4 disk lights.								
												
			WM_KEYUP:									
				If a function key was pressed (VK_F1 to VK_F8), the calculation needs to be
				adjusted to skip Disk3 and Disk4, similar to WM_KEYDOWN processing.								
												
												
			WM_LBUTTONDOWN:									
				Calculations for y need to be adjusted for the 2 half height buttons.								
				The first "if" statement with x and y determines if the click was over one
				of the 10 buttons.  If a button was clicked, then buttonactive and buttondown
				are calculated taking into account the half height buttons.								
												
												
			WM_LBUTTONUP:									
				Add a FrameDrawDiskLEDS() to keep the lights on.								
												
			WM_MOUSEMOVE:									
				Calculate newover taking into account the half height buttons.								
				Similar code to WM_LBUTTONDOWN.								
				Call FrameDrawDiskLEDS() after a down-click on a disk button and with
				the mouse still down followed by a mouse move off the button.								
												
			WM_RBUTTONDOWN:									
			WM_RBUTTONUP:									
				Calculate iButton taking into account the half height buttons, and then
				calculate iDrive to determine if one of the 4 disk buttons was clicked.								
												
				Test for all 4 buttons (including BTN_DRIVE3 and BTN_DRIVE4).  If a drive
				button was involved, then call FrameDrawDiskLEDS() after the DrawButton().								
												
		ProcessButtonClick()										
			BTN_DRIVE1:									
			BTN_DRIVE2:									
			BTN_DRIVE3:		<--- inserted							
			BTN_DRIVE4:		<--- inserted							
				Processing for all 4 buttons is the same.								
				Insert a FrameDrawDiskLEDS() after the DrawButton()								
												
		SetupTooltipControls()										
			There are 4 separate "tooltips"; one for each disk drive button (toolinfo.uId = 
			0, 1, 2, 3).  The top and bottom of each rect is determined taking into account
			the half height sizes.									
			Added SendMessage() for the 2 new buttons (Disk3 and Disk4).									
												
	Memory.cpp											
		MemInitializeIO()										
			Load up slot 5 ($C500) with the following:									
				DiskLoadRom(pCxRomPeripheral, 5);								
			This code overwrites anything that was previously done with g_Slot5.  
			for example: Mockingboard, Z80, SAM, etc.									
			Obviously this code would need work to be properly integrated into master.									
												
	Mockingboard.cpp											
		No changes, but note made in MB_InitializeIO() regarding g_Slot5 == CT_MockingboardC.										
												
	SAM.cpp											
		No changes, but note made in CongifurSAM() regarding RegisterIoHandler().										
												
	SaveState.cpp											
		No changes, but note made in Snapshot_LoadState_v1() regarding Slot5:
		and DiskSetSnapshot_v1().										
		I imagine there need to be some sanity checks when mixing and matching save
		state files where one has 2 disks and the other has 4 disks.										
												

Here ends the reading.
Tommy
