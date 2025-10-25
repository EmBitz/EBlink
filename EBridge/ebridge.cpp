/******************************************************************************
 * File:        ebridge.cpp
 * Author:      Gerard Zagema (https://embitz.org)
 * Created:     2025-10-25
 *
 *
 * Description:
 *   This application establishes an exclusive one-to-one TCP bridge.
 *   It uses two TCP ports, a main port and a client port, that are directly linked
 *   together. The client is only permitted to connect when the main port is already
 *   connected.
 *
 *   If the client disconnects, the main connection is also terminated. The remote
 *   EBlink instance will then automatically create a new connection with a new GDB
 *   context, as if EBlink were running locally.
 *
 *   Both ports accept only a single active connection at any given time.
 *
 * Copyright (c) 2025 Sysdes - Netherlands
 * All rights reserved.
 *
 * Licensed under the MIT License. See LICENSE file in the project root
 * for full license information.
 *
 * Build:
 *    g++ -std=c++17 -O2  -Wall -pthread ebridge.cpp -o ebridge
 *
  * Purpose:
 *   If you need to establish a remote debugging session with a colleague who is
 *   behind one or more NAT networks (for example, in a company or a basement lab),
 *   you can normally only reach their EBlink instance if port forwarding is enabled.
 *   In many cases, this is not allowed (due to company policies) or not possible
 *   (due to network limitations). Therefore, a different approach is needed, hence
 *   the creation of EBridge.
 *
 *   By installing EBridge on your local server and exposing its main port to the
 *   public network (e.g., by opening it in your firewall), your colleague can actively
 *   connect their EBlink instance to that main port using the GDB proxy option.
 *
 *   On your side, you simply connect GDB (for example, from the EmBitz IDE’s remote
 *   server settings) to your EBridge server’s client port. You are then connected as
 *   if the remote EBlink were running directly on your desktop.
 *
 *   If you disconnect, the proxy connection is automatically closed. However, EBlink
 *   will immediately re-establish a new connection with a fresh GDB context, just as
 *   if you had disconnected from a local EBlink instance.
 *
 *   Happy debugging!
 *
 * Usage:
 *    Use as CLI or daemon. If used with systemd then daemonizing is not necessary.
 *
 *    Usage: ebridge [options]
 *    -h, --help        Show help
 *    -d                Run as daemon
 *    -m <port>         Main listening port, default 3333
 *    -c <port>         Client listening port, default 2331
 *    -l <file>         Log file path
 *    -p <file>         PID file path
 *
 * ---------------------------------------------------------------------------
 * Revision History:
 *   Date        Author          Description
 *   ----------  -------------- -----------------------------------------------
 *   2025-10-25  Gerard Zagema   Initial version
 *
 ******************************************************************************/

#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <csignal>
#include <syslog.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <ctime>

/////////////////////////////////////////////////
/// \brief  Our global variables
/////////////////////////////////////////////////

std::atomic<bool> g_running{true};
std::atomic<int> g_mainSock{-1};
std::atomic<int> g_clientSock{-1};
std::atomic<bool> g_mainConnected{false};
std::string g_mainIP, g_clientIP;
std::ofstream logfile;
std::string pidFilePath;


/////////////////////////////////////////////////
/// \brief Create timestamp string for logging
///
/// \return std::string with the current date and time
///
/////////////////////////////////////////////////
std::string timestamp() {
    std::time_t now = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return buf;
}


/////////////////////////////////////////////////
/// \brief The central logging of the application
///
/// \param msg const std::string& which need to be logged
/// \param bool toSyslog set to true is the message should be logged in the linux syslog
/// \return void
///
/////////////////////////////////////////////////
void log(const std::string &msg, bool toSyslog = false) {
    std::string full = "[" + timestamp() + "] " + msg;

    // Write to console in CLI mode
    if (isatty(STDOUT_FILENO))
        std::cout << full << std::endl;

    // Always write to log file if provided
    if (logfile.is_open())
        logfile << full << std::endl;

    // Only daemon start/stop goes to syslog
    if (toSyslog)
        syslog(LOG_INFO, "%s", full.c_str());
}


/////////////////////////////////////////////////
/// \brief Handler called ion system signals to quit application
///
/// \param int
/// \return void
///
/////////////////////////////////////////////////
void signalHandler(int) {
    g_running = false;
}


/////////////////////////////////////////////////
/// \brief Daemonize for linux if the -d option is set
///
/// \return void
///
/////////////////////////////////////////////////
void daemonize() {
    if (fork() != 0)
        exit(0);
    setsid();
    if (fork() != 0)
        exit(0);
    umask(0);

    int unused  __attribute__((unused));
    unused = chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}


/////////////////////////////////////////////////
/// \brief Print the help screen
///
/// \return void
///
/////////////////////////////////////////////////
void printHelp() {
    std::cout << "EBlink TCP Bridge - ebridge\n"
              << "Usage: ebridge [options]\n"
              << "-h, --help        Show help\n"
              << "-d                Run as daemon\n"
              << "-m <port>         Main listening port, default 3333\n"
              << "-c <port>         Client listening port, default 2331\n"
              << "-l <file>         Log file path\n"
              << "-p <file>         PID file path\n";
}


/////////////////////////////////////////////////
/// \brief Set the socket as non-blocking
///
/// \param sock int
/// \return void
///
/////////////////////////////////////////////////
void setNonBlocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}


/////////////////////////////////////////////////
/// \brief Main Listener which should be publiek reachable
///
/// \param mainPort int where EBlink is connected to as proxy
/// \return void
///
/////////////////////////////////////////////////
void mainListenerThread(int mainPort) {
    int listenSock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(mainPort);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        log("Failed to bind main port");
        return;
    }
    if (listen(listenSock, 1) < 0) {
        log("Failed to listen on main port");
        return;
    }

    log("Main listener started...");

    while (g_running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listenSock, &readfds);
        struct timeval tv {
            1,0
        };
        int ret = select(listenSock+1, &readfds, nullptr, nullptr, &tv);
        if (ret > 0 && FD_ISSET(listenSock, &readfds)) {
            sockaddr_in clientAddr{};
            socklen_t len = sizeof(clientAddr);
            int sock = accept(listenSock, (sockaddr*)&clientAddr, &len);
            if (sock >= 0) {
                if (g_mainSock != -1) {
                    log("Main rejected: already connected");
                    close(sock);
                    continue;
                }
                setNonBlocking(sock);
                g_mainSock = sock;
                g_mainConnected = true;

                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));
                g_mainIP = ipStr;
                log(std::string("Main connected from ") + g_mainIP);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    close(listenSock);
}

/////////////////////////////////////////////////
/// \brief Client Listener, the GDB side
///
/// \param clientPort int where GDB (ide) has to connect to
/// \return void
///
/////////////////////////////////////////////////
void clientListenerThread(int clientPort) {
    int listenSock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(clientPort);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        log("Failed to bind client port");
        return;
    }
    if (listen(listenSock, 1) < 0) {
        log("Failed to listen on client port");
        return;
    }

    log("Client listener started...");

    while (g_running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listenSock, &readfds);
        struct timeval tv {
            1,0
        };
        int ret = select(listenSock+1, &readfds, nullptr, nullptr, &tv);
        if (ret > 0 && FD_ISSET(listenSock, &readfds)) {
            sockaddr_in clientAddr{};
            socklen_t len = sizeof(clientAddr);
            int sock = accept(listenSock, (sockaddr*)&clientAddr, &len);
            if (sock >= 0) {
                if (!g_mainConnected) {
                    log("Client rejected: main not connected");
                    close(sock);
                    continue;
                }
                if (g_clientSock != -1) {
                    log("Client rejected: already connected");
                    close(sock);
                    continue;
                }
                setNonBlocking(sock);
                g_clientSock = sock;

                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));
                g_clientIP = ipStr;
                log(std::string("Client connected from ") + g_clientIP);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    close(listenSock);
}

/////////////////////////////////////////////////
/// \brief Bridge data Loop
///
/// \return void
///
/////////////////////////////////////////////////
void bridgeLoop() {
    char buffer[4096];
    while (g_running) {
        int mainSockLocal = g_mainSock;
        int clientSockLocal = g_clientSock;

        bool haveMain = (g_mainSock != -1);
        bool haveClient = (g_clientSock != -1);


        if (!haveMain && !haveClient) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        int maxfd = -1;

        if (haveMain) {
            FD_SET(mainSockLocal, &readfds);
            maxfd = std::max(maxfd, mainSockLocal);
        }
        if (haveClient) {
            FD_SET(clientSockLocal, &readfds);
            maxfd = std::max(maxfd, clientSockLocal);
        }

        struct timeval tv {
            1, 0
        };
        int ret = select(maxfd + 1, &readfds, nullptr, nullptr, &tv);
        if (ret <= 0)
            continue;

        // --- Main -> Client or detect main disconnect ---
        if (haveMain && FD_ISSET(mainSockLocal, &readfds)) {
            int n = recv(mainSockLocal, buffer, sizeof(buffer), 0);
            if (n <= 0) {
                log("Main disconnected");
                if (mainSockLocal != -1)
                    close(mainSockLocal);
                g_mainSock = -1;
                g_mainConnected = false;
                if (g_clientSock != -1) {
                    log("Closing client because main disconnected");
                    close(g_clientSock);
                    g_clientSock = -1;
                }
                continue;
            }
            if (haveClient)
                send(g_clientSock, buffer, n, 0);
        }

        // --- Client -> Main or detect client disconnect ---
        if (haveClient && FD_ISSET(clientSockLocal, &readfds)) {
            int n = recv(clientSockLocal, buffer, sizeof(buffer), 0);
            if (n <= 0) {
                log("Client disconnected");
                if (g_clientSock != -1)
                    close(clientSockLocal);
                g_clientSock = -1;
                if (g_mainSock != -1) {
                    log("Closing main because client disconnected");
                    close(g_mainSock);
                    g_mainSock = -1;
                    g_mainConnected = false;
                }
                continue;
            }
            if (haveMain)
                send(g_mainSock, buffer, n, 0);
        }
    }

    if (g_clientSock != -1)
        close(g_clientSock);
    if (g_mainSock != -1)
        close(g_mainSock);
}


/////////////////////////////////////////////////
/// \brief Main application entry
///
/// \param argc int
/// \param argv[] char*
/// \return int
///
/////////////////////////////////////////////////
int main(int argc, char *argv[]) {
    bool runDaemon = false;
    uint16_t mainPort = 3333;
    uint16_t clientPort = 2331;
    std::string logFile = "/var/log/ebridge.log";

    int opt;
    static struct option long_options[] = {
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    while ((opt = getopt_long(argc, argv, "hdm:c:l:p:", long_options, nullptr)) != -1) {
        switch (opt) {
        case 'h':
            printHelp();
            return 0;
        case 'd':
            runDaemon = true;
            break;
        case 'm':
            mainPort = std::stoi(optarg);
            break;
        case 'c':
            clientPort = std::stoi(optarg);
            break;
        case 'l':
            logFile = optarg;
            break;
        case 'p':
            pidFilePath = optarg;
            break;
        default:
            printHelp();
            return 1;
        }
    }

    logfile.open(logFile, std::ios::app);
    openlog("ebridge", LOG_PID | LOG_CONS, LOG_DAEMON);
    log("EBlink EBridge daemon starting...", true);

    if (runDaemon)
        daemonize();

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    if (!pidFilePath.empty()) {
        std::ofstream pidFile(pidFilePath);
        if (pidFile.is_open()) {
            pidFile << getpid() << std::endl;
            pidFile.close();
            log("PID file written: " + pidFilePath);
        } else {
            log("Failed to write PID file: " + pidFilePath);
        }
    }

    std::thread mainThread(mainListenerThread, mainPort);
    std::thread clientThread(clientListenerThread, clientPort);
    std::thread bridgeThread(bridgeLoop);

    log("EBridge started. Waiting for main and client...");

    mainThread.join();
    clientThread.join();
    bridgeThread.join();

    if (!pidFilePath.empty())
        unlink(pidFilePath.c_str());

    log("EBlink EBridge daemon stopping...", true);
    logfile.close();
    closelog();
    return 0;
}
