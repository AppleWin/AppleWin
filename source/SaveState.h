#pragma once

const bool kSaveStateOnExit_Default = false;

void Snapshot_SetFilename(const std::string& filename, const std::string& path="");
const std::string& Snapshot_GetFilename(void);
const std::string& Snapshot_GetPath(void);
const std::string& Snapshot_GetPathname(void);
void Snapshot_GetDefaultFilenameAndPath(std::string& defaultFilename, std::string& defaultPath);
void Snapshot_UpdatePath(void);
void Snapshot_LoadState();
void Snapshot_SaveState();
void Snapshot_Startup();
void Snapshot_Shutdown();

bool GetSaveStateOnExit();
void SetSaveStateOnExit(bool saveStateOnExit);

bool Snapshot_GetIgnoreHdcFirmware();
void Snapshot_SetIgnoreHdcFirmware(const bool ignoreHdcFirmware);
