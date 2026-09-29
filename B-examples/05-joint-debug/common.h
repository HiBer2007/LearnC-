/*
 * common.h —— 联合调试示例的通信协议
 *
 * 客户端与服务端共用这一份定义，因此协议只有一处描述。
 * 两端各自包含本文件，修改时不会出现「只改了一边」的问题。
 *
 * 由于客户端在 Windows 上、服务端在 Linux 上，两边的套接字类型不同
 * （Windows 是 SOCKET，Linux 是 int），这里用 sock_t 统一。
 */
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET sock_t;
  #define CLOSE_SOCKET(s) closesocket(s)
#else
  #include <sys/socket.h>
  #include <unistd.h>
  typedef int sock_t;
  #define CLOSE_SOCKET(s) close(s)
#endif

/* 监听端口。改为其他值时，客户端与服务端要一起改——
   但因为共用本文件，实际只需要改这一处。 */
#define DEMO_PORT 34567

/* 一次请求：两个待相加的整数 */
typedef struct {
    int32_t a;
    int32_t b;
} Request;

/* 一次应答：和，以及服务端计算所用的步数 */
typedef struct {
    int32_t sum;
    int32_t steps;
} Response;

/* 读写固定长度的数据。
 *
 * 网络上一次 recv/send 未必能传完整个结构体，因此需要循环补齐。
 * 这部分逻辑两端相同，放在头文件里以免重复实现。
 */
static int read_full(sock_t fd, void *buf, int len)
{
    char *p = (char *)buf;
    int got = 0;
    while (got < len) {
        int n = (int)recv(fd, p + got, len - got, 0);
        if (n <= 0) return got;
        got += n;
    }
    return got;
}

static int write_full(sock_t fd, const void *buf, int len)
{
    const char *p = (const char *)buf;
    int sent = 0;
    while (sent < len) {
        int n = (int)send(fd, p + sent, len - sent, 0);
        if (n <= 0) return sent;
        sent += n;
    }
    return sent;
}

#endif /* COMMON_H */
