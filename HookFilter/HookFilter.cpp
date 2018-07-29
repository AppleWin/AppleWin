#include <windows.h>

static HWND g_hFrameWindow = (HWND)0;
static bool g_bHookAltTab = false;
static bool g_bHookAltGrControl = false;

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

extern "C" __declspec(dllexport) void __cdecl RegisterHWND(HWND hWnd, bool bHookAltTab, bool bHookAltGrControl)
{
	g_hFrameWindow = hWnd;
	g_bHookAltTab = bHookAltTab;
	g_bHookAltGrControl = bHookAltGrControl;
}
