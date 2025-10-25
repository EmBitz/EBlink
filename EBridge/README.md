# EBridge : One-to-One TCP Debug Bridge

EBridge is a lightweight utility that creates a **one-to-one TCP bridge** between two endpoints.  
It was designed to make **remote debugging with EBlink and GDB** possible even when the target
is behind **NAT networks** or **restricted firewalls** where port forwarding is not available.

## Features

-  **Single connection per port** – ensures clean one-to-one bridging.  
-  **Auto-reconnect behavior** – EBlink reconnects seamlessly after client disconnects.  
-  **Firewall/NAT-friendly** – only one public port needs to be exposed.  
-  **Lightweight & dependency-free** – minimal C++ implementation.  
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

If you stop the debugging session, EBridge automatically resets the bridge so EBlink can
re-establish a new connection.


---

## Build Instructions
```bash
g++ -std=c++17 -O2  -Wall -pthread ebridge.cpp -o ebridge
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
