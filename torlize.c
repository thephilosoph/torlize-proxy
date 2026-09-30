
#include "torlize.h"


Req *request(const char* dip, const int dport)
{
    Req *req;
    req = malloc(reqsize);
    memset(req, 0, reqsize);
    req->vn = 4;
    req->cd = 1;
    req->dport = htons(dport);
    req->dip = inet_addr(dip);
    strncpy(req->userid , USERNAME , 8);

    return req;
}

int main(int argc , char *argv[])
{

    #ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
            return 1;
        }
    #endif


    char *host;
    int port , s;
    struct sockaddr_in sock;
    Req *req;
    Res *res;
    char buffer[ressize];
    int success; 
    char tmp[512];

    if (argc < 3)
    {
        fprintf(stderr , "Usage: %s <host> <port>\n" , argv[0]);
        return -1;
    }

    host = argv[1];
    port = atoi(argv[2]);

    s = socket(AF_INET, SOCK_STREAM ,0);

    if (s == INVALID_SOCKET)
    {
        fprintf(stderr, "socket error: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    sock.sin_family = AF_INET;
    sock.sin_port = htons(PROXYPORT);
    sock.sin_addr.s_addr = inet_addr(PROXY);

    if (connect(s, (struct sockaddr *)&sock, sizeof(sock)) != 0) {
        fprintf(stderr, "connect error: %d\n", WSAGetLastError());
        closesocket(s);
        WSACleanup();
        return 1;
    }
    printf("connected to proxy\n");
    req = request(host , port);
    // write(s , req , reqsize);
    send(s, (const char *)req, reqsize, 0);


    memset(buffer , 0 ,ressize);
    // if(read(s, buffer ,ressize) < 1 ){
    if (recv(s, buffer, ressize, 0) < 1)
    {
        perror("read error");
        free(req);
        closesocket(s);
        return -1;

    }

    res = (Res*)buffer;
    success = (res->cd == 90);

    if (!success)
    {
        fprintf(stderr , "Unable to traverse proxy , code error: %d\n" , res->cd);
        closesocket(s);
        free(req);
        return -1;
    }

    printf("successfully connected through the proxy to: %s:%d \n" , host ,port );

    memset(tmp , 0 , 512);
    snprintf(tmp , 511 , "HEAD / HTTP/1.0\r\n"
    "HOST: www.google.com\r\n"
    "\r\n");
    // write(s,tmp,strlen(tmp));
    send(s, tmp, strlen(tmp), 0);

    memset(tmp , 0 , 512);
    // read(s , tmp ,511);
    recv(s, tmp, 511, 0);
    printf("%s\n" , tmp);
    
    closesocket(s);
    WSACleanup();
    free(req);

    return 0;

}