#pragma once

#define MoveMemory(Destination,Source,Length) memmove((Destination),(Source),(Length))
#define FillMemory(Destination,Length,Fill) memset((Destination),(Fill),(Length))
#define EqualMemory(Destination,Source,Length) (!memcmp((Destination),(Source),(Length)))
#define CopyMemory(Destination,Source,Length) memcpy((Destination),(Source),(Length))
