#include "../Utils/SC_Utils.h"

#define BUF_SIZE 1024
#define QUEUE_LENGTH 5
int main(int argc, char const *argv[])
{
    int server_socket, client_socket;
    char message[BUF_SIZE];
    int str_len;

    Sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size;
    if(argc != 2){
        program_helper(argv[0],"<Port>\n");
    }

    server_socket = Socket(PF_INET, SOCK_STREAM, 0);
    ZeroMem(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(argv[1]));

    Bind(server_socket, &server_addr, sizeof(server_addr));

    Listen(server_socket, QUEUE_LENGTH);

    client_addr_size = sizeof(client_addr);

    for(int i = 0; i < 5; i++)
    {
        client_socket = Accept(server_socket, &client_addr, &client_addr_size);
        printf("Connect client Success!:%d\n", i + 1);
        while((str_len = read(client_socket, message, BUF_SIZE), printf("strlen:%d\n", str_len)) != 0)
            write(client_socket, message, str_len);
	
	printf("Close the client_sock");
        close(client_socket);
    }   
    close(server_socket);
    return 0;
}
