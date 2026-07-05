#pragma once

class Disk2CardManager
{
public:
	Disk2CardManager() : m_stepperDeferred(true) {}
	~Disk2CardManager() {}

	bool IsConditionForFullSpeed();
	void Update(const ULONG nExecutedCycles);
	bool GetEnhanceDisk();
	void SetEnhanceDisk(bool enhanceDisk);
	void LoadLastDiskImage();
	bool IsAnyFirmware13Sector();
	void GetFilenameAndPathForSaveState(std::string& filename, std::string& path);
	void SetStepperDefer(bool defer);
	bool IsStepperDeferred() { return m_stepperDeferred; }

private:
	bool m_stepperDeferred;	// debug: can disable via cmd-line
};
