#pragma once

extern bool       g_bSaveStateOnExit;

void    Snapshot_SetFilename(std::string strPathname);
const char* Snapshot_GetFilename();
const char* Snapshot_GetPath();
void    Snapshot_LoadState();
void    Snapshot_SaveState();
void    Snapshot_Startup();
void    Snapshot_Shutdown();
