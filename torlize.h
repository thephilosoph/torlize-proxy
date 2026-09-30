#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>



#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
#endif

#define PROXY "127.0.0.1"
#define PROXYPORT 9150
#define USERNAME "toraliz"
#define reqsize sizeof(struct proxy_request) 
#define ressize sizeof(struct proxy_response) 

typedef unsigned char int8 ; 
typedef unsigned short int16 ;
typedef unsigned int int32 ;


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

 struct proxy_request {
    int8 vn;
    int8 cd;
    int16 dport; 
    int32 dip;
    unsigned char userid[8]; 
 };
 typedef struct proxy_request Req;

 


 /*
 
Field	    Description	            Size (bytes)
VN	        Version Number	        1
CD	        Command Code	        1
DSTPORT	    Destination Port	    2
DSTIP	    Destination IP Address	4

 */

 struct proxy_response{
    int8 vn;
    int8 cd;
    int16 _; 
    int32 __;
 };
 typedef struct proxy_response Res;

Req *request(const char*, const int);
 int main(int,char**);