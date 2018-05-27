#include <windows.h>

// https://stackoverflow.com/questions/2490577/suppress-task-switch-keys-winkey-alt-tab-alt-esc-ctrl-esc-using-low-level-k

// NB. __stdcall (or WINAPI) and extern "C":
// . symbol is decorated as _<symbol>@bytes
// . so use the #pragma to create an undecorated alias for our symbol
extern "C" __declspec(dllexport) LRESULT CALLBACK LowLevelKeyboardProc(
  _In_ int    nCode,
  _In_ WPARAM wParam,
  _In_ LPARAM lParam
)
{
	#pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)

	if (nCode >= 0)
	{
		bool suppress = false;

		PKBDLLHOOKSTRUCT pKbdLlHookStruct = (PKBDLLHOOKSTRUCT) lParam;

		// Suppress alt-tab.
		if (pKbdLlHookStruct->vkCode == VK_TAB && (pKbdLlHookStruct->flags & LLKHF_ALTDOWN)) 
			suppress = true;

		// Suppress alt-escape.
		if (pKbdLlHookStruct->vkCode == VK_ESCAPE && (pKbdLlHookStruct->flags & LLKHF_ALTDOWN)) 
			suppress = true;

		// Suppress ctrl-escape.
		bool ControlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
		if (pKbdLlHookStruct->vkCode == VK_ESCAPE && ControlDown)
			suppress = true;

		// Suppress keys by returning 1.
		if (suppress)
			return 1;
	}

	return CallNextHookEx(0/*parameter is ignored*/, nCode, wParam, lParam);
}
