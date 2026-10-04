#include "torlize.h"

/* ------------------------------------------------------------------ */
/* Network init / cleanup                                             */
/* ------------------------------------------------------------------ */

int network_init(void)
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
        return -1;
    }
#endif
    return 0;
}

void network_cleanup(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

const char *socks_reply_str(uint8_t code)
{
    switch (code) {
        case SOCKS_REP_SUCCESS: return "request granted";
        case SOCKS_REP_REJECT:  return "request rejected or failed";
        case SOCKS_REP_NOIDENT: return "rejected because SOCKS server cannot connect to identd on the client";
        case SOCKS_REP_IDENT:   return "rejected because the client program and identd report different user-ids";
        default:                return "unknown reply code";
    }
}

/* Returns 1 if host looks like a dotted-quad IPv4 address, 0 otherwise.
   Fully portable – no dependency on inet_pton. */
int is_ip_address(const char *host)
{
    unsigned int a, b, c, d;
    char extra;

    /* Must match exactly "A.B.C.D" with each part 0-255 */
    if (sscanf(host, "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4)
        return 0;
    if (a > 255 || b > 255 || c > 255 || d > 255)
        return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Build SOCKS4 / SOCKS4a request packet                              */
/* ------------------------------------------------------------------ */

uint8_t *build_socks4a_request(const char *host, int port,
                               const char *userid, size_t *out_len)
{
    size_t user_len = strlen(userid);
    size_t host_len = 0;
    int use_4a = !is_ip_address(host);
    size_t total;
    uint8_t *buf;
    uint8_t *p;

    if (user_len > MAX_USERID)
        user_len = MAX_USERID;

    if (use_4a) {
        host_len = strlen(host);
        if (host_len > MAX_HOSTNAME)
            host_len = MAX_HOSTNAME;
        /* VN CD PORT IP(0.0.0.x) USERID\0 HOSTNAME\0 */
        total = 1 + 1 + 2 + 4 + (user_len + 1) + (host_len + 1);
    } else {
        /* classic SOCKS4 */
        total = 1 + 1 + 2 + 4 + (user_len + 1);
    }

    buf = malloc(total);
    if (!buf)
        return NULL;

    p = buf;
    *p++ = SOCKS_VN;               /* VN */
    *p++ = SOCKS_CD_CONNECT;       /* CD = CONNECT */
    *p++ = (port >> 8) & 0xff;     /* DSTPORT high */
    *p++ = port & 0xff;            /* DSTPORT low  */

    if (use_4a) {
        /* DSTIP = 0.0.0.x (x != 0) signals SOCKS4a */
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 1;
    } else {
        /* inet_addr is available on all Winsock / POSIX systems */
        uint32_t addr = inet_addr(host);
        memcpy(p, &addr, 4);
        p += 4;
    }

    memcpy(p, userid, user_len);
    p += user_len;
    *p++ = '\0';                   /* null terminator for userid */

    if (use_4a) {
        memcpy(p, host, host_len);
        p += host_len;
        *p++ = '\0';               /* null terminator for hostname */
    }

    *out_len = total;
    return buf;
}

/* ------------------------------------------------------------------ */
/* Perform SOCKS handshake                                            */
/* ------------------------------------------------------------------ */

int socks_connect(socket_t sock, const char *host, int port,
                  const options_t *opts)
{
    uint8_t *req = NULL;
    size_t req_len = 0;
    uint8_t resp[8];
    ssize_t n;
    int ret = -1;

    req = build_socks4a_request(host, port, opts->userid, &req_len);
    if (!req) {
        fprintf(stderr, "out of memory building SOCKS request\n");
        return -1;
    }

    if (opts->verbose) {
        fprintf(stderr, "[*] SOCKS request size: %zu bytes (%s)\n",
                req_len, is_ip_address(host) ? "SOCKS4" : "SOCKS4a");
    }

    n = send(sock, (const char *)req, req_len, 0);
    if (n != (ssize_t)req_len) {
        fprintf(stderr, "send() of SOCKS request failed: %d\n", SOCKET_ERROR_CODE);
        goto out;
    }

    memset(resp, 0, sizeof(resp));
    n = recv(sock, (char *)resp, sizeof(resp), 0);
    if (n < 8) {
        fprintf(stderr, "short or failed SOCKS response (%zd bytes)\n", n);
        goto out;
    }

    /* resp[0] should be 0, resp[1] is the status code */
    if (opts->verbose) {
        fprintf(stderr, "[*] SOCKS reply: VN=%u CD=%u (%s)\n",
                resp[0], resp[1], socks_reply_str(resp[1]));
    }

    if (resp[1] != SOCKS_REP_SUCCESS) {
        fprintf(stderr, "SOCKS proxy rejected connection: %s (code %u)\n",
                socks_reply_str(resp[1]), resp[1]);
        goto out;
    }

    ret = 0; /* success */

out:
    free(req);
    return ret;
}

/* ------------------------------------------------------------------ */
/* Bidirectional relay (Unix-focused, select-based)                   */
/* ------------------------------------------------------------------ */

int relay(socket_t sock)
{
#ifndef _WIN32
    fd_set fds;
    char buf[BUFFER_SIZE];
    int maxfd = sock > STDIN_FILENO ? sock : STDIN_FILENO;
    int stdin_open = 1;

    /* Make stdin non-blocking so we can detect EOF cleanly */
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0)
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    for (;;) {
        FD_ZERO(&fds);
        if (stdin_open)
            FD_SET(STDIN_FILENO, &fds);
        FD_SET(sock, &fds);

        if (select(maxfd + 1, &fds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR)
                continue;
            perror("select");
            return -1;
        }

        /* Data from remote → stdout */
        if (FD_ISSET(sock, &fds)) {
            ssize_t n = recv(sock, buf, sizeof(buf), 0);
            if (n <= 0) {
                if (n < 0)
                    perror("recv");
                break; /* remote closed or error */
            }
            if (write(STDOUT_FILENO, buf, n) != n) {
                perror("write stdout");
                return -1;
            }
        }

        /* Data from stdin → remote */
        if (stdin_open && FD_ISSET(STDIN_FILENO, &fds)) {
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) {
                /* EOF on stdin – shut down write side of socket */
                stdin_open = 0;
                shutdown(sock, SHUT_WR);
            } else {
                if (send(sock, buf, n, 0) != n) {
                    perror("send");
                    return -1;
                }
            }
        }
    }
    return 0;
#else
    /* Windows: keep the connection open.
       Full bidirectional relay needs threads or overlapped I/O. */
    (void)sock;   /* silence unused-parameter warning */
    fprintf(stderr, "[!] Bidirectional relay not fully implemented on Windows.\n");
    fprintf(stderr, "    Connection is open; press Ctrl+C to close.\n");
    Sleep(INFINITE);
    return 0;
#endif
}

/* ------------------------------------------------------------------ */
/* Simple HTTP HEAD test (useful to verify the tunnel works)          */
/* ------------------------------------------------------------------ */

int do_test_request(socket_t sock, const char *host)
{
    char req[512];
    char buf[BUFFER_SIZE];
    ssize_t n;

    snprintf(req, sizeof(req),
             "HEAD / HTTP/1.0\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "\r\n", host);

    if (send(sock, req, strlen(req), 0) < 0) {
        perror("send test request");
        return -1;
    }

    while ((n = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[n] = '\0';
        fputs(buf, stdout);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* CLI                                                                */
/* ------------------------------------------------------------------ */

void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [options] <host> <port>\n"
        "\n"
        "Connect to <host>:<port> through a SOCKS4/4a proxy (Tor).\n"
        "\n"
        "Options:\n"
        "  -H, --proxy-host HOST   Proxy host (default: %s)\n"
        "  -P, --proxy-port PORT   Proxy port (default: %d)\n"
        "  -u, --user USERID       SOCKS userid (default: %s)\n"
        "  -t, --test              After connect, send HTTP HEAD and print response\n"
        "  -v, --verbose           Verbose output\n"
        "  -h, --help              Show this help\n"
        "\n"
        "Default behaviour (no -t):\n"
        "  Open a bidirectional tunnel (stdin ↔ remote). Type HTTP or any\n"
        "  protocol data, or pipe data into the program.\n"
        "\n"
        "Examples:\n"
        "  %s check.torproject.org 80\n"
        "  %s -t example.com 80\n"
        "  echo -e \"GET / HTTP/1.0\\r\\nHost: example.com\\r\\n\\r\\n\" | %s example.com 80\n"
        "  %s -H 127.0.0.1 -P 9050 some.onion 80\n",
        prog, DEFAULT_PROXY_HOST, DEFAULT_PROXY_PORT, DEFAULT_USERID,
        prog, prog, prog, prog);
}

int parse_options(int argc, char **argv, options_t *opts,
                  char **target_host, int *target_port)
{
    int i;

    /* defaults */
    strncpy(opts->proxy_host, DEFAULT_PROXY_HOST, sizeof(opts->proxy_host) - 1);
    opts->proxy_host[sizeof(opts->proxy_host) - 1] = '\0';
    opts->proxy_port = DEFAULT_PROXY_PORT;
    strncpy(opts->userid, DEFAULT_USERID, sizeof(opts->userid) - 1);
    opts->userid[sizeof(opts->userid) - 1] = '\0';
    opts->verbose = false;
    opts->test_mode = false;

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        }
        else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            opts->verbose = true;
        }
        else if (strcmp(arg, "-t") == 0 || strcmp(arg, "--test") == 0) {
            opts->test_mode = true;
        }
        else if (strcmp(arg, "-H") == 0 || strcmp(arg, "--proxy-host") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "error: %s requires an argument\n", arg);
                return -1;
            }
            strncpy(opts->proxy_host, argv[i], sizeof(opts->proxy_host) - 1);
            opts->proxy_host[sizeof(opts->proxy_host) - 1] = '\0';
        }
        else if (strcmp(arg, "-P") == 0 || strcmp(arg, "--proxy-port") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "error: %s requires an argument\n", arg);
                return -1;
            }
            opts->proxy_port = atoi(argv[i]);
            if (opts->proxy_port <= 0 || opts->proxy_port > 65535) {
                fprintf(stderr, "invalid proxy port: %s\n", argv[i]);
                return -1;
            }
        }
        else if (strcmp(arg, "-u") == 0 || strcmp(arg, "--user") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "error: %s requires an argument\n", arg);
                return -1;
            }
            strncpy(opts->userid, argv[i], sizeof(opts->userid) - 1);
            opts->userid[sizeof(opts->userid) - 1] = '\0';
        }
        else if (arg[0] == '-') {
            fprintf(stderr, "unknown option: %s\n\n", arg);
            print_usage(argv[0]);
            return -1;
        }
        else {
            /* first non-option argument is the target host */
            break;
        }
    }

    /* Remaining arguments must be exactly: host port */
    if (argc - i != 2) {
        fprintf(stderr, "error: host and port are required\n\n");
        print_usage(argv[0]);
        return -1;
    }

    *target_host = argv[i];
    *target_port = atoi(argv[i + 1]);

    if (*target_port <= 0 || *target_port > 65535) {
        fprintf(stderr, "invalid target port: %s\n", argv[i + 1]);
        return -1;
    }

    if (strlen(*target_host) == 0 || strlen(*target_host) > MAX_HOSTNAME) {
        fprintf(stderr, "invalid host\n");
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    options_t opts;
    char *target_host = NULL;
    int target_port = 0;
    socket_t sock = INVALID_SOCK;
    struct sockaddr_in proxy_addr;
    int ret = 1;

    if (parse_options(argc, argv, &opts, &target_host, &target_port) != 0)
        return 1;

    if (network_init() != 0)
        return 1;

    if (opts.verbose) {
        fprintf(stderr, "[*] target      : %s:%d\n", target_host, target_port);
        fprintf(stderr, "[*] proxy       : %s:%d\n", opts.proxy_host, opts.proxy_port);
        fprintf(stderr, "[*] userid      : %s\n", opts.userid);
        fprintf(stderr, "[*] mode        : %s\n",
                opts.test_mode ? "test (HTTP HEAD)" : "relay (stdin/stdout)");
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCK) {
        fprintf(stderr, "socket() failed: %d\n", SOCKET_ERROR_CODE);
        goto cleanup;
    }

    memset(&proxy_addr, 0, sizeof(proxy_addr));
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_port   = htons(opts.proxy_port);

    {
        unsigned long addr = inet_addr(opts.proxy_host);
        if (addr == INADDR_NONE) {
            fprintf(stderr, "invalid proxy host: %s (must be IPv4 address)\n",
                    opts.proxy_host);
            goto cleanup;
        }
        proxy_addr.sin_addr.s_addr = addr;
    }

    if (opts.verbose)
        fprintf(stderr, "[*] connecting to proxy...\n");

    if (connect(sock, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) != 0) {
        fprintf(stderr, "connect() to proxy failed: %d\n", SOCKET_ERROR_CODE);
        fprintf(stderr, "Is Tor running? (Tor Browser uses 9150, system tor uses 9050)\n");
        goto cleanup;
    }

    if (opts.verbose)
        fprintf(stderr, "[*] connected to proxy, performing SOCKS handshake...\n");

    if (socks_connect(sock, target_host, target_port, &opts) != 0)
        goto cleanup;

    fprintf(stderr, "[+] successfully connected through proxy to %s:%d\n",
            target_host, target_port);

    if (opts.test_mode) {
        if (opts.verbose)
            fprintf(stderr, "[*] sending test HTTP HEAD request...\n");
        do_test_request(sock, target_host);
    } else {
        if (opts.verbose)
            fprintf(stderr, "[*] entering relay mode (Ctrl+C or EOF to quit)\n");
        relay(sock);
    }

    ret = 0;

cleanup:
    if (sock != INVALID_SOCK)
        CLOSESOCKET(sock);
    network_cleanup();
    return ret;
}
