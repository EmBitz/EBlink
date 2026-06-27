# Using EBlink in STM32CubeIDE

EBlink can be used as a drop-in replacement for the GDB server in STM32CubeIDE via the OpenOCD wrapper. CubeIDE will auto-launch EBlink just as it would OpenOCD — no manual startup required.

## Setup

1. Copy `openocd.exe` (Windows) or `openocd` (Linux) from the wrappers folder into the EBlink installation directory, next to `eblink`.

2. In CubeIDE, open your debug configuration:
   `Run` → `Debug Configurations` → select your configuration → `Debugger` tab

3. Under **GDB Server Settings**, set the server to **OpenOCD** and point the executable path to the wrapper:
   `C:\path\to\eblink\openocd.exe`

That's all. CubeIDE will now auto-launch EBlink at the start of each debug session and close it at the end, exactly as it does with OpenOCD.

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
