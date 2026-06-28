# EBlink GDB server wrappers

> **Disclaimer:** This software is provided "as is", without warranty of any kind. The author is not liable for any damage, data loss, or hardware failure resulting from its use. Use at your own risk.

These are drop-in replacements for common GDB servers. When placed in PATH before the original tools, IDEs that auto-start a GDB server will transparently use EBlink instead.

EBlink must be installed and reachable via PATH or placed in the same directory as the wrappers.

## Available platforms

| Folder          | Platform                  |
|-----------------|---------------------------|
| `win32/`        | Windows 32-bit            |
| `win64/`        | Windows 64-bit            |
| `linux_amd64/`  | Linux x86-64              |
| `linux_armhf/`  | Linux ARM 32-bit (Pi etc) |
| `linux_aarch64/`| Linux ARM 64-bit          |

## Installation

### Windows
The wrappers are included in the EBlink installer. They are placed in the EBlink installation folder.

To activate, make sure the EBlink folder appears in PATH **before** any other GDB server installation.

### Linux
Download the binaries for your platform and place them in a directory that appears in PATH **before** `/usr/bin`, for example `/usr/local/bin`:

```
sudo cp linux_amd64/* /usr/local/bin/
sudo chmod +x /usr/local/bin/openocd /usr/local/bin/ST-LINK_gdbserver
```

## How it works

Each wrapper accepts the command-line arguments of the tool it replaces, extracts the relevant parameters (port, speed, interface, serial number) and launches EBlink with the equivalent options. The wrapper replaces itself with the EBlink process, so the IDE sees EBlink directly by PID.

## Supported arguments

The wrappers handle the most common IDE-generated arguments. Unknown arguments are silently ignored.


> **Legal Notice:** **JLinkGDBServerCL** and **ST-LINK_gdbserver** are properties of their respective owners (SEGGER Microcontroller GmbH and STMicroelectronics). This wrapper is an independent compatibility layer and is not affiliated with, authorized, or endorsed by these companies.
