# The MCP server in AppleWin

This directory adds a [Model Context Protocol](https://modelcontextprotocol.io)
server to AppleWin. When AppleWin starts with `-mcp`, it listens on
`http://127.0.0.1:6502/mcp`. An MCP client such as Claude Code can then read the
screen, type on the keyboard, read and write memory, change disks, save and
load the machine state, and run debugger commands, all from inside the
emulator process.

## Use

Activate the MCP server with the `-mcp` switch:

```
AppleWin-x64.exe -mcp
```

AppleWin will open at its logo screen. The client can then put a disk in the drive
with `insert_disk` and boot it with `reset` (type `cold`). To boot a disk at
startup instead, add `-d1`:

```
AppleWin-x64.exe -mcp -d1 "Ultima I - The Beginning (4am crack).dsk"
```

You can select a different MCP port with `-mcp=<port>`. All the other AppleWin
switches work as before.

## Instructions for use with Claude Code

For Claude Code, you can register the MCP server by running this command in the
project directory where you want to use it:

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

`--scope project` writes this `.mcp.json` file:

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
running at that moment, start AppleWin and type `/mcp` in Claude to reconnect.

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
