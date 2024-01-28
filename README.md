# Cortex-M tool e.g. STlink V2 & V3 support
## Win32, Linux_x86_64 and Raspberry 
### Auto detects Silabs, STmicro, Atmel, NXP and Renesas

EBlink ARM Cortex-M debug tool with squirrel scripting device support

<u>Windows installer</u> available with windows shell context menu.  
The installer will set the environment variable EB_DEFAULT_SCRIPT to "auto"(.script) so that all supported vendors are automaticlly detected (currently Silabs, STmicro, Atmel, NXP and Renesas)
and the variable EB_DEFAULT_PROBE is set to "stlink".

![alt text](https://www.embitz.org/context3.png)  

__Project bucket list__
- _Flash-breakpoints_ to increase the number of allowable breakpoints. Now we don't use external loaders anymore this is in reach as soon as we have ARMulator incoperated. Together with the speed of e.g. STlinkV3 and small page-size devices, this will be very convenient.
- Complete _CMIS-DAP(v2)_, this is for 40% ready
- Implement _SWO_ and _ITM_
- EBlink CLI wrappers 'ST-LINK_gdbserver.exe' and 'JLinkGDBServerCL.exe' so that EBlink can replace those GDB servers and act as if they are there.
   
 ##### When to consider EBlink instead of OpenOCD:
- if you need live variables for e.g. Embitz (OpenOCD doesn't support live variables)
- as a non-intrusive memory inspector (eblink supports hot-plugging and non-stop mode )
- if you need a CLI memory reader to read particular memory locations (also on running targets) and print them at stdout
- if you need a CLI programmer to modify particular in-place flash locations (checksum, serials etc)
- for easy complex custom board reset strategies or memory maps with special options
- for faster debug sessions and flash operations because of the EBlink flash cache
- for using easy auto configuration scripts e.g. custom flashing of ext. EEprom's etc
- as a remote (wifi) GDB server e.g. Raspberry (lightweight)
- very fast and easy to use standalone flash tool (program, verify, compare or dump)
  
### EBlink features:
- **_MultiCore support_**  _Currently STM32H7x5 - dual core auto detected, no additional configuration needed_.
- Integrated target stack frame UNWIND in case of exception with message box popup in windows.
- GDB (**MultiCore**) server with flash caching, with EmBitz live variables/expression support!
- Full Semi-hosting support
- Target voltage CLI override (e.g. 3 wire debugging or clone probes)
- Execute (user) script functions from CLI e.g. option bytes reading/writing etc.
- Execute (user) script functions from GDB terminal with the "monitor exec_script" command e.g. Special setups on GDB connect or on user request during runtime.
- Supports Hotplug current Embitz 2.x (monitor command "IsRunning" for target state query)
- Inplace memory (flash or ram) modifications of any length byte array from the command line (e.g. serials or checksum programming)
- Any length byte array memory reading also on running target from the command line (automated testing)
- **MultiCore** Core control (halt, reset and resume) from the command line (automated testing) 
- Stand alone command line flashing tool (auto detect ELF, IHEX and SREC) for production
- Dump memory (also on running target) to file in Intel hex or binary format
- Compare MCU flash against a ELF, IHEX or SREC file.
- All device related functions by c-like squirrel scripting e.g. flash or ext. EEprom algorithms, device reset strategy etc etc 
- Ready for multiple interfaces

#### Remarks:

1) EBlink uses ROM caching for speed performance. It is parsing the same XML memory map, which is provided by scripting, which is offered to GDB to get the memory information. If GDB reads memory from the ROM (flash) region then EBlink will not query the target but will instead return from cache. Sometimes, like debugging flash writing applications (e.g. bootloader), this behavior is not preferred and doesn't show the real flash modifications in GDB. If flash modifying code is debugged, turn off the caching with the "nc" GDB server option.

2) By default, the "Connection under reset" for the stlink interface is enabled. For hotplugging use the CLI option --hotplug (-H) or the stlink interface swith "dr" (Disable Reset) e.g. -I stlink,dr  Both options are the same but set at different levels.

3) From EBlink version 4.4 and above, this GDB server can be used for CubeMX IDE in either STlink GDB-server mode or OpenOCD. Be sure that Live variables are unchecked. Just launch EBlink from the context menu and keep it running to have full benefit of flash caching and exception unwind inside CubeMX IDE.

#### ISSUES
- If flash is empty but was started by e.g. powerup, a target exception is detected and an UNWIND is happening after first GDB launch because the exception flags of the MCU are set. This is not a bug but by design. Just ignore!
- Non STmicro devices (e.g. Silabs, NXP) are only working with STlink-V2.

## EBlink - usage:

	EBlink <options>

	-h,           --help			Print this help
	-g,           --nogui			No GUI message boxes
	-v <level>,   --verbose <0..8>		Specify level of verbose logging (default 4)
	-a [type],    --animation [0..]		Set the animation type (0=off, 1 = cursor, >1 = dot)
	-H,           --hotplug                 Don't reset/stop at target connection
	-I <options>, --interf			Select interface
	-T <options>, --target			Select target(optional)
	-S <file>,    --script <file>		Add a device script file
	-P <path>,    --path <path>		Add a search path for scripts
	-D <def>,     --define <def>		Add a script global define "name=value"
	-E <func>,    --execute <func>		Execute script function(s) from cli e.g. "setopt(WRREG, 5);lcdwr(\"foo\")"    
	-F <options>, --flash <options>		Run image flashing
	-G [options], --gdb <options>		Launch GDB server
	
	Multiple --script, --path, --execute and --define are allowed
    and --interf is mandatory if EB_DEFAULT_PROBE is not set.

       e.g.
        EBlink -I stlink -S auto -G
        EBlink -I stlink -S stm32 -G -D FLASH_SIZE=1024 -D RAM_SIZE=16
        EBlink -I stlink,dr,speed=3000 -S silabs -F erase,verify,run,file=mytarget.elf
        EBlink -I cmsis-dap -T cortex-m,fu=0 -S renesas -G port=4242,nc,s -S myReset.scr


==== **Interfaces**


name: ***STlink*** - STmicro V2/3 interface driver 
	
     Usage -I stlink[,options]

        dr              : Disable reset at connection (same as cli --hotplug)
        speed=<speed>   : Interface speed (default max possible)
        swd             : use SWD (default)
        jtag            : use Jtag
        vcc=<voltage>   : Set explicit target voltage (like 3.3)
        ap=<port>       : Select target DAP_AP port (default 0)        
        serial=<serial> : Select probe explicit with serial#
        device=<usb_bus>:<usb_addr> : Select probe explicit on bus

        e.g.  -I stlink,dr,speed=3000

==== **Targets**

name: ***cortex-m***
     
     Usage -T cortex-m[,options]

        fu=[0..2]    : Fault unwind level; 0=off, 1=forced only, 2= active break(default)
        reset[=0..2] : Reset the target, 0(default)=system,1=core,2=jtag
        halt         : Halt target
        resume       : Resume target
	
        e.g.  -T cortex-m,fu=1
              -T cortex-m,reset,resume

==== **Flash loader**
	
	Usage -F [options]

        erase        : Chip erase the flash
        verify       : Verify flash after upload
        run          : Start target

        write=<hex byte(s)>@<address>
                     : Modify flash location at address with a given hex byte array

        read=<length>@<address>
                     : returns hex byte array of memory location at address with a given length
                       use verbose level 0 to filter all unnecessary info.

        file=<file>  : Load the file,  <file>. Valid formats: ELF, IHEX or SREC (auto detect)
        cmp=<file>   : Memory compare, <file>. Valid formats: ELF, IHEX or SREC (auto detect)
        dump=<length>@<address>:<file>
                     : Dump memory to file, <file>.hex  = Intel HEX format
                                            Default     = Binary format

        e.g. -F file=test.elf
             -F run,file=test.hex
             -F read=4@0x80000004,-F read=4@0x80000008
             -F write=DEAD@80000004
             -F run,file=test.hex,write=45FECA1245@0x80000004,write=DEAD@0x80000100
             -F erase,verify,run,file=test.s
             -F erase
             -F run

        Default (without erase) only modified sectors are (re)flashed.
        Multiple reads and writes are allowed and is done after any file upload

==== **Services**
     
name: ***GDB-server***

     Usage -G [options]

        address=<x.x.x.x> : Select different listen address, default 0.0.0.0
        port=<tcp port>   : Select different TCP port, default (2331 + DAP_AP)
        ap=<port>         : Select the DAP_AP port, default 0
        s                 : Shutdown after disconnect
        nc                : Don't use EBlink flash cache

        e.g.  -G s,nc
        
====
