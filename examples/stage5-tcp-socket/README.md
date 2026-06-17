# 阶段 5 示例：TCP Socket 编程

> 对应 [ROADMAP](../../ROADMAP.md) · **阶段 5：系统编程** · 网络编程

从最简 TCP 服务端 / 客户端起步，逐步实现 echo 服务，练熟 POSIX socket API。每个子目录（SC = Socket Communication）是一个递进的练习。

## 目录

| 目录 | 内容 |
|---|---|
| `SC1/` | 最简 `server.c` / `client.c`（`accept` + `connect` + `recv`/`send`） |
| `SC2/` | 改进版收发流程 |
| `SC3/` | echo 回显服务 `echo_server.c` / `echo_client.c` |
| `SC4/` | 进一步封装 |
| `Utils/` | 公共工具 `SC_Utils`（错误处理、封装 API） |

每个子目录均带 `CMakeLists.txt`。

## 如何使用

```bash
cd SC1 && mkdir build && cd build && cmake .. && make
# 终端 1
./server
# 终端 2
./client 127.0.0.1
```

## 学习要点

- `socket` / `bind` / `listen` / `accept` / `connect` 五件套
- 字节序转换 `htonl` / `htons` / `inet_pton`
- 阻塞 IO 与基本错误处理

---
*整理自 2023–2024 学习存档。*
