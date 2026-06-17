#include "../Utils/SC_Utils.h"

int main(int argc, char const *argv[])
{
    int client_socket;
    Sockaddr_in server_addr;
    char message[30];
    int str_len = 0;
    int idx = 0, read_len = 0;
    if(argc != 3){
        program_helper(argv[0], "%s <IP> <Port>\n");
    }

    client_socket = Socket(PF_INET, SOCK_STREAM, 0);
    ZeroMem(&server_addr,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(atoi(argv[2]));

    Connect(client_socket, &server_addr, sizeof(server_addr));

    while(read_len = read(client_socket, &message[idx++], 1)){
        if(read_len == -1)
            error_handler("Read_Error");
        str_len += read_len;
    }

    printf("Message received: %s\n", message);
    printf("Gotta read count: %d\n", str_len);
    close(client_socket);

    return 0;
}
