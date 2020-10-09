#pragma once

class Disk2CardManager
{
public:
	Disk2CardManager(void) {}
	~Disk2CardManager(void) {}

	bool IsConditionForFullSpeed(void);
	void UpdateDriveState(UINT cycles);
	void Reset(const bool powerCycle = false);
	bool GetEnhanceDisk(void);
	void SetEnhanceDisk(bool enhanceDisk);
	void LoadLastDiskImage(void);
	void Destroy(void);
	bool IsAnyFirmware13Sector(void);
};
