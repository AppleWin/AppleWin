#pragma once

class HookFilter
{
public:
	HookFilter()
	{
		m_hFrameWindow = (HWND)0;
		m_bHookAltTab = false;
		m_bHookAltGrControl = false;

		m_hhook = 0;
		m_hHookThread = NULL;
		m_HookThreadId = 0;
	}
	~HookFilter()
	{
	}

	bool InitHookThread();
	void UninitHookThread();

private:
	static LRESULT CALLBACK LowLevelKeyboardProc(_In_ int nCode, _In_ WPARAM wParam, _In_ LPARAM lParam);
	bool HookFilterForKeyboard();
	void UnhookFilterForKeyboard();
	static DWORD WINAPI HookThread(LPVOID lpParameter);

	HWND m_hFrameWindow;
	bool m_bHookAltTab;
	bool m_bHookAltGrControl;

	HHOOK m_hhook;
	HANDLE m_hHookThread;
	DWORD m_HookThreadId;
};

HookFilter& GetHookFilter();
