#pragma once

class CardManager
{
public:
	CardManager() {}
	~CardManager() {}

	bool Disk2IsConditionForFullSpeed(void);
	void Disk2UpdateDriveState(UINT cycles);
	void Disk2Reset(bool powerCycle = false);
	void Disk2SetEnhanceDisk(bool enhanceDisk);
};
