/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2021, Tom Charlesworth, Michael Pohoreski

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

/* Description: HookFilter
 *
 * Author: Various
 */

#include "StdAfx.h"

#include "HookFilter.h"

#include "AppleWin.h"
#include "Interface.h"
#include "Log.h"

HookFilter& GetHookFilter(void)
{
	static HookFilter hookFilter;
	return hookFilter;
}

LRESULT CALLBACK HookFilter::LowLevelKeyboardProc(_In_ int nCode, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		bool suppress = false;

		PKBDLLHOOKSTRUCT pKbdLlHookStruct = (PKBDLLHOOKSTRUCT) lParam;
		UINT newMsg = pKbdLlHookStruct->flags & LLKHF_UP ? WM_KEYUP : WM_KEYDOWN;
		LPARAM newlParam = newMsg == WM_KEYUP ? 3<<30 : 0;	// b31:transition state, b30:previous key state

		//

		// NB. Alt Gr (Right-Alt): this normally send 2 WM_KEYDOWN messages for: VK_LCONTROL, then VK_RMENU
		// Keyboard scanCodes: LCONTROL=0x1D, LCONTROL_from_RMENU=0x21D
		// . For: Microsoft PS/2/Win7-64, VAIO laptop/Win7-64, Microsoft USB/Win10-64
		// NB. WM_KEYDOWN also includes a 9/10-bit? scanCode: LCONTROL=0x1D, RCONTROL=0x11D, RMENU=0x1D(not 0x21D)
		// . Can't suppress in app, since scanCode is not >= 0x200
		if (GetHookFilter().m_bHookAltGrControl && pKbdLlHookStruct->vkCode == VK_LCONTROL && pKbdLlHookStruct->scanCode >= 0x200)	// GH#558
		{
			suppress = true;
		}

		// Suppress alt-tab
		if (GetHookFilter().m_bHookAltTab && pKbdLlHookStruct->vkCode == VK_TAB && (pKbdLlHookStruct->flags & LLKHF_ALTDOWN))
		{
			PostMessage(GetHookFilter().m_hFrameWindow, newMsg, VK_TAB, newlParam);
			suppress = true;
		}

		// Suppress alt-escape
		if (pKbdLlHookStruct->vkCode == VK_ESCAPE && (pKbdLlHookStruct->flags & LLKHF_ALTDOWN))
		{
			PostMessage(GetHookFilter().m_hFrameWindow, newMsg, VK_ESCAPE, newlParam);
			suppress = true;
		}

		// Suppress alt-space
		if (pKbdLlHookStruct->vkCode == VK_SPACE && (pKbdLlHookStruct->flags & LLKHF_ALTDOWN))
		{
			PostMessage(GetHookFilter().m_hFrameWindow, newMsg, VK_SPACE, newlParam);
			suppress = true;
		}

		// Suppress ctrl-escape
		if (pKbdLlHookStruct->vkCode == VK_ESCAPE)
		{
			// But don't suppress CTRL+SHIFT+ESC
			if (GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_SHIFT) >= 0)
				suppress = true;
		}

		// Suppress keys by returning 1
		if (suppress)
			return 1;
	}

	return CallNextHookEx(0/*parameter is ignored*/, nCode, wParam, lParam);
}

//---------------------------------------------------------------------------

// The hook filter code can be static (within the application) rather than in a DLL.
// Pre: g_hFrameWindow must be valid
bool HookFilter::HookFilterForKeyboard(void)
{
	_ASSERT(GetFrame().g_hFrameWindow);

	m_hFrameWindow = GetFrame().g_hFrameWindow;
	m_bHookAltTab = GetHookAltTab();
	m_bHookAltGrControl = GetHookAltGrControl();

	// Since no DLL gets injected anyway for low-level hooks, we can use, for example, GetModuleHandle("kernel32.dll")
	HINSTANCE hinstDLL = GetModuleHandle("kernel32.dll");

	m_hhook = SetWindowsHookEx(
		WH_KEYBOARD_LL,
		LowLevelKeyboardProc,
		hinstDLL,
		0);

	if (m_hhook != 0 && GetFrame().g_hFrameWindow != 0)
		return true;

	std::string msg("Failed to install hook filter for system keys");

	DWORD dwErr = GetLastError();
	GetFrame().FrameMessageBox(msg.c_str(), "Warning", MB_ICONASTERISK | MB_OK);

	msg += "\n";
	LogFileOutput(msg.c_str());
	return false;
}

void HookFilter::UnhookFilterForKeyboard(void)
{
	UnhookWindowsHookEx(m_hhook);
}

DWORD WINAPI HookFilter::HookThread(LPVOID lpParameter)
{
	HookFilter* hf = (HookFilter*)lpParameter;

	if (!hf->HookFilterForKeyboard())
		return -1;

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		if (msg.message == WM_QUIT)
			break;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	hf->UnhookFilterForKeyboard();
	return 0;
}

bool HookFilter::InitHookThread(void)
{
	m_hHookThread = CreateThread(NULL,			// lpThreadAttributes
		0,				// dwStackSize
		(LPTHREAD_START_ROUTINE)HookThread,
		this,			// lpParameter
		0,				// dwCreationFlags : 0 = Run immediately
		&m_HookThreadId);	// lpThreadId

	if (m_hHookThread == NULL)
		return false;

	return true;
}

void HookFilter::UninitHookThread(void)
{
	if (m_hHookThread)
	{
		if (!PostThreadMessage(m_HookThreadId, WM_QUIT, 0, 0))
		{
			_ASSERT(0);
			return;
		}

		do
		{
			DWORD dwExitCode;
			if (GetExitCodeThread(m_hHookThread, &dwExitCode))
			{
				if (dwExitCode == STILL_ACTIVE)
					Sleep(10);
				else
					break;
			}
		} 		while (1);

		CloseHandle(m_hHookThread);
		m_hHookThread = NULL;
		m_HookThreadId = 0;
	}
}
