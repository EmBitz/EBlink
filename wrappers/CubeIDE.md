# EBlink in STM32CubeIDE

EBlink can be used as a seamless drop-in replacement for the built-in GDB servers in STM32CubeIDE (OpenOCD, ST-LINK GDB server, J-Link GDB server). CubeIDE will auto-launch EBlink at the start of each debug session and close it afterwards — no manual startup required.

## Installation

Run **eblink-cube.exe** as administrator. The tool searches the registry for STM32CubeIDE installations. If none are found, enter the CubeIDE root path manually (e.g. `C:\ST\STM32CubeIDE_1.16.0`).

```
=== EBlink CubeIDE wrapper installer ===
Architecture: 64-bit
No STM32CubeIDE installation found in registry.
Enter path manually: C:\ST\STM32CubeIDE_1.16.0

What do you want to do?
  [1] Install (replace with EBlink wrapper)
  [2] Uninstall (restore originals)
  Choice: 1

Install which server(s)?
  [1] OpenOCD
  [2] ST-LINK GDB
  [3] J-Link GDB
  [A] All
  Choice: A
```

What the installer does:
- The original executable (e.g. `openocd.exe`) is **renamed to `openocd.exe.wrapped`** — it stays in place as a backup
- The EBlink wrapper is written in its place under the same original name
- CubeIDE keeps working exactly as before, but now launches EBlink instead

To uninstall, run eblink-cube.exe again and select option 2. The `.wrapped` files are renamed back to their original names and the wrapper is removed.

The wrappers find `eblink.exe` automatically when EBlink was installed with the standard installer — the install path is read from the registry (`HKCU\SOFTWARE\EBlink\Path`). No PATH change is needed in that case. As a fallback, placing `eblink.exe` next to the wrapper also works, as does adding the EBlink directory to the system PATH.

## What changes in CubeIDE

Nothing. Select OpenOCD (or ST-LINK / J-Link) in your debug configuration as usual. The wrapper intercepts the launch, translates the arguments to EBlink format, and starts EBlink transparently.

## Debug Console

The CubeIDE Debugger Console shows EBlink output directly, including:

- Interface and target detection info
- Flash operations
- Fault unwind information in case of a CPU exception

## Fault Unwind

When the target hits a hard fault or bus fault, EBlink halts execution and displays a full exception analysis:

```
Target  Exception!

    [LR]:    0x080009D9
    [PC]:    0x080009E1

Additional Info:
    Bus fault(s):
      - Return address caused the fault
      - Fault Address: 0xE000EDF8
```

This appears both in the Debugger Console and as a popup message box on Windows.

## Troubleshooting

**"Wrong network parameter" / port bind failure**
Windows (with Hyper-V, WSL2, or Docker installed) reserves dynamic port ranges that can include the GDB server default ports. Change the GDB server port in the CubeIDE debug configuration to a port outside the excluded range. To list excluded ranges:
```
netsh int ipv4 show excludedportrange protocol=tcp
```

## Why using EBlink

- Incremental flashing — only changed sectors are written, reducing flash wear and cutting download time
- Faster flashing — significantly faster than OpenOCD for typical firmware sizes
- Fault unwind — on a hard fault or bus fault, EBlink shows a full exception analysis with stack unwind in the CubeIDE console and as a popup
- Lightweight — single small executable, no scripts or config files needed


For full EBlink documentation, including all command-line options and scripting, refer to the EBlink User Manual.

> **Legal Notice:** **STM32CubeIDE** and **ST-LINK_gdbserver** are properties of STMicroelectronics. **JLinkGDBServerCL** is a property of SEGGER Microcontroller GmbH. These wrappers are independent compatibility layers and are not affiliated with, authorized, or endorsed by these companies.
