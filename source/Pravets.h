#pragma once

class Pravets
{
public:
	Pravets(void);
	~Pravets(void)
	{
		delete [] m_Kir8ACapital;
		delete [] m_Kir8ALowerCase;
		delete [] m_Kir82;
	}

	void Reset(void);

	void ToggleP8ACapsLock(void) { P8CAPS_ON = !P8CAPS_ON; }

	BYTE SetCapsLockAllowed(BYTE value);
	BYTE GetKeycode(BYTE floatingBus);

	BYTE ConvertToKeycode(WPARAM key, BYTE keycode);
	BYTE ConvertToPrinterChar(BYTE value);

private:
	char* ConvertUtf8ToAnsi(const char* pUTF8);

	bool g_CapsLockAllowed;
	bool P8CAPS_ON;
	bool P8Shift;

	static const char m_Lat8A[];
	static const char m_Lat82[];

	char* m_Kir8ACapital;
	char* m_Kir8ALowerCase;
	char* m_Kir82;
};
