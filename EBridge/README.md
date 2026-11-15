# EBridge : One-to-One TCP Debug Bridge

> **Disclaimer:** This software is provided "as is", without warranty of any kind. The author is not liable for any damage, data loss, or hardware failure resulting from its use. Use at your own risk.

EBridge is a lightweight utility that creates a **one-to-one TCP bridge** between two endpoints.  
It was designed to make **remote debugging with EBlink and GDB** possible even when the target
is behind **NAT networks** or **restricted firewalls** where port forwarding is not available.

## Features

-  **Single connection per port** – ensures clean one-to-one bridging.  
-  **Auto-reconnect behavior** – EBlink reconnects seamlessly after client disconnects.  
-  **Firewall/NAT-friendly** – only one public port needs to be exposed.  
-  **Lightweight & dependency-free** – minimal C implementation.  
-  **GDB/EBlink compatible** – works with existing remote debug setups.

---

## Overview

Sometimes you need to debug a device remotely because the DUT (Device Under Test) is located elsewhere.
In such cases, your colleague’s EBlink debugger often runs behind one or more NAT layers — for example, inside a company network or a lab environment.
Normally, you can only access that EBlink instance if port forwarding has been configured.
However, this is often not permitted due to company IT security policies, or simply not feasible because of ISP or router limitations.

EBridge solves this by reversing the connection strategy.

---

## How It Works

EBridge uses **two TCP ports**:

- **Main port** – waits for an incoming connection from the remote EBlink instance (using its GDB proxy option).  
- **Client port** – accepts a single connection from your local GDB (for example, from EmBitz IDE or another debugger).

The two ports are internally bridged.  
The client can connect **only when the main port is already connected**.

If the client disconnects, EBridge also terminates the main connection.  
EBlink will then automatically re-connect, creating a **fresh GDB context**, just as if it were running locally.

---

## Example Use Case

1. Install or run **EBridge** on your **local** server or PC.  
2. Open the **main port** on your firewall to make it publicly reachable.  
3. Ask your colleague to start EBlink with the **GDB proxy** option and connect it to your server’s EBridge main port.  
4. In your IDE (e.g. **EmBitz**, **VS Code**, or **GDB client**), connect to EBridge’s **client port**.  
5. You’re now debugging remotely — as if the EBlink were on your own machine.

```text
GDB client [10.2.1.10]
     │
     │  (private network)
     ▼
[10.2.0.1:2331]  EBridge [myserver.net:3333] ◀──(public) ── NAT/firewall 
                                                               ▲
                                                               │ (private network)
                                                               │
                                                        Remote PC runs: eblink -Gproxy=myserver.net
```                                                                   

If you stop the debugging session, EBridge automatically resets the bridge, allowing EBlink to re-establish a new connection automatically with a fresh GDB context — just as it would in local listen mode.


---

## Build Instructions
```bash
gcc -Wall -O2 ebridge.c -o ebridge
```

## Command Line

```bash
Usage: ebridge [options]
   -h, --help        Show help
   -d                Run as daemon
   -m <port>         Main listening port, default 3333
   -c <port>         Client listening port, default 2331
   -l <file>         Log file path
   -p <file>         PID file path
```

---
---

# EBridgeAuth : Authenticated TCP Debug Bridge

> **Disclaimer:** This software is provided "as is", without warranty of any kind. The author is not liable for any damage, data loss, or hardware failure resulting from its use. Use at your own risk.

**EBridgeAuth** (`ebridge_auth`) is EBridge with a **probe handshake** added to the main
port. It behaves exactly like EBridge — a one-to-one, zero-copy (`splice()`) TCP bridge —
but a remote EBlink is only allowed onto the main port when the **serial number of its probe**
is listed in an *allow file*. Everything after a successful handshake is a transparent GDB
stream, identical to EBridge.

## Purpose

The EBridge main port has to be **publicly reachable** (opened in your firewall) so a colleague
behind NAT can connect outward to it. That also means anyone on the internet can reach it.
Because a plain EBridge accepts a single connection per port, a stranger or a port scanner
could occupy your main port and block the colleague you actually want to debug with.

EBridgeAuth closes that gap: the main connection must first prove **which probe** it is. Only
serials you have explicitly allowed get through; anything else is answered with `DENIED` and
logged (with the source IP), so scanners can even be banned automatically with `fail2ban`.

## How It Works

Like EBridge, EBridgeAuth uses **two TCP ports**:

- **Main port** (default **8433**) – waits for the incoming connection from the remote EBlink.
- **Client port** (default **8431**) – accepts your local GDB / IDE connection.

The difference is the **handshake on the main port**, performed *before* the GDB stream starts
and *before* a client is allowed to connect:

1. EBlink connects and immediately sends a block of `FIELD value` lines, terminated by an
   empty line (`\r\n` line endings are also accepted):

   ```text
   PROBE <probe serial>
   <empty line>
   ```

   - `PROBE` is **required** and must appear **exactly once**.
   - The serial is **1..64 printable characters without spaces**.
   - Unknown fields are ignored, so the protocol can be extended later without breaking
     existing bridges. The whole block is at most **512 bytes** and must arrive within
     **5 seconds**.

2. EBridgeAuth replies with a single line:

   | Reply       | Meaning                                                                 |
   |-------------|-------------------------------------------------------------------------|
   | `OK`        | Serial is allowed. The main connection becomes a transparent GDB stream as soon as a client connects. |
   | `DENIED`    | Serial unknown or the handshake was invalid. The connection is closed.  |

A client can only connect **after** a successful handshake. If the client disconnects, the
main connection is terminated too and EBlink automatically reconnects with a fresh GDB context,
exactly as with EBridge.

### On the EBlink side

Use the `a:` scheme of the GDB proxy option (the `a:` prefix selects the authenticated bridge
and defaults to port 8433):

```bash
eblink ... -G proxy=a:myserver.net          # default main port 8433
eblink ... -G proxy=a:myserver.net:8433     # explicit port
```

EBlink sends its probe serial automatically. If the probe does not report a serial number,
EBlink does not start the proxy.

## Build Instructions
```bash
gcc -Wall -O2 ebridge_auth.c -o ebridge_auth
```

## Command Line

```bash
Usage: ebridge_auth -a <file> [options]
   -h, --help        Show help
   -a <file>         Allow file with permitted probe serials (required)
   -d                Run as daemon
   -m <port>         Main listening port, default 8433
   -c <port>         Client listening port, default 8431
   -l <file>         Log file path
   -p <file>         PID file path
```

## The allow (probe) file

The allow file (`-a <file>`) lists the probe serials that may use the bridge:

- **One serial per line**, optionally followed by a description after a space or tab.
- Lines starting with `#` are comments (a `#` *inside* a serial is part of the serial).
- Serials are compared **case-insensitive**.
- The file is **re-read on every new main connection**, so you can add or remove probes
  **without restarting** the service.

```text
# Probes that may use this bridge
# <serial>                   <description>
066DFF575251717867114355     Jan, lab 2
0672FF485550755187121723     Piet
```

**Adding a new probe the easy way:** every rejected serial is logged (with its IP), for example

```text
[main] Denied 203.0.113.9 (serial 51FF71065067485753401787 not allowed)
```

Let the remote connect once, then copy the serial straight from the log into the allow file —
no restart needed. (Only do this for a connection you are actually expecting.)

## Installation (systemd)

The provided `ebridge_auth.service` runs the bridge with its allow file at
`/etc/ebridge_auth_probes` and logs to `/var/log/ebridge_auth.log`.

```bash
# 1. Build and install the binary
gcc -Wall -O2 ebridge_auth.c -o ebridge_auth
sudo cp ebridge_auth /usr/local/bin/

# 2. Install the allow file (edit it to list your probe serials)
sudo cp ebridge_probes /etc/ebridge_auth_probes
sudo nano /etc/ebridge_auth_probes

# 3. Install and enable the service
sudo cp ebridge_auth.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now ebridge_auth

# 4. Check it
systemctl status ebridge_auth
journalctl -u ebridge_auth -f
```

Finally, open the **main port (8433)** in your firewall so the remote EBlink can reach it.
The client port (8431) only needs to be reachable from your local GDB/IDE.

> The `ExecStart` in `ebridge_auth.service` is `systemd Type=simple`, so the `-d` daemon flag
> is **not** used there — systemd manages the process directly. Rejected handshakes are written
> to syslog under the identifier `ebridge_auth`, which makes a `fail2ban` jail straightforward.
