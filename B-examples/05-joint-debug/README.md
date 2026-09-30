# 示例 05 · 联合调试

一个窗口中同时调试两个相互通信的程序：

```
┌─────────────────────┐          TCP          ┌─────────────────────┐
│  Windows 客户端      │ ────────────────────→ │  Linux 服务端        │
│  client.exe          │ ←──────────────────── │  server              │
│  GDB 在 Windows 上   │      127.0.0.1:34567  │  GDB 在 WSL 里       │
└─────────────────────┘                       └─────────────────────┘
```

---

## 文件说明

| 文件 | 说明 |
|---|---|
| `common.h` | 通信协议定义，两端共用。修改协议只需改这一处 |
| `server.c` | Linux 服务端。监听端口，接收两个整数，累加求和后返回 |
| `client.c` | Windows 客户端。连接服务端，发送两个整数，校验结果 |
| `.vscode/tasks.json` | 两端的构建任务 |
| `.vscode/launch.json` | 两个调试配置 + 一个复合启动 |

---

## 前置条件

| 项目 | 要求 |
|---|---|
| WSL | 已安装 Ubuntu，且其中已装 `gcc`、`gdb` |
| Windows 编译器 | MinGW 的 `gcc` 已在 PATH 中 |
| VS Code 扩展 | C/C++（`ms-vscode.cpplite` 之外的那个 `ms-vscode.cpptools`） |

WSL 环境的搭建见《02-调试器/02-跨系统调试.md》第 2 章。

**若你的 WSL 发行版不叫 `Ubuntu`**，需要修改两处：

- `.vscode/tasks.json` 中构建服务端任务的 `-d` 参数
- `.vscode/launch.json` 中 `pipeTransport.pipeArgs` 的 `-d` 参数

---

## 运行

1. 用 VS Code 打开**本文件夹**（不是上级目录）

   VS Code 只识别工作区根目录下的 `.vscode`，因此必须单独打开本目录。

2. 在「运行和调试」面板顶部的下拉框中选择 **`联合调试: 客户端 + 服务端`**
3. 按 `F5`

两个会话会同时启动，两边的断点都会生效。

---

## 建议的观察点

按数据流动的顺序，在下列位置下断点：

| 顺序 | 位置 | 观察内容 |
|---|---|---|
| 1 | 客户端 `send_request` | `req.a` 与 `req.b` 的发送值 |
| 2 | 服务端 `compute_sum` | 收到的 `a`、`b` 是否与客户端一致 |
| 3 | 服务端 `compute_sum` 循环内 | `acc` 与 `step` 的逐步变化 |
| 4 | 客户端 `main` 收结果处 | `resp.sum` 是否等于 `a + b` |

这样可以看到一个值从 Windows 出发、在 Linux 被处理、再回到 Windows 的完整过程。

---

## 命令行验证方式

不使用 VS Code 时，也可以用命令行复现同样的场景。

**注意**：Windows 的 GDB **不能**连接 Linux 的 gdbserver（原因见
《02-调试器/02-跨系统调试.md》第 4.6 节），因此下面的 GDB 命令要在 **WSL 内**执行。

### 1. 构建两端

```powershell
# Windows 客户端
gcc -g -O0 -Wall -finput-charset=UTF-8 -fexec-charset=GBK client.c -o build\client.exe -lws2_32
```

```bash
# Linux 服务端（在 WSL 中）
gcc -g -O0 -Wall server.c -o server
```

### 2. 在 WSL 中用 gdbserver 托管服务端

```bash
gdbserver :2345 ./server
```

### 3. 在 WSL 中另开一个终端，用 GDB 连接

把下列内容保存为 `j.gdb`：

```
set pagination off
target remote 127.0.0.1:2345
break compute_sum
continue
bt
info args
continue
quit
```

然后执行：

```bash
gdb --batch -x j.gdb
```

> **注意**：这里用 `-x 脚本文件` 而不是 `-ex`。本机的 GDB 会把含空格的 `-ex` 参数按空格拆开，
> 导致命令失效（见《02-调试器/02-跨系统调试.md》第 6.9 节）。

### 4. 在 Windows 上运行客户端

```powershell
build\client.exe 127.0.0.1 3 4
```

### 实测输出

```
Windows 客户端:
  发送请求: a=3 b=4
  收到应答: sum=7 steps=4
  校验: 3 + 4 = 7 → 正确

Linux 侧 GDB:
  Breakpoint 1, compute_sum (a=3, b=4) at server.c:29
  29	    int acc = a;
  #0  compute_sum (a=3, b=4) at server.c:29
  #1  0x0000555555555508 in handle_client (fd=4) at server.c:54
  #2  0x0000555555555718 in main () at server.c:106
  a = 3
  b = 4
```

---

## 原理要点

### 为什么服务端要由 WSL 里的 GDB 调试

Windows 版 GDB 的编译参数是：

```
--target=x86_64-w64-mingw32  --enable-targets=x86_64-w64-mingw32
```

**目标只有 Windows。** 它无法调试 Linux 程序，即使通过 gdbserver 也不行。

`pipeTransport` 的做法是让 `wsl.exe` 在 Linux 侧启动 GDB，VS Code 通过管道与它交换 MI 文本。
因此**调试器运行在 Linux 上**，符合「调试器必须与目标程序同侧」的原则。

### 为什么客户端可以用 Windows 的 GDB

客户端是 Windows 程序，调试器与目标同侧，属于普通的本机调试。

### 关于中文路径

本工程位于含中文的路径下，但**服务端调试不受影响**——Linux 侧的 GDB 原生使用 UTF-8。

受影响的只有 Windows 侧：`client.c` 的编译输出路径使用了反斜杠
（`${workspaceFolder}\build\client.exe`），原因见《01-编译器/02-环境配置.md》第 9 章第 5 节。

### 关于 client.c 的编译选项

| 选项 | 作用 |
|---|---|
| `-lws2_32` | 链接 Windows 套接字库，必须放在命令行末尾 |
| `-finput-charset=UTF-8` | 源文件按 UTF-8 读取 |
| `-fexec-charset=GBK` | 字符串常量按 GBK 写入，匹配中文控制台 |

### 关于 gdbserver 的地址

`pipeTransport` 通过管道通信，不涉及网络地址，无需关心。
但若手工使用 gdbserver，需要注意 WSL 的端口转发行为并不一致（<sup>实测</sup>：
同一实例上某测试服务可用 `127.0.0.1` 访问，而 gdbserver 只能用 WSL 的实际地址）。
连接被拒时两个地址都试一下。

---

## 相关文档

| 文档 | 内容 |
|---|---|
| 《02-调试器/02-跨系统调试.md》 | WSL 环境搭建、联合调试原理、远程调试 |
| 《01-编译器/02-环境配置.md》 | Windows 侧配置、中文路径限制、字段参考 |
