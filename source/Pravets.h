#pragma once

//Pravets 8A/C only variables
//extern bool P8CAPS_ON;
//extern bool	P8Shift; 
//void PravetsReset(void);

class Pravets
{
public:
	Pravets(void);
	~Pravets(void){}

	void Reset(void);

	bool GetCapsOn(void) { return P8CAPS_ON; }
	void SetCapsOn(bool state) { P8CAPS_ON = state; }
	bool GetShift(void) { return P8Shift; }
	void SetShift(bool state) { P8Shift = state; }

	BYTE SetCapsLockAllowed(BYTE value);
	bool GetCapsLockAllowed(void) { return g_CapsLockAllowed; }
	BYTE GetKeycode(BYTE floatingBus);

	BYTE ConvertKeyToKeycode(WPARAM key, BYTE keycode);

private:
	bool g_CapsLockAllowed;
	bool P8CAPS_ON;
	bool P8Shift;
};
