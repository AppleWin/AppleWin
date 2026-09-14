#pragma once

/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2019, Tom Charlesworth, Michael Pohoreski, Nick Westgate

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: The MCP server's interface to AppleWin
 *
 * Author: Copyright (C) 2026 Robert Baruch
 *
 * All the other code for the Model Context Protocol server is in source/MCP. The
 * emulator uses only the three functions below. To remove the server, delete the
 * directory and the three calls to these functions.
 */

// This function reads the -mcp and the -mcp=<port> arguments. It returns true if
// the argument belongs to this module. The command line parser calls it.
bool MCP_ParseCmdLineArg(const char* arg);

// This function starts the server. Call it from the emulator thread after the
// machine is ready. It does nothing if the command line has no -mcp argument.
// It also does nothing on a second call.
void MCP_Initialize();

// This function stops the server and releases the port. Call it from the
// emulator thread.
void MCP_Destroy();
