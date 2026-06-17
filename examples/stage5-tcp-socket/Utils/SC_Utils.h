#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// typedef
typedef struct sockaddr_in  Sockaddr_in;
typedef struct sockaddr     Sockaddr;
// program helper
void program_helper(const char* program_name,const char* message);

// handle the error shell be a neccessity
void error_handler(const char* message);

// MemZero
#define ZeroMem(Buf, BufSize) memset(Buf, 0, BufSize)

// Wrapper
int Socket(int domain, int type, int protocol);
int Bind(int fd, Sockaddr_in* pAddr, socklen_t len);
int Listen(int fd, int n);
int Accept(int fd, Sockaddr_in* pAddr, socklen_t* addr_len);
int Connect(int fd, Sockaddr_in* pAddr, socklen_t len);
