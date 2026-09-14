# The MCP server in AppleWin

This directory adds a [Model Context Protocol](https://modelcontextprotocol.io)
server to AppleWin. When AppleWin starts with `-mcp`, it listens on
`http://127.0.0.1:6502/mcp`. An MCP client such as Claude Code can then read the
screen, type on the keyboard, read and write memory, change disks, save and
load the machine state, and run debugger commands, all from inside the
emulator process.

## Use

Build AppleWin as usual (`AppleWin-VS2022.sln`, Release, x64). Then start it
with the `-mcp` switch:

```
AppleWin-x64.exe -mcp
```

AppleWin opens at its logo screen. The client can then put a disk in the drive
with `insert_disk` and boot it with `reset` (type `cold`). To boot a disk at
startup instead, add `-d1`:

```
AppleWin-x64.exe -mcp -d1 "Ultima I - The Beginning (4am crack).dsk"
```

`-mcp=7000` selects a different port. All the other AppleWin switches work as
before.

Register the server with Claude Code one time. Run this command in the project
directory where you want to use it:

```
claude mcp add --transport http --scope local applewin http://127.0.0.1:6502/mcp
```

`--scope local` is the default. It stores the entry in `~/.claude.json` under
the key for the current directory. The server is then available only when
Claude Code starts in that directory or in a subdirectory.

| Scope | Stored in | Visible to |
| --- | --- | --- |
| `local` (default) | `~/.claude.json`, keyed by project path | You, in this project only |
| `project` | `.mcp.json` in the project root | Each person who clones the repository |
| `user` | `~/.claude.json`, top level | You, in each project |

`--scope project` writes this `.mcp.json` file, which you can commit:

```json
{
  "mcpServers": {
    "applewin": {
      "type": "http",
      "url": "http://127.0.0.1:6502/mcp"
    }
  }
}
```

Claude Code asks for approval the first time it loads a project-scope server.

`claude mcp list` shows the entry as connected only while AppleWin runs with
`-mcp`. Claude Code connects when a conversation starts. If AppleWin is not
running at that moment, start AppleWin and type `/mcp` to reconnect.

To remove the entry, run `claude mcp remove applewin`. Add `--scope user` or
`--scope project` if you added the entry with that scope.

## Tools

| Tool | Function |
| --- | --- |
| `get_status` | Mode, machine model, speed, video mode, the disk in each drive, and the CPU registers |
| `read_screen` | The text screen as 24 lines of 40 or 80 characters |
| `screenshot` | The video output as a PNG image, 560x384 or 280x192 |
| `type_text` | Type ASCII text. Each key waits for the program to read the key before it |
| `press_keys` | Press named keys: arrows, `return`, `escape`, `ctrl-c`, `open-apple-r` |
| `wait` | Let the machine run for a number of seconds |
| `wait_for_text` | Poll the text screen until a string appears |
| `read_memory` | Read bytes from the CPU view, the main bank or the auxiliary bank |
| `write_memory` | Write bytes as hex, base64 or an array |
| `get_cpu_state` | The 6502 registers, the flags and the cycle count |
| `set_speed` | 1 MHz, a multiple of 1 MHz, or maximum speed |
| `set_run_state` | Pause the machine or let it run |
| `reset` | Ctrl+Reset, or a power cycle that boots drive 1 |
| `insert_disk` / `eject_disk` | Change the disk in a drive while the machine runs |
| `save_state` / `load_state` | Write or read a `.aws.yaml` file |
| `debugger_command` | Run a command in the AppleWin debugger and return its output |

Addresses and byte values accept a decimal number or a hexadecimal string such
as `"$0400"`.

## Design

The server is a module in `source/MCP`. AppleWin calls three functions from it:

- `MCP_ParseCmdLineArg` reads the `-mcp` switch. `CmdLine.cpp` calls it in its
  chain of `else if` tests, in the position before the "unsupported argument"
  branch.
- `MCP_Initialize` starts the server. `WinMain` calls it after
  `RepeatInitialization`, when the frame window and the machine exist.
- `MCP_Destroy` stops the server. `Shutdown` calls it.

Those calls, with their two `#include` lines, are the only changes to existing
AppleWin source files. They add 8 lines and change none. To remove the feature,
delete this directory, the 8 lines, and the entries in the project file.

### Threads

The HTTP server runs on its own threads. The machine runs on the AppleWin main
thread, and the emulator code is not safe to call from a different thread. The
server solves this with a message-only window.

`MCP_Initialize` runs on the main thread. It creates a hidden window with
`HWND_MESSAGE` as the parent. The window belongs to the main thread. The
AppleWin message loop calls `PeekMessage` with a null window handle, and such a
call returns the messages of every window on the thread. The loop calls
`DispatchMessage` for each message between two calls to `ContinueExecution`.
Therefore the window procedure of the hidden window runs on the main thread at
a moment when the CPU is between instructions.

A tool that must touch the machine calls `RunOnEmulatorThread` with a lambda.
The function allocates a `Call` record, posts a message with a pointer to the
record, and waits on a condition variable. The window procedure runs the lambda
and signals the condition variable. If the main thread does not run the lambda
before the timeout, the caller sets the `finished` flag in the record. The
window procedure tests that flag before it runs the lambda, and the two sides
share one mutex for the test and the run. Thus a lambda never runs after the
stack frame of its caller is gone.

A tool that waits, such as `wait_for_text` or `type_text`, waits on the socket
thread. It sends short lambdas to the main thread in a loop and sleeps between
them. The machine continues to run during the wait.

### Keyboard pacing

The Apple II keyboard has one latch. A program reads the latch and then clears
the strobe. A key that arrives before the program clears the strobe replaces
the previous key, and the previous key is lost.

`type_text` and `press_keys` read the strobe with `KeybReadData` before each
key. If bit 7 is set, the program has not read the previous key, and the tool
waits. The tool sends the next key only when the strobe is clear. If the
program does not read the keyboard within `key_wait_ms`, the tool sends the key
anyway and reports the count of such keys in its result.

### Transport

The server speaks the MCP streamable HTTP transport. It accepts `POST` with a
JSON-RPC message and replies with `application/json`. It answers a notification
with `202 Accepted`. It refuses `GET`, because it never sends a message on its
own initiative. It binds to `127.0.0.1` only, and it refuses a request with an
`Origin` header from a different host.

### Files

| File | Content |
| --- | --- |
| `MCP.h` | The three functions that AppleWin calls |
| `MCPServer.cpp` | The server lifetime, the bridge to the main thread, and the JSON-RPC dispatch |
| `MCPServer.h` | The interface that a tool uses |
| `MCPTools.cpp`, `MCPTools.h` | The tools and their JSON schemas |
| `MCPHttp.cpp`, `MCPHttp.h` | The HTTP server, on winsock |
| `MCPJson.cpp`, `MCPJson.h` | A JSON parser and writer |
| `MCPEncode.cpp`, `MCPEncode.h` | The PNG and base64 encoders, on zlib |
| `MCPHelpers.cpp`, `MCPHelpers.h` | String helpers shared by the files above |

The module has no dependency outside AppleWin. It uses zlib, which AppleWin
already links, for the PNG output.
