#pragma once

#include <string>
#include <linux/windows/wincompat.h>

bool DoDiskInsert(const UINT slot, const int nDrive, const std::string & fileName, const bool createMissingDisk);

void setSnapshotFilename(const std::string & filename);
