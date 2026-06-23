
# 08 音频采集与编码完整实现 + Bug修复记录

### 一、音频模块概述

项目原有 `Audio_Read` 类已实现音频采集和重采样，但未接入编码流程（`hasAudio = false`，音频流添加代码被注释）。本次工作将音频完整接入，实现**录屏+声音同步录制**。

---

### 二、音频采集架构

```
麦克风(QAudioInput, 44100Hz, 双声道, S16 PCM)
    │
    ▼
Audio_Read::slot_readMore()  ← 定时器每20ms触发
    │  1. 从 QAudioInput 读取 PCM 数据
    │  2. 缓冲区累积到一帧 AAC 所需数据量（4096字节）
    │  3. swr_convert: S16 → FLTP 重采样
    │  4. 拆分左右声道为平面模式（LLLL...RRRR...）
    │  5. Q_EMIT SIG_sendAudioFrameData(buf, size)
    ▼
SaveVideoFileThread::slot_writeAudioFrameData()
    │  1. memcpy 到音频帧缓冲区
    │  2. 计算 PTS 时间戳
    │  3. write_frame() → AAC 编码 + FLV 封装
    ▼
音频数据写入 FLV 文件（与视频流同步封装）
```

---

### 三、音频关键参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 采集采样率 | 44100 Hz | QAudioInput 采集频率 |
| 采集格式 | S16 (16bit PCM) | 声卡输出格式 |
| 采集声道 | 双声道（立体声） | AV_CH_LAYOUT_STEREO |
| 编码采样率 | 44100 Hz | 与采集一致，无需重采样 |
| 编码格式 | FLTP (32bit浮点平面) | AAC 编码器要求 |
| 编码器 | AAC (AV_CODEC_ID_AAC) | FLV 标准音频编码 |
| 编码码率 | 64000 bps | 64kbps，语音够用 |
| 一帧采样点 | 1024 | AAC 标准帧大小 |
| 一帧数据量 | 8192 字节 | 1024 × 2声道 × 4字节(float) |
| 采集间隔 | 20ms | 定时器触发周期 |

---

### 四、S16 → FLTP 重采样原理

AAC 编码器要求输入 FLTP（Float Planar）格式，而声卡采集的是 S16（Short Interleaved）格式，必须转换：

```
S16 交错格式（采集）：L R L R L R L R ...    ← 左右交替
FLTP 平面格式（编码）：L L L L ... R R R R ... ← 左右分开，float存储
```

转换使用 FFmpeg 的 `SwrContext`：

```cpp
// 创建重采样上下文
swr = swr_alloc_set_opts(nullptr,
    AV_CH_LAYOUT_STEREO,          // 输出声道布局
    AV_SAMPLE_FMT_FLTP,           // 输出格式：FLTP
    44100,                        // 输出采样率
    AV_CH_LAYOUT_STEREO,          // 输入声道布局
    AV_SAMPLE_FMT_S16,            // 输入格式：S16
    44100,                        // 输入采样率
    0, nullptr);

swr_init(swr);

// 执行转换
int count = swr_convert(swr,
    out_buffer, nFrameCount * OneAudioSize / 4,  // 输出
    (const uint8_t **)&in_buffer, nFrameCount * OneAudioSize / 4);  // 输入
```

---

### 五、音频编码器初始化流程

```cpp
// 1. 添加音频流
add_audio_stream(&audio_st, oc, &audio_codec, AV_CODEC_ID_AAC);

// 2. 打开编码器，分配帧缓冲区
open_audio(oc, audio_codec, &audio_st);
    // avcodec_open2() 打开 AAC 编码器
    // av_frame_alloc() + av_samples_fill_arrays() 分配帧和数据缓冲区
    // avcodec_parameters_from_context() 复制参数到流

// 3. 开始采集
m_audioRead->slot_openAudio();
    // new QAudioInput(format, this) 创建音频输入
    // audio_in->start() 开始采集
    // swr_alloc_set_opts() + swr_init() 初始化重采样
    // timer->start(20) 启动20ms定时器
```

---

### 六、音频帧 PTS 计算方式

```cpp
AVRational rational;
rational.num = 1;
rational.den = c->sample_rate;  // 44100

audio_st.frame->pts = av_rescale_q(audio_st.samples_count, rational, c->time_base);
audio_st.samples_count += audio_st.frame->nb_samples;
```

**为什么这样算？** PTS 的单位是编码器的时间基（time_base），对于音频来说，时间基 = 1/采样率。`samples_count` 累计已编码的采样点数，通过 `av_rescale_q` 转换到编码器时间基，得到精确的时间戳。

---

### 七、Bug修复记录

#### Bug 7：ImageToYuvBuffer 中 pFrameRGB 引用临时数据导致崩溃

**位置**：`picinpic_read.cpp` 的 `ImageToYuvBuffer`

**原始代码**：
```cpp
avpicture_fill((AVPicture *)pFrameRGB, buffer1, AV_PIX_FMT_RGB24, w, h);
pFrameRGB->data[0] = image.bits();  // ← 直接指向 QImage 的内部数据！
```

**问题分析**：

`image.bits()` 返回的是 QImage 内部数据的指针。`avpicture_fill` 已经把 `buffer1` 绑定到了 `pFrameRGB->data[0]`，但紧接着又被 `image.bits()` 覆盖了。这意味着：
1. `buffer1` 分配的内存被浪费了（泄漏）
2. `sws_scale` 读取时，`pFrameRGB->data[0]` 指向 QImage 的数据，但 `pFrameRGB->linesize` 仍然是 `avpicture_fill` 设置的值。如果 QImage 的行对齐方式与 FFmpeg 不同，`sws_scale` 会读取越界
3. QImage 是值传递或栈上临时对象，其 `bits()` 指针可能在后续操作中失效

**修复**：
```cpp
avpicture_fill((AVPicture *)pFrameRGB, buffer1, AV_PIX_FMT_RGB24, w, h);
// 将 QImage 数据复制到 pFrameRGB 的缓冲区，避免引用临时数据
memcpy(buffer1, image.bits(), numBytes1);
```

---

#### Bug 8：av_free 误用导致 AVFrame 结构体泄漏

**位置**：`picinpic_read.cpp` 的 `ImageToYuvBuffer`

**原始代码**：
```cpp
av_free(pFrameRGB);   // ← 只释放了 AVFrame 结构体指针本身
av_free(pFrameYUV);   // ← 同上
```

**问题分析**：

`av_frame_alloc()` 分配的 AVFrame 应该用 `av_frame_free()` 释放，它会：
1. 释放 frame 内部关联的缓冲区（data[] 指向的内存）
2. 释放 AVFrame 结构体本身

`av_free()` 只释放了结构体本身，内部缓冲区泄漏。

**修复**：
```cpp
av_frame_free(&pFrameRGB);   // 正确释放 AVFrame 及其内部缓冲区
av_frame_free(&pFrameYUV);
```

---

#### Bug 9：QApplication::desktop() 废弃导致崩溃

**位置**：`picinpic_read.cpp` 的 `slot_getVideoFrame`

**原始代码**：
```cpp
QPixmap map = src->grabWindow(QApplication::desktop()->winId());
```

**问题分析**：

`QApplication::desktop()` 在 Qt 5.15+ 中已被标记为废弃（deprecated），在某些平台上可能返回 nullptr 或行为异常。`desktop()->winId()` 如果返回无效值，`grabWindow` 会失败或崩溃。

**修复**：
```cpp
QPixmap map = src->grabWindow(0);  // 传入0表示截取整个屏幕
```

`grabWindow(0)` 的参数是 WID（Window ID），传入 0 表示截取整个桌面，功能等价且更安全。

---

#### Bug 10：write_frame 中 exit(1) 导致程序直接崩溃

**位置**：`savevideofilethread.cpp` 的 `write_frame`

**原始代码**：
```cpp
ret = avcodec_send_frame(c, frame);
if (ret < 0) {
    fprintf(stderr, "Error sending a frame to the encoder\n");
    exit(1);  // ← 直接终止整个进程！
}
// ... 同理 avcodec_receive_packet 和 av_interleaved_write_frame 中也有 exit(1)
```

**问题分析**：

编码过程中偶尔出现错误是正常的（如编码器缓冲区满返回 EAGAIN），但 `exit(1)` 会直接终止整个程序，用户看到的就是"程序异常结束"。应该让函数返回错误码，由调用者决定如何处理。

**修复**：
```cpp
ret = avcodec_send_frame(c, frame);
if (ret < 0) {
    fprintf(stderr, "Error sending a frame to the encoder\n");
    return -1;  // 返回错误码，不终止程序
}
```

---

#### Bug 11：close_stream 双重释放 frameBuffer

**位置**：`savevideofilethread.cpp` 的 `close_stream`

**原始代码**：
```cpp
void SaveVideoFileThread::close_stream(AVFormatContext *oc, OutputStream *ost)
{
    avcodec_free_context(&ost->enc);
    av_frame_free(&ost->frame);        // ← 释放 frame 及其内部缓冲区
    av_free(ost->frameBuffer);          // ← 再次释放同一块内存！双重释放！
    // ...
}
```

**问题分析**：

在 `open_video` 中，`frameBuffer` 通过 `avpicture_fill` 绑定到了 `ost->frame` 的 `data[]` 数组。`av_frame_free(&ost->frame)` 会释放 frame 及其关联的内部数据缓冲区，其中就包括 `frameBuffer` 指向的内存。之后再 `av_free(ost->frameBuffer)` 就是双重释放（double free），导致未定义行为或崩溃。

**修复**：
```cpp
void SaveVideoFileThread::close_stream(AVFormatContext *oc, OutputStream *ost)
{
    avcodec_free_context(&ost->enc);
    // frameBuffer 的内存已通过 avpicture_fill 绑定到 frame 中
    // av_frame_free 会释放 frame 及其关联的内部缓冲区
    // 所以不需要再单独 av_free(frameBuffer)
    av_frame_free(&ost->frame);
    ost->frameBuffer = nullptr;  // 置空防止野指针
    // ...
}
```

---

#### Bug 12：malloc/free 和 delete[] 混用

**位置**：`audio_read.cpp` 的 `slot_readMore`

**原始代码**：
```cpp
out_buffer[0] = (uint8_t *)malloc(OneAudioSize * nFrameCount);  // malloc 分配
out_buffer[1] = (uint8_t *)malloc(OneAudioSize * nFrameCount);  // malloc 分配
// ...
delete [] out_buffer[0];   // ← delete[] 释放！与 malloc 不匹配！
delete [] out_buffer[1];   // ← 同上
```

**问题分析**：

`malloc` / `free` 和 `new` / `delete` 是两套不同的内存管理机制，混用是未定义行为。在 MSVC 上可能碰巧不崩，但在 MinGW 或其他编译器上可能崩溃或导致堆损坏。

**修复**：
```cpp
free(out_buffer[0]);   // malloc 配套 free
free(out_buffer[1]);
```

---

#### Bug 13：重复点击停止按钮导致崩溃

**位置**：`savevideofilethread.cpp` 的 `slot_closeVideo`

**原始代码**：
```cpp
void SaveVideoFileThread::slot_closeVideo()
{
    m_picInPicRead->slot_closeVideo();
    m_audioRead->slot_closeAudio();
    av_write_trailer(oc);           // ← 第二次调用时 oc 已被释放！
    close_stream(oc, &video_st);    // ← 同上
    close_stream(oc, &audio_st);
    avio_closep(&oc->pb);
    avformat_free_context(oc);      // ← 释放 oc
    oc = nullptr;
}
```

**问题分析**：

用户快速多次点击"停止"按钮时，`slot_closeVideo` 被调用多次：
- 第一次：正常执行，释放所有资源，`oc` 置 nullptr
- 第二次：`av_write_trailer(oc)` 传入已释放的 `oc`（野指针或 nullptr）→ 崩溃

虽然第一次执行后 `oc = nullptr`，但 `av_write_trailer(oc)` 在 `oc` 置空之前就被调用了，且 `avformat_free_context(oc)` 释放后 `oc` 才置空，第二次进入时 `oc` 已经是野指针。

**修复**：

添加录制状态标志，防止重复操作：

```cpp
// savevideofilethread.h
bool m_isRecording = false;

// savevideofilethread.cpp
void SaveVideoFileThread::slot_openVideo()
{
    if (m_isRecording) return;   // 防止重复点击开始
    m_isRecording = true;
    // ...
}

void SaveVideoFileThread::slot_closeVideo()
{
    if (!m_isRecording) return;  // 防止重复点击停止
    m_isRecording = false;
    // ...
    if (oc)
        av_write_trailer(oc);
    // ...
    if (oc) {
        avformat_free_context(oc);
        oc = nullptr;
    }
}
```

---

#### Bug 14：m_saveUrl 初始为空导致文件创建失败

**位置**：`recorderdialog.cpp` 构造函数

**原始代码**：
```cpp
QString m_saveUrl;  // 默认为空字符串
```

**问题分析**：

UI 中 `le_url` 有默认值 `E:/kugou/2042.flv`，但 `m_saveUrl` 只有在用户点击"设置直播地址"按钮后才会被赋值。如果用户直接点"开始"，`m_saveUrl` 为空，`avio_open` 打开空路径失败，`slot_setInfo` 提前 return，后续操作异常。

**修复**：
```cpp
// RecorderDialog 构造函数中
m_saveUrl = ui->le_url->text();  // 初始化为UI中的默认值
```

---

#### Bug 15：slot_writeVideoFrameData 缺少缓冲区溢出检查

**位置**：`savevideofilethread.cpp` 的 `slot_writeVideoFrameData`

**原始代码**：
```cpp
memcpy(video_st.frameBuffer, picture_buf, buffer_size);  // ← 无边界检查
```

**问题分析**：

如果 `buffer_size` 大于 `frameBufferSize`（例如分辨率变化导致 YUV 数据量增大），`memcpy` 会越界写入，导致堆损坏或崩溃。

**修复**：
```cpp
if (buffer_size > video_st.frameBufferSize) {
    fprintf(stderr, "Video frame buffer overflow\n");
    av_free(picture_buf);
    return;
}
memcpy(video_st.frameBuffer, picture_buf, buffer_size);
```

---

### 八、Bug汇总表

| Bug# | 位置 | 类型 | 严重程度 | 简述 |
|------|------|------|---------|------|
| 7 | picinpic_read.cpp | 内存安全 | **高** | pFrameRGB->data[0] 引用 QImage 临时数据 |
| 8 | picinpic_read.cpp | 内存泄漏 | 中 | av_free 误用，AVFrame 内部缓冲区泄漏 |
| 9 | picinpic_read.cpp | 兼容性 | **高** | QApplication::desktop() 废弃导致崩溃 |
| 10 | savevideofilethread.cpp | 健壮性 | **高** | write_frame 中 exit(1) 直接终止进程 |
| 11 | savevideofilethread.cpp | 内存安全 | **高** | close_stream 双重释放 frameBuffer |
| 12 | audio_read.cpp | 内存安全 | 中 | malloc/free 和 delete[] 混用 |
| 13 | savevideofilethread.cpp | 健壮性 | **高** | 重复点击停止按钮导致崩溃 |
| 14 | recorderdialog.cpp | 逻辑错误 | 中 | m_saveUrl 初始为空 |
| 15 | savevideofilethread.cpp | 内存安全 | 中 | 视频帧写入缺少缓冲区溢出检查 |

---

### 九、面试高频问题

#### Q1：为什么 AAC 编码器要求 FLTP 格式而不是 S16？

**答**：FLTP（Float Planar）有两个优势：
1. **精度**：32位浮点比16位整数的动态范围大得多，编码时量化噪声更低
2. **平面布局**：左右声道数据连续存储（LLLL...RRRR...），编码器处理声道时内存访问更高效，缓存命中率更高

#### Q2：音频重采样（SwrContext）的作用是什么？什么场景需要？

**答**：SwrContext 用于音频格式转换，包括：采样格式转换（S16→FLTP）、采样率转换（48kHz→44.1kHz）、声道布局转换（单声道→立体声）。本项目只需要格式转换，采样率和声道布局不变。

#### Q3：malloc/free 和 new/delete 为什么不能混用？

**答**：它们是两套不同的内存管理机制：
- `malloc/free` 是 C 标准库函数，只分配/释放原始内存
- `new/delete` 是 C++ 运算符，会调用构造/析构函数

混用的核心问题是：`malloc` 分配的内存元信息由 C 运行时管理，`delete` 期望的内存元信息由 C++ 运行时管理，两者格式不同，`delete` 一个 `malloc` 的指针会读取错误的元信息，导致未定义行为。

#### Q4：如何防止用户重复点击按钮导致重复操作？

**答**：三种常见方案：
1. **状态标志**：用 `bool m_isRecording` 记录当前状态，操作前检查（本项目采用）
2. **按钮禁用**：点击开始后 `pb_start->setEnabled(false)`，点击停止后恢复
3. **信号防抖**：用 QTimer 延迟处理，短时间内多次点击只执行一次

#### Q5：av_frame_free 和 av_free 的区别？

**答**：
- `av_free(ptr)`：等价于 `free(ptr)`，只释放指针指向的内存块
- `av_frame_free(&frame)`：先调用 `av_frame_unref(frame)` 释放 frame 内部引用的缓冲区（data[] 指向的内存），再释放 AVFrame 结构体本身，最后将指针置 NULL

对于 `av_frame_alloc()` 创建的 AVFrame，必须用 `av_frame_free()` 释放，否则内部缓冲区泄漏。

#### Q6：录屏项目中音频和视频如何同步？

**答**：本项目通过 PTS（Presentation Time Stamp）实现音视频同步：
- 视频帧 PTS：按帧率递增（`next_pts++`），时间基 = 1/帧率
- 音频帧 PTS：按采样点数累计（`samples_count += nb_samples`），时间基 = 1/采样率
- FLV 封装器（`av_interleaved_write_frame`）根据 PTS 排序交错写入音视频包
- 播放端根据 PTS 时间戳同步渲染音视频

---

# 笔记14：Redis 协同过滤推荐算法 + 点赞功能完整实现

### 一、推荐系统整体架构

```
用户注册（选兴趣标签）→ Redis ZSet 初始化用户爱好向量
        │
用户登录 → 检查Redis是否有爱好向量（兼容老用户）
        │
用户浏览视频 → PlayRq → updateUserHobby(+1) + recordAction("play")
        │
用户点赞/取消 → LikeRq → updateUserHobby(±10) + recordAction("like"/"unlike")
        │
推荐请求 → RecommendRq
        │
        ├─ 1. 查Redis缓存（SETEX 300s）→ 命中则直接返回
        ├─ 2. 协同过滤（User-Based CF）→ 计算相似用户 → 取其上传视频
        ├─ 3. 热度补齐（不够15个时用热门视频填充）
        ├─ 4. 结果写入Redis缓存（300秒过期）
        └─ 5. Redis不可用时降级为时间倒序
```

---

### 二、Redis 数据结构设计

#### 2.1 用户爱好向量（ZSet）

```
Key:   user:hobby:{userId}
Type:  ZSet (有序集合)
Member: 爱好标签名称
Score:  爱好权重值

示例：
user:hobby:1
  food    → 50    (注册时勾选，初始50)
  funny   → 0     (注册时未勾选，初始0)
  music   → 61    (注册50 + 观看1次音乐视频 +1 + 点赞1次音乐视频 +10)
  dance   → 60    (注册50 + 点赞1次舞蹈视频 +10)
```

**8个爱好维度**：food / funny / ennergy / dance / music / video / outside / edu

#### 2.2 推荐结果缓存（String + SETEX）

```
Key:    recommend:cache:{userId}
Type:   String
Value:  "1,3,5,7,9,11,13,2,4,6,8,10,12,14,15"  (逗号分隔的videoId)
TTL:    300秒（5分钟过期，平衡实时性与性能）
```

---

### 三、协同过滤算法详解

#### 3.1 User-Based Collaborative Filtering 流程

```
当前用户A的爱好向量: [50, 0, 51, 60, 0, 0, 0, 0]
其他用户B的爱好向量: [0, 55, 0, 70, 0, 0, 0, 0]

Step1: 计算A与所有用户的余弦相似度
Step2: 取相似度最高的Top-5用户
Step3: 查询这5个用户上传的视频，按热度排序
Step4: 不足15个时用全局热门视频补齐
```

#### 3.2 余弦相似度计算

```cpp
double CLogic::cosineSimilarity(map<string,double>& vecA, map<string,double>& vecB)
{
    double dotProduct = 0, normA = 0, normB = 0;
    for(int i = 0; i < DEF_HOBBY_COUNT; i++) {
        double a = vecA[HOBBY_NAMES[i]];
        double b = vecB[HOBBY_NAMES[i]];
        dotProduct += a * b;
        normA += a * a;
        normB += b * b;
    }
    if(normA == 0 || normB == 0) return 0;
    return dotProduct / (sqrt(normA) * sqrt(normB));
}
```

**公式**：`sim(A,B) = (A·B) / (|A| × |B|)`

值域 [0, 1]，1表示完全相同，0表示完全无关。

#### 3.3 冷启动策略

新用户注册时只勾选了几个兴趣标签，爱好向量稀疏：

```
注册时: food=50, funny=50, 其余=0
```

**冷启动判断**：如果用户所有爱好权重之和 < 10，视为冷启动用户，直接走热门排行。

```cpp
// 检查冷启动：爱好向量全为0或极低
double totalScore = 0;
for(auto& p : userVec) totalScore += p.second;
if(totalScore < 10) {
    // 冷启动：直接走热门排行
    return getRecommendByHot(videoIds);
}
```

---

### 四、四大企业级特性

#### 4.1 Redis 故障降级

```cpp
// 构造函数中连接Redis，失败不退出
m_redis = new CRedis;
m_redisConnected = m_redis->connect("127.0.0.1", 6379);
if(!m_redisConnected) {
    cout << "Redis连接失败，推荐功能降级为时间倒序" << endl;
}

// RecommendRq中：Redis不可用时走MySQL降级
if(!m_redisConnected) {
    // 降级：按时间倒序
    getRecommendByTime(videoIds);
}
```

**意义**：Redis 挂了不影响核心业务，系统仍可用。

#### 4.2 推荐结果缓存

```cpp
// 查缓存
string cacheKey = "recommend:cache:" + to_string(userId);
string cacheVal;
if(m_redis->getString(cacheKey, cacheVal)) {
    // 解析逗号分隔的videoId列表
    // 命中缓存，直接返回
}

// 未命中，计算推荐后写入缓存（300秒过期）
m_redis->setStringEx(cacheKey, cachedResult, 300);
```

**意义**：避免每次推荐都重新计算相似度，减少CPU和Redis压力。

#### 4.3 用户行为记录

```sql
CREATE TABLE t_UserAction (
    id INT AUTO_INCREMENT PRIMARY KEY,
    userId INT NOT NULL,
    videoId INT NOT NULL,
    action VARCHAR(20) NOT NULL,  -- play/like/unlike
    createTime TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

```cpp
void CLogic::recordAction(int userId, int videoId, const string& action)
{
    char sql[512] = "";
    sprintf(sql, "INSERT INTO t_UserAction(userId,videoId,action) VALUES(%d,%d,'%s');",
            userId, videoId, action.c_str());
    m_sql->UpdataMysql(sql);
}
```

**意义**：行为数据可用于离线分析、A/B测试、推荐效果评估。

#### 4.4 冷启动策略

- **新用户**：注册时勾选兴趣标签 → 初始化爱好向量（勾选=50，未勾选=0）
- **老用户**：登录时检查Redis是否有爱好向量，没有则从MySQL的t_UserAction重建
- **极低活跃**：爱好权重之和<10 → 走热门排行

---

### 五、点赞功能实现

#### 5.1 点赞流程

```
客户端点击点赞按钮
    → SIG_LikeRq(videoId)
    → CKernel::slot_likeRq → 发送 STRU_LIKE_RQ
    → 服务端 LikeRq:
        1. 查t_UserLike判断是否已赞 → 切换逻辑（已赞→取消，未赞→点赞）
        2. 查视频hobby标签 → updateUserHobby(±10)
        3. 更新hotdegree
        4. 写入/删除 t_UserLike 记录
        5. recordAction("like"/"unlike")
        6. 清除推荐缓存
        7. 查询点赞总数
        8. 返回 STRU_LIKE_RS(videoId, likeCount, isLiked)
    → 客户端更新按钮状态
```

#### 5.2 点赞状态持久化

播放视频时（PlayRq）服务端会查询点赞状态并返回：

```cpp
// 服务端 PlayRq 中
rs.videoId = rq->m_videoId;

// 查询是否已点赞
SELECT COUNT(*) FROM t_UserLike WHERE userId=? AND videoId=?

// 查询点赞总数
SELECT COUNT(*) FROM t_UserLike WHERE videoId=?
```

客户端收到 PLAY_RS 后更新 PlayerDialog 的点赞按钮状态。

#### 5.3 点赞数据表

```sql
CREATE TABLE t_UserLike (
    id INT AUTO_INCREMENT PRIMARY KEY,
    userId INT NOT NULL,
    videoId INT NOT NULL,
    createTime TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_user_video (userId, videoId)  -- 每人每视频只能一条记录
);
```

---

### 六、协议定义

```cpp
// packdef.h 新增协议号
#define _DEF_PACK_LIKE_RQ  10012
#define _DEF_PACK_LIKE_RS  10013

// 注册请求增加爱好字段
typedef struct STRU_REGISTER_RQ {
    // ... 原有字段 ...
    char hobby[DEF_HOBBY_COUNT];  // 8个爱好标签 0/1
} STRU_REGISTER_RQ;

// 点赞请求
typedef struct STRU_LIKE_RQ {
    PackType type;
    int userId;
    int videoId;
    char like;  // 1=点赞 0=取消（服务端自动切换，客户端固定发1）
} STRU_LIKE_RQ;

// 点赞回复
typedef struct STRU_LIKE_RS {
    PackType type;
    int result;      // like_fail / like_success
    int videoId;
    int likeCount;   // 当前视频点赞总数
    char isLiked;    // 1=已点赞 0=未点赞
} STRU_LIKE_RS;

// 播放回复增加点赞信息
typedef struct STRU_PLAY_RS {
    PackType type;
    int result;
    char url[_MAX_FILE_PATH];
    int videoId;     // 视频ID
    int likeCount;   // 点赞总数
    char isLiked;    // 当前用户是否已点赞
} STRU_PLAY_RS;
```

---

### 七、CRedis 封装类关键方法

```cpp
class CRedis {
public:
    bool connect(const string& host, int port);
    void disconnect();

    // 基础操作
    bool setString(const string& key, const string& val);
    bool getString(const string& key, string& val);
    bool removeKey(const string& key);
    bool isExist(const string& key);

    // 推荐系统专用
    bool setStringEx(const string& key, const string& val, int expireSec);  // SETEX
    bool appondOneToZSet(const string& key, const string& member, int score);  // ZADD
    bool incrZSetScore(const string& key, const string& member, int increment);  // ZINCRBY
    bool getZSetValsWithScores(const string& key, map<string,double>& out);  // ZRANGE WITHSCORES
};
```

---

### 八、客户端关键改动

#### 8.1 注册界面 - 8个爱好复选框

```
选择兴趣标签
[✓] 美食  [✓] 搞笑  [ ] 运动  [ ] 舞蹈
[ ] 音乐  [ ] 影视  [ ] 户外  [ ] 教育
```

注册时收集勾选状态，通过 `SIG_RegisterRq(tel, name, pwd, Hobby)` 传递。

#### 8.2 播放器 - 点赞按钮

```
[打开] [播放] [暂停] [停止]    [点赞] 0    [★]
       ↑ 播放控制按钮          ↑ 点赞按钮  ↑ 登录
```

- `pb_like`：checkable 按钮，已赞显示"已赞"，未赞显示"点赞"
- `lb_likeCount`：显示点赞数
- `setCurrentVideo(videoId, likeCount, isLiked)`：设置当前视频信息

#### 8.3 信号连接

```
LoginDialog::SIG_RegisterRq(tel,name,pwd,Hobby) → CKernel::slot_registerRq
PlayerDialog::SIG_LikeRq(videoId)                → CKernel::slot_likeRq
MainDialog::SIG_PlayVideo(url, videoId)           → CKernel::slot_playRtmpUrl
```

---

### 九、视频爱好标签与推荐的关系

```
上传视频时勾选爱好标签（food/funny/.../edu）
        │
        ▼ 存入 t_VideoData 的8个hobby列
        │
用户观看/点赞该视频
        │
        ▼ 服务端读取视频的hobby标签
        │
        ▼ updateUserHobby(): 对用户Redis ZSet中对应的维度 +1(观看) 或 ±10(点赞/取消)
        │
        ▼ 用户爱好向量逐渐偏向常看/常赞的视频类型
        │
        ▼ 协同过滤找到爱好相似的用户 → 推荐他们上传的视频
```

---

### 十、面试高频问题

#### Q1：为什么用 User-Based CF 而不是 Item-Based CF？

**答**：短视频平台用户数远小于视频数，且用户兴趣相对稳定，User-Based 更适合：
- 用户爱好向量只有8维，计算相似度很快
- Item-Based 需要维护视频-视频相似度矩阵，视频量大时矩阵巨大
- 用户注册时就有初始爱好向量，冷启动问题更容易解决

#### Q2：Redis 缓存推荐结果的过期时间为什么设300秒？

**答**：权衡实时性和性能：
- 太短（如30秒）：缓存命中率低，频繁重算浪费资源
- 太长（如1小时）：用户点赞后推荐结果长时间不变，体验差
- 300秒（5分钟）：用户行为变化后最多5分钟就能看到新推荐，同时减少计算压力
- 点赞/取消点赞时会主动清除缓存，确保关键行为立即生效

#### Q3：协同过滤的冷启动问题怎么解决？

**答**：三层策略：
1. **新用户注册时**：勾选兴趣标签，初始化爱好向量（勾选=50），避免全零向量
2. **老用户首次登录**：检查Redis是否有爱好向量，没有则从t_UserAction历史记录重建
3. **极低活跃用户**：爱好权重之和<10，直接走热门排行（getRecommendByHot）

#### Q4：Redis 挂了怎么办？

**答**：故障降级策略：
- Redis连接失败时 `m_redisConnected = false`，程序不退出
- 推荐请求走MySQL降级：`SELECT * FROM t_VideoData ORDER BY createTime DESC`
- 点赞功能不依赖Redis（只更新MySQL），正常可用
- Redis恢复后，下次登录会重新初始化爱好向量

#### Q5：为什么用 ZSet 存用户爱好向量而不是 Hash？

**答**：ZSet 的优势：
1. `ZRANGE WITHSCORES` 一次获取所有成员和分数，比 HGETALL 更方便排序
2. `ZINCRBY` 原子递增，并发安全，不需要先读再写
3. 可以按分数范围查询，例如找"最感兴趣的3个爱好"
4. Hash 的 HINCRBY 也能递增，但无法直接按分数排序

#### Q6：点赞的切换逻辑为什么放在服务端而不是客户端？

**答**：
1. **数据一致性**：客户端可能伪造请求，服务端校验更安全
2. **并发安全**：多个设备同时操作时，服务端查数据库判断状态更可靠
3. **客户端简单**：客户端只需发一个请求，不用维护本地状态
4. **服务端逻辑**：查 t_UserLike 是否有记录 → 有则删除(取消) → 无则插入(点赞)

#### Q7：推荐算法中如何避免推荐用户自己上传的视频？

**答**：在 `getRecommendByCF` 中用 `set<int> existIds` 排除：
```cpp
set<int> existIds;
// 排除当前用户自己上传的视频
char sqlSelf[256] = "";
sprintf(sqlSelf, "SELECT id FROM t_VideoData WHERE userid=%d;", userId);
// 查询结果加入 existIds
// 后续查询时 WHERE id NOT IN (existIds)
```

#### Q8：hiredis 的 C 封装类设计思路？

**答**：
- 构造时 `redisConnect()` 建立连接，析构时 `redisFree()` 释放
- 每个Redis命令封装为一个成员函数，内部处理 `redisCommand` + 结果解析 + `freeReplyObject`
- 返回 bool 表示成功/失败，输出参数返回查询结果
- 线程安全由调用方保证（本项目服务端单线程处理逻辑，无需加锁）
