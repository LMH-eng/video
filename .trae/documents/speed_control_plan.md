# 视频倍速功能实现计划

## 一、需求分析

### 1.1 功能目标
- 支持 0.5x / 1.0x / 1.5x / 2.0x 四档倍速切换
- **音频：变速不变调**（使用 FFmpeg `atempo` 滤镜）
- **视频：与音频完美同步**（使用 `setpts` 滤镜调整时间戳）
- 播放过程中可实时切换倍速

### 1.2 技术方案
参考文档 `102.md` 的标准实现：
- 音频：`atempo` 滤镜串联实现（支持 0.5~2.0 范围）
- 视频：`setpts=PTS*1/speed` 滤镜调整时间戳
- 同步：以音频时钟为主时钟，视频根据音频时钟延迟显示或丢帧

---

## 二、实现计划

### 2.1 修改文件清单

| 文件 | 修改内容 | 说明 |
|------|----------|------|
| `videoplayer.h` | 添加滤镜相关成员变量 | AVFilterGraph、AVFilterContext |
| `videoplayer.cpp` | 添加头文件、滤镜初始化、滤镜使用逻辑 | 核心实现 |

### 2.2 详细步骤

#### 步骤1：修改 videoplayer.h 添加滤镜成员

```cpp
// 在 VideoState 结构体中添加：
AVFilterGraph    *audio_filter_graph;
AVFilterContext  *audio_filter_src;   // abuffer
AVFilterContext  *audio_filter_sink;  // abuffersink
AVFilterGraph    *video_filter_graph;
AVFilterContext  *video_filter_src;   // buffer
AVFilterContext  *video_filter_sink;  // buffersink
```

#### 步骤2：修改 videoplayer.cpp 添加头文件

```cpp
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <inttypes.h>  // 用于 PRIx64
```

#### 步骤3：实现音频滤镜初始化函数

```cpp
bool init_audio_filter(VideoState *is) {
    if (is->audio_filter_graph) {
        avfilter_graph_free(&is->audio_filter_graph);
        is->audio_filter_src = is->audio_filter_sink = nullptr;
    }

    is->audio_filter_graph = avfilter_graph_alloc();
    
    const AVFilter *abuffersrc = avfilter_get_by_name("abuffer");
    char args[512];
    snprintf(args, sizeof(args),
             "time_base=1/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%" PRIx64,
             is->pAudioCodecCtx->sample_rate,
             is->pAudioCodecCtx->sample_rate,
             av_get_sample_fmt_name(is->pAudioCodecCtx->sample_fmt),
             is->pAudioCodecCtx->channel_layout);
    avfilter_graph_create_filter(&is->audio_filter_src, abuffersrc, "in", 
                                  args, NULL, is->audio_filter_graph);

    const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
    avfilter_graph_create_filter(&is->audio_filter_sink, abuffersink, "out", 
                                  NULL, NULL, is->audio_filter_graph);

    AVFilterContext *last = is->audio_filter_src;
    double remaining = is->playbackSpeed;
    while (remaining > 2.0) {
        AVFilterContext *atempo;
        avfilter_graph_create_filter(&atempo, avfilter_get_by_name("atempo"),
                                     "atempo1", "tempo=2.0", NULL, is->audio_filter_graph);
        avfilter_link(last, 0, atempo, 0);
        last = atempo;
        remaining /= 2.0;
    }
    if (fabs(remaining - 1.0) > 0.001) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "tempo=%.4f", remaining);
        AVFilterContext *atempo;
        avfilter_graph_create_filter(&atempo, avfilter_get_by_name("atempo"),
                                     "atempo_last", tmp, NULL, is->audio_filter_graph);
        avfilter_link(last, 0, atempo, 0);
        last = atempo;
    }

    avfilter_link(last, 0, is->audio_filter_sink, 0);
    avfilter_graph_config(is->audio_filter_graph, NULL);
    
    return true;
}
```

#### 步骤4：实现视频滤镜初始化函数

```cpp
bool init_video_filter(VideoState *is) {
    if (is->video_filter_graph) {
        avfilter_graph_free(&is->video_filter_graph);
        is->video_filter_src = is->video_filter_sink = nullptr;
    }

    is->video_filter_graph = avfilter_graph_alloc();
    
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    char args[512];
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=1/1",
             is->pVideoCodecCtx->width, is->pVideoCodecCtx->height,
             is->pVideoCodecCtx->pix_fmt,
             is->pVideoCodecCtx->time_base.num, is->pVideoCodecCtx->time_base.den);
    avfilter_graph_create_filter(&is->video_filter_src, buffersrc, "in", 
                                  args, NULL, is->video_filter_graph);

    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    avfilter_graph_create_filter(&is->video_filter_sink, buffersink, "out", 
                                  NULL, NULL, is->video_filter_graph);

    char pts_args[64];
    snprintf(pts_args, sizeof(pts_args), "setpts=%.4f*PTS", 1.0 / is->playbackSpeed);
    AVFilterContext *setpts;
    avfilter_graph_create_filter(&setpts, avfilter_get_by_name("setpts"),
                                 "setpts", pts_args, NULL, is->video_filter_graph);
    
    avfilter_link(is->video_filter_src, 0, setpts, 0);
    avfilter_link(setpts, 0, is->video_filter_sink, 0);
    
    avfilter_graph_config(is->video_filter_graph, NULL);
    
    return true;
}
```

#### 步骤5：修改音频解码和视频解码使用滤镜
- 在 `audio_decode_frame` 中，解码后送入音频滤镜处理
- 在视频解码线程中，解码后送入视频滤镜处理

#### 步骤6：修改 setSpeed 方法重建滤镜

```cpp
void VideoPlayer::setSpeed(double speed) {
    m_videoState.playbackSpeed = speed;
    
    if (m_videoState.pAudioCodecCtx) {
        init_audio_filter(&m_videoState);
    }
    if (m_videoState.pVideoCodecCtx) {
        init_video_filter(&m_videoState);
    }
}
```

---

## 三、风险与注意事项

### 3.1 编译风险
- 确保正确包含 `inttypes.h` 头文件
- FFmpeg 版本兼容性（项目使用 4.2.2，`atempo` 滤镜已支持）

### 3.2 性能风险
- 高倍速下视频解码可能跟不上，需要丢帧逻辑

---

## 四、代码亮点（面试时强调）

1. **专业的变速方案**：使用 FFmpeg 滤镜实现变速不变调
2. **音视频同步机制**：以音频时钟为主时钟
3. **滤镜图动态重建**：支持播放过程中实时切换倍速