/*
 * server.c —— 服务端（在 WSL 或 Linux 虚拟机中编译运行）
 *
 * 本文件是「联合调试」示例的一半。另一半是 client.c，运行在 Windows 上。
 *
 * 编译（在 Linux 中）：
 *   gcc -g -O0 -Wall server.c -o server
 *
 * 联合调试时建议在 compute_sum 与 handle_client 中各下一个断点，
 * 观察数据如何从 Windows 侧的客户端传进来。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "common.h"

/* 累加求和。
 *
 * 刻意写成逐次累加而不是 a + b，是为了在调试时能观察循环过程：
 * 单步执行时可以看着 acc 一格一格地变化。
 */
static int compute_sum(int a, int b)
{
    int acc = a;
    int step = 0;

    while (step < b) {
        acc += 1;
        step += 1;
    }

    return acc;
}

static void handle_client(int fd)
{
    Request req;
    Response resp;

    memset(&req, 0, sizeof(req));
    if (read_full(fd, &req, (int)sizeof(req)) != (int)sizeof(req)) {
        fprintf(stderr, "读取请求失败\n");
        return;
    }

    printf("收到请求: a=%d b=%d\n", req.a, req.b);
    fflush(stdout);

    resp.sum   = compute_sum(req.a, req.b);
    resp.steps = req.b;

    printf("计算结果: sum=%d steps=%d\n", resp.sum, resp.steps);
    fflush(stdout);

    write_full(fd, &resp, (int)sizeof(resp));
}

int main(void)
{
    int listen_fd, conn_fd;
    struct sockaddr_in addr;
    int opt = 1;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(DEMO_PORT);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 1) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    printf("服务端已启动，监听端口 %d，等待客户端连接……\n", DEMO_PORT);
    fflush(stdout);

    conn_fd = accept(listen_fd, NULL, NULL);
    if (conn_fd < 0) {
        perror("accept");
        close(listen_fd);
        return 1;
    }

    printf("客户端已连接\n");
    fflush(stdout);

    handle_client(conn_fd);

    close(conn_fd);
    close(listen_fd);
    printf("服务端结束\n");
    fflush(stdout);
    return 0;
}
