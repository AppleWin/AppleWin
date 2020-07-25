/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

/* Description: KiwiPad IP input for joystick, mouse etc
 *
 * Author: Nick Westgate
 */

#include "StdAfx.h"
#include "Applewin.h"
#include "Configuration\PropertySheet.h"
#include "KiwiPad.h"
#include "Keyboard.h"
#include "GenericSocketDriver.h"
#include "Log.h"

#include <queue>

//===========================================================================
// TCP/IP Joystick

static char joystick_device_name[] = "IP Joystick";
static SocketInfo TcpIpJoystickSocketInfo;
static SocketInfo UdpIpJoystickSocketInfo;

static int const TcpIpJoystickCommandMaxLength = 1 << 5; // must be a power of 2
static int const TcpIpJoystickMaxConnections = 4;

static char TcpIpJoystickCommandBuffer[TcpIpJoystickMaxConnections][TcpIpJoystickCommandMaxLength]; // "J1 65535 65535 15\n"; // sample data
static int TcpIpJoystickCommandLength[TcpIpJoystickMaxConnections] = { 0, 0, 0, 0 };
static bool TcpIpJoystickInitialized = false;

static char KiwiPadClipboardFake[2] = "K";
static std::queue<char> KiwiPadClipboardFakeChars;

BOOL TcpIpJoystickSocketRxHandler(SocketInfo *socket_info_ptr, int c)
{
	int connection = socket_info_ptr->connection_number;

	if (c < 0) {
		TcpIpJoystickCommandLength[connection] = 0; // signal to clear the buffer (eg from socket_close)
		return TRUE; // we handled this
	}

	if (c != '\n') {
		TcpIpJoystickCommandBuffer[connection][TcpIpJoystickCommandLength[connection]++] = c;
		TcpIpJoystickCommandLength[connection] &= (TcpIpJoystickCommandMaxLength - 1);
		return TRUE; // we handled this
	}

	char *token;
	int device = 0;
	int x = 0, y = 0, buttons = 0;

	TcpIpJoystickCommandBuffer[connection][TcpIpJoystickCommandLength[connection]] = '\0';
	TcpIpJoystickCommandLength[connection] = 0;
	LogOutput("KiwiPad command=%s\n", TcpIpJoystickCommandBuffer);

	token = strtok(TcpIpJoystickCommandBuffer[connection], " ");
	if (token != NULL) {
		// Device ID: J1, J2, K1, M1, P1, P2
		if (strlen(token) == 2) {
			device = token[0] << (token[1] & 0xF);
		}
		token = strtok(NULL, " ");
	}
	if (token != NULL) {
		x = atoi(token);
		token = strtok(NULL, " ");
	}
	if (token != NULL) {
		y = atoi(token);
		token = strtok(NULL, " ");
	}
	if (token != NULL) {
		buttons = atoi(token); // bitfield: 1=SW0 2=SW1 4=SW2 8=SW3
	}

	switch (device)
	{
	case 'J' << 1:
		JoySetPositionX(0, x >> 8); // 0 to 65335 -> 0 to 255
		JoySetPositionY(0, y >> 8); // 0 to 65335 -> 0 to 255
		JoySetButton(0, buttons & 0x1);
		JoySetButton(1, buttons & 0x2);
		break;

	case 'J' << 2:
		JoySetPositionX(1, x >> 8); // 0 to 65335 -> 0 to 255
		JoySetPositionY(1, y >> 8); // 0 to 65335 -> 0 to 255
		JoySetButton(2, buttons & 0x1);
		break;

	case 'K' << 1:
	{
		char key = x & 0x7F;
		//KeybQueueKeypress(key, ASCII);
		KiwiPadClipboardFakeChars.push(key);
	}
	break;

	case 'M' << 1:
		//int iX, iMinX, iMaxX;
		//int iY, iMinY, iMaxY;
		//sg_Mouse.GetXY(iX, iMinX, iMaxX, iY, iMinY, iMaxY);
		// x >>= 8; // 0 to 65335 -> 0 to 255
		// y >>= 8; // 0 to 65335 -> 0 to 255
		//float fScaleX = (float)x / (float)255;
		//float fScaleY = (float)y / (float)255;
		//int iAppleX = iMinX + (int)(fScaleX * (float)(iMaxX - iMinX));
		//int iAppleY = iMinY + (int)(fScaleY * (float)(iMaxY - iMinY));
		//sg_Mouse.SetCursorPos(iAppleX, iAppleY);	// Set new entry position
		//sg_Mouse.SetButton(BUTTON0, (buttons & 0x1) ? BUTTON_DOWN : BUTTON_UP);
		//sg_Mouse.SetButton(BUTTON1, (buttons & 0x2) ? BUTTON_DOWN : BUTTON_UP);
		break;

	case 'P' << 1:
		JoySetPositionX(0, x >> 8); // 0 to 65335 -> 0 to 255
		JoySetButton(0, buttons & 0x1);
		break;

	case 'P' << 2:
		JoySetPositionY(0, x >> 8); // 0 to 65335 -> 0 to 255
		JoySetButton(1, buttons & 0x1);
		break;
	}
	return TRUE; // we handled this
}

void TcpIpJoystickInit()
{
	if (TcpIpJoystickInitialized)
		return;
	
	TcpIpJoystickSocketInfo.device_name = joystick_device_name;
	TcpIpJoystickSocketInfo.device_data = NULL;
	TcpIpJoystickSocketInfo.listen_port = 6503;
	TcpIpJoystickSocketInfo.listen_tries = 1;
	TcpIpJoystickSocketInfo.max_connections = TcpIpJoystickMaxConnections;
	TcpIpJoystickSocketInfo.connection_number = 0;
	TcpIpJoystickSocketInfo.rx_handler = TcpIpJoystickSocketRxHandler;

	UdpIpJoystickSocketInfo.device_name = joystick_device_name;
	UdpIpJoystickSocketInfo.device_data = NULL;
	UdpIpJoystickSocketInfo.listen_port = 6504;
	UdpIpJoystickSocketInfo.udp = TRUE;
	UdpIpJoystickSocketInfo.listen_tries = 1;
	UdpIpJoystickSocketInfo.connection_number = 0;
	UdpIpJoystickSocketInfo.rx_handler = TcpIpJoystickSocketRxHandler;

	//if (sg_PropertySheet.GetTcpIpJoystock())
	{
		socket_init(&TcpIpJoystickSocketInfo);
		socket_init(&UdpIpJoystickSocketInfo);
		TcpIpJoystickInitialized = true;
	}
}

void TcpIpJoystickShutdown()
{
	if (TcpIpJoystickInitialized)
	{
		socket_shutdown(&TcpIpJoystickSocketInfo);
		socket_shutdown(&UdpIpJoystickSocketInfo);
		TcpIpJoystickInitialized = false;
	}
}

void TcpIpJoystickUpdate()
{
	//if (sg_PropertySheet.GetTcpIpJoystock())
	{
		if (TcpIpJoystickInitialized)
		{
			socket_fill_readbuf(&TcpIpJoystickSocketInfo, 100, 0);
			socket_fill_readbuf(&UdpIpJoystickSocketInfo, 100, 0);
			if (!KiwiPadClipboardFakeChars.empty())
			{
				if (ClipboardInitiatePasteFake(KiwiPadClipboardFake))
				{
					char key = KiwiPadClipboardFakeChars.front();
					KiwiPadClipboardFakeChars.pop();
					KiwiPadClipboardFake[0] = key;
				}
			}
		}
		else
			TcpIpJoystickInit();
	}
	//else
	//{
	//	if (TcpIpJoystickInitialized)
	//		TcpIpJoystickShutdown();
	//}
}
