[![Stand With Ukraine](https://raw.githubusercontent.com/vshymanskyy/StandWithUkraine/main/banner2-direct.svg)](https://stand-with-ukraine.pp.ua)

# Cortex-M Tool Support (e.g., STlink V2 & V3)
## Win32, Linux x86_64, and Raspberry Pi (32/64-bit)
### Auto-detects Silabs, STMicro, Atmel, NXP, and Renesas

EBlink is an ARM Cortex-M debug tool with Squirrel scripting support for various devices.

[Link to EmBitz EBlink forum](https://embitz.org/forum/forum-3.html)

Windows installer includes a shell context menu, and **all files are digitally signed**.  
The installer sets the environment variable `EB_DEFAULT_SCRIPT` to `"auto"` (`.script`), so that all supported vendors are automatically detected (currently Silabs, STMicro, Atmel, NXP, and Renesas).  
The variable `EB_DEFAULT_PROBE` is set to `"stlink"`.

![alt text](https://www.embitz.org/context3.png)  

Starting from version 6.21, **EBmon** is included in the packages. EBmon is the host-side component of the real-time monitor that has been available for years as a plugin in EmBitz. The **EBmon CLI** is very fast because it does not require a GDB client. With the CLI, you can also control your target by running, halting, or restarting it. Importantly, the EBmon CLI is **not strictly tied to EBlink**.


![alt text](https://www.embitz.org/EBmon.png)  

---

## When to Consider EBlink Instead of OpenOCD

- If you need live variables for EmBitz (OpenOCD only supports them via Tandem-GDB).  
- As a non-intrusive memory inspector (supports hot-plugging and non-stop mode).  
- If you need a CLI memory reader to read specific memory locations (even on running targets) and print them to stdout.  
- If you need a CLI programmer to modify in-place flash locations (e.g., checksums, serials).  
- For complex board reset strategies or special memory maps.  
- For faster debug sessions and flash operations due to EBlink’s flash cache.  
- To use auto-configuration scripts (e.g., custom flashing of external EEPROMs).  
- As a remote (Wi-Fi) GDB server (lightweight, e.g., Raspberry Pi).  
- As a fast, standalone flash tool (program, verify, compare, or dump).  
- If you need to establish a remote debug connection behind a NAT network.

---

## EBlink Features

- **MultiCore support** – currently STM32H7x5 dual-core auto-detected, no additional configuration required.  
- Integrated target stack frame unwind in case of exceptions, with message box popup on Windows.  
- GDB (**MultiCore**) server with flash caching and EmBitz live variable/expression support.  
- Each GDB server instance supports up to four concurrent connections.  
- Full semi-hosting support.  
- Target voltage CLI override (e.g., 3-wire debugging or clone probes).  
- Execute user script functions from CLI or from GDB terminal using `monitor exec_script`.  
- Hotplug support in EmBitz 2.x (`monitor IsRunning` command for target state query).  
- In-place memory (flash or RAM) modifications of any byte array from the command line.  
- Read memory from running targets via CLI (automated testing).  
- MultiCore control (halt, reset, resume) from the command line.  
- Standalone command-line flashing tool (auto-detect ELF, IHEX, SREC).  
- Dump memory (even on running targets) to Intel HEX or binary formats.  
- Compare MCU flash against ELF, IHEX, or SREC files.  
- Device-related functions using C-like Squirrel scripting (flash, EEPROM algorithms, reset strategies, etc.).  
- Squirrel script tracing and debugging (version 6.0+), including breakpoints and local variable watch.  
- Use a GDB proxy connection for remote debugging behind NAT networks via EBridge.  
- Ready for multiple interfaces.

---

## Remarks

1. EBlink uses ROM caching for performance. It parses the XML memory map (provided by scripting) to supply memory information to GDB. If GDB reads memory from the ROM (flash) region, EBlink returns cached data instead of querying the target. For debugging flash-writing applications (e.g., bootloader), disable caching with the `nc` GDB server option.  
2. By default, "Connection under reset" is enabled for the STlink interface. For hotplugging, use CLI `--hotplug` (`-H`) or the STlink option `dr` (Disable Reset). Both achieve the same result but at different levels.  
3. From EBlink version 5.30+, this GDB server can be used with **CubeIDE** in **STlink GDB-server, JLink, or OpenOCD mode**. Live variables work on all supported probes. Launch EBlink from the context menu and keep it running to benefit from flash caching and exception unwind inside CubeIDE.

---

## Issues

- Non-STMicro devices (e.g., Silabs, NXP) currently work only with STlink V2.

---

## EBlink Usage


	EBlink <options>

	-h,           --help            Print this help
	-g,           --nogui           No GUI message boxes
	-v <level>,   --verbose <0..8>  Specify level of verbose logging (default 4)
	-a [type],    --animation [0..] Set the animation type (0=off, 1 = cursor, >1 = dot)
	-H,           --hotplug         Don't reset/stop at target connection
	-I <options>, --interf          Select interface
	-T <options>, --target          Select target(optional)
	-S <file>,    --script <file>   Add a device script file
	-P <path>,    --path <path>     Add a search path for scripts
	-D <def>,     --define <def>    Add a script global define "name=value"
	-E <func>,    --execute <func>  Execute script function(s) from cli   
	-F <options>, --flash <options> Run image flashing
	-G [options], --gdb <options>   Launch GDB server
	
- Multiple `--script`, `--path`, `--execute`, and `--define` are allowed.  
- `--interf` is mandatory if `EB_DEFAULT_PROBE` is not set.

**Examples:**

        EBlink -I stlink -S auto -G
        EBlink -I stlink -S stm32 -G -D FLASH_SIZE=1024 -D RAM_SIZE=16
        EBlink -I stlink,dr,speed=3000 -S silabs -F erase,verify,run,file=mytarget.elf
        EBlink -I cmsis-dap -T cortex-m,fu=0 -S renesas -G port=4242,nc,s -S myReset.scr
        EBlink -I stlink -S auto -E "setopt(WRREG, 5);dataflash_erase(0x08000500)" 

---

## Interfaces

### STlink – STMicro V2/3 interface driver  

**Usage:** `-I stlink[,options]`

Options:

- `dr` – Disable reset at connection (same as `--hotplug`)  
- `speed=<speed>` – Interface speed (default: max possible)  
- `swd` – Use SWD (default)  
- `jtag` – Use JTAG  
- `vcc=<voltage>` – Set explicit target voltage (e.g., 3.3V)  
- `ap=<port>` – Select target DAP_AP port (default 0)  
- `serial=<serial>` – Select specific probe by serial number  
- `device=<usb_bus>:<usb_addr>` – Select specific probe on USB bus  

Example: `-I stlink,dr,speed=3000`

---

## Targets

### cortex-m  

**Usage:** `-T cortex-m[,options]`

Options:

- `fu=<0..2>` – Fault unwind level (0=off, 1=passive, 2=active [default])  
- `fc=<rmncsbih>` – Fault catch active flags (`rESET, mEM, nCOP, cHK, sTAT, bUS, iNT, hARD`)  
- `reset[=0..2]` – Reset type (0=system [default], 1=core, 2=JTAG)  
- `halt` – Halt target  
- `resume` – Resume target  

Examples:

        -T cortex-m,fu=1
        -T cortex-m,reset,resume


---

## Flash Loader

**Usage:** `-F [options]`

Options:

- `erase` – Chip erase flash  
- `verify` – Verify flash after upload  
- `run` – Start target  
- `write=<hex bytes>@<address>` – Modify flash at specific address  
- `read=<length>@<address>` – Read memory at specific address  
- `file=<file>` – Load file (ELF, IHEX, SREC auto-detect)  
- `cmp=<file>` – Compare memory against file  
- `dump=<length>@<address>:<file>` – Dump memory to file (Intel HEX or binary)  

Examples:

        -F file=test.elf
        -F run,file=test.hex
        -F read=4@0x8000004,read=6@0x8000008
        -F write=DEAD@0x8000004
        -F run,file=test.hex,write=45FECA1245@0x8000004,write=DEAD@0x8000100
        -F erase,verify,run,file=test.s
        -F erase
        -F run

Default (without `erase`): only modified sectors are reflashed. Multiple reads/writes are allowed and executed after file upload.

---

## Services

### GDB Server  

**Usage:** `-G [options]`

Options:

- `address=<x.x.x.x>` – Listen address (default 0.0.0.0)  
- `port=<tcp port>` – TCP port (default 2331)  
- `proxy=<host>[:<port>]` – Connect to external EBridge (default port 3333)  
- `ap=<port>` – DAP_AP port (default 0)  
- `s` – Shutdown after disconnect  
- `nc` – Disable EBlink flash cache  

Example: `-G nc,proxy=foo.nl`

---

## Debian Packages (AMD64 / ARM32 / ARM64)

Packages include all dependencies for EBlink with STlink.  

**AMD64:**
```bash
# Debian
sudo apt install ./eblink-debian-amd64.deb
sudo dpkg --remove eblink-debian-amd64

# Ubuntu
sudo dpkg -i ./eblink-debian-amd64.deb
sudo dpkg -r ./eblink-debian-amd64
```

**Raspberry Pi:**
```bash
# 32-bit
sudo apt install ./eblink-debian-armhf.deb
sudo dpkg --remove eblink-debian-armhf

# 64-bit
sudo apt install ./eblink-debian-arm64.deb
sudo dpkg --remove eblink-debian-arm64
```

**Installed files**

    /usr/bin/eblink                     // Executable  
    /usr/share/eblink                   // Scripts folder  
    /etc/profile.d/eblink.sh            // EBlink Environment variables export at linux startup  
    /etc/udev/rules.d/80-eblink.rules   // The STlink device registrations  


The following environment variables are active <u>after linux reboot</u>:

    EB_DEFAULT_PROBE=stlink
    EB_DEFAULT_SCRIPT=auto

**Usage**

You can launch EBlink from every terminal, e.g. to launch GDB server 
```bash
eblink -G
```
IF you need a particular script file you can just overrule the environment by **_eblink -S silabs -G_**  
or if other interface options are needed **_eblink -I stlink,speed=1000 -G_** etc.

