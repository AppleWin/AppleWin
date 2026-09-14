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

/* Description: The MCP server's tools
 *
 * Author: Copyright (C) 2026 Robert Baruch
 *
 * Each tool has two parts. The first part runs on the socket thread. It reads the
 * arguments, waits when a wait is necessary, and writes the response. The second
 * part is a lambda that RunOnEmulatorThread sends to the emulator thread. Only
 * that lambda touches the machine.
 */

#include "StdAfx.h"

#include "MCP/MCPTools.h"

#include "MCP/MCPEncode.h"
#include "MCP/MCPHelpers.h"
#include "MCP/MCPServer.h"

#include "Card.h"
#include "CardManager.h"
#include "Common.h"
#include "Core.h"
#include "CPU.h"
#include "Disk.h"
#include "DiskImage.h"
#include "FrameBase.h"
#include "Interface.h"
#include "Joystick.h"
#include "Keyboard.h"
#include "Memory.h"
#include "SaveState.h"
#include "SoundCore.h"
#include "StrFormat.h"
#include "Utilities.h"
#include "Video.h"
#include "Debugger/Debug.h"
#include "Debugger/Debugger_Console.h"
#include "Debugger/Debugger_Display.h"

#include <windows.h>

#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace MCP
{
	namespace
	{
		typedef std::chrono::steady_clock Clock;

		//=======================================================================
		// Timeouts, defaults and limits

		// A debugger command can take longer than kEmulatorCallTimeoutMs allows.
		const unsigned int kDebuggerCommandTimeoutMs = 30000;

		// How long to wait for the program to read each key before sending the
		// next one anyway, and how long the Apple buttons and the any-key-down
		// flag stay pressed.
		const unsigned int kKeyWaitMsDefault = 500;
		const unsigned int kKeyHoldMsDefault = 30;

		const double kWaitSecondsDefault = 1.0;
		const double kWaitSecondsMax = 120.0;
		const unsigned int kWaitPollMs = 10;

		const double kWaitForTextTimeoutSecondsDefault = 10.0;
		const double kWaitForTextTimeoutSecondsMax = 300.0;
		const unsigned int kWaitForTextPollMs = 50;

		const long long kReadMemoryLengthDefault = 256;
		const size_t kHexDumpBytesPerRow = 16;

		// The custom speed is a multiple of 1 MHz. AppleWin keeps the speed in
		// tenths, with SPEED_NORMAL for 1 MHz and SPEED_MAX for no limit, so the
		// largest custom multiplier is one step below SPEED_MAX.
		const double kSpeedMultiplierMin = 0.5;
		const double kSpeedMultiplierMax = (SPEED_MAX - 1) / static_cast<double>(SPEED_NORMAL);

		// Bit 7 of the keyboard latch is set from a keypress until the program
		// reads the key.
		const uint8_t kKeyboardStrobe = 0x80;

		//=======================================================================
		// Argument helpers

		// This function reads an address or a byte value. A JSON number is
		// decimal. A JSON string is hexadecimal, with or without a "$" or "0x"
		// prefix. The function returns false if the value is not a number.
		bool ParseNumber(const Json& value, long long& out)
		{
			if (value.IsNumber())
			{
				out = value.AsInteger();
				return true;
			}

			if (!value.IsString())
				return false;

			std::string text = value.AsString();
			if (text.empty())
				return false;

			if (text[0] == '$')
				text.erase(0, 1);
			else if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
				text.erase(0, 2);

			if (text.empty())
				return false;

			char* end = NULL;
			out = strtoll(text.c_str(), &end, 16);
			return end && *end == 0;
		}

		//=======================================================================
		// Machine helpers. Call each one on the emulator thread only.

		const char* AppModeName(AppMode_e mode)
		{
			switch (mode)
			{
			case MODE_LOGO:      return "logo (power off, no disk booted)";
			case MODE_PAUSED:    return "paused";
			case MODE_RUNNING:   return "running";
			case MODE_DEBUG:     return "debugger (CPU stopped)";
			case MODE_STEPPING:  return "stepping";
			case MODE_BENCHMARK: return "benchmark";
			default:             return "unknown";
			}
		}

		const char* Apple2TypeName(eApple2Type type)
		{
			switch (type)
			{
			case A2TYPE_APPLE2:          return "Apple ][";
			case A2TYPE_APPLE2PLUS:      return "Apple ][+";
			case A2TYPE_APPLE2JPLUS:     return "Apple ][ J-Plus";
			case A2TYPE_APPLE2E:         return "Apple //e";
			case A2TYPE_APPLE2EENHANCED: return "Enhanced Apple //e";
			case A2TYPE_APPLE2C:         return "Apple //c";
			case A2TYPE_PRAVETS82:       return "Pravets 82";
			case A2TYPE_PRAVETS8M:       return "Pravets 8M";
			case A2TYPE_PRAVETS8A:       return "Pravets 8A";
			case A2TYPE_TK30002E:        return "TK3000 //e";
			case A2TYPE_BASE64A:         return "Base 64A";
			default:                     return "unknown";
			}
		}

		// This function returns the text screen as 24 lines. The lines are 40 or
		// 80 characters wide. Each line ends with a newline.
		std::string GetTextScreen()
		{
			char* text = NULL;
			size_t size = 0;

			if (g_nAppMode == MODE_DEBUG && !DebugGetVideoMode(NULL))
				size = Util_GetDebuggerText(text);
			else
				size = Util_GetTextScreen(text);

			std::string result;
			if (text && size)
				result.assign(text, size);

			// The Windows build writes CR LF at the end of each line. Keep LF only.
			std::string clean;
			clean.reserve(result.size());
			for (size_t i = 0; i < result.size(); i++)
			{
				if (result[i] != '\r')
					clean += result[i];
			}

			return clean;
		}

		Disk2InterfaceCard* GetDiskCard(UINT slot)
		{
			if (GetCardMgr().QuerySlot(slot) != CT_Disk2)
				return NULL;

			return dynamic_cast<Disk2InterfaceCard*>(GetCardMgr().GetObj(slot));
		}

		enum Bank_e { BANK_CPU, BANK_MAIN, BANK_AUX };

		bool ParseBank(const Json& value, Bank_e& bank)
		{
			const std::string name = ToLower(value.AsString("cpu"));

			if (name == "cpu")
				bank = BANK_CPU;
			else if (name == "main")
				bank = BANK_MAIN;
			else if (name == "aux" || name == "auxiliary")
				bank = BANK_AUX;
			else
				return false;

			return true;
		}

		uint8_t ReadBankByte(Bank_e bank, uint16_t address)
		{
			switch (bank)
			{
			case BANK_MAIN: return *MemGetMainPtr(address);
			case BANK_AUX:  return *MemGetAuxPtr(address);
			default:        return ReadByteFromMemory(address);
			}
		}

		void WriteBankByte(Bank_e bank, uint16_t address, uint8_t value)
		{
			if (bank == BANK_CPU)
			{
				WriteByteToMemory(address, value);
				return;
			}

			LPBYTE ptr = (bank == BANK_AUX) ? MemGetAuxPtr(address) : MemGetMainPtr(address);
			if (!ptr)
				return;

			*ptr = value;

			// The pointer can point into the CPU cache. The cache page must then
			// go back to the bank before the next bank switch.
			if (ptr == mem + address)
				memdirty[address >> 8] = 1;
		}

		std::string HexDump(const std::vector<uint8_t>& data, unsigned int base)
		{
			std::string out;

			for (size_t row = 0; row < data.size(); row += kHexDumpBytesPerRow)
			{
				out += StrFormat("$%04X:", static_cast<unsigned int>((base + row) & 0xFFFF));

				std::string ascii;
				for (size_t i = row; i < row + kHexDumpBytesPerRow; i++)
				{
					if (i < data.size())
					{
						out += StrFormat(" %02X", data[i]);
						const char ch = static_cast<char>(data[i] & 0x7F);
						ascii += (ch >= 0x20 && ch < 0x7F) ? ch : '.';
					}
					else
					{
						out += "   ";
					}
				}

				out += "  |" + ascii + "|\n";
			}

			return out;
		}

		// This function reads the frame buffer and gives a top-down RGB image.
		// The frame buffer is a bottom-up DIB with BGRA pixels and a border.
		bool GrabFrame(bool half, unsigned int& width, unsigned int& height, std::vector<uint8_t>& rgb)
		{
			Video& video = GetVideo();
			const uint32_t* frame = reinterpret_cast<const uint32_t*>(video.GetFrameBuffer());
			if (!frame)
				return false;

			const unsigned int frameWidth = video.GetFrameBufferWidth();
			const unsigned int borderX = video.GetFrameBufferBorderWidth();
			const unsigned int borderY = video.GetFrameBufferBorderHeight();
			const unsigned int fullWidth = video.GetFrameBufferBorderlessWidth();
			const unsigned int fullHeight = video.GetFrameBufferBorderlessHeight();

			const unsigned int step = half ? 2 : 1;
			width = fullWidth / step;
			height = fullHeight / step;
			rgb.resize(static_cast<size_t>(width) * height * 3);

			for (unsigned int y = 0; y < height; y++)
			{
				// AppleWin draws the odd lines in the 50% scan line modes. Thus
				// the half-size image takes the odd lines and the odd pixels.
				const unsigned int sourceY = half ? (y * 2 + 1) : y;
				const uint32_t* line = frame + static_cast<size_t>(borderY + (fullHeight - 1 - sourceY)) * frameWidth + borderX;
				uint8_t* dst = &rgb[static_cast<size_t>(y) * width * 3];

				for (unsigned int x = 0; x < width; x++)
				{
					const uint32_t pixel = line[half ? (x * 2 + 1) : x];
					*dst++ = static_cast<uint8_t>(pixel >> 16);
					*dst++ = static_cast<uint8_t>(pixel >> 8);
					*dst++ = static_cast<uint8_t>(pixel);
				}
			}

			return true;
		}

		// This function puts one character or one virtual key into the keyboard
		// latch, but only if the program read the previous key. It returns false
		// if the latch is full.
		bool QueueKeyIfLatchClear(WPARAM key, bool isAscii)
		{
			if (KeybReadData() & kKeyboardStrobe)
				return false;

			KeybQueueKeypress(key, isAscii ? ASCII : NOT_ASCII);
			return true;
		}

		// This function sends one key from the socket thread. It waits for the
		// keyboard latch for the given time. If the latch stays full, it sends
		// the key anyway and returns false.
		bool SendKeyPaced(WPARAM key, bool isAscii, unsigned int waitMs, bool& unresponsive)
		{
			const Clock::time_point deadline = Clock::now() + std::chrono::milliseconds(waitMs);

			while (!IsStopping())
			{
				bool queued = false;
				if (!RunOnEmulatorThread([&] { queued = QueueKeyIfLatchClear(key, isAscii); }))
				{
					unresponsive = true;
					return false;
				}

				if (queued)
					return true;

				if (Clock::now() >= deadline)
					break;

				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}

			if (!RunOnEmulatorThread([&] { KeybQueueKeypress(key, isAscii ? ASCII : NOT_ASCII); }))
				unresponsive = true;

			return false;
		}

		//=======================================================================
		// Key names for press_keys

		struct KeyAction
		{
			int character;   // ASCII code, or -1
			int virtualKey;  // VK_ code, or -1
			bool openApple;
			bool closedApple;
		};

		// This function removes prefix from the front of text and returns true. If
		// text does not begin with prefix, it returns false and leaves text alone.
		// The letters in prefix must be lower case; the letters in text can be
		// either case.
		bool StripPrefixIgnoreCase(std::string& text, const char* prefix)
		{
			if (!StartsWith(ToLower(text), prefix))
				return false;
			text.erase(0, strlen(prefix));
			return true;
		}

		// This function reads one key name. Examples: "a", "return", "ctrl-c",
		// "open-apple-r", "left". It returns false if the name is not known.
		bool ParseKeyName(const std::string& spec, KeyAction& action, std::string& error)
		{
			action.character = -1;
			action.virtualKey = -1;
			action.openApple = false;
			action.closedApple = false;

			std::string rest = spec;
			bool control = false;

			// Read the modifiers from the front of the name.
			while (true)
			{
				if (StripPrefixIgnoreCase(rest, "open-apple-") || StripPrefixIgnoreCase(rest, "openapple-"))
					action.openApple = true;
				else if (StripPrefixIgnoreCase(rest, "closed-apple-") || StripPrefixIgnoreCase(rest, "closedapple-"))
					action.closedApple = true;
				else if (StripPrefixIgnoreCase(rest, "ctrl-") || StripPrefixIgnoreCase(rest, "control-"))
					control = true;
				else
					break;
			}

			const std::string name = ToLower(rest);

			if (name == "open-apple" || name == "openapple")
			{
				action.openApple = true;
				return true;
			}

			if (name == "closed-apple" || name == "closedapple")
			{
				action.closedApple = true;
				return true;
			}

			struct NamedKey { const char* name; int character; int virtualKey; };
			static const NamedKey namedKeys[] =
			{
				{ "return",    0x0D, -1 }, { "enter", 0x0D, -1 },
				{ "escape",    0x1B, -1 }, { "esc",   0x1B, -1 },
				{ "tab",       0x09, -1 },
				{ "space",     0x20, -1 },
				{ "backspace", 0x08, -1 },
				{ "delete",    -1, VK_DELETE }, { "del", -1, VK_DELETE },
				{ "left",      -1, VK_LEFT },
				{ "right",     -1, VK_RIGHT },
				{ "up",        -1, VK_UP },
				{ "down",      -1, VK_DOWN },
			};

			for (size_t i = 0; i < sizeof(namedKeys) / sizeof(namedKeys[0]); i++)
			{
				if (name == namedKeys[i].name)
				{
					action.character = namedKeys[i].character;
					action.virtualKey = namedKeys[i].virtualKey;
					break;
				}
			}

			if (action.character < 0 && action.virtualKey < 0)
			{
				if (rest.size() != 1 || static_cast<uint8_t>(rest[0]) < 0x20 || static_cast<uint8_t>(rest[0]) > 0x7E)
				{
					error = "Unknown key name: " + spec;
					return false;
				}

				action.character = static_cast<uint8_t>(rest[0]);
			}

			if (control)
			{
				if (action.character < 0)
				{
					error = "ctrl- applies to a letter or a punctuation key, not to " + rest;
					return false;
				}

				const int upper = toupper(action.character);
				if (upper >= '@' && upper <= '_')
					action.character = upper - '@';
				else
				{
					error = "ctrl- has no meaning with the key " + rest;
					return false;
				}
			}

			return true;
		}

		//=======================================================================
		// Tools

		void ToolGetStatus(const Json&, ToolResponse& response)
		{
			std::string text;

			const bool ok = RunOnEmulatorThread([&]
			{
				text += StrFormat("Mode: %s\n", AppModeName(g_nAppMode));
				text += StrFormat("Machine: %s\n", Apple2TypeName(GetApple2Type()));

				if (g_dwSpeed == SPEED_MAX)
					text += "Speed: maximum (no limit)\n";
				else
					text += StrFormat("Speed: %.1fx of 1 MHz%s\n", g_dwSpeed / 10.0, g_bFullSpeed ? " (full speed at this moment)" : "");

				text += StrFormat("Cycles: %llu\n", static_cast<unsigned long long>(g_nCumulativeCycles));

				Video& video = GetVideo();
				text += StrFormat("Video: %s, %s columns, page %d%s%s\n",
					video.VideoGetSWTEXT() ? "text" : (video.VideoGetSWHIRES() ? (video.VideoGetSWDHIRES() ? "double hi-res" : "hi-res") : "lo-res"),
					video.VideoGetSW80COL() ? "80" : "40",
					video.VideoGetSWPAGE2() ? 2 : 1,
					(video.VideoGetSWMIXED() && !video.VideoGetSWTEXT()) ? ", mixed" : "",
					video.VideoGetSWAltCharSet() ? ", alternate character set" : "");

				for (UINT slot = 1; slot < NUM_SLOTS; slot++)
				{
					Disk2InterfaceCard* card = GetDiskCard(slot);
					if (!card)
						continue;

					for (int drive = 0; drive < 2; drive++)
					{
						if (!card->IsDriveConnected(drive))
							continue;

						if (card->IsDriveEmpty(drive))
							text += StrFormat("Slot %u drive %d: empty\n", slot, drive + 1);
						else
							text += StrFormat("Slot %u drive %d: %s (track %d%s)\n", slot, drive + 1,
								card->GetFullDiskFilename(drive).c_str(), card->GetTrack(drive),
								card->GetProtect(drive) ? ", write protected" : "");
					}
				}

				const regsrec& r = regs;
				text += StrFormat("CPU: PC=$%04X A=$%02X X=$%02X Y=$%02X SP=$%04X P=$%02X\n", r.pc, r.a, r.x, r.y, r.sp, r.ps);
			});

			if (!ok)
			{
				response.Unresponsive();
				return;
			}

			response.Text(text);
		}

		void ToolReadScreen(const Json&, ToolResponse& response)
		{
			std::string screen;
			bool is80 = false;
			bool isDebugger = false;

			const bool ok = RunOnEmulatorThread([&]
			{
				screen = GetTextScreen();
				is80 = GetVideo().VideoGetSW80COL();
				isDebugger = (g_nAppMode == MODE_DEBUG) && !DebugGetVideoMode(NULL);
			});

			if (!ok)
			{
				response.Unresponsive();
				return;
			}

			// Remove the spaces at the end of each line. This makes the text
			// shorter for the model.
			std::string trimmed;
			size_t start = 0;
			while (start < screen.size())
			{
				size_t end = screen.find('\n', start);
				if (end == std::string::npos)
					end = screen.size();
				trimmed += TrimRight(screen.substr(start, end - start));
				trimmed += '\n';
				start = end + 1;
			}

			std::string header = isDebugger
				? "Debugger screen (80x43):\n"
				: StrFormat("Text screen (%d columns x 24 lines):\n", is80 ? 80 : 40);

			response.Text(header + trimmed);
		}

		void ToolScreenshot(const Json& args, ToolResponse& response)
		{
			const std::string size = args["size"].AsString("560x384");
			const bool half = (size == "280x192");

			if (!half && size != "560x384")
			{
				response.Error("size must be \"560x384\" or \"280x192\".");
				return;
			}

			unsigned int width = 0;
			unsigned int height = 0;
			std::vector<uint8_t> rgb;
			bool grabbed = false;

			const bool ok = RunOnEmulatorThread([&]
			{
				// The frame buffer holds the last drawn frame. Draw the current
				// screen first, so the image is not older than the machine.
				if (g_nAppMode != MODE_DEBUG)
					GetFrame().VideoRedrawScreen();

				grabbed = GrabFrame(half, width, height, rgb);
			});

			if (!ok)
			{
				response.Unresponsive();
				return;
			}

			if (!grabbed)
			{
				response.Error("The frame buffer is not available.");
				return;
			}

			std::vector<uint8_t> png;
			if (!EncodePng(&rgb[0], width, height, png))
			{
				response.Error("PNG encoding failed.");
				return;
			}

			response.Image(EncodeBase64(&png[0], png.size()), "image/png");
		}

		void ToolTypeText(const Json& args, ToolResponse& response)
		{
			if (!args["text"].IsString())
			{
				response.Error("text is required.");
				return;
			}

			std::string text = args["text"].AsString();
			if (args["submit"].AsBool(false))
				text += '\r';

			const unsigned int waitMs = static_cast<unsigned int>(args["key_wait_ms"].AsInteger(kKeyWaitMsDefault));

			size_t sent = 0;
			size_t overruns = 0;
			bool unresponsive = false;

			for (size_t i = 0; i < text.size() && !unresponsive; i++)
			{
				uint8_t ch = static_cast<uint8_t>(text[i]);

				if (ch == '\n')
				{
					// A newline in the text means the Return key. Skip the LF of
					// a CR LF pair.
					if (i > 0 && text[i - 1] == '\r')
						continue;
					ch = '\r';
				}

				if (ch > 0x7F)
				{
					response.Error(StrFormat("The text has a character at index %u that the Apple II keyboard cannot type.", static_cast<unsigned int>(i)));
					return;
				}

				if (!SendKeyPaced(ch, true, waitMs, unresponsive))
					overruns++;

				sent++;
			}

			if (unresponsive)
			{
				response.Unresponsive();
				return;
			}

			std::string summary = StrFormat("Typed %u characters.", static_cast<unsigned int>(sent));
			if (overruns)
				summary += StrFormat(" The program did not read the keyboard in time for %u of them, so those may be lost. "
					"Use wait_for_text to find the moment when the program reads the keyboard, or increase key_wait_ms.",
					static_cast<unsigned int>(overruns));

			response.Text(summary);
		}

		void ToolPressKeys(const Json& args, ToolResponse& response)
		{
			const Json& keys = args["keys"];
			if (!keys.IsArray() || keys.Size() == 0)
			{
				response.Error("keys must be a non-empty array of key names.");
				return;
			}

			std::vector<KeyAction> actions;
			for (size_t i = 0; i < keys.Size(); i++)
			{
				KeyAction action;
				std::string error;
				if (!ParseKeyName(keys.At(i).AsString(), action, error))
				{
					response.Error(error);
					return;
				}
				actions.push_back(action);
			}

			const unsigned int waitMs = static_cast<unsigned int>(args["key_wait_ms"].AsInteger(kKeyWaitMsDefault));
			const unsigned int holdMs = static_cast<unsigned int>(args["hold_ms"].AsInteger(kKeyHoldMsDefault));

			size_t overruns = 0;
			bool unresponsive = false;

			for (size_t i = 0; i < actions.size() && !unresponsive; i++)
			{
				const KeyAction& action = actions[i];

				// The Apple keys are joystick buttons. Push them before the key
				// and release them after it.
				if (action.openApple || action.closedApple)
				{
					if (!RunOnEmulatorThread([&]
					{
						if (action.openApple) JoySetButton(BUTTON0, BUTTON_DOWN);
						if (action.closedApple) JoySetButton(BUTTON1, BUTTON_DOWN);
					}))
					{
						unresponsive = true;
						break;
					}

					std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));
				}

				if (action.character >= 0)
				{
					if (!SendKeyPaced(static_cast<WPARAM>(action.character), true, waitMs, unresponsive))
						overruns++;
				}
				else if (action.virtualKey >= 0)
				{
					// An arrow key also sets the any-key-down flag, which some
					// programs read. Set the flag, send the key, then release.
					const WPARAM vk = static_cast<WPARAM>(action.virtualKey);

					if (!RunOnEmulatorThread([&] { KeybAnyKeyDown(WM_KEYDOWN, vk, true); }))
					{
						unresponsive = true;
						break;
					}

					if (!SendKeyPaced(vk, false, waitMs, unresponsive))
						overruns++;

					std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));

					if (!RunOnEmulatorThread([&] { KeybAnyKeyDown(WM_KEYUP, vk, true); }))
					{
						unresponsive = true;
						break;
					}
				}

				if (action.openApple || action.closedApple)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));

					if (!RunOnEmulatorThread([&]
					{
						if (action.openApple) JoySetButton(BUTTON0, BUTTON_UP);
						if (action.closedApple) JoySetButton(BUTTON1, BUTTON_UP);
					}))
					{
						unresponsive = true;
						break;
					}
				}
			}

			if (unresponsive)
			{
				response.Unresponsive();
				return;
			}

			std::string summary = StrFormat("Pressed %u keys.", static_cast<unsigned int>(actions.size()));
			if (overruns)
				summary += StrFormat(" The program did not read the keyboard in time for %u of them, so those may be lost.",
					static_cast<unsigned int>(overruns));

			response.Text(summary);
		}

		void ToolWait(const Json& args, ToolResponse& response)
		{
			double seconds = args["seconds"].AsNumber(kWaitSecondsDefault);
			if (seconds < 0)
				seconds = 0;
			if (seconds > kWaitSecondsMax)
				seconds = kWaitSecondsMax;

			const Clock::time_point deadline = Clock::now() + std::chrono::milliseconds(static_cast<long long>(seconds * 1000));
			while (Clock::now() < deadline && !IsStopping())
				std::this_thread::sleep_for(std::chrono::milliseconds(kWaitPollMs));

			response.Text(StrFormat("Waited %.2f seconds.", seconds));
		}

		void ToolWaitForText(const Json& args, ToolResponse& response)
		{
			if (!args["text"].IsString() || args["text"].AsString().empty())
			{
				response.Error("text is required.");
				return;
			}

			const std::string wanted = args["text"].AsString();
			const bool ignoreCase = args["ignore_case"].AsBool(true);
			double timeoutSeconds = args["timeout_seconds"].AsNumber(kWaitForTextTimeoutSecondsDefault);
			if (timeoutSeconds < 0)
				timeoutSeconds = 0;
			if (timeoutSeconds > kWaitForTextTimeoutSecondsMax)
				timeoutSeconds = kWaitForTextTimeoutSecondsMax;

			const std::string needle = ignoreCase ? ToLower(wanted) : wanted;
			const Clock::time_point start = Clock::now();
			const Clock::time_point deadline = start + std::chrono::milliseconds(static_cast<long long>(timeoutSeconds * 1000));

			std::string screen;
			while (!IsStopping())
			{
				if (!RunOnEmulatorThread([&] { screen = GetTextScreen(); }))
				{
					response.Unresponsive();
					return;
				}

				const std::string haystack = ignoreCase ? ToLower(screen) : screen;
				if (haystack.find(needle) != std::string::npos)
				{
					const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
					response.Text(StrFormat("Found \"%s\" after %.2f seconds.\n\n", wanted.c_str(), elapsed) + screen);
					return;
				}

				if (Clock::now() >= deadline)
					break;

				std::this_thread::sleep_for(std::chrono::milliseconds(kWaitForTextPollMs));
			}

			response.Error(StrFormat("\"%s\" did not appear within %.1f seconds. The screen is:\n\n", wanted.c_str(), timeoutSeconds) + screen);
		}

		void ToolReadMemory(const Json& args, ToolResponse& response)
		{
			long long address = 0;
			if (!ParseNumber(args["address"], address) || address < 0 || address > 0xFFFF)
			{
				response.Error("address is required. Give a number, or a hexadecimal string such as \"$0400\".");
				return;
			}

			long long length = args["length"].IsNull() ? kReadMemoryLengthDefault : 0;
			if (!args["length"].IsNull() && (!ParseNumber(args["length"], length) || length < 1 || length > 0x10000))
			{
				response.Error("length must be between 1 and 65536.");
				return;
			}

			Bank_e bank;
			if (!ParseBank(args["bank"], bank))
			{
				response.Error("bank must be \"cpu\", \"main\" or \"aux\".");
				return;
			}

			const std::string format = ToLower(args["format"].AsString("hex_dump"));
			if (format != "hex_dump" && format != "hex" && format != "base64")
			{
				response.Error("format must be \"hex_dump\", \"hex\" or \"base64\".");
				return;
			}

			std::vector<uint8_t> data(static_cast<size_t>(length));

			const bool ok = RunOnEmulatorThread([&]
			{
				for (size_t i = 0; i < data.size(); i++)
					data[i] = ReadBankByte(bank, static_cast<uint16_t>((address + i) & 0xFFFF));
			});

			if (!ok)
			{
				response.Unresponsive();
				return;
			}

			if (format == "base64")
			{
				response.Text(EncodeBase64(&data[0], data.size()));
			}
			else if (format == "hex")
			{
				std::string hex;
				hex.reserve(data.size() * 2);
				for (size_t i = 0; i < data.size(); i++)
					hex += StrFormat("%02X", data[i]);
				response.Text(hex);
			}
			else
			{
				response.Text(HexDump(data, static_cast<unsigned int>(address)));
			}
		}

		void ToolWriteMemory(const Json& args, ToolResponse& response)
		{
			long long address = 0;
			if (!ParseNumber(args["address"], address) || address < 0 || address > 0xFFFF)
			{
				response.Error("address is required. Give a number, or a hexadecimal string such as \"$0300\".");
				return;
			}

			Bank_e bank;
			if (!ParseBank(args["bank"], bank))
			{
				response.Error("bank must be \"cpu\", \"main\" or \"aux\".");
				return;
			}

			std::vector<uint8_t> data;

			if (args["hex"].IsString())
			{
				const std::string hex = args["hex"].AsString();
				std::string digits;
				for (size_t i = 0; i < hex.size(); i++)
				{
					if (isxdigit(static_cast<uint8_t>(hex[i])))
						digits += hex[i];
					else if (hex[i] != ' ' && hex[i] != ',' && hex[i] != '$' && hex[i] != '\n')
					{
						response.Error(StrFormat("hex has an unexpected character '%c'.", hex[i]));
						return;
					}
				}

				if (digits.empty() || digits.size() % 2)
				{
					response.Error("hex must have an even number of hexadecimal digits.");
					return;
				}

				for (size_t i = 0; i < digits.size(); i += 2)
					data.push_back(static_cast<uint8_t>(strtol(digits.substr(i, 2).c_str(), NULL, 16)));
			}
			else if (args["base64"].IsString())
			{
				if (!DecodeBase64(args["base64"].AsString(), data))
				{
					response.Error("base64 is not valid.");
					return;
				}
			}
			else if (args["bytes"].IsArray())
			{
				for (size_t i = 0; i < args["bytes"].Size(); i++)
				{
					long long value = 0;
					if (!ParseNumber(args["bytes"].At(i), value) || value < 0 || value > 0xFF)
					{
						response.Error(StrFormat("bytes[%u] is not a byte value.", static_cast<unsigned int>(i)));
						return;
					}
					data.push_back(static_cast<uint8_t>(value));
				}
			}

			if (data.empty())
			{
				response.Error("Give the data as hex, base64 or bytes.");
				return;
			}

			if (address + static_cast<long long>(data.size()) > 0x10000)
			{
				response.Error("The data does not fit below $FFFF.");
				return;
			}

			const bool ok = RunOnEmulatorThread([&]
			{
				for (size_t i = 0; i < data.size(); i++)
					WriteBankByte(bank, static_cast<uint16_t>(address + i), data[i]);
			});

			if (!ok)
			{
				response.Unresponsive();
				return;
			}

			response.Text(StrFormat("Wrote %u bytes at $%04X.", static_cast<unsigned int>(data.size()), static_cast<unsigned int>(address)));
		}

		void ToolGetCpuState(const Json&, ToolResponse& response)
		{
			std::string text;

			const bool ok = RunOnEmulatorThread([&]
			{
				const regsrec& r = regs;

				text += StrFormat("PC=$%04X A=$%02X X=$%02X Y=$%02X SP=$%04X P=$%02X\n", r.pc, r.a, r.x, r.y, r.sp, r.ps);
				text += StrFormat("Flags: N=%d V=%d B=%d D=%d I=%d Z=%d C=%d\n",
					(r.ps & AF_SIGN) ? 1 : 0, (r.ps & AF_OVERFLOW) ? 1 : 0, (r.ps & AF_BREAK) ? 1 : 0,
					(r.ps & AF_DECIMAL) ? 1 : 0, (r.ps & AF_INTERRUPT) ? 1 : 0, (r.ps & AF_ZERO) ? 1 : 0,
					(r.ps & AF_CARRY) ? 1 : 0);
				text += StrFormat("Cycles: %llu\n", static_cast<unsigned long long>(g_nCumulativeCycles));
				text += StrFormat("Mode: %s\n", AppModeName(g_nAppMode));

				if (r.bJammed)
					text += "The CPU is jammed (it ran an invalid opcode).\n";

				std::vector<uint8_t> bytes(8);
				for (size_t i = 0; i < bytes.size(); i++)
					bytes[i] = ReadByteFromMemory(static_cast<uint16_t>((r.pc + i) & 0xFFFF));
				text += "Bytes at PC:\n" + HexDump(bytes, r.pc);
			});

			if (!ok)
			{
				response.Unresponsive();
				return;
			}

			response.Text(text);
		}

		void ToolSetSpeed(const Json& args, ToolResponse& response)
		{
			const std::string mode = ToLower(args["mode"].AsString("normal"));
			uint32_t speed = SPEED_NORMAL;

			if (mode == "maximum" || mode == "max")
			{
				speed = SPEED_MAX;
			}
			else if (mode == "custom")
			{
				const double multiplier = args["multiplier"].AsNumber(1.0);
				if (multiplier < kSpeedMultiplierMin || multiplier > kSpeedMultiplierMax)
				{
					response.Error(StrFormat("multiplier must be between %g and %g. Use mode \"maximum\" for no limit.", kSpeedMultiplierMin, kSpeedMultiplierMax));
					return;
				}
				speed = static_cast<uint32_t>(multiplier * SPEED_NORMAL + 0.5);
			}
			else if (mode != "normal")
			{
				response.Error("mode must be \"normal\", \"maximum\" or \"custom\".");
				return;
			}

			const bool ok = RunOnEmulatorThread([&]
			{
				g_dwSpeed = speed;
				SetCurrentCLK6502();
			});

			if (!ok)
			{
				response.Unresponsive();
				return;
			}

			if (speed == SPEED_MAX)
				response.Text("Speed set to maximum. Sound is off at this speed. Set the speed back to normal before the next keyboard input.");
			else
				response.Text(StrFormat("Speed set to %.1fx of 1 MHz.", speed / 10.0));
		}

		void ToolSetRunState(const Json& args, ToolResponse& response)
		{
			const std::string state = ToLower(args["state"].AsString());
			if (state != "running" && state != "paused")
			{
				response.Error("state must be \"running\" or \"paused\".");
				return;
			}

			const bool pause = (state == "paused");
			std::string result;

			const bool ok = RunOnEmulatorThread([&]
			{
				if (pause)
				{
					if (g_nAppMode == MODE_RUNNING)
					{
						g_nAppMode = MODE_PAUSED;
						SoundCore_SetFade(FADE_OUT);
						result = "The machine is paused.";
					}
					else if (g_nAppMode == MODE_STEPPING)
					{
						DebugStopStepping();
						result = "Stepping stopped. The debugger is open.";
					}
					else
					{
						result = StrFormat("The machine is in the mode \"%s\". No change.", AppModeName(g_nAppMode));
					}
				}
				else
				{
					if (g_nAppMode == MODE_PAUSED)
					{
						g_nAppMode = MODE_RUNNING;
						SoundCore_SetFade(FADE_IN);
						result = "The machine is running.";
					}
					else if (g_nAppMode == MODE_DEBUG)
					{
						DebugExitDebugger();
						result = "The debugger is closed. The machine is running.";
					}
					else if (g_nAppMode == MODE_LOGO)
					{
						ResetMachineState();
						g_nAppMode = MODE_RUNNING;
						result = "The machine is powered on and running.";
					}
					else
					{
						result = StrFormat("The machine is in the mode \"%s\". No change.", AppModeName(g_nAppMode));
					}
				}

				GetFrame().FrameRefreshStatus(DRAW_TITLE);
			});

			if (!ok)
			{
				response.Unresponsive();
				return;
			}

			response.Text(result);
		}

		void ToolReset(const Json& args, ToolResponse& response)
		{
			const std::string type = ToLower(args["type"].AsString("warm"));
			if (type != "warm" && type != "cold")
			{
				response.Error("type must be \"warm\" (Ctrl+Reset) or \"cold\" (power cycle and boot).");
				return;
			}

			const bool cold = (type == "cold");

			const bool ok = RunOnEmulatorThread([&]
			{
				if (cold)
				{
					ResetMachineState();
					g_nAppMode = MODE_RUNNING;
					SoundCore_SetFade(FADE_IN);
				}
				else
				{
					if (g_nAppMode == MODE_DEBUG)
						DebugExitDebugger();
					else if (g_nAppMode == MODE_LOGO || g_nAppMode == MODE_PAUSED)
						g_nAppMode = MODE_RUNNING;

					CtrlReset();
				}

				GetFrame().FrameRefreshStatus(DRAW_TITLE | DRAW_LEDS | DRAW_DISK_STATUS);
			});

			if (!ok)
			{
				response.Unresponsive();
				return;
			}

			response.Text(cold ? "Cold reset done. The machine boots from slot 6 drive 1." : "Warm reset done (Ctrl+Reset).");
		}

		const char* ImageErrorText(ImageError_e error)
		{
			switch (error)
			{
			case eIMAGE_ERROR_NONE:                        return "no error";
			case eIMAGE_ERROR_BAD_POINTER:                 return "internal error (bad pointer)";
			case eIMAGE_ERROR_BAD_SIZE:                    return "the file size is not a known disk image size";
			case eIMAGE_ERROR_BAD_FILE:                    return "the file is not a known disk image format";
			case eIMAGE_ERROR_UNSUPPORTED:                 return "the image format is not supported";
			case eIMAGE_ERROR_UNSUPPORTED_HDV:             return "a hard disk image cannot go into a floppy drive";
			case eIMAGE_ERROR_GZ:                          return "the gzip file is not valid";
			case eIMAGE_ERROR_ZIP:                         return "the zip file is not valid";
			case eIMAGE_ERROR_REJECTED_MULTI_ZIP:          return "the zip file holds more than one file";
			case eIMAGE_ERROR_UNABLE_TO_OPEN:              return "the file cannot be opened (does it exist, and is it in use by another program?)";
			case eIMAGE_ERROR_UNABLE_TO_OPEN_GZ:           return "the gzip file cannot be opened";
			case eIMAGE_ERROR_UNABLE_TO_OPEN_ZIP:          return "the zip file cannot be opened";
			case eIMAGE_ERROR_FAILED_TO_GET_PATHNAME:      return "the path cannot be resolved";
			case eIMAGE_ERROR_ZEROLENGTH_WRITEPROTECTED:   return "the file is empty and write protected";
			case eIMAGE_ERROR_FAILED_TO_INIT_ZEROLENGTH:   return "the empty file cannot be initialised";
			default:                                       return "unknown error";
			}
		}

		void ToolInsertDisk(const Json& args, ToolResponse& response)
		{
			const std::string path = args["path"].AsString();
			if (path.empty())
			{
				response.Error("path is required. Give the full path of a .dsk, .do, .po, .nib or .woz file.");
				return;
			}

			const long long drive = args["drive"].AsInteger(1);
			if (drive != 1 && drive != 2)
			{
				response.Error("drive must be 1 or 2.");
				return;
			}

			const long long slot = args["slot"].AsInteger(6);
			if (slot < 1 || slot >= NUM_SLOTS)
			{
				response.Error("slot must be between 1 and 7.");
				return;
			}

			const bool writeProtected = args["write_protected"].AsBool(false);

			ImageError_e error = eIMAGE_ERROR_NONE;
			bool hasCard = false;
			std::string name;

			const bool ok = RunOnEmulatorThread([&]
			{
				Disk2InterfaceCard* card = GetDiskCard(static_cast<UINT>(slot));
				if (!card)
					return;

				hasCard = true;
				error = card->InsertDisk(static_cast<int>(drive - 1), path, writeProtected, IMAGE_DONT_CREATE);
				if (error == eIMAGE_ERROR_NONE)
				{
					name = card->GetFullDiskFilename(static_cast<int>(drive - 1));
					GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);
				}
			});

			if (!ok)
			{
				response.Unresponsive();
				return;
			}

			if (!hasCard)
			{
				response.Error(StrFormat("Slot %lld has no Disk II card.", slot));
				return;
			}

			if (error != eIMAGE_ERROR_NONE)
			{
				response.Error(StrFormat("Cannot insert \"%s\": %s.", path.c_str(), ImageErrorText(error)));
				return;
			}

			response.Text(StrFormat("Inserted \"%s\" into slot %lld drive %lld. The machine did not reset. Use reset with type \"cold\" to boot it.",
				name.c_str(), slot, drive));
		}

		void ToolEjectDisk(const Json& args, ToolResponse& response)
		{
			const long long drive = args["drive"].AsInteger(1);
			if (drive != 1 && drive != 2)
			{
				response.Error("drive must be 1 or 2.");
				return;
			}

			const long long slot = args["slot"].AsInteger(6);
			if (slot < 1 || slot >= NUM_SLOTS)
			{
				response.Error("slot must be between 1 and 7.");
				return;
			}

			bool hasCard = false;

			const bool ok = RunOnEmulatorThread([&]
			{
				Disk2InterfaceCard* card = GetDiskCard(static_cast<UINT>(slot));
				if (!card)
					return;

				hasCard = true;
				card->EjectDisk(static_cast<int>(drive - 1));
				GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);
			});

			if (!ok)
			{
				response.Unresponsive();
				return;
			}

			if (!hasCard)
			{
				response.Error(StrFormat("Slot %lld has no Disk II card.", slot));
				return;
			}

			response.Text(StrFormat("Ejected the disk from slot %lld drive %lld.", slot, drive));
		}

		// This function splits a full path into the directory and the file name,
		// in the form that Snapshot_SetFilename expects.
		void SplitPath(const std::string& fullPath, std::string& filename, std::string& directory)
		{
			const size_t slash = fullPath.find_last_of("\\/");
			if (slash == std::string::npos)
			{
				filename = fullPath;
				directory.clear();
			}
			else
			{
				filename = fullPath.substr(slash + 1);
				directory = fullPath.substr(0, slash + 1);
			}
		}

		void ToolSaveState(const Json& args, ToolResponse& response)
		{
			std::string path = args["path"].AsString();
			std::string result;

			const bool ok = RunOnEmulatorThread([&]
			{
				if (!path.empty())
				{
					std::string filename, directory;
					SplitPath(path, filename, directory);
					Snapshot_SetFilename(filename, directory);
				}

				Snapshot_SaveState();
				result = Snapshot_GetPathname();
			});

			if (!ok)
			{
				response.Unresponsive();
				return;
			}

			const DWORD attributes = GetFileAttributes(result.c_str());
			if (attributes == INVALID_FILE_ATTRIBUTES)
			{
				response.Error(StrFormat("AppleWin did not write \"%s\". Is the directory writable?", result.c_str()));
				return;
			}

			response.Text(StrFormat("Saved the machine state to \"%s\".", result.c_str()));
		}

		void ToolLoadState(const Json& args, ToolResponse& response)
		{
			const std::string path = args["path"].AsString();
			if (path.empty())
			{
				response.Error("path is required. Give the full path of a .aws.yaml file.");
				return;
			}

			if (GetFileAttributes(path.c_str()) == INVALID_FILE_ATTRIBUTES)
			{
				response.Error(StrFormat("The file \"%s\" does not exist.", path.c_str()));
				return;
			}

			std::string result;

			const bool ok = RunOnEmulatorThread([&]
			{
				std::string filename, directory;
				SplitPath(path, filename, directory);
				Snapshot_SetFilename(filename, directory);
				Snapshot_LoadState();

				// A load moves the save-state path next to the disk image, so
				// report the path that was given, not the path after the load.
				result = StrFormat("Loaded the machine state from \"%s\". Mode: %s.", path.c_str(), AppModeName(g_nAppMode));
			});

			if (!ok)
			{
				response.Unresponsive();
				return;
			}

			response.Text(result);
		}

		void ToolDebuggerCommand(const Json& args, ToolResponse& response)
		{
			const std::string command = args["command"].AsString();
			if (command.empty())
			{
				response.Error("command is required. Examples: \"u fdf0\", \"md1 400\", \"bpx 300\", \"help\".");
				return;
			}

			const bool close = args["close"].AsBool(true);
			std::string output;
			std::string modeAfter;

			const bool ok = RunOnEmulatorThread([&]
			{
				if (g_nAppMode != MODE_DEBUG)
				{
					if (g_nAppMode == MODE_LOGO)
						ResetMachineState();
					if (g_nAppMode == MODE_STEPPING)
						DebugStopStepping();
					else
						DebugBegin();
				}

				// A paused console ignores input. Escape ends the pause.
				if (g_bConsoleBufferPaused)
					DebuggerProcessKey(VK_ESCAPE);

				// Remove the lines that other commands left in the buffer. Then
				// the buffer holds only the output of this command.
				while (g_nConsoleBuffer > 0)
					ConsoleBufferToDisplay();

				for (size_t i = 0; i < command.size(); i++)
					DebuggerInputConsoleChar(command[i]);
				DebuggerProcessKey(VK_RETURN);

				while (g_nConsoleBuffer > 0)
				{
					const conchar_t* line = ConsoleBufferPeek();
					std::string text;
					for (int x = 0; x < CONSOLE_WIDTH && line[x]; x++)
						text += static_cast<char>(line[x] & _CONSOLE_COLOR_MASK);
					output += TrimRight(text) + '\n';

					ConsoleBufferToDisplay();
				}

				// Many commands write to the disassembly or the memory windows,
				// not to the console. For those, give the full debugger screen.
				if (output.empty() && g_nAppMode == MODE_DEBUG)
				{
					char* text = NULL;
					const size_t size = Util_GetDebuggerText(text);
					if (text && size)
					{
						output = "The command wrote to the debugger windows. The debugger screen (80x43) is:\n\n";
						for (size_t i = 0; i < size; i++)
						{
							if (text[i] != '\r')
								output += text[i];
						}
					}
				}

				// A command such as "g" or "t" leaves the debugger by itself.
				if (close && g_nAppMode == MODE_DEBUG)
					DebugExitDebugger();

				modeAfter = AppModeName(g_nAppMode);
			}, kDebuggerCommandTimeoutMs);

			if (!ok)
			{
				response.Unresponsive();
				return;
			}

			if (output.empty())
				output = "(the command produced no output)\n";

			response.Text(output + "\nMode after the command: " + modeAfter);
		}

		//=======================================================================
		// The tool table

		const ToolDefinition g_tools[] =
		{
			{
				"get_status", "Machine status",
				"Report the emulator state: mode, machine model, speed, video mode, disks in each drive, and the CPU registers.",
				"{\"type\":\"object\",\"properties\":{}}",
				ToolGetStatus
			},
			{
				"read_screen", "Read the text screen",
				"Read the Apple II text screen as 24 lines of 40 or 80 characters. This gives exact characters and is cheaper than a screenshot. "
				"Programs that draw in a graphics mode put nothing here; use screenshot for those. When the debugger is open, this gives the debugger screen.",
				"{\"type\":\"object\",\"properties\":{}}",
				ToolReadScreen
			},
			{
				"screenshot", "Screenshot",
				"Capture the video output as a PNG image. Use this for graphics modes, or to see colours and the exact rendering.",
				"{\"type\":\"object\",\"properties\":{"
				"\"size\":{\"type\":\"string\",\"enum\":[\"560x384\",\"280x192\"],\"default\":\"560x384\",\"description\":\"Image size. 280x192 is one quarter of the data and enough for most screens.\"}"
				"}}",
				ToolScreenshot
			},
			{
				"type_text", "Type text",
				"Type ASCII text on the Apple II keyboard. Each key waits until the program has read the previous one, so text is not lost while the program keeps up. "
				"A newline in the text presses Return. Letters arrive as typed; most Apple II software expects upper case.",
				StrFormat(
				"{\"type\":\"object\",\"required\":[\"text\"],\"properties\":{"
				"\"text\":{\"type\":\"string\",\"description\":\"The text to type. Use \\\\n for Return.\"},"
				"\"submit\":{\"type\":\"boolean\",\"default\":false,\"description\":\"Press Return after the text.\"},"
				"\"key_wait_ms\":{\"type\":\"integer\",\"default\":%u,\"description\":\"How long to wait for the program to read each key before sending the next one anyway.\"}"
				"}}", kKeyWaitMsDefault),
				ToolTypeText
			},
			{
				"press_keys", "Press keys",
				"Press named keys in sequence. A single character presses that key. Names: return, escape, tab, space, backspace, delete, left, right, up, down. "
				"Prefixes: ctrl- (for example ctrl-c), open-apple- and closed-apple- (held as the Apple buttons around the key). "
				"Up and down produce no key code on an Apple ][ or ][+.",
				StrFormat(
				"{\"type\":\"object\",\"required\":[\"keys\"],\"properties\":{"
				"\"keys\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"description\":\"Key names in the order to press them.\"},"
				"\"key_wait_ms\":{\"type\":\"integer\",\"default\":%u,\"description\":\"How long to wait for the program to read each key before sending the next one anyway.\"},"
				"\"hold_ms\":{\"type\":\"integer\",\"default\":%u,\"description\":\"How long the Apple buttons and the any-key-down flag stay pressed.\"}"
				"}}", kKeyWaitMsDefault, kKeyHoldMsDefault),
				ToolPressKeys
			},
			{
				"wait", "Wait",
				"Let the machine run for a number of seconds. Use this after a boot or a command that takes time.",
				StrFormat(
				"{\"type\":\"object\",\"properties\":{"
				"\"seconds\":{\"type\":\"number\",\"default\":%g,\"minimum\":0,\"maximum\":%g}"
				"}}", kWaitSecondsDefault, kWaitSecondsMax),
				ToolWait
			},
			{
				"wait_for_text", "Wait for text",
				"Poll the text screen until the given text appears, then return the whole screen. Use this to find the moment when a program is ready for input.",
				StrFormat(
				"{\"type\":\"object\",\"required\":[\"text\"],\"properties\":{"
				"\"text\":{\"type\":\"string\",\"description\":\"The text to wait for.\"},"
				"\"timeout_seconds\":{\"type\":\"number\",\"default\":%g,\"maximum\":%g},"
				"\"ignore_case\":{\"type\":\"boolean\",\"default\":true}"
				"}}", kWaitForTextTimeoutSecondsDefault, kWaitForTextTimeoutSecondsMax),
				ToolWaitForText
			},
			{
				"read_memory", "Read memory",
				"Read bytes from the machine while it runs. Bank \"cpu\" is what the 6502 sees at this moment, with the language card and ROM as switched in. "
				"Bank \"main\" and \"aux\" are the raw 64K banks.",
				"{\"type\":\"object\",\"required\":[\"address\"],\"properties\":{"
				"\"address\":{\"description\":\"Start address. A number is decimal; a string is hexadecimal, for example \\\"$0400\\\" or \\\"2000\\\".\"},"
				"\"length\":{\"description\":\"Number of bytes, 1 to 65536. Default 256.\"},"
				"\"bank\":{\"type\":\"string\",\"enum\":[\"cpu\",\"main\",\"aux\"],\"default\":\"cpu\"},"
				"\"format\":{\"type\":\"string\",\"enum\":[\"hex_dump\",\"hex\",\"base64\"],\"default\":\"hex_dump\"}"
				"}}",
				ToolReadMemory
			},
			{
				"write_memory", "Write memory",
				"Write bytes into the machine while it runs. Give the data as a hex string, a base64 string, or an array of byte values. "
				"With bank \"cpu\" a write to ROM has no effect.",
				"{\"type\":\"object\",\"required\":[\"address\"],\"properties\":{"
				"\"address\":{\"description\":\"Start address. A number is decimal; a string is hexadecimal, for example \\\"$0300\\\".\"},"
				"\"hex\":{\"type\":\"string\",\"description\":\"Bytes as hexadecimal digits. Spaces are permitted.\"},"
				"\"base64\":{\"type\":\"string\"},"
				"\"bytes\":{\"type\":\"array\",\"items\":{\"type\":\"integer\"}},"
				"\"bank\":{\"type\":\"string\",\"enum\":[\"cpu\",\"main\",\"aux\"],\"default\":\"cpu\"}"
				"}}",
				ToolWriteMemory
			},
			{
				"get_cpu_state", "CPU registers",
				"Report the 6502 registers, the flags, the cycle count and the bytes at the program counter. The machine keeps running.",
				"{\"type\":\"object\",\"properties\":{}}",
				ToolGetCpuState
			},
			{
				"set_speed", "Set the CPU speed",
				"Set the emulation speed. Use \"maximum\" to skip through a long boot or a slow program, then return to \"normal\" before keyboard input.",
				StrFormat(
				"{\"type\":\"object\",\"properties\":{"
				"\"mode\":{\"type\":\"string\",\"enum\":[\"normal\",\"maximum\",\"custom\"],\"default\":\"normal\"},"
				"\"multiplier\":{\"type\":\"number\",\"minimum\":%g,\"maximum\":%g,\"description\":\"For mode custom: the speed as a multiple of 1 MHz.\"}"
				"}}", kSpeedMultiplierMin, kSpeedMultiplierMax),
				ToolSetSpeed
			},
			{
				"set_run_state", "Pause or run",
				"Pause the machine or let it run. \"running\" also powers on a machine at the logo screen and closes the debugger.",
				"{\"type\":\"object\",\"required\":[\"state\"],\"properties\":{"
				"\"state\":{\"type\":\"string\",\"enum\":[\"running\",\"paused\"]}"
				"}}",
				ToolSetRunState
			},
			{
				"reset", "Reset",
				"Reset the machine. \"warm\" is Ctrl+Reset, which stops a program and keeps memory. \"cold\" is a power cycle that boots from slot 6 drive 1.",
				"{\"type\":\"object\",\"properties\":{"
				"\"type\":{\"type\":\"string\",\"enum\":[\"warm\",\"cold\"],\"default\":\"warm\"}"
				"}}",
				ToolReset
			},
			{
				"insert_disk", "Insert a disk",
				"Put a disk image into a drive while the machine runs. The machine does not reset. Use reset with type \"cold\" to boot the new disk.",
				"{\"type\":\"object\",\"required\":[\"path\"],\"properties\":{"
				"\"path\":{\"type\":\"string\",\"description\":\"Full path of the disk image (.dsk, .do, .po, .nib, .woz, or a .zip or .gz of one).\"},"
				"\"drive\":{\"type\":\"integer\",\"enum\":[1,2],\"default\":1},"
				"\"slot\":{\"type\":\"integer\",\"default\":6,\"description\":\"The slot of the Disk II card.\"},"
				"\"write_protected\":{\"type\":\"boolean\",\"default\":false}"
				"}}",
				ToolInsertDisk
			},
			{
				"eject_disk", "Eject a disk",
				"Take the disk image out of a drive.",
				"{\"type\":\"object\",\"properties\":{"
				"\"drive\":{\"type\":\"integer\",\"enum\":[1,2],\"default\":1},"
				"\"slot\":{\"type\":\"integer\",\"default\":6}"
				"}}",
				ToolEjectDisk
			},
			{
				"save_state", "Save the machine state",
				"Write the whole machine (memory, CPU, cards, disk positions) to a .aws.yaml file. load_state restores it later.",
				"{\"type\":\"object\",\"properties\":{"
				"\"path\":{\"type\":\"string\",\"description\":\"Full path of the file to write. Default: the save-state path that AppleWin has configured.\"}"
				"}}",
				ToolSaveState
			},
			{
				"load_state", "Load a machine state",
				"Restore the whole machine from a .aws.yaml file written by save_state.",
				"{\"type\":\"object\",\"required\":[\"path\"],\"properties\":{"
				"\"path\":{\"type\":\"string\",\"description\":\"Full path of the .aws.yaml file.\"}"
				"}}",
				ToolLoadState
			},
			{
				"debugger_command", "Debugger command",
				"Run a command in the AppleWin 6502 debugger. The result is the console output, or the whole debugger screen when the command "
				"wrote to the disassembly or memory windows instead. The CPU stops while the debugger is open. "
				"Examples: \"u fdf0\" (disassemble), \"md1 400\" (memory window), \"bpx 300\" (breakpoint), \"bpl\" (list breakpoints), \"help\". "
				"For the registers use get_cpu_state. By default the debugger closes again and the machine resumes.",
				"{\"type\":\"object\",\"required\":[\"command\"],\"properties\":{"
				"\"command\":{\"type\":\"string\"},"
				"\"close\":{\"type\":\"boolean\",\"default\":true,\"description\":\"Close the debugger after the command and let the machine run.\"}"
				"}}",
				ToolDebuggerCommand
			},
		};
	}

	const ToolDefinition* GetToolTable(size_t& count)
	{
		count = sizeof(g_tools) / sizeof(g_tools[0]);
		return g_tools;
	}

	const char* GetServerInstructions()
	{
		return
			"This server controls a running AppleWin Apple II emulator from inside the emulator process.\n"
			"\n"
			"Start with get_status to see the machine state. Use read_screen to read text and screenshot to see graphics. "
			"Use type_text for typing and press_keys for special keys, arrows and control combinations. "
			"A program reads the keyboard one key at a time, so wait_for_text is the reliable way to find the moment when it is ready for input.\n"
			"\n"
			"Addresses and byte values accept a decimal number or a hexadecimal string such as \"$0400\". "
			"read_memory and write_memory work while the machine runs. debugger_command gives the full AppleWin debugger; it stops the CPU during the command.\n"
			"\n"
			"set_speed with mode \"maximum\" makes a boot or a slow program finish quickly. Return to \"normal\" before keyboard input, because a program at maximum speed reads the keyboard faster than keys can be paced.";
	}
}
