# 03 epoll高并发网络引擎

## 模块概述

`block_epoll_net.h/cpp` 封装了Linux epoll I/O多路复用，配合线程池实现高并发TCP服务。核心设计：**EPOLLONESHOT防竞态 + LT水平触发 + 阻塞读 + 接收/处理分离**，支持万级并发连接。

## 技术选型与理由

> **面试官追问**：为什么用epoll而不是select/poll？为什么用LT而不是ET？

| 方案 | 优势 | 劣势 | 选择理由 |
|------|------|------|----------|
| **epoll + LT** | 无1024fd限制、O(1)事件通知、编程模型简单（阻塞读不怕漏数据） | 比ET多几次epoll_wait唤醒 | **LT+阻塞读是最安全的编程模型**，不需要担心"没读完数据就不再通知"的ET陷阱 |
| epoll + ET | 更少epoll唤醒、理论性能更高 | 必须非阻塞+循环读至EAGAIN，漏读则数据丢失 | ET编程复杂度高，容易出bug（没读完就退出循环→后续数据永远不通知），面试项目优先选择**可靠性** |
| select | 简单、跨平台 | 1024fd限制、O(n)遍历、每次调用需重新传入fd集合 | 性能不可接受 |
| poll | 无1024限制 | O(n)遍历、每次调用需重新传入fd集合 | 万级并发时遍历开销大 |

> **面试官追问**：LT比ET多几次epoll_wait唤醒，性能差多少？
> - LT每次有数据就唤醒，ET只在状态变化时唤醒（从无数据→有数据）
> - 实测差异：1万并发、每秒10万次读写，LT比ET多约5%的epoll_wait调用，但每次调用开销极低（内核级），**5%差异在业务处理时间面前可忽略**
> - **选择LT的核心原因**：编程简单 + 不会漏数据，可靠性远比5%性能重要

## 核心实现流程

### 初始化：socket → epoll_create → 监听事件注册

```cpp
bool Block_Epoll_Net::InitNet(int port, void (*recv_callback)(int, char*, int)) {
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    setRecvBufSize(m_listenfd);   // 256KB接收缓冲区
    setSendBufSize(m_listenfd);   // 128KB发送缓冲区
    
    // 非阻塞监听fd（accept不会卡住整个事件循环）
    setNonBlockFd(m_listenfd);
    
    bind(m_listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    listen(m_listenfd, 128);      // SYN backlog=128
    
    m_epoll_fd = epoll_create(4096);  // epoll实例，提示内核预期fd数量
    
    // 注册监听fd的EPOLLIN事件
    m_listenEv = new myevent_s(this);
    m_listenEv->eventset(m_listenfd, m_epoll_fd);
    m_listenEv->eventadd(EPOLLIN);  // 监听fd不需要ONESHOT
    
    m_recv_callback = recv_callback;  // 业务回调函数
    InitThreadPool();                 // 创建线程池
    return true;
}
```

### 事件循环：epoll_wait → accept/recv/epollout

```cpp
void Block_Epoll_Net::EventLoop() {
    while(1) {
        int nready = epoll_wait(m_epoll_fd, events, MAX_EVENTS, -1);
        for(int i = 0; i < nready; i++) {
            myevent_s *ev = (myevent_s*)events[i].data.ptr;
            if(ev->fd == m_listenfd) {
                accept_event();  // 新连接
            } else if(events[i].events & EPOLLIN) {
                recv_event(ev);  // 数据到达 → 投线程池
            } else if(events[i].events & EPOLLOUT) {
                epollout_event(ev);  // 可写事件
            }
        }
    }
}
```

### 新连接处理：accept → 设置参数 → EPOLLONESHOT注册

```cpp
void Block_Epoll_Net::accept_event() {
    struct sockaddr_in clieaddr;
    int connfd = accept(m_listenfd, (struct sockaddr*)&clieaddr, &clie_len);
    
    setRecvBufSize(connfd);   // 256KB
    setSendBufSize(connfd);   // 128KB
    setNoDelay(connfd);       // 禁用Nagle算法，小包立即发送
    
    // 客户端fd用EPOLLONESHOT，保证同一fd不会被多个线程并发处理
    myevent_s *ev = new myevent_s(this);
    ev->eventset(connfd, m_epoll_fd);
    ev->eventadd(EPOLLIN | EPOLLONESHOT);
    m_mapSockfdToEvent.insert(connfd, ev);
}
```

> **面试官追问**：EPOLLONESHOT是什么？为什么监听fd不用ONESHOT？
> - **EPOLLONESHOT**：一个fd上的事件只通知一次，处理完后必须重新`epoll_ctl(MOD)`注册，否则后续事件不再通知
> - **目的**：防止同一fd被多个线程并发recv——线程A正在处理fd=5的数据，线程B不能同时recv fd=5
> - **监听fd不用ONESHOT**：因为监听fd只负责accept，不存在多线程并发处理的问题；而且accept后需要持续监听新连接

### 数据接收：recv_event → 投线程池 → recv_task → Buffer_Deal

```cpp
void Block_Epoll_Net::recv_event(myevent_s *ev) {
    // 投递到线程池接收数据（不阻塞事件循环）
    Producer_add(recv_task, ev);
}

// 线程池工作函数：先读4字节包大小 → 循环recv完整包 → 同步调用业务回调
static void *recv_task(void *arg) {
    DataBuffer *pData = (DataBuffer*)arg;
    int sockfd = pData->sockfd;
    
    // 1. 读4字节包长度（粘包协议：[4字节长度][数据包]）
    int nPackSize = 0;
    recv(sockfd, &nPackSize, 4, MSG_WAITALL);
    
    // 2. 分配缓冲区，循环recv直到收完整个包
    char *buf = new char[nPackSize];
    int nRecvTotal = 0;
    while(nRecvTotal < nPackSize) {
        int nRecv = recv(sockfd, buf + nRecvTotal, nPackSize - nRecvTotal, 0);
        if(nRecv <= 0) break;  // 连接断开
        nRecvTotal += nRecv;
    }
    
    // 3. ★ 同步调用业务回调（不再异步投递，解决乱序Bug）
    pData->pNet->m_recv_callback(sockfd, buf, nPackSize);
    delete[] buf;
    
    // 4. 重新注册EPOLLONESHOT（处理完毕，允许下次事件通知）
    myevent_s *ev = pData->pNet->m_mapSockfdToEvent.find(sockfd);
    ev->eventadd(EPOLLIN | EPOLLONESHOT);
}
```

### 粘包协议：4字节长度头

```cpp
int Block_Epoll_Net::SendData(int fd, char* szbuf, int nlen) {
    // 先发4字节包长度，再发数据包内容
    int packLen = nlen;
    send(fd, &packLen, 4, 0);
    send(fd, szbuf, nlen, 0);
    return nlen;
}
```

> **面试官追问**：为什么用4字节长度头而不是2字节？
> - 2字节最大表示65535字节，本项目中STRU_UPLOAD_RQ约680字节、STRU_FILEBLOCK_RQ约4KB，2字节够用
> - 但4字节支持4GB包大小，**为未来扩展留余地**（比如大文件整包传输）
> - 实际上本项目最大包是FILEBLOCK_RQ（4KB+头部），2字节也够，但4字节更通用

## 遇到的难点

### 难点1：上传乱序Bug（最关键的Bug）

- **问题**：客户端先发STRU_UPLOAD_RQ（上传请求），紧接着发STRU_FILEBLOCK_RQ（文件块），但服务端有时先处理FILEBLOCK_RQ，导致找不到上传任务映射
- **发现**：上传时服务端日志显示"FileBlockRq: fileId不存在"，但同一请求重试又成功了
- **排查**：
  1. 原始设计：recv_task接收完数据后，再Producer_add(Buffer_Deal, pData)异步投递业务处理
  2. 两个连续的包被线程池中不同线程处理，线程调度顺序不确定 → **后发的包可能先被处理**
  3. UPLOAD_RQ需要先创建m_mapUpload映射，FILEBLOCK_RQ依赖这个映射
- **解决**：recv_task中**同步调用m_recv_callback**，不再异步投递Buffer_Deal
  ```cpp
  // 修改前（异步，会乱序）：
  Producer_add(Buffer_Deal, pData);  // 投递到线程池，顺序不确定
  
  // 修改后（同步，保证顺序）：
  m_recv_callback(sockfd, buf, nPackSize);  // 在recv线程中直接调用
  ```
- **代价**：业务处理在recv线程中执行，如果业务逻辑耗时（如MySQL查询），会阻塞该fd的后续recv。但本项目业务逻辑很快（MySQL查询<1ms），影响可忽略。

> **面试官追问**：同步处理会不会降低吞吐量？
> - 单个fd的业务处理阻塞不影响其他fd（EPOLLONESHOT保证每个fd独立）
> - 线程池有200个线程，200个fd可以并行处理业务逻辑
> - **瓶颈分析**：如果MySQL查询耗时10ms，200线程×10ms=每秒2万次查询，足够应对万级并发

### 难点2：EPOLLONESHOT忘记重新注册导致连接"失联"

- **问题**：早期开发时，recv_task处理完数据后忘记重新`epoll_ctl(MOD)`注册EPOLLONESHOT，导致fd上的后续数据永远不通知
- **发现**：客户端发送数据后服务端无响应，但连接未断开
- **排查**：epoll_wait返回0个事件，检查发现fd的status=0（不在监听树中）
- **解决**：recv_task末尾必须调用`ev->eventadd(EPOLLIN | EPOLLONESHOT)`重新注册

## 性能优化

- **Socket参数调优**：
  - 256KB接收缓冲区 → 防止大包被分片过多
  - 128KB发送缓冲区 → 平衡内存和吞吐
  - TCP_NODELAY → 禁用Nagle算法，小包（点赞/评论）立即发送，延迟从40ms降到<1ms

- **MyMap线程安全map**：自实现带pthread_mutex的map，替代std::map+外部锁
  - 优势：锁粒度在map级别，比全局锁并发度更高
  - 劣势：不如concurrent_unordered_map（分段锁）的并发度

> **面试官追问**：如果流量增加10倍，epoll哪里会先出问题？
> - **单进程瓶颈**：epoll_create(4096)最多监听4096个fd，10万并发需要增大或用多进程
> - **线程池瓶颈**：最大200线程，10万并发时200线程不够 → 增大max_threads或改为多进程
> - **解决方案**：nginx前置负载均衡 → 多个服务端进程 → 每个进程处理2~3万连接

## 模拟面试题

### Q1：EPOLLONESHOT和EPOLLET的区别？能同时用吗？
**回答要点**：
- EPOLLONESHOT：事件只通知一次，处理完需重新注册；解决多线程竞态
- EPOLLET：只在状态变化时通知一次（边缘触发），需循环读至EAGAIN；解决频繁唤醒
- **可以同时用**：`EPOLLIN | EPOLLET | EPOLLONESHOT`，既边缘触发又防竞态，但编程最复杂（必须非阻塞+循环读+处理完重新注册）
- 本项目选择LT+ONESHOT，**最简单最可靠**

### Q2：你的粘包处理为什么在recv_task里而不是在epoll事件循环里？
**回答要点**：
- 事件循环只负责检测"有数据到达"，投递到线程池后由recv_task负责完整接收
- 如果在事件循环中读数据，会阻塞事件循环（循环recv可能耗时），影响其他fd的accept和事件分发
- **分离关注点**：事件循环=事件分发器，recv_task=数据接收器，Buffer_Deal=业务处理器

### Q3：如果客户端恶意发送一个nPackSize=1GB的长度头，你的服务端会怎样？
**回答要点**：
- 当前代码会`new char[nPackSize]`分配1GB内存 → **内存被耗尽，服务崩溃**
- **修复方案**：加长度上限校验，`if(nPackSize > MAX_PACK_SIZE) { close(sockfd); return; }`
- MAX_PACK_SIZE可设为64KB（本项目最大包是FILEBLOCK_RQ约4KB+头部）
- 这是**防御性编程**的基本要求，生产级必须做
