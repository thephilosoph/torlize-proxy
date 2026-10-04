
/**
 * 
Field	    Description	            Size (bytes)
VN	        Version Number	        1
CD	        Command Code	        1
DSTPORT	    Destination Port	    2
DSTIP	    Destination IP Address	4
USERID	    User ID	                variable
NULL	    Null Terminator	        1

 */




 /*
 
Field	    Description	            Size (bytes)
VN	        Version Number	        1
CD	        Command Code	        1
DSTPORT	    Destination Port	    2
DSTIP	    Destination IP Address	4

 */



#ifndef TORLIZE_H
#define TORLIZE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    typedef SOCKET socket_t;
    #define CLOSESOCKET closesocket
    #define SOCKET_ERROR_CODE WSAGetLastError()
    #define INVALID_SOCK INVALID_SOCKET
    /* ssize_t is not always defined on MinGW */
    #ifndef _SSIZE_T_DEFINED
        typedef int ssize_t;
        #define _SSIZE_T_DEFINED
    #endif
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <sys/select.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <fcntl.h>
    typedef int socket_t;
    #define CLOSESOCKET close
    #define SOCKET_ERROR_CODE errno
    #define INVALID_SOCK (-1)
    #define SOCKET_ERROR (-1)
#endif

/* Defaults */
#define DEFAULT_PROXY_HOST "127.0.0.1"
#define DEFAULT_PROXY_PORT 9150
#define DEFAULT_USERID     "torlize"
#define MAX_HOSTNAME       255
#define MAX_USERID         255
#define BUFFER_SIZE        8192

/* SOCKS4 / SOCKS4a constants */
#define SOCKS_VN           4
#define SOCKS_CD_CONNECT   1
#define SOCKS_REP_SUCCESS  90
#define SOCKS_REP_REJECT   91
#define SOCKS_REP_NOIDENT  92
#define SOCKS_REP_IDENT    93

/* Global options (set by CLI) */
typedef struct {
    char     proxy_host[256];
    int      proxy_port;
    char     userid[MAX_USERID + 1];
    bool     verbose;
    bool     test_mode;   /* send a simple HTTP HEAD after connect */
} options_t;

/* Function prototypes */
int  network_init(void);
void network_cleanup(void);
const char *socks_reply_str(uint8_t code);
int  is_ip_address(const char *host);
uint8_t *build_socks4a_request(const char *host, int port,
                               const char *userid, size_t *out_len);
int  socks_connect(socket_t sock, const char *host, int port,
                   const options_t *opts);
int  relay(socket_t sock);
int  do_test_request(socket_t sock, const char *host);
void print_usage(const char *prog);
int  parse_options(int argc, char **argv, options_t *opts,
                   char **target_host, int *target_port);

#endif /* TORLIZE_H */
