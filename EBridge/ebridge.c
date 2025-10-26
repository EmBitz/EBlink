/******************************************************************************
 * File:        ebridge.c
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
 *    gcc -Wall -O2 ebridge.c -o ebridge
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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <syslog.h>

#define DEFAULT_PORT_MAIN   3333
#define DEFAULT_PORT_CLIENT 2331
#define BUF_SIZE 65536

// ---------- Globals ----------
static FILE *log_file = NULL;
static int is_daemon = 0;
static char *pid_file_path = NULL;

// Listen-sockets
static int listen_main = -1;
static int listen_client = -1;

// Active connected sockets
static int fd_main = -1;
static int fd_client = -1;

// Loop control
static volatile sig_atomic_t prog_running = 1;

// ---------- Logging ----------
void logmsg(int toSyslog, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);

    FILE *out = log_file ? log_file : stdout;

    fprintf(out, "[%s] ", tbuf);
    vfprintf(out, fmt, args);
    fprintf(out, "\n");
    fflush(out);

    va_end(args);

    if(toSyslog) {
        va_list args;
        va_start(args, fmt);
        vsyslog(LOG_INFO, fmt, args);
        va_end(args);
    }
}

void log_peer_ip(int fd, const char *name) {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    if (getpeername(fd, (struct sockaddr*)&addr, &addrlen) == 0) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
        logmsg(0, "[%s] Connected from %s:%d", name, ip, ntohs(addr.sin_port));
    } else {
        logmsg(1, "[%s] Could not get peer address: %s", name, strerror(errno));
    }
}


// ---------- PID-file ----------
void write_pidfile(void) {
    logmsg(0, "Daemon PID %d", getpid());
    if (!pid_file_path)
        return;
    FILE *f = fopen(pid_file_path, "w");
    if (!f) {
        logmsg(1,"Failed to write PID file %s: %s", pid_file_path, strerror(errno));
        return;
    }
    fprintf(f, "%d\n", getpid());
    fclose(f);
    logmsg(0,"Daemon PID file written %s", pid_file_path);
}

void remove_pidfile(void) {
    if (pid_file_path)
        unlink(pid_file_path);
}

// ---------- Signal handling ----------
void handle_signal(int sig) {
    logmsg(1, "Daemon stopping (signal %d)", sig);
    prog_running = 0; // stop main loop
}

// ---------- Socket helper ----------
int make_listen_socket(int port, const char *name) {
    int s = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (s < 0) {
        perror("socket");
        exit(1);
    }

    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        logmsg(1,"[%s] bind failed on port %d: %s", name, port, strerror(errno));
        exit(1);
    }

    if (listen(s, 4) < 0) {
        logmsg(1,"[%s] listen failed: %s", name, strerror(errno));
        exit(1);
    }

    logmsg(0,"[%s] Listening on port %d", name, port);
    return s;
}

// ---------- Daemonize ----------
void daemonize(void) {
    pid_t pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);

    umask(0);
    setsid();

    int unused  __attribute__((unused));
    unused = chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    int fd = open("/dev/null", O_RDWR);
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > 2)
        close(fd);

    is_daemon = 1;
}

// ---------- Bridge ----------
void bridge_sockets(int main_fd, int client_fd) {
    int pipe_m2c[2], pipe_c2m[2];

    if (pipe(pipe_m2c) < 0) {
        logmsg(1,"Failed to create pipe_m2c: %s", strerror(errno));
        close(main_fd);
        close(client_fd);
        return;
    }
    if (pipe(pipe_c2m) < 0) {
        logmsg(1,"Failed to create pipe_c2m: %s", strerror(errno));
        close(main_fd);
        close(client_fd);
        close(pipe_m2c[0]);
        close(pipe_m2c[1]);
        return;
    }

    logmsg(0,"[bridge] Established main<->client");

    int running = 1;
    while (running && prog_running) {
        ssize_t n1 = splice(main_fd, NULL, pipe_m2c[1], NULL,
                            BUF_SIZE, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
        if (n1 > 0)
            splice(pipe_m2c[0], NULL, client_fd, NULL,
                   n1, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);

        ssize_t n2 = splice(client_fd, NULL, pipe_c2m[1], NULL,
                            BUF_SIZE, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
        if (n2 > 0)
            splice(pipe_c2m[0], NULL, main_fd, NULL,
                   n2, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);

        if (n1 == 0 || n2 == 0)
            running = 0;
        if ((n1 < 0 && errno != EAGAIN && errno != EWOULDBLOCK) ||
                (n2 < 0 && errno != EAGAIN && errno != EWOULDBLOCK))
            running = 0;

        usleep(100);
    }

    close(pipe_m2c[0]);
    close(pipe_m2c[1]);
    close(pipe_c2m[0]);
    close(pipe_c2m[1]);
    close(main_fd);
    close(client_fd);
    logmsg(0,"[bridge] Closed main<->client");
}

// ---------- Help ----------
void print_help() {
    printf("EBlink TCP Bridge - ebridge\n");
    printf("Usage: ebridge [options]\n");
    printf("Options:\n");
    printf("  -m <port>     Main port (default %d)\n", DEFAULT_PORT_MAIN);
    printf("  -c <port>     Client port (default %d)\n", DEFAULT_PORT_CLIENT);
    printf("  -l <file>     Log file path\n");
    printf("  -p <file>     PID file path\n");
    printf("  -d            Run as daemon\n");
    printf("  -h            Show this help message\n");
    printf("\nBehavior:\n");
    printf("  Waits for main connection first, then accepts client.\n");
    printf("  Client cannot connect before main is connected.\n");
    printf("  Main is disconnected if client disconnect or if\n");
    printf("  it receives data with unconnected client.\n");    
}

// ---------- Main ----------
int main(int argc, char *argv[]) {
    int port_main = DEFAULT_PORT_MAIN;
    int port_client = DEFAULT_PORT_CLIENT;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-m") && i + 1 < argc)
            port_main = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-c") && i + 1 < argc)
            port_client = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-l") && i + 1 < argc)
            log_file = fopen(argv[++i], "a");
        else if (!strcmp(argv[i], "-p") && i + 1 < argc)
            pid_file_path = argv[++i];
        else if (!strcmp(argv[i], "-d"))
            is_daemon = 1;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_help();
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_help();
            return 1;
        }
    }

    if (log_file)
        setvbuf(log_file, NULL, _IOLBF, 0);

    openlog("ebridge", LOG_PID | LOG_NDELAY, LOG_DAEMON);

    // Signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Daemonize
    if (is_daemon) {
        daemonize();
        logmsg(1, "Daemon started (main=%d, client=%d)", port_main, port_client);
        write_pidfile();
    }

    logmsg(0,"Starting ebridge (main=%d, client=%d)%s",
           port_main, port_client, is_daemon ? " [daemon]" : "");

    listen_main = make_listen_socket(port_main, "main");
    listen_client = make_listen_socket(port_client, "client");

    logmsg(0,"Waiting for main connection first...");

    // ---------- Main loop ----------
    while (prog_running) {
        // Accept main connection if not connected
        if (fd_main < 0) {
            fd_main = accept4(listen_main, NULL, NULL, SOCK_NONBLOCK);
            if (fd_main > 0) {
                // Main connected, log IP
                log_peer_ip(fd_main, "main");
            }
        }

        // Accept client connection if main is connected and client not yet connected
        if (fd_main > 0 && fd_client < 0) {
            fd_client = accept4(listen_client, NULL, NULL, SOCK_NONBLOCK);
            if (fd_client > 0){
                // Client connected, log IP
                log_peer_ip(fd_client, "client");
            }
        }

        // If both connected, run bridge
        if (fd_main > 0 && fd_client > 0) {
            bridge_sockets(fd_main, fd_client);
            fd_main = fd_client = -1;
            logmsg(0,"Waiting for new main connection...");
        }

        // If main receives data and client not connected, disconnect main
        if (fd_main > 0 && fd_client < 0) {
            char buf[BUF_SIZE];
            ssize_t n = read(fd_main, buf, sizeof(buf));
            if ( (n >= 0) || (errno != EAGAIN && errno != EWOULDBLOCK ) ) {
                logmsg(0, "[main] closed on early data or error");
                close(fd_main);
                fd_main = -1;
            }
        }

        // Reject client connections before main connects
        if (fd_main < 0) {
            int temp = accept4(listen_client, NULL, NULL, SOCK_NONBLOCK);
            if (temp > 0) {
                logmsg(0,"[client] Tried to connect without main. Closing.");
                close(temp);
            }
        }

        usleep(50000);
    }

    // ---------- Cleanup ----------
    if (fd_main > 0)
        close(fd_main);

    if (fd_client > 0)
        close(fd_client);

    if (listen_main > 0)
        close(listen_main);

    if (listen_client > 0)
        close(listen_client);

    if (log_file)
        fclose(log_file);

    remove_pidfile();

    closelog();

    return 0;
}
