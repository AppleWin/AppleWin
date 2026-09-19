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

/* Description: The MCP server's lifetime, its bridge to the emulator thread, and its JSON-RPC dispatch
 *
 * Author: Copyright (C) 2026 Robert Baruch
 *
 * The bridge operates as follows. The AppleWin message loop calls PeekMessage
 * with a null window handle. Thus the loop sends messages to all the windows of
 * the emulator thread. The loop does this between two calls to ContinueExecution,
 * when no 6502 instruction is in progress.
 * This file makes a message-only window on the emulator thread. A socket thread
 * posts a message to that window. The emulator thread then does the work at a
 * safe time. This method changes no code in the AppleWin window procedure.
 */

#include "StdAfx.h"

#include "MCP/MCPServer.h"

#include "MCP/MCP.h"
#include "MCP/MCPHelpers.h"
#include "MCP/MCPHttp.h"
#include "MCP/MCPTools.h"

#include "Core.h"
#include "FrameBase.h"
#include "Interface.h"
#include "Log.h"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <mutex>
#include <set>
#include <string>

namespace MCP
{
	namespace
	{
		const char* const kWindowClassName = "AppleWinMCPBridge";
		const UINT WM_MCP_RUN = WM_APP + 1;

		const char* const kServerName = "applewin";
		const char* const kPreferredProtocolVersion = "2025-06-18";
		const char* const kSupportedProtocolVersions[] = { "2025-06-18", "2025-03-26", "2024-11-05" };

		// Error codes reserved by the JSON-RPC 2.0 specification, section 5.1.
		const int kRpcParseError = -32700;
		const int kRpcInvalidRequest = -32600;
		const int kRpcMethodNotFound = -32601;
		const int kRpcInvalidParams = -32602;
		const int kRpcInternalError = -32603;

		// The listening port, unless the command line sets one with -mcp=port.
		const uint16_t kDefaultPort = 6502;
		const char kPortArgPrefix[] = "-mcp=";

		//=======================================================================
		// Options

		bool g_enabled = false;
		uint16_t g_requestedPort = kDefaultPort;

		//=======================================================================
		// The bridge

		struct Call
		{
			explicit Call(const std::function<void()>& fn)
				: work(fn), finished(false), executed(false), refs(2) {}

			std::function<void()> work;
			std::mutex mutex;
			std::condition_variable done;

			bool finished;   // no longer pending, whether or not it ran
			bool executed;   // the work actually ran
			std::atomic<int> refs;
		};

		void ReleaseCall(Call* call)
		{
			if (--call->refs == 0)
				delete call;
		}

		HWND g_bridgeWindow = NULL;
		DWORD g_emulatorThreadId = 0;
		std::atomic<bool> g_stopping(false);

		std::mutex g_pendingMutex;
		std::set<Call*> g_pendingCalls;

		HttpServer g_httpServer;
		bool g_running = false;

		LRESULT CALLBACK BridgeWndProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
		{
			if (message != WM_MCP_RUN)
				return DefWindowProc(window, message, wparam, lparam);

			Call* call = reinterpret_cast<Call*>(lparam);

			{
				// The lock stays closed while the work runs. This makes the
				// cancel operation safe. A caller with a timeout can set the
				// finished flag only when no work runs. Thus a tool never writes
				// into a stack frame that is no longer valid.
				std::lock_guard<std::mutex> lock(call->mutex);

				if (!call->finished)
				{
					call->work();
					call->executed = true;
					call->finished = true;
				}
			}

			call->done.notify_all();
			ReleaseCall(call);

			return 0;
		}

		void CancelPendingCalls()
		{
			std::lock_guard<std::mutex> lock(g_pendingMutex);

			for (std::set<Call*>::const_iterator it = g_pendingCalls.begin(); it != g_pendingCalls.end(); ++it)
			{
				Call* call = *it;

				{
					std::lock_guard<std::mutex> callLock(call->mutex);
					call->finished = true;  // the queued message then runs no work
				}

				call->done.notify_all();
			}
		}
	}

	//===========================================================================

	bool IsStopping()
	{
		return g_stopping;
	}

	bool RunOnEmulatorThread(const std::function<void()>& work, unsigned int timeoutMs)
	{
		if (g_stopping || g_bridgeWindow == NULL)
			return false;

		if (GetCurrentThreadId() == g_emulatorThreadId)
		{
			work();
			return true;
		}

		Call* call = new Call(work);

		{
			std::lock_guard<std::mutex> lock(g_pendingMutex);
			g_pendingCalls.insert(call);
		}

		if (!PostMessage(g_bridgeWindow, WM_MCP_RUN, 0, reinterpret_cast<LPARAM>(call)))
		{
			std::lock_guard<std::mutex> lock(g_pendingMutex);
			g_pendingCalls.erase(call);
			ReleaseCall(call);
			ReleaseCall(call);
			return false;
		}

		bool executed = false;
		{
			std::unique_lock<std::mutex> lock(call->mutex);
			call->done.wait_for(lock, std::chrono::milliseconds(timeoutMs), [call] { return call->finished; });

			// The finished flag tells the queued message to run no work. The
			// emulator thread can find that message after this function stops.
			call->finished = true;
			executed = call->executed;
		}

		{
			std::lock_guard<std::mutex> lock(g_pendingMutex);
			g_pendingCalls.erase(call);
		}

		ReleaseCall(call);
		return executed;
	}

	//===========================================================================
	// Tool results

	void ToolResponse::Text(const std::string& text)
	{
		Json block = Json::Object();
		block["type"] = Json("text");
		block["text"] = Json(text);
		m_content.Append(block);
	}

	void ToolResponse::Image(const std::string& base64Data, const char* mimeType)
	{
		Json block = Json::Object();
		block["type"] = Json("image");
		block["data"] = Json(base64Data);
		block["mimeType"] = Json(mimeType);
		m_content.Append(block);
	}

	void ToolResponse::Error(const std::string& message)
	{
		m_isError = true;
		Text(message);
	}

	void ToolResponse::Unresponsive()
	{
		Error("The emulator did not respond. It may be showing a modal dialog, "
			"or the machine may be paused in a way that stops the message loop.");
	}

	//===========================================================================
	// JSON-RPC

	namespace
	{
		Json g_toolListCache;
		std::mutex g_toolListMutex;

		Json BuildToolList()
		{
			std::lock_guard<std::mutex> lock(g_toolListMutex);

			if (!g_toolListCache.IsNull())
				return g_toolListCache;

			size_t count = 0;
			const ToolDefinition* tools = GetToolTable(count);

			Json list = Json::Array();

			for (size_t i = 0; i < count; i++)
			{
				Json schema;
				std::string error;
				if (!Json::Parse(tools[i].inputSchema, schema, error))
				{
					LogFileOutput("MCP: schema for tool '%s' is malformed: %s\n", tools[i].name, error.c_str());
					schema = Json::Object();
					schema["type"] = Json("object");
				}

				Json entry = Json::Object();
				entry["name"] = Json(tools[i].name);
				entry["title"] = Json(tools[i].title);
				entry["description"] = Json(tools[i].description);
				entry["inputSchema"] = schema;
				list.Append(entry);
			}

			g_toolListCache = list;
			return list;
		}

		Json MakeError(const Json& id, int code, const std::string& message)
		{
			Json error = Json::Object();
			error["code"] = Json::Integer(code);
			error["message"] = Json(message);

			Json response = Json::Object();
			response["jsonrpc"] = Json("2.0");
			response["id"] = id;
			response["error"] = error;
			return response;
		}

		Json MakeResult(const Json& id, const Json& result)
		{
			Json response = Json::Object();
			response["jsonrpc"] = Json("2.0");
			response["id"] = id;
			response["result"] = result;
			return response;
		}

		Json HandleInitialize(const Json& params)
		{
			const std::string requested = params["protocolVersion"].AsString();

			std::string agreed = kPreferredProtocolVersion;
			for (size_t i = 0; i < sizeof(kSupportedProtocolVersions) / sizeof(kSupportedProtocolVersions[0]); i++)
			{
				if (requested == kSupportedProtocolVersions[i])
				{
					agreed = requested;
					break;
				}
			}

			Json capabilities = Json::Object();
			capabilities["tools"] = Json::Object();

			Json serverInfo = Json::Object();
			serverInfo["name"] = Json(kServerName);
			serverInfo["title"] = Json("AppleWin");
			serverInfo["version"] = Json(GetAppleWinVersionAndBuild());

			Json result = Json::Object();
			result["protocolVersion"] = Json(agreed);
			result["capabilities"] = capabilities;
			result["serverInfo"] = serverInfo;
			result["instructions"] = Json(GetServerInstructions());
			return result;
		}

		Json HandleToolCall(const Json& params, bool& isProtocolError, int& errorCode, std::string& errorMessage)
		{
			const std::string name = params["name"].AsString();

			size_t count = 0;
			const ToolDefinition* tools = GetToolTable(count);

			for (size_t i = 0; i < count; i++)
			{
				if (name != tools[i].name)
					continue;

				ToolResponse response;

				try
				{
					tools[i].handler(params["arguments"], response);
				}
				catch (const std::exception& exception)
				{
					// A tool that throws must not stop the socket thread. The
					// model can use the message.
					response = ToolResponse();
					response.Error(std::string("The tool raised an exception: ") + exception.what());
				}

				Json result = Json::Object();
				result["content"] = response.GetContent();
				result["isError"] = Json::Boolean(response.IsError());
				return result;
			}

			isProtocolError = true;
			errorCode = kRpcInvalidParams;
			errorMessage = "Unknown tool: " + name;
			return Json();
		}

		// Returns false for a notification, which carries no reply.
		bool DispatchRpcMessage(const Json& message, Json& response)
		{
			const std::string method = message["method"].AsString();
			const bool isNotification = !message.Has("id") || message["id"].IsNull();
			const Json id = message["id"];
			const Json params = message["params"];

			if (isNotification)
				return false;

			if (method == "initialize")
			{
				response = MakeResult(id, HandleInitialize(params));
				return true;
			}

			if (method == "ping")
			{
				response = MakeResult(id, Json::Object());
				return true;
			}

			if (method == "tools/list")
			{
				Json result = Json::Object();
				result["tools"] = BuildToolList();
				response = MakeResult(id, result);
				return true;
			}

			if (method == "tools/call")
			{
				bool isProtocolError = false;
				int code = 0;
				std::string errorMessage;

				const Json result = HandleToolCall(params, isProtocolError, code, errorMessage);

				if (isProtocolError)
					response = MakeError(id, code, errorMessage);
				else
					response = MakeResult(id, result);

				return true;
			}

			response = MakeError(id, kRpcMethodNotFound, "Method not found: " + method);
			return true;
		}

		bool HandleJsonRpc(const std::string& request, std::string& reply)
		{
			Json message;
			std::string error;

			if (!Json::Parse(request, message, error))
			{
				reply = MakeError(Json(), kRpcParseError, "Parse error: " + error).Serialise();
				return true;
			}

			if (message.IsArray())
			{
				// A batch has a reply only for the members that have an id.
				Json responses = Json::Array();

				for (size_t i = 0; i < message.Size(); i++)
				{
					Json response;
					if (DispatchRpcMessage(message.At(i), response))
						responses.Append(response);
				}

				if (responses.Size() == 0)
					return false;

				reply = responses.Serialise();
				return true;
			}

			Json response;
			if (!DispatchRpcMessage(message, response))
				return false;

			reply = response.Serialise();
			return true;
		}
	}
}

//=============================================================================
// The interface AppleWin calls

bool MCP_ParseCmdLineArg(const char* arg)
{
	if (!arg)
		return false;

	if (strcmp(arg, "-mcp") == 0)
	{
		MCP::g_enabled = true;
		return true;
	}

	if (MCP::StartsWith(arg, MCP::kPortArgPrefix))
	{
		const int port = atoi(arg + strlen(MCP::kPortArgPrefix));
		if (port < 0 || port > UINT16_MAX)
		{
			LogFileOutput("MCP: port %d is out of range, using the default\n", port);
		}
		else
		{
			MCP::g_requestedPort = static_cast<uint16_t>(port);
		}

		MCP::g_enabled = true;
		return true;
	}

	return false;
}

void MCP_Initialize()
{
	if (!MCP::g_enabled || MCP::g_running)
		return;

	MCP::g_stopping = false;
	MCP::g_emulatorThreadId = GetCurrentThreadId();

	WNDCLASSEX windowClass;
	memset(&windowClass, 0, sizeof(windowClass));
	windowClass.cbSize = sizeof(windowClass);
	windowClass.lpfnWndProc = MCP::BridgeWndProc;
	windowClass.hInstance = GetFrame().g_hInstance;
	windowClass.lpszClassName = MCP::kWindowClassName;
	RegisterClassEx(&windowClass);  // a second call after a restart has no effect

	// HWND_MESSAGE makes a window that receives posted messages only. It has no
	// pixels, no taskbar entry and no position in the z-order.
	MCP::g_bridgeWindow = CreateWindowEx(0, MCP::kWindowClassName, "AppleWin MCP",
		0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetFrame().g_hInstance, NULL);

	if (MCP::g_bridgeWindow == NULL)
	{
		LogFileOutput("MCP: could not create the bridge window (error %u)\n", GetLastError());
		GetFrame().FrameMessageBox("The MCP server could not create its bridge window.", "AppleWin MCP", MB_ICONWARNING | MB_OK);
		return;
	}

	std::string error;
	if (!MCP::g_httpServer.Start(MCP::g_requestedPort, MCP::HandleJsonRpc, error))
	{
		LogFileOutput("MCP: %s\n", error.c_str());

		std::string message("The MCP server could not start: ");
		message += error;
		message += "\n\nAppleWin will run normally without it.";
		GetFrame().FrameMessageBox(message.c_str(), "AppleWin MCP", MB_ICONWARNING | MB_OK);

		DestroyWindow(MCP::g_bridgeWindow);
		MCP::g_bridgeWindow = NULL;
		return;
	}

	MCP::g_running = true;

	LogFileOutput("MCP: listening on http://127.0.0.1:%u/mcp\n", MCP::g_httpServer.GetPort());
}

void MCP_Destroy()
{
	if (!MCP::g_running)
		return;

	// The sequence is important. First refuse new work. Then release the callers
	// that already wait. The socket threads then stop immediately. They do not
	// wait for their timeouts.
	MCP::g_stopping = true;
	MCP::CancelPendingCalls();

	MCP::g_httpServer.Stop();

	if (MCP::g_bridgeWindow)
	{
		DestroyWindow(MCP::g_bridgeWindow);
		MCP::g_bridgeWindow = NULL;
	}

	MCP::g_running = false;

	LogFileOutput("MCP: stopped\n");
}
