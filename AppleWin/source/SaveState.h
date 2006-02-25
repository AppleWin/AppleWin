#pragma once

extern bool       g_bSaveStateOnExit;

char*   Snapshot_GetFilename();
void    Snapshot_SetFilename(char* pszFilename);
void    Snapshot_LoadState();
void    Snapshot_SaveState();
void    Snapshot_Startup();
void    Snapshot_Shutdown();
