#pragma once

class Pravets
{
public:
	Pravets(void);
	~Pravets(void){}

	void Reset(void);

	void ToggleP8ACapsLock(void) { P8CAPS_ON = !P8CAPS_ON; }

	BYTE SetCapsLockAllowed(BYTE value);
	BYTE GetKeycode(BYTE floatingBus);

	BYTE ConvertToKeycode(WPARAM key, BYTE keycode);
	BYTE ConvertToPrinterChar(BYTE value);

private:
	bool g_CapsLockAllowed;
	bool P8CAPS_ON;
	bool P8Shift;
};
