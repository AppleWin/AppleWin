#include "StdAfx.h"

#include "Interface.h"
#include "Log.h"

static HWND g_hFrameWindow = (HWND)0;
static bool g_bHookAltTab = false;
static bool g_bHookAltGrControl = false;

static LRESULT CALLBACK LowLevelKeyboardProc(
  _In_ int    nCode,
  _In_ WPARAM wParam,
  _In_ LPARAM lParam)
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
		if (g_bHookAltGrControl && pKbdLlHookStruct->vkCode == VK_LCONTROL && pKbdLlHookStruct->scanCode >= 0x200)	// GH#558
		{
			suppress = true;
		}

		// Suppress alt-tab
		if (g_bHookAltTab && pKbdLlHookStruct->vkCode == VK_TAB && (pKbdLlHookStruct->flags & LLKHF_ALTDOWN))
		{
			PostMessage(g_hFrameWindow, newMsg, VK_TAB, newlParam);
			suppress = true;
		}

		// Suppress alt-escape
		if (pKbdLlHookStruct->vkCode == VK_ESCAPE && (pKbdLlHookStruct->flags & LLKHF_ALTDOWN))
		{
			PostMessage(g_hFrameWindow, newMsg, VK_ESCAPE, newlParam);
			suppress = true;
		}

		// Suppress alt-space
		if (pKbdLlHookStruct->vkCode == VK_SPACE && (pKbdLlHookStruct->flags & LLKHF_ALTDOWN))
		{
			PostMessage(g_hFrameWindow, newMsg, VK_SPACE, newlParam);
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

static void RegisterHWND(HWND hWnd, bool bHookAltTab, bool bHookAltGrControl)
{
	g_hFrameWindow = hWnd;
	g_bHookAltTab = bHookAltTab;
	g_bHookAltGrControl = bHookAltGrControl;
}

//---------------------------------------------------------------------------

static HHOOK g_hhook = 0;
static HANDLE g_hHookThread = NULL;
static DWORD g_HookThreadId = 0;

// The hook filter code can be static (within the application) rather than in a DLL.
// Pre: g_hFrameWindow must be valid
static bool HookFilterForKeyboard(void)
{
	_ASSERT(GetFrame().g_hFrameWindow);

	RegisterHWND(GetFrame().g_hFrameWindow, g_bHookAltTab, g_bHookAltGrControl);

	// Since no DLL gets injected anyway for low-level hooks, we can use, for example, GetModuleHandle("kernel32.dll")
	HINSTANCE hinstDLL = GetModuleHandle("kernel32.dll");

	g_hhook = SetWindowsHookEx(
		WH_KEYBOARD_LL,
		LowLevelKeyboardProc,
		hinstDLL,
		0);

	if (g_hhook != 0 && GetFrame().g_hFrameWindow != 0)
		return true;

	std::string msg("Failed to install hook filter for system keys");

	DWORD dwErr = GetLastError();
	GetFrame().FrameMessageBox(msg.c_str(), "Warning", MB_ICONASTERISK | MB_OK);

	msg += "\n";
	LogFileOutput(msg.c_str());
	return false;
}

static void UnhookFilterForKeyboard(void)
{
	UnhookWindowsHookEx(g_hhook);
}

static DWORD WINAPI HookThread(LPVOID lpParameter)
{
	if (!HookFilterForKeyboard())
		return -1;

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		if (msg.message == WM_QUIT)
			break;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UnhookFilterForKeyboard();
	return 0;
}

bool InitHookThread(void)
{
	g_hHookThread = CreateThread(NULL,			// lpThreadAttributes
		0,				// dwStackSize
		(LPTHREAD_START_ROUTINE)HookThread,
		0,				// lpParameter
		0,				// dwCreationFlags : 0 = Run immediately
		&g_HookThreadId);	// lpThreadId
	if (g_hHookThread == NULL)
		return false;

	return true;
}

void UninitHookThread(void)
{
	if (g_hHookThread)
	{
		if (!PostThreadMessage(g_HookThreadId, WM_QUIT, 0, 0))
		{
			_ASSERT(0);
			return;
		}

		do
		{
			DWORD dwExitCode;
			if (GetExitCodeThread(g_hHookThread, &dwExitCode))
			{
				if (dwExitCode == STILL_ACTIVE)
					Sleep(10);
				else
					break;
			}
		} 		while (1);

		CloseHandle(g_hHookThread);
		g_hHookThread = NULL;
		g_HookThreadId = 0;
	}
}
