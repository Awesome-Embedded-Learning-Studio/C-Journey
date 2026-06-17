---
title: "Socket 编程：用 TCP 从零写一个客户端/服务端"
description: "把 socket 当成『网络版文件描述符』，亲手走通 TCP 服务端四件套与客户端两件套，踩掉字节序、read 返回 0、SIGPIPE 这几个经典坑。"
chapter: 5
order: 0
tags:
  - host
  - system-programming
  - networking
  - posix
difficulty: intermediate
reading_time_minutes: 14
platform: host
c_standard: [99, 11]
prerequisites:
  - "Chapter 0：编译流程与命令行基础"
  - "ROADMAP 阶段 5：系统编程"
related:
  - "IO 多路复用：select / poll / epoll"
  - "多线程：pthread"
---

# Socket 编程：用 TCP 从零写一个客户端/服务端

## 引言

说实话，我第一次写网络程序的时候是有点懵的——明明只是想"让两个进程说上话"，怎么一上来就是 `socket`、`bind`、`listen`、`accept` 一长串，而且每个函数签名长得还都挺像。后来我搞明白了一件事，**socket 这套 API 一点都不神秘，它就是把"网络"也塞进了 Linux "一切皆文件"那个筐里**。你拿到一个 socket，本质上就是拿到一个文件描述符（fd），剩下的事情跟读写本地文件几乎没区别：`write(fd, …)` 是把数据发出去，`read(fd, …)` 是把对方发过来的数据读进来。

所以这一章我们要做的，不是去背一堆 API，而是顺着"fd"这条线，亲手把一个最简的 TCP 服务端和客户端走通。配套示例就在 [examples/stage5-tcp-socket](../../examples/stage5-tcp-socket/)，我们边讲边对照里面的 SC1。

## 核心概念：一次 TCP 通信到底发生了什么

TCP 是面向连接、可靠的流式协议，这八个字翻译成人话就是：数据发出去之前，双方得先"握上手"，而且发出去的东西内核会替你保证送到、保证顺序。整个通信由服务端和客户端两个角色配合完成，每个动作都恰好对应一个 API，我们先把这张全景图记住，后面往里填血肉就不乱了：

```text
   服务端                                客户端
   -------                               -------
   socket()        创建套接字             socket()
     │                                    │
   bind()          绑定 IP:端口             │
     │                                    │
   listen()        开始监听                 │
     │                                    │
   accept()  ◄──── 阻塞等连接 ────►   connect()
     │           (三次握手在此完成)          │
     │                                    │
   read()/write()  ◄──── 数据收发 ────► read()/write()
     │                                    │
   close()         关闭                 close()
```

这里有个特别容易记混的地方，**服务端是四个动作 `socket → bind → listen → accept`，客户端却只有两个 `socket → connect`**。为什么服务端要多两步？因为服务端是被动的——它得先告诉操作系统"我在这个 IP、这个端口上候着"（`bind`），再声明"我愿意接客了，门口最多排 N 个"（`listen`），最后才真的从队列里领一个已经握完手的连接进来（`accept`）。客户端是主动出击，闷头 `connect` 就完事了。

## 七个 API，挨个拆

### socket()：先拿到那个 fd

```c
int socket(int domain, int type, int protocol);
```

`domain` 决定地址族，IPv4 用 `PF_INET`（和 `AF_INET` 等价）；`type` 决定语义，TCP 用 `SOCK_STREAM`，UDP 用 `SOCK_DGRAM`；返回值是一个非负的 fd。SC1 里写的是 `Socket(PF_INET, SOCK_STREAM, 0)`——注意这里是大写的 `Socket`，那是 [Utils/SC_Utils](../../examples/stage5-tcp-socket/Utils/) 封的 wrapper，出错会自动调 `error_handler`，省得我们每次都手写 `if (ret < 0) perror(...)`。

### bind()：把 fd 钉在一个地址上

```c
int bind(int fd, const struct sockaddr *addr, socklen_t len);
```

这一步是给 socket 安个"门牌号"——IP + 端口。服务端**必须** bind，否则客户端根本不知道往哪连；客户端通常让系统在连接时自动分配一个临时端口就行，所以一般不显式 bind。

### listen()：从"普通 socket"变成"迎宾 socket"

```c
int listen(int fd, int backlog);
```

调用 `listen` 之后，这个 fd 的身份就变了——它不再用来收发数据，只用来接连接，相当于挂上了"迎宾"的牌子。`backlog` 是等待队列的长度，SC1 传的是 5，意思是有 5 个已完成三次握手但还没被 `accept` 领走的连接可以暂存着。

### accept()：真正领一个连接进来

```c
int accept(int fd, struct sockaddr *addr, socklen_t *len);
```

这是整个流程里最容易被忽视、但最关键的一步。`accept` 会从"已完成握手的队列"里取出一个连接，**返回一个全新的 fd**——注意，是新 fd，不是原来那个监听 fd。之后服务端跟这个客户端的所有收发，都走这个新 fd。它还是个阻塞调用，队列里没货的时候就老老实实卡着等。

> **踩坑预警**：很多人把"监听 fd"和"连接 fd"搞混，拿监听 fd 去 `read`，结果永远读不到数据。你可以这么记——`listen` 的那个是"迎宾 socket"，站门口的，永远不聊天；`accept` 返回的才是"包间 socket"，真正聊天都在包间里。

### connect()：客户端主动伸手

```c
int connect(int fd, const struct sockaddr *addr, socklen_t len);
```

客户端拿着目标地址发起三次握手，成功之后这个 fd 就活过来了，可以收发数据。

### read() / write()：和读写文件一模一样

```c
ssize_t read (int fd, void *buf, size_t n);
ssize_t write(int fd, const void *buf, size_t n);
```

你看，签名和操作本地文件的那个 `read`/`write` 完全一样——这就是"一切皆文件"落地的地方。想要更细的控制（非阻塞、带外数据），可以换成 `recv`/`send`，它们多一个 flags 参数。

### close()：挂断

关掉 fd，内核在底下触发 TCP 的四次挥手，把连接体面地拆掉。

## 字节序：为什么到处都是 htonl / htons

现在回头看 SC1 服务端那三行赋值，你可能会问：端口和 IP 进结构体之前，为什么非得套一层 `htonl` / `htons`？

```c
server_addr.sin_family      = AF_INET;
server_addr.sin_addr.s_addr = htonl(INADDR_ANY);    // 监听所有网卡
server_addr.sin_port        = htons(atoi(argv[1])); // 命令行传进来的端口
```

原因在于字节序。我们日常用的 x86 CPU 是小端（低位字节放低地址），但网络协议规定走大端。如果把端口号 `9999` 直接按主机字节序塞进结构体，到了网络上就变成另一个数字了——本机测试能连、换台机器就死活连不上，这种 bug 能坑你一整天。所以规则很简单：**凡是多字节的整数（端口、IP）要进地址结构体，统统先转成网络字节序**。常用的几个转换函数速查如下：

| 函数 | 作用 |
|---|---|
| `htonl` / `htons` | host → network（long / short） |
| `ntohl` / `ntohs` | network → host |
| `inet_addr("127.0.0.1")` | 点分十进制 IP → 网络字节序整数（旧接口） |
| `inet_pton(AF_INET, "127.0.0.1", &addr)` | 推荐：IP 字符串 → 二进制 |
| `INADDR_ANY` | "本机所有网卡"，服务端常用，省得写死 IP |

## 跟着 SC1 跑一遍（真实输出）

很好，概念都凑齐了，现在我们把它们拼起来，直接在 [SC1](../../examples/stage5-tcp-socket/SC1/) 上跑一把。这个例子干的事情很简单：服务端起来等连接，有客户端连进来，就给它回一句 `Hello, World`。服务端的关键逻辑顺着读下来就是标准的四件套：

```c
server_socket = Socket(PF_INET, SOCK_STREAM, 0);    // 1. socket
Bind(server_socket, &server_addr, …);               // 2. bind
Listen(server_socket, 5);                           // 3. listen
client_socket = Accept(server_socket, …);           // 4. accept（阻塞）
write(client_socket, message, sizeof(message));     // 5. 发数据
close(client_socket); close(server_socket);         // 6. 关闭
```

客户端更短，`socket` 完直接 `connect`，然后 `read` 收数据：

```c
client_socket = Socket(PF_INET, SOCK_STREAM, 0);    // 1. socket
Connect(client_socket, &server_addr, …);            // 2. connect
str_len = read(client_socket, message, …);          // 3. 收数据
close(client_socket);                               // 4. 关闭
```

接下来我们实打实地编译运行一次。先 cmake 再 make，在我的机器上输出如下（真实日志，没编）：

```text
$ cd examples/stage5-tcp-socket/SC1 && mkdir build && cd build && cmake ..
-- Configuring done (1.7s)
-- Generating done (0.1s)

$ make
[ 50%] Linking C executable client
[ 50%] Built target client
[100%] Linking C executable server
[100%] Built target server
```

一个终端起服务端，另一个终端跑客户端连上去：

```text
# 终端 1
$ ./server 9999

# 终端 2
$ ./client 127.0.0.1 9999
received: 14 Bytes
Get the message: Hello, World
```

到这里你会发现 client 打印的是 **14 Bytes**，不是 13。这不是笔误——服务端写的是 `write(client_socket, message, sizeof(message))`，而 `message` 是 `"Hello, World\0"`，`sizeof` 把结尾那个 `\0` 也算进去了，所以发出去的是 14 个字节。这个"多一个 `\0`"的细节现在看着无害，等到后面我们做"按消息边界收发"的时候就会回头咬人，先在这里埋个点。

## 常见坑（真正的坑在后面）

把上面的例子跑通只算热身，真正会让你调到怀疑人生的，是下面这几个：

> **坑 1：端口被占用 / 重启连不上。** 服务端重启的时候，上一个连接很可能还卡在 `TIME_WAIT` 状态没释放端口，于是 `bind` 直接报 `Address already in use`。解法是打开地址复用：
> ```c
> int opt = 1;
> setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
> ```
> 这一行的位置很讲究——必须在 `bind` **之前**调用，否则不生效。

> **坑 2：把 `read` 返回 0 当成错误。** 很多人一看到 `read` 不返回正数就以为出错了。其实 `read` 返回 `0` 表示的是**对端关闭了连接**（读到 EOF），这是正常的结束信号，不是错误。把它当错误处理，往往会陷进死循环或者逻辑错乱。真正出错是返回 `-1`，而且返回 `-1` 时还得看 `errno`——如果是 `EINTR`（被信号打断了），正确做法是重试，而不是直接退出。

> **坑 3：SIGPIPE 直接杀进程。** 如果你往一个已经被对端关闭的连接上 `write`，内核会给你发一个 `SIGPIPE` 信号，而这个信号的默认行为是**直接把进程干掉**，连个遗言都不留。线上服务莫名其妙挂了，八成是它。要么在程序开头 `signal(SIGPIPE, SIG_IGN)` 忽略掉，要么用 `send(fd, buf, n, MSG_NOSIGNAL)` 走带标志的发送接口。

> **坑 4：把 TCP 当成"消息"。** TCP 是字节流，不是消息边界。你一次 `write` 100 字节，对端可能分两次 `read` 才收全，也可能跟下一条消息粘在一起一次收到——这就是常说的"粘包"。要解决就得自己在应用层定边界，比如每条消息前面带个长度字段，或者用特定分隔符。SC1 这种"发一句就关连接"的玩法之所以没事，是因为它根本不依赖边界。

## 小结

走完这一趟，TCP socket 的骨架应该清楚了，关键点 checklist：

- [ ] socket 的本质是 fd，`read`/`write` 和文件通用
- [ ] 服务端四件套 `socket → bind → listen → accept`，客户端两件套 `socket → connect`
- [ ] `accept` 返回的是**新** fd，监听 fd 只负责迎宾
- [ ] 端口、IP 进结构体前一律 `htonl`/`htons`
- [ ] `read` 返回 0 是对端关闭，不是出错；返回 -1 才是，且 `EINTR` 要重试
- [ ] 别忘了 `SO_REUSEADDR` 和处理 `SIGPIPE`

## 练习

配套示例从 SC1 到 SC4 是递进的，挨个挑战：

- [ ] SC2：把 SC1 改成能循环服务多个客户端（一次服务完别急着退出，回去再 `accept`）
- [ ] SC3：实现一个 echo 服务，收到什么就原样回什么
- [ ] SC4：把接口封装得更通用一点，体会 wrapper 的价值

做完这些，下一个自然的问题就来了——上面这套一次只能服务一个连接，服务端在 `accept` 之后要是慢慢处理，后面的客户端全得排队。要同时服务很多人，就得引入并发，那就是阶段 5 的另外两块：多线程（`pthread`）和 IO 多路复用（`epoll`）。

## 参考资源

- `man 2 socket` / `man 2 accept` / `man 7 tcp`——最权威的一手资料，遇到拿不准的参数先查 man
- W. Richard Stevens 等，《UNIX Network Programming, Volume 1: The Sockets Networking API》——网络编程经典
- Michael Kerrisk，《The Linux Programming Interface》——Linux 系统 API 的百科全书（第 4 部分讲 socket）

---
*配套示例整理自 2023–2024 学习存档。*
