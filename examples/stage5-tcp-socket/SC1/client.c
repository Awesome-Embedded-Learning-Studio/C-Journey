#include "../Utils/SC_Utils.h"

int main(int argc, char const *argv[])
{
    int client_socket;
    Sockaddr_in server_addr;
    char message[30]; // ready for the msg
    int str_len;

    if(argc != 3){
        program_helper(argv[0],"<IP> <Port>");
    }

    client_socket = Socket(PF_INET, SOCK_STREAM, 0);

    ZeroMem(&server_addr,sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(atoi(argv[2]));

    Connect(client_socket, &server_addr, sizeof(server_addr));

    str_len = read(client_socket, message, sizeof(message) - 1);
    if(str_len == -1){
        error_handler("Read Error");
    } 
    printf("received: %d Bytes\n", str_len);

    message[29] = '\0';

    printf("Get the message: %s\n", message);

    close(client_socket);

    return 0;
}
