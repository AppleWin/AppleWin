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
 */

#include "StdAfx.h"

#include "MCP/MCPHttp.h"

#include "MCP/MCPHelpers.h"

#include "StrFormat.h"

#include <winsock.h>

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <thread>

namespace MCP
{
	namespace
	{
		const int kReceiveChunk = 8192;
		const size_t kMaxRequestBytes = 8 * 1024 * 1024;

		// How long the accept loop waits in select before it checks whether the
		// server is stopping.
		const long kAcceptPollMicroseconds = 200000;

		// 2 tells shutdown to stop both directions. winsock.h defines no name
		// for this value.
		const int kShutdownBoth = 2;

		bool SendAll(SOCKET client, const std::string& data)
		{
			size_t sent = 0;
			while (sent < data.size())
			{
				const int n = send(client, data.c_str() + sent, static_cast<int>(data.size() - sent), 0);
				if (n <= 0)
					return false;
				sent += n;
			}
			return true;
		}

		// This function adds the data that the socket holds. It returns false when
		// the client closes the connection, or when the request is too large.
		bool ReceiveMore(SOCKET client, std::string& buffer)
		{
			if (buffer.size() > kMaxRequestBytes)
				return false;

			char chunk[kReceiveChunk];
			const int n = recv(client, chunk, sizeof(chunk), 0);
			if (n <= 0)
				return false;

			buffer.append(chunk, n);
			return true;
		}

		std::string BuildResponse(const char* status, const char* contentType, const std::string& body, bool keepAlive, const char* extraHeader = NULL)
		{
			const std::string header = StrFormat(
				"HTTP/1.1 %s\r\n"
				"Content-Type: %s\r\n"
				"Content-Length: %u\r\n"
				"Connection: %s\r\n"
				"Cache-Control: no-store\r\n"
				"%s"
				"\r\n",
				status,
				contentType,
				static_cast<unsigned int>(body.size()),
				keepAlive ? "keep-alive" : "close",
				extraHeader ? extraHeader : "");

			return header + body;
		}

		// Only a browser sends an Origin header. The MCP specification tells a
		// local server to refuse a request from a different host. Without this
		// test, a page that the user did not write can control the emulator.
		bool IsOriginAcceptable(const std::string& origin)
		{
			if (origin.empty())
				return true;

			const std::string lower = ToLower(origin);
			return StartsWith(lower, "http://localhost")
				|| StartsWith(lower, "http://127.0.0.1")
				|| StartsWith(lower, "http://[::1]");
		}
	}

	//===========================================================================

	HttpServer::HttpServer()
		: m_handler(NULL)
		, m_listenSocket(INVALID_SOCKET)
		, m_port(0)
		, m_activeConnections(0)
		, m_stopping(false)
		, m_winsockStarted(false)
	{
	}

	HttpServer::~HttpServer()
	{
		Stop();
	}

	bool HttpServer::Start(uint16_t port, RequestHandler handler, std::string& error)
	{
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			error = "WSAStartup failed";
			return false;
		}
		m_winsockStarted = true;

		m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_listenSocket == INVALID_SOCKET)
		{
			error = "could not create a socket";
			Stop();
			return false;
		}

		// Without this option, a new bind fails during the TIME_WAIT period.
		int reuse = 1;
		setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

		sockaddr_in address;
		memset(&address, 0, sizeof(address));
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = htons(port);

		if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
		{
			error = StrFormat("could not bind to 127.0.0.1:%u (winsock error %d)", port, WSAGetLastError());
			Stop();
			return false;
		}

		// Port 0 tells the operating system to select a port. Read the port that
		// the operating system selected.
		int addressLength = sizeof(address);
		if (getsockname(m_listenSocket, reinterpret_cast<sockaddr*>(&address), &addressLength) == 0)
			m_port = ntohs(address.sin_port);
		else
			m_port = port;

		if (listen(m_listenSocket, 8) == SOCKET_ERROR)
		{
			error = "could not listen on the socket";
			Stop();
			return false;
		}

		m_handler = handler;
		m_stopping = false;
		m_acceptThread = std::thread(&HttpServer::AcceptLoop, this);

		return true;
	}

	void HttpServer::Stop()
	{
		m_stopping = true;

		if (m_listenSocket != INVALID_SOCKET)
		{
			closesocket(m_listenSocket);
			m_listenSocket = INVALID_SOCKET;
		}

		if (m_acceptThread.joinable())
			m_acceptThread.join();

		// Each connection thread has its own socket and closes that socket. Only
		// stop the data flow here. A closesocket call here can close a handle
		// that the operating system already gave to a different thread.
		{
			std::unique_lock<std::mutex> lock(m_mutex);

			for (std::set<SOCKET>::const_iterator it = m_clientSockets.begin(); it != m_clientSockets.end(); ++it)
				shutdown(*it, kShutdownBoth);

			m_idle.wait_for(lock, std::chrono::seconds(5), [this] { return m_activeConnections == 0; });
		}

		if (m_winsockStarted)
		{
			WSACleanup();
			m_winsockStarted = false;
		}
	}

	void HttpServer::AcceptLoop()
	{
		while (!m_stopping)
		{
			const SOCKET listenSocket = m_listenSocket;
			if (listenSocket == INVALID_SOCKET)
				break;

			// The timeout in select makes the loop examine the stop flag. This is
			// necessary when no client connects.
			fd_set readable;
			FD_ZERO(&readable);
			FD_SET(listenSocket, &readable);

			timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = kAcceptPollMicroseconds;

			if (select(0, &readable, NULL, NULL, &timeout) <= 0)
				continue;

			const SOCKET client = accept(listenSocket, NULL, NULL);
			if (client == INVALID_SOCKET)
				continue;

			if (m_stopping)
			{
				closesocket(client);
				break;
			}

			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_clientSockets.insert(client);
				m_activeConnections++;
			}

			std::thread(&HttpServer::ServeConnection, this, client).detach();
		}
	}

	bool HttpServer::ReadRequest(SOCKET client, std::string& buffer, Request& request)
	{
		size_t headerEnd = buffer.find("\r\n\r\n");
		while (headerEnd == std::string::npos)
		{
			if (!ReceiveMore(client, buffer))
				return false;
			headerEnd = buffer.find("\r\n\r\n");
		}

		const std::string head = buffer.substr(0, headerEnd);
		buffer.erase(0, headerEnd + 4);

		size_t lineStart = 0;
		bool firstLine = true;

		while (lineStart < head.size())
		{
			size_t lineEnd = head.find("\r\n", lineStart);
			if (lineEnd == std::string::npos)
				lineEnd = head.size();

			const std::string line = head.substr(lineStart, lineEnd - lineStart);
			lineStart = lineEnd + 2;

			if (firstLine)
			{
				firstLine = false;

				const size_t firstSpace = line.find(' ');
				if (firstSpace == std::string::npos)
					return false;

				const size_t secondSpace = line.find(' ', firstSpace + 1);
				if (secondSpace == std::string::npos)
					return false;

				request.method = line.substr(0, firstSpace);
				request.target = line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
				continue;
			}

			const size_t colon = line.find(':');
			if (colon == std::string::npos)
				continue;

			const std::string name = ToLower(Trim(line.substr(0, colon)));
			const std::string value = Trim(line.substr(colon + 1));

			if (name == "content-length")
				request.contentLength = atol(value.c_str());
			else if (name == "transfer-encoding")
				request.chunked = (ToLower(value).find("chunked") != std::string::npos);
			else if (name == "connection")
				request.keepAlive = (ToLower(value) != "close");
			else if (name == "origin")
				request.origin = value;
		}

		if (request.chunked)
		{
			// Some clients send the request body in chunks. They do not measure
			// the body first. Therefore this code must read the chunked format.
			while (true)
			{
				const size_t lineEnd = buffer.find("\r\n");
				if (lineEnd == std::string::npos)
				{
					if (!ReceiveMore(client, buffer))
						return false;
					continue;
				}

				const long chunkSize = strtol(buffer.substr(0, lineEnd).c_str(), NULL, 16);
				if (chunkSize < 0)
					return false;

				const size_t needed = lineEnd + 2 + static_cast<size_t>(chunkSize) + 2;
				if (buffer.size() < needed)
				{
					if (!ReceiveMore(client, buffer))
						return false;
					continue;
				}

				if (chunkSize)
					request.body.append(buffer, lineEnd + 2, static_cast<size_t>(chunkSize));

				buffer.erase(0, needed);

				if (chunkSize == 0)
					break;
			}
		}
		else if (request.contentLength > 0)
		{
			const size_t length = static_cast<size_t>(request.contentLength);
			if (length > kMaxRequestBytes)
				return false;

			while (buffer.size() < length)
			{
				if (!ReceiveMore(client, buffer))
					return false;
			}

			request.body = buffer.substr(0, length);
			buffer.erase(0, length);
		}

		return true;
	}

	std::string HttpServer::HandleRequest(const Request& request)
	{
		if (!IsOriginAcceptable(request.origin))
			return BuildResponse("403 Forbidden", "text/plain", "cross-origin requests are refused\n", false);

		if (request.method == "POST")
		{
			std::string reply;
			const bool hasReply = m_handler ? m_handler(request.body, reply) : false;

			if (hasReply)
				return BuildResponse("200 OK", "application/json", reply, request.keepAlive);

			return BuildResponse("202 Accepted", "text/plain", "", request.keepAlive);
		}

		if (request.method == "OPTIONS")
			return BuildResponse("204 No Content", "text/plain", "", request.keepAlive, "Allow: POST, GET, DELETE, OPTIONS\r\n");

		if (request.method == "DELETE")
		{
			// The client closes the session. This server keeps no session data.
			// Therefore there is nothing to remove.
			return BuildResponse("200 OK", "text/plain", "", request.keepAlive);
		}

		if (request.method == "GET")
		{
			// The specification permits a server to refuse the event stream from
			// the server to the client. This server sends no message of its own.
			return BuildResponse("405 Method Not Allowed", "text/plain",
				"this server does not offer a server-sent event stream\n", request.keepAlive,
				"Allow: POST, DELETE, OPTIONS\r\n");
		}

		return BuildResponse("405 Method Not Allowed", "text/plain", "", false, "Allow: POST, GET, DELETE, OPTIONS\r\n");
	}

	void HttpServer::ServeConnection(SOCKET client)
	{
		std::string buffer;

		while (!m_stopping)
		{
			Request request;
			if (!ReadRequest(client, buffer, request))
				break;

			const std::string response = HandleRequest(request);

			if (!SendAll(client, response))
				break;

			if (!request.keepAlive)
				break;
		}

		closesocket(client);

		std::lock_guard<std::mutex> lock(m_mutex);
		m_clientSockets.erase(client);
		if (--m_activeConnections == 0)
			m_idle.notify_all();
	}
}
