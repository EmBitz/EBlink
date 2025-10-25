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

In many development setups, your colleague’s EBlink debugger runs behind one or multiple
NAT layers — for example, within a company network or a basement lab.  
Normally, you could only reach that EBlink if explicit **port forwarding** is configured,
which is often **not allowed** (company IT policies) or **not possible** (e.g. ISP or router
restrictions).

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

1. Install and run **EBridge** on your **local** server or PC.  
2. Open the **main port** on your firewall to make it publicly reachable.  
3. Ask your colleague to configure EBlink with the **GDB proxy** option and connect it to your server’s EBridge main port.  
4. In your IDE (e.g. **EmBitz**, **VS Code**, or **GDB client**), connect to EBridge’s **client port**.  
5. You’re now debugging remotely — as if the EBlink were on your own machine.

If you stop the debugging session, EBridge automatically resets the bridge so EBlink can
re-establish a new connection.


---

## Build Instructions
```bash
g++ -std=c++17 -O2 -o ebridge tcp_bridge.cpp
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
