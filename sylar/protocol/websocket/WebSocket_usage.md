# WebSocket 使用文档

> 原作者：katzarsky@gmail.com  
> 版本：v1.00 (2012-09-13)  
> 协议：RFC 6455 (RFC6544)

纯 C++ 实现的 WebSocket 编解码器，**不涉及网络 IO**，只负责握手解析、帧编码、帧解码，可直接嵌入任何网络框架。

---

## 依赖

源码依赖以下三个小型库，需要一并引入：

| 库 | 用途 |
|---|---|
| `sha1/sha1.h` | 握手 Accept Key 计算 |
| `base64/base64.h` | Accept Key Base64 编码 |

---

## 数据结构

### `WebSocketFrameType` 枚举

函数返回值和参数都用这个枚举表示帧类型。

```cpp
enum WebSocketFrameType {
    ERROR_FRAME            = 0xFF00, // 解析出错
    INCOMPLETE_FRAME       = 0xFE00, // 数据不完整，需要继续读取

    OPENING_FRAME          = 0x3300, // 握手请求帧
    CLOSING_FRAME          = 0x3400, // 关闭帧

    INCOMPLETE_TEXT_FRAME   = 0x01,  // 未完整的文本分片帧
    INCOMPLETE_BINARY_FRAME = 0x02,  // 未完整的二进制分片帧

    TEXT_FRAME             = 0x81,   // 完整文本帧
    BINARY_FRAME           = 0x82,   // 完整二进制帧

    PING_FRAME             = 0x19,   // Ping 帧
    PONG_FRAME             = 0x1A,   // Pong 帧
};
```

### `WebSocket` 类

每个连接对应一个实例，握手完成后保存连接元信息。

```cpp
class WebSocket {
public:
    string resource;  // 请求路径，如 "/chat"
    string host;      // Host 头
    string origin;    // Origin 头
    string protocol;  // Sec-WebSocket-Protocol
    string key;       // Sec-WebSocket-Key（握手用，内部使用）
};
```

---

## API 说明

### 握手

#### `parseHandshake` — 解析客户端握手请求

```cpp
WebSocketFrameType parseHandshake(unsigned char* input_frame, int input_len);
```

把从 TCP 读到的原始 HTTP 升级请求喂进去，解析并保存到成员变量。

**返回值：**
- `OPENING_FRAME` — 解析成功，可以调用 `answerHandshake()`
- `INCOMPLETE_FRAME` — 数据不完整，继续读取后重试
- `ERROR_FRAME` — 解析失败

**示例：**
```cpp
WebSocket ws;
char buf[4096];
int len = recv(sockfd, buf, sizeof(buf), 0);

WebSocketFrameType type = ws.parseHandshake((unsigned char*)buf, len);
if (type == OPENING_FRAME) {
    // 握手成功，发送响应
    string response = ws.answerHandshake();
    send(sockfd, response.c_str(), response.size(), 0);
}
```

---

#### `answerHandshake` — 构建 101 握手响应

```cpp
string answerHandshake();
```

必须在 `parseHandshake` 返回 `OPENING_FRAME` 之后调用，内部自动计算 `Sec-WebSocket-Accept`。

**返回值：** 完整的 HTTP 101 响应字符串，直接 `send` 给客户端即可。

**响应示例：**
```
HTTP/1.1 101 Switching Protocols
Upgrade: WebSocket
Connection: Upgrade
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
```

---

### 接收帧

#### `getFrame` — 解析收到的 WebSocket 帧

```cpp
WebSocketFrameType getFrame(
    unsigned char* in_buffer,   // 输入：从 TCP 读到的原始数据
    int            in_length,   // 输入：数据长度
    unsigned char* out_buffer,  // 输出：解码后的 payload
    int            out_size,    // 输入：out_buffer 的容量上限
    int*           out_length   // 输出：payload 实际长度（含结尾 \0）
);
```

**返回值：**

| 返回值 | 含义 | 处理方式 |
|---|---|---|
| `TEXT_FRAME` | 完整文本帧 | 读取 out_buffer |
| `BINARY_FRAME` | 完整二进制帧 | 读取 out_buffer |
| `PING_FRAME` | Ping | 回复 Pong |
| `PONG_FRAME` | Pong | 心跳确认，忽略或记录 |
| `INCOMPLETE_FRAME` | 数据未读完 | 继续 recv 后重试 |
| `ERROR_FRAME` | 帧格式错误 | 关闭连接 |

**示例：**
```cpp
unsigned char in_buf[4096];
unsigned char out_buf[4096];
int out_len = 0;

int in_len = recv(sockfd, in_buf, sizeof(in_buf), 0);

WebSocketFrameType type = ws.getFrame(in_buf, in_len, out_buf, sizeof(out_buf), &out_len);

switch (type) {
    case TEXT_FRAME:
        // out_buf 是解码后的文本，out_len 包含结尾 \0
        printf("收到文本: %s\n", out_buf);
        break;
    case BINARY_FRAME:
        // out_buf 是原始二进制数据，长度是 out_len - 1
        break;
    case PING_FRAME:
        // 必须回 Pong，否则客户端会断开
        sendPong(sockfd, ws);
        break;
    case INCOMPLETE_FRAME:
        // 继续读取，拼接后重试
        break;
    case ERROR_FRAME:
        close(sockfd);
        break;
}
```

> **注意：** `out_length` 的值是 `payload长度 + 1`（含结尾 `\0`），实际 payload 长度是 `*out_length - 1`。

---

### 发送帧

#### `makeFrame` — 构建 WebSocket 帧

```cpp
int makeFrame(
    WebSocketFrameType frame_type,  // 帧类型
    unsigned char*     msg,         // 要发送的原始数据
    int                msg_len,     // 数据长度
    unsigned char*     buffer,      // 输出缓冲区
    int                buffer_size  // 缓冲区容量
);
```

**返回值：** 构建好的帧总字节数，直接 `send` 这么多字节即可。

**buffer 容量建议：** `msg_len + 10`（帧头最大 10 字节）。

**示例：**
```cpp
// 发送文本
void sendText(int sockfd, WebSocket& ws, const string& text) {
    unsigned char buf[4096 + 10];
    int frame_len = ws.makeFrame(
        TEXT_FRAME,
        (unsigned char*)text.c_str(),
        text.size(),
        buf,
        sizeof(buf)
    );
    send(sockfd, buf, frame_len, 0);
}

// 回复 Pong
void sendPong(int sockfd, WebSocket& ws) {
    unsigned char buf[10];
    int frame_len = ws.makeFrame(PONG_FRAME, nullptr, 0, buf, sizeof(buf));
    send(sockfd, buf, frame_len, 0);
}

// 发送关闭帧
void sendClose(int sockfd, WebSocket& ws) {
    unsigned char buf[10];
    int frame_len = ws.makeFrame(CLOSING_FRAME, nullptr, 0, buf, sizeof(buf));
    send(sockfd, buf, frame_len, 0);
}
```

---

## 完整流程示例

```cpp
#include "WebSocket.h"
#include <sys/socket.h>

void handleConnection(int sockfd) {
    WebSocket ws;
    unsigned char buf[8192];
    unsigned char out[8192];

    // ── 1. 握手 ──────────────────────────────────────────
    int len = recv(sockfd, buf, sizeof(buf), 0);
    if (ws.parseHandshake(buf, len) != OPENING_FRAME) {
        close(sockfd);
        return;
    }
    string response = ws.answerHandshake();
    send(sockfd, response.c_str(), response.size(), 0);

    printf("连接建立，路径: %s\n", ws.resource.c_str());

    // ── 2. 收发消息 ───────────────────────────────────────
    while (true) {
        len = recv(sockfd, buf, sizeof(buf), 0);
        if (len <= 0) break;

        int out_len = 0;
        WebSocketFrameType type = ws.getFrame(buf, len, out, sizeof(out), &out_len);

        if (type == TEXT_FRAME) {
            printf("收到: %s\n", out);

            // Echo 回去
            unsigned char frame[8192 + 10];
            int frame_len = ws.makeFrame(TEXT_FRAME, out, out_len - 1, frame, sizeof(frame));
            send(sockfd, frame, frame_len, 0);
        }
        else if (type == PING_FRAME) {
            unsigned char pong[10];
            int pong_len = ws.makeFrame(PONG_FRAME, nullptr, 0, pong, sizeof(pong));
            send(sockfd, pong, pong_len, 0);
        }
        else if (type == ERROR_FRAME || type == CLOSING_FRAME) {
            break;
        }
        // INCOMPLETE_FRAME: 生产环境需要缓冲拼接，此处略
    }

    close(sockfd);
}
```

---

## 已知限制

| 问题 | 说明 |
|---|---|
| **粘包/半包** | `getFrame` 不处理粘包，一次 `recv` 必须恰好是一帧，生产环境需要自己做缓冲拼接 |
| **分片消息** | `INCOMPLETE_TEXT_FRAME` / `INCOMPLETE_BINARY_FRAME` 返回后没有提供拼装机制，需自行实现 |
| **out_length 含 \0** | `*out_length` 是 `payload + 1`，实际长度要减 1 |
| **out_buffer 溢出** | `payload_length > out_size` 时没有截断保护，调用方需确保缓冲足够大 |
| **线程安全** | 一个 `WebSocket` 实例对应一个连接，多连接需各自独立实例 |
| **2012年代码** | 不支持 permessage-deflate 压缩扩展 |
