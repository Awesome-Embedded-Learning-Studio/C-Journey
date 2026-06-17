#include "../Utils/SC_Utils.h"
#define BUF_SIZE 1024
int main(int argc, char const *argv[])
{
    int server_socket;
    Sockaddr_in server_addr;
    char message[BUF_SIZE];
    if(argc != 3){
        program_helper(argv[0], "%s <IP> <Port>\n");
    }

    server_socket = Socket(PF_INET, SOCK_STREAM, 0);
    ZeroMem(&server_addr,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(atoi(argv[2]));

    Connect(server_socket, &server_addr, sizeof(server_addr));
    puts("Connecting ...");

    while(1)
    {
        puts("Input Message, Q for quit\n");
        fgets(message, BUF_SIZE, stdin);

        if(!strcmp(message, "q\n") || !strcmp(message, "Q\n"))
        {
            break;
        }

        write(server_socket, message, strlen(message));
        int str_len = read(server_socket, message, BUF_SIZE - 1);
        message[str_len] = '\0';
        printf("Getta Message: %s\n",message);

    }

    close(server_socket);

    return 0;
}
