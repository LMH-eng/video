# 10 RTMP流媒体播放

## 模块概述

客户端通过FFmpeg avformat_open_input直接打开RTMP流地址（由nginx-rtmp服务器提供），实现网络视频播放。针对RTMP流的特殊性（网络延迟、数据到达不稳定），做了rtmp_buffer增大、视频队列丢包策略、自适应降速等优化。

## 技术选型与理由

> **面试官追问**：为什么选RTMP而不是HLS？为什么不本地播放？

| 方案 | 优势 | 劣势 | 选择理由 |
|------|------|------|----------|
| **RTMP** | 实时性好（延迟1~3秒）、FFmpeg原生支持 | 单码率、带宽占用高、不支持CDN | 项目用nginx-rtmp-module搭建流媒体服务器，RTMP是nginx-rtmp的默认协议 |
| HLS | 支持CDN、多码率自适应、兼容HTTP | 延迟高（10~30秒）、需TS分片 | 短视频需要低延迟，HLS延迟不可接受 |
| 本地文件播放 | 无网络延迟、最简单 | 无法多人同时观看、无流媒体体验 | 项目目标是搭建完整的短视频平台，必须支持网络流 |

> **面试官追问**：RTMP和HLS的核心区别？
> - **RTMP**：基于TCP的长连接流，服务器持续推流→客户端持续拉流→实时性好但带宽高
> - **HLS**：基于HTTP的短连接分片，服务器生成.m3u8索引+多个.ts分片→客户端依次下载→延迟高但支持CDN
> - 短视频场景：RTMP更适合（延迟低），直播场景：HLS更适合（CDN分发）

## 核心实现流程

### RTMP流打开

```cpp
void VideoPlayer::run() {
    // RTMP网络流：增大缓冲时间
    AVDictionary *options = NULL;
    if(m_fileName.startsWith("rtmp")) {
        av_dict_set(&options, "rtmp_buffer", "30000", 0);  // 30秒客户端缓冲
    }
    
    // 打开流（RTMP或本地文件）
    if(avformat_open_input(&pFormatCtx, file_path, NULL, &options) != 0) {
        qDebug() << "can't open file";
        return;
    }
    av_dict_free(&options);
    
    // 获取流信息
    avformat_find_stream_info(pFormatCtx, NULL);
}
```

> **面试官追问**：rtmp_buffer=30000是什么意思？
> - RTMP客户端缓冲大小，单位毫秒（30秒）
> - 客户端预缓冲30秒的数据→即使网络短暂波动（5~10秒），播放不中断
> - **代价**：开始播放时需要等30秒缓冲填满→用户感知延迟
> - 实际上FFmpeg的rtmp_buffer是内部缓冲，不是播放延迟→播放可以在缓冲开始填充后就启动

### 视频队列丢包策略

```cpp
// 读线程中的视频包处理
if(packet->stream_index == m_videoState.videoStream) {
    // ★ 视频队列满时丢包（避免阻塞读线程饿死音频队列）
    if(m_videoState.videoq->size > MAX_VIDEO_SIZE) {
        av_packet_unref(packet);  // 丢包
    } else {
        packet_queue_put(m_videoState.videoq, packet);
    }
}
// ★ 音频包永远入队（不限大小，保证倍速时音频不中断）
else if(packet->stream_index == m_videoState.audioStream) {
    packet_queue_put(m_videoState.audioq, packet);
}
```

> **面试官追问**：为什么视频丢包而音频不丢包？
> - 视频帧大（H264一帧几十KB~几百KB），队列积压→内存暴涨→必须丢包
> - 音频帧小（AAC一帧几KB），且音频中断比视频卡顿更明显→不能丢包
> - 丢包不影响体验：视频线程解码时会跳过丢掉的帧，下一帧会覆盖上一帧

### 中断回调：quit时安全退出

```cpp
// 设置中断回调：让avformat_open_input/av_read_frame等阻塞操作可以被quit中断
pFormatCtx->interrupt_callback.callback = decode_interrupt_cb;
pFormatCtx->interrupt_callback.opaque = &m_videoState;

int decode_interrupt_cb(void *ctx) {
    VideoState *is = (VideoState*)ctx;
    return (is && is->quit) ? 1 : 0;  // quit时返回1→中断阻塞操作
}
```

> **面试官追问**：没有中断回调会怎样？
> - av_read_frame在RTMP流上可能阻塞数秒（等待网络数据）
> - 用户点击停止→quit=true→但av_read_frame还在阻塞→线程无法退出→程序卡住
> - 中断回调让FFmpeg在每次内部检查时调用decode_interrupt_cb→quit时立即返回→线程安全退出

## 遇到的难点

### 难点1：RTMP流数据到达不稳定

- **问题**：1.5x/2.0x倍速播放RTMP流时，音频队列经常耗空→播放中断→恢复→又中断
- **排查**：日志显示aqueue从1000+骤降到0→audio_decode_frame返回-1→插入静音
- **解决**：自适应降速机制（详见09章节）——队列<3包时降速到1.0x，队列恢复后回到目标倍速

### 难点2：RTMP流seek不支持

- **问题**：RTMP流不支持av_seek_frame→用户拖动进度条时seek失败
- **排查**：av_seek_frame返回-1→日志显示"error while seeking"
- **解决**：RTMP流播放时禁用进度条拖动（或改为重新打开流从新位置开始播放）

> **面试官追问**：为什么RTMP不支持seek？
> - RTMP是实时推流协议，服务器持续发送数据，没有"文件偏移量"的概念
> - 本项目nginx-rtmp配置的是VOD模式（点播），理论上可以seek
> - 但FFmpeg的RTMP客户端实现不支持seek→需要改为HLS或重新打开流

## 模拟面试题

### Q1：你的RTMP播放和B站/抖音的流媒体有什么区别？
**回答要点**：
- **B站**：HLS+ABR多码率自适应（1080p/720p/480p），根据带宽自动切换
- **抖音**：自研流媒体协议+CDN分发+P2P加速
- **本项目**：RTMP单码率，无CDN，无多码率
- **差距**：单码率→带宽不够时只能卡顿或降速；无CDN→所有用户直连服务器→带宽瓶颈
- **但核心播放引擎（FFmpeg+SDL+同步）的设计思路和生产级一致**

### Q2：如果10万用户同时播放RTMP流，你的架构哪里会先出问题？
**回答要点**：
- **nginx-rtmp带宽**：10万用户×2Mbps=200Gbps→单机带宽不够→需要CDN分发
- **nginx-rtmp连接数**：10万TCP连接→nginx-rtmp单进程可能不够→需要多进程+负载均衡
- **客户端缓冲**：10万用户同时拉流→服务器推流速率×10万→服务器CPU和带宽爆炸
- **解决方案**：CDN分发+HLS多码率+边缘节点缓存

### Q3：RTMP和WebRTC的区别？短视频平台应该用哪个？
**回答要点**：
- **RTMP**：基于TCP→可靠但延迟1~3秒→适合点播/直播推流
- **WebRTC**：基于UDP→不可靠但延迟<500ms→适合实时互动（视频通话）
- **短视频平台**：点播场景用RTMP/HLS（延迟不重要），直播场景用RTMP推流+HLS拉流（兼容性好）
- **抖音的直播**：推流用RTMP，拉流用HLS/FLV（HTTP-FLV延迟比HLS低，约2~3秒）
