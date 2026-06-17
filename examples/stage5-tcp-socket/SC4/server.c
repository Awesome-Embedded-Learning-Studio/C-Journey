#include "../Utils/SC_Utils.h"
#define BUF_SIZE 1024
#define OP_SIZE 4
#define QUEUE_LENGTH 5
int calculate(int opnum, int opnds[], char operator);

int main(int argc, char const *argv[])
{
    int server_socket, client_socket;
    char opinfo[BUF_SIZE];
    int result, opnc_cnt, i;
    int receive_cnt, receive_len;
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
        opnc_cnt = 0;
        client_socket = Accept(server_socket, &client_addr, &client_addr_size);
        read(client_socket, &opnc_cnt, 1);

        receive_len = 0;
        while((opnc_cnt*OP_SIZE + 1) > receive_len)
        {
            receive_cnt = read(client_socket, &opinfo[receive_len], BUF_SIZE - 1);
            receive_len += receive_cnt; 
        }

        result = calculate(opnc_cnt, (int*)opinfo, opinfo[receive_len - 1]);
        write(client_socket, (char*)&result, sizeof(result));
        close(client_socket);
    }   
    close(server_socket);

    return 0;
}

int calculate(int opnum, int opnds[], char operator)
{
    int result = opnds[0];
    switch (operator)
    {
    case '+':
        for(int i = 1; i < opnum; i++) result += opnds[i];
        break;
    case '-':
        for(int i = 1; i < opnum; i++) result -= opnds[i];
        break;
    case '*':
        for(int i = 1; i < opnum; i++) result *= opnds[i];
        break;
    
    default:
        break;
    }

    return result;
}
