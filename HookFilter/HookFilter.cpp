#include <windows.h>

static HWND g_hFrameWindow = (HWND)0;

// NB. __stdcall (or WINAPI) and extern "C":
// . symbol is decorated as _<symbol>@bytes
// . so use the #pragma to create an undecorated alias for our symbol
extern "C" __declspec(dllexport) LRESULT CALLBACK LowLevelKeyboardProc(
  _In_ int    nCode,
  _In_ WPARAM wParam,
  _In_ LPARAM lParam)
{
	#pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)

	if (nCode == HC_ACTION)
	{
		bool suppress = false;

		PKBDLLHOOKSTRUCT pKbdLlHookStruct = (PKBDLLHOOKSTRUCT) lParam;
		UINT newMsg = pKbdLlHookStruct->flags & LLKHF_UP ? WM_KEYUP : WM_KEYDOWN;
		LPARAM newlParam = newMsg == WM_KEYUP ? 3<<30 : 0;	// b31:transition state, b30:previous key state

		// Note about PostMessage() and use of VkKeyScan():
		// . Convert the ascii code to virtual key code, so that the message pump can do TranslateMessage()
		// . NB. From MSDN for "WM_KEYDOWN" && "WM_KEYUP" : "Applications must pass wParam to TranslateMessage without altering it at all."

		// Suppress alt-tab
		if (pKbdLlHookStruct->vkCode == VK_TAB && (pKbdLlHookStruct->flags & LLKHF_ALTDOWN))
		{
			PostMessage(g_hFrameWindow, newMsg, LOBYTE(VkKeyScan(0x09)), newlParam);
			suppress = true;
		}

		// Suppress alt-escape
		if (pKbdLlHookStruct->vkCode == VK_ESCAPE && (pKbdLlHookStruct->flags & LLKHF_ALTDOWN))
		{
			PostMessage(g_hFrameWindow, newMsg, LOBYTE(VkKeyScan(0x1B)), newlParam);
			suppress = true;
		}

		// Suppress alt-space
		if (pKbdLlHookStruct->vkCode == VK_SPACE && (pKbdLlHookStruct->flags & LLKHF_ALTDOWN))
		{
			PostMessage(g_hFrameWindow, newMsg, LOBYTE(VkKeyScan(0x20)), newlParam);
			suppress = true;
		}

		// Suppress ctrl-escape
		bool ControlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
		if (pKbdLlHookStruct->vkCode == VK_ESCAPE && ControlDown)
			suppress = true;

		// Suppress keys by returning 1
		if (suppress)
			return 1;
	}

	return CallNextHookEx(0/*parameter is ignored*/, nCode, wParam, lParam);
}

extern "C" __declspec(dllexport) void __cdecl RegisterHWND(HWND hWnd)
{
	g_hFrameWindow = hWnd;
}
