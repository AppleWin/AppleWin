#pragma once

class Disk2CardManager
{
public:
	Disk2CardManager(void) : m_stepperDeferred(true) {}
	~Disk2CardManager(void) {}

	bool IsConditionForFullSpeed(void);
	void Update(const ULONG nExecutedCycles);
	void Reset(const bool powerCycle = false);
	bool GetEnhanceDisk(void);
	void SetEnhanceDisk(bool enhanceDisk);
	void LoadLastDiskImage(void);
	void Destroy(void);
	bool IsAnyFirmware13Sector(void);
	void GetFilenameAndPathForSaveState(std::string& filename, std::string& path);
	void SetStepperDefer(bool defer);
	bool IsStepperDeferred(void) { return m_stepperDeferred; }

private:
	bool m_stepperDeferred;	// debug: can disable via cmd-line
};
