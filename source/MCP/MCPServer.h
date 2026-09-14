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

/* Description: The MCP server's interface for a tool
 *
 * Author: Copyright (C) 2026 Robert Baruch
 *
 * A tool runs on a socket thread. The 6502 emulation runs on a different thread.
 * Therefore a tool must send all work that touches the machine to the emulator
 * thread. The RunOnEmulatorThread function does this. A tool that waits for a
 * condition in the machine must wait on the socket thread. A delay there has no
 * effect on the emulation.
 */

#include "MCP/MCPJson.h"

#include <functional>
#include <string>

namespace MCP
{
	// How long a tool waits for the emulator thread to do the work it sent.
	const unsigned int kEmulatorCallTimeoutMs = 15000;

	// This function sends work to the emulator thread and waits for the result.
	// The emulator thread does the work between two calls to ContinueExecution,
	// when no 6502 instruction is in progress.
	// The function returns false if the emulator thread does not do the work
	// before the timeout, or if the server stops. In these two conditions, the
	// work does not run at all.
	bool RunOnEmulatorThread(const std::function<void()>& work, unsigned int timeoutMs = kEmulatorCallTimeoutMs);

	// This function returns true after MCP_Destroy starts. A tool that polls in a
	// loop must then stop and give an error.
	bool IsStopping();

	class ToolResponse
	{
	public:
		ToolResponse() : m_content(Json::Array()), m_isError(false) {}

		void Text(const std::string& text);
		void Image(const std::string& base64Data, const char* mimeType);

		// This function makes the full call fail. The model reads the message.
		// Therefore the message must tell the model what to do differently.
		void Error(const std::string& message);

		// This function reports that the emulator thread did not do the work.
		void Unresponsive();

		const Json& GetContent() const { return m_content; }
		bool IsError() const { return m_isError; }

	private:
		Json m_content;
		bool m_isError;
	};

	typedef void (*ToolHandler)(const Json& arguments, ToolResponse& response);

	struct ToolDefinition
	{
		const char* name;
		const char* title;
		const char* description;

		// The JSON Schema for the arguments, written as JSON text. The server
		// reads this text one time, at the first tools/list request.
		std::string inputSchema;

		ToolHandler handler;
	};
}
