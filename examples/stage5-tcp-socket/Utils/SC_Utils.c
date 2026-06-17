#include "SC_Utils.h"

void error_handler(const char* message){
    printf("Caught the error: %s\n", message);
    exit(1);
}

void program_helper(const char* programe_name, const char* message)
{
    printf("Usage:%s %s\n",programe_name, message);
    exit(1);
}

int Socket(int domain, int type, int protocol)
{
    int serv_sock = socket(domain, type, protocol);
    if(serv_sock == -1){
        error_handler("Socket Error!");
    }

    return serv_sock;
}

int Bind(int fd, Sockaddr_in* pAddr, socklen_t len)
{
    int res = bind(fd, (Sockaddr*)pAddr,len);
    if(res == -1){
        error_handler("Bind Error");
    }
    return res;
}

int Listen(int fd, int n)
{
    int res = listen(fd, n);
    if(res == -1){
        error_handler("Listen Error");
    }
    return res;
}

int Accept(int fd, Sockaddr_in* pAddr, socklen_t* addr_len)
{
    int res = accept(fd, (Sockaddr*)pAddr, addr_len);
    if(res == -1){
        error_handler("Accept Error");
    }
    return res;
}

int Connect(int fd, Sockaddr_in* pAddr, socklen_t len)
{
    int res = connect(fd, (Sockaddr*)pAddr, len);
    if(res == -1){
        error_handler("Connnect Error");
    }

    return res;
}