/*
 * client.c —— 客户端（在 Windows 上编译运行）
 *
 * 本文件是「联合调试」示例的另一半。服务端是 server.c，运行在 WSL 或 Linux 中。
 *
 * 编译（Windows + MinGW）：
 *   gcc -g -O0 -Wall client.c -o client.exe -lws2_32
 *
 * 联合调试时建议在 send_request 与 main 的收结果处各下一个断点，
 * 与服务端的断点配合观察数据跨越系统边界的过程。
 */
#include <stdio.h>
#include <string.h>

#include "common.h"

/* 连接服务端。
 *
 * 服务端在 WSL 中时，Windows 可以直接用 127.0.0.1 连接——
 * WSL2 提供了本机到 WSL 的端口转发，无需查询虚拟网卡地址。
 * 服务端在 Hyper-V 虚拟机中时，需要改成虚拟机的实际地址。
 */
static sock_t connect_to_server(const char *host)
{
    sock_t fd;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) {
        fprintf(stderr, "socket 创建失败: %d\n", WSAGetLastError());
        return INVALID_SOCKET;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(DEMO_PORT);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "地址无效: %s\n", host);
        CLOSE_SOCKET(fd);
        return INVALID_SOCKET;
    }

    printf("正在连接 %s:%d ……\n", host, DEMO_PORT);
    fflush(stdout);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "连接失败: %d\n", WSAGetLastError());
        CLOSE_SOCKET(fd);
        return INVALID_SOCKET;
    }

    printf("已连接\n");
    fflush(stdout);
    return fd;
}

static int send_request(sock_t fd, int a, int b, Response *out)
{
    Request req;

    req.a = a;
    req.b = b;

    if (write_full(fd, &req, (int)sizeof(req)) != (int)sizeof(req)) {
        fprintf(stderr, "发送请求失败\n");
        return -1;
    }

    memset(out, 0, sizeof(*out));
    if (read_full(fd, out, (int)sizeof(*out)) != (int)sizeof(*out)) {
        fprintf(stderr, "读取应答失败\n");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    WSADATA wsa;
    sock_t fd;
    Response resp;
    const char *host = (argc > 1) ? argv[1] : "127.0.0.1";
    int a = (argc > 2) ? atoi(argv[2]) : 3;
    int b = (argc > 3) ? atoi(argv[3]) : 4;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup 失败\n");
        return 1;
    }

    fd = connect_to_server(host);
    if (fd == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    printf("发送请求: a=%d b=%d\n", a, b);
    fflush(stdout);

    if (send_request(fd, a, b, &resp) != 0) {
        CLOSE_SOCKET(fd);
        WSACleanup();
        return 1;
    }

    printf("收到应答: sum=%d steps=%d\n", resp.sum, resp.steps);
    printf("校验: %d + %d = %d → %s\n", a, b, resp.sum,
           (resp.sum == a + b) ? "正确" : "不一致");
    fflush(stdout);

    CLOSE_SOCKET(fd);
    WSACleanup();
    return (resp.sum == a + b) ? 0 : 1;
}
