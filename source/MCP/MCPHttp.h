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

/* Description: The MCP server's HTTP transport
 *
 * Author: Copyright (C) 2026 Robert Baruch
 *
 * MCP has a streamable HTTP transport. The client sends a JSON-RPC message with
 * POST. The client then reads the reply from the response to that POST. AppleWin
 * is a program with a graphical interface, and it has no stdio. Therefore this
 * transport is the transport that AppleWin can supply.
 *
 * This server listens on the loopback address only. It also refuses a request
 * that has an Origin header from a different host. Thus a page in a browser
 * cannot control the emulator.
 */

#include <winsock.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <thread>

namespace MCP
{
	class HttpServer
	{
	public:
		// This function receives one JSON-RPC message and writes the reply. If
		// it returns false, the message is a notification. The server then sends
		// 202 Accepted with an empty body.
		typedef bool (*RequestHandler)(const std::string& request, std::string& reply);

		HttpServer();
		~HttpServer();

		bool Start(uint16_t port, RequestHandler handler, std::string& error);
		void Stop();

		uint16_t GetPort() const { return m_port; }
		bool IsRunning() const { return m_listenSocket != INVALID_SOCKET; }

	private:
		struct Request
		{
			std::string method;
			std::string target;
			std::string body;
			std::string origin;
			bool keepAlive;
			bool chunked;
			long contentLength;

			Request() : keepAlive(true), chunked(false), contentLength(-1) {}
		};

		void AcceptLoop();
		void ServeConnection(SOCKET client);
		bool ReadRequest(SOCKET client, std::string& buffer, Request& request);
		std::string HandleRequest(const Request& request);

		RequestHandler m_handler;
		SOCKET m_listenSocket;
		uint16_t m_port;

		std::thread m_acceptThread;

		// Each connection thread runs detached. A thread decreases the counter
		// when it stops. Thus a long session does not collect one thread object
		// for each request.
		std::mutex m_mutex;
		std::condition_variable m_idle;
		std::set<SOCKET> m_clientSockets;
		int m_activeConnections;

		std::atomic<bool> m_stopping;
		bool m_winsockStarted;
	};
}
