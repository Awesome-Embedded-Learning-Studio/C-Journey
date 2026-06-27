/* 注意:本文件与 SC1/server.c 内容一致。SC2 的"改进"在客户端
 * (SC2/client.c 改为逐字节读取并统计读取次数),服务端无需改动,
 * 此处保留一份让 SC2 能独立编译运行。 */
#include "../Utils/SC_Utils.h"
#define QUEUE_LENGTH 5


int main(int argc, char** agrv) {
    int server_socket;
    int client_socket;

    Sockaddr_in server_addr;
    Sockaddr_in client_addr;
    socklen_t client_addr_size;

    const char message[] = "Hello, World\0";

    if (argc != 2) {
        program_helper(agrv[0], "<port>\n");
    }

    server_socket = Socket(PF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(agrv[1]));

    Bind(server_socket, &server_addr, sizeof(server_addr));

    Listen(server_socket, QUEUE_LENGTH);

    client_addr_size = sizeof(client_addr);

    client_socket = Accept(server_socket, &client_addr, &client_addr_size);

    write(client_socket, message, sizeof(message));
    close(client_socket);
    close(server_socket);

    return 0;
}
