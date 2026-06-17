#include "../Utils/SC_Utils.h"
#define BUF_SIZE 1024
#define OP_SIZE 4
#define RLT_SIZE 4
int main(int argc, char const *argv[])
{
    int server_socket;
    char opmsg[BUF_SIZE];
    int result, opnd_cnt, i;
    Sockaddr_in server_addr;
    server_socket = Socket(PF_INET, SOCK_STREAM, 0);
    ZeroMem(&server_addr,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(atoi(argv[2]));

    Connect(server_socket, &server_addr, sizeof(server_addr));
    puts("Connecting ...");
    fputs("Operand count: ",stdout);
    scanf("%d", &opnd_cnt);
    opmsg[0] = (char)opnd_cnt;
    for(int i = 0; i < opnd_cnt; i++)
    {
        printf("Operator %d: ", i+1);
        scanf("%d", (int*)&opmsg[i*OP_SIZE + 1]);
    }

    getchar();
    printf("Operator: ");
    scanf("%c", &opmsg[opnd_cnt*OP_SIZE + 1]);
    write(server_socket, opmsg, opnd_cnt*OP_SIZE + 2);
    read(server_socket, &result, RLT_SIZE);

    printf("Res: %d\n",result);
    close(server_socket);

    return 0;
}
