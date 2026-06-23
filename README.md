# 短视频平台（Short Video Platform）

> 基于 Qt + FFmpeg + SDL + epoll + MySQL + Redis + nginx-rtmp 的全栈短视频平台

## 项目简介

一个完整的短视频平台，覆盖**注册登录、视频上传、RTMP 流播放、倍速播放、评论点赞、协同过滤推荐**等核心功能链路。项目采用 C/S 架构，客户端基于 Qt 开发，服务端基于 Linux epoll + 线程池实现高并发处理。

## 技术栈

| 层级 | 技术 |
|------|------|
| **客户端** | Qt 5（信号槽/QThread/QTcpSocket）、FFmpeg 4.2、SDL2 |
| **服务端** | C++、epoll、动态伸缩线程池、TCP 粘包协议 |
| **流媒体** | nginx-rtmp-module、RTMP 推流/拉流 |
| **存储** | MySQL（业务数据）、Redis（缓存/限流/兴趣向量） |
| **协议** | 自定义二进制协议（28 个协议号，紧凑字节对齐） |

## 项目结构

```
短视频平台/
├── src/                     # 服务端源代码
│   ├── main.cpp             # 服务端入口
│   ├── block_epoll_net.cpp  # epoll 网络引擎
│   ├── clogic.cpp           # 业务逻辑层（协议路由）
│   └── thread_pool.cpp      # 动态伸缩线程池
├── include/                 # 服务端头文件
│   ├── packdef.h            # 协议定义（28 个协议号）
│   ├── block_epoll_net.h    # epoll 网络引擎接口
│   └── clogic.h             # 业务逻辑接口
├── netapi/                  # 客户端网络通信层
│   ├── INetMediator.h       # 中介者抽象接口
│   └── TcpClientMediator.*  # TCP 客户端实现
├── deploy/                  # 部署相关文件
├── 项目整合/                # 面试复习笔记（15 篇模块化文档）
├── 简历.md                  # 项目简历
├── README.md                # 本文件
└── *.md                     # 开发设计文档
```

> 注：客户端完整代码见 [VideoPlayer 仓库](https://github.com/LMH-eng/VideoPlayer)，服务端代码见 [NetDisk 仓库](https://github.com/LMH-eng/NetDisk)。

## 核心功能

### 1. 用户系统
- 注册：手机号 + 密码 + 昵称 + 8 维兴趣标签
- 登录：MySQL 验证 + Redis 兴趣向量衰减
- 配置文件持久化（config.ini）

### 2. 视频播放
- **三线程模型**：读包线程 → 视频解码线程 → 音频 SDL 回调
- **音视频同步**：以音频时钟为主时钟驱动视频帧显示
- **RTMP 流播放**：支持 nginx-rtmp 推流地址实时拉流
- **倍速播放**：0.5x ~ 4.0x（atempo 滤镜串联），变速不变调
- **网络自适应**：队列不足时自动降速，5 秒冷却防抖动

### 3. 视频上传
- 文件分块上传（4KB/块）
- MD5 秒传校验（避免重复上传）
- 上传工作线程（QThread），UI 不阻塞
- 上传后自动生成 GIF 缩略图（FFmpeg 抽 I 帧合成）

### 4. 互动系统
- 评论发表/分页拉取/删除
- 敏感词过滤 + Redis 频率限制（10 秒防刷）
- 点赞数全服广播同步

### 5. 推荐算法
- 8 维兴趣向量（注册初始化 → 行为更新 → 登录衰减）
- 余弦相似度协同过滤
- 热度排序兜底（解决冷启动）

### 6. 服务端高并发
- epoll + 动态伸缩线程池
- EPOLLONESHOT 防竞态
- 自定义粘包协议（4 字节长度头）
- 接收与处理分离（解决上传乱序 Bug）

## 难点攻克

### 🔧 倍速播放电音问题
- **现象**：1.5x / 2.0x 时出现"正常声音叠加电音"
- **排查**：SDL 混音方式 → rubberband 滤镜相位 → aformat 格式转换，三层逐步定位
- **解决**：SDL_MixAudioFormat 改为 memcpy、移除 rubberband、统一 S16 输出格式
- **详见**：[视频倍速电音问题修复记录.md](./视频倍速电音问题修复记录.md)

### 🔧 上传请求乱序 Bug
- **现象**：高频上传时 UploadRq 和 FileBlockRq 到达顺序错乱
- **根因**：接收线程异步投递到线程池，多线程竞争导致处理顺序不确定
- **解决**：改为同步处理（接收线程直接调用 Buffer_Deal），保证顺序

## 快速开始

### 环境依赖
- Qt 5.12+
- FFmpeg 4.2.2（需包含 avformat/avcodec/avfilter/swresample）
- SDL2 2.0.10
- MySQL 5.7+
- Redis 5.0+
- nginx + nginx-rtmp-module
- Linux 环境（服务端依赖 epoll）

### 编译运行
```bash
# 客户端
cd VideoPlayer/netdisk/NetDisk
qmake NetDisk.pro
make

# 服务端
cd NetDisk
qmake NetDisk.pro
make
```

## 面试复习资料

`项目整合/` 目录下包含 15 篇模块化面试笔记：

| 编号 | 内容 |
|------|------|
| 01 | 项目总览与架构 |
| 02 | 二进制协议设计与序列化 |
| 03 | epoll 高并发网络引擎 |
| 04 | 动态伸缩线程池 |
| 05 | 业务逻辑层 |
| 06 | 推荐算法（协同过滤） |
| 07 | 客户端核心调度器 |
| 08 | 视频播放器核心（FFmpeg+SDL） |
| 09 | 倍速播放与电音修复 |
| 10 | RTMP 流媒体播放 |
| 11 | 文件分块上传与 MD5 秒传 |
| 12 | GIF 缩略图生成 |
| 13 | 评论系统 |
| 14 | 客户端网络通信层 |
| 15 | 项目难点总结与个人成长 |

## 作者

- **李明鸿**（LMH-eng）
- 岗位方向：C++/音视频/后端开发实习
- GitHub：[https://github.com/LMH-eng](https://github.com/LMH-eng)

## License

本项目仅用于学习与面试展示。
