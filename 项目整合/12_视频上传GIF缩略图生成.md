# 12 视频上传GIF缩略图生成

## 模块概述

`uploaddialog.h/cpp` 在上传视频时，通过FFmpeg解码视频抽取6帧I帧→保存为jpg→QProcess调用ffmpeg合成GIF动图，作为视频缩略图。`QMyMovieLabel`自定义控件实现鼠标悬停时播放GIF动图、离开时显示静态封面。

## 技术选型与理由

> **面试官追问**：为什么抽I帧而不是任意帧？为什么用GIF而不是WebP？

| 方案 | 优势 | 劣势 | 选择理由 |
|------|------|------|----------|
| **FFmpeg抽I帧+GIF** | I帧解码快（无需参考帧）、GIF兼容性好 | GIF画质差、文件大 | I帧是视频关键帧，解码最简单；GIF是所有浏览器/Qt原生支持的动图格式 |
| FFmpeg抽任意帧+WebP | WebP画质好、文件小 | 任意帧需要参考前帧→解码慢、WebP动图Qt支持不完善 | 解码任意帧需要从上一个I帧开始解码→耗时5~10倍 |
| 服务端生成缩略图 | 客户端无需计算 | 上传完成后才能看到缩略图 | 用户需要在上传前预览缩略图 |

> **面试官追问**：I帧是什么？为什么解码快？
> - H264编码中，I帧（Intra-frame）是**完全独立编码的帧**，不依赖其他帧→可以单独解码
> - P帧（Predicted-frame）依赖前一个I/P帧→需要先解码参考帧
> - B帧（Bi-directional-frame）依赖前后两个帧→解码最复杂
> - 抽I帧：直接解码，无需找参考帧→速度最快

## 核心实现流程

### SaveVideoJpg：FFmpeg抽帧 + QProcess合成GIF

```cpp
QString UploadDialog::SaveVideoJpg(const QString& videoPath) {
    // 1. FFmpeg解码视频，抽取6帧I帧
    // 使用QProcess调用ffmpeg命令行：
    //   ffmpeg -i input.mp4 -vf "select=eq(n\,0)+eq(n\,30)+eq(n\,60)+..." -vsync vfr output%d.jpg
    // 或使用FFmpeg C API解码（本项目两种方式都支持）
    
    // 2. 保存6帧jpg：01.jpg~06.jpg
    // 3. QProcess调用ffmpeg合成GIF：
    //   ffmpeg -f image2 -framerate 2 -i %02d.jpg -vf "scale=320:-1" output.gif
    //   -framerate 2：GIF帧率2fps（每帧500ms）
    //   -vf "scale=320:-1"：缩放到320px宽度
    
    // 4. 返回GIF本地路径
    return gifPath;
}
```

> **面试官追问**：为什么GIF帧率是2fps而不是更高？
> - 2fps=每帧500ms，6帧=3秒动图→足够展示视频内容
> - 更高帧率（如10fps）→文件更大（10fps×6帧×每帧50KB=3MB）→上传慢
> - 2fps×6帧×每帧30KB=360KB→上传快，展示效果够用

### QMyMovieLabel：自定义视频缩略图控件

```cpp
class QMyMovieLabel : public QWidget {
    Q_OBJECT
signals:
    void SIG_labelClicked();  // 点击信号→播放视频

private slots:
    void onMovieFrameChanged();  // GIF帧变化时按比例缩放显示

protected:
    void enterEvent(QEvent *event) override;   // 鼠标进入→播放GIF
    void leaveEvent(QEvent *event) override;   // 鼠标离开→显示静态封面
    bool eventFilter(QObject *watched, QEvent *event) override;  // 点击→emit信号
};
```

> **面试官追问**：为什么鼠标悬停才播放GIF？为什么不一直播放？
> - 15个缩略图同时播放GIF→15个QMovie同时解码→CPU占用高→界面卡顿
> - 悬停播放：只有1个GIF在解码→CPU占用低→界面流畅
> - 这是**资源优化策略**：按需播放，避免不必要的CPU消耗

### HTTP下载服务端GIF

```cpp
// MainDialog中下载服务端GIF缩略图
QNetworkAccessManager* m_netManager;  // Qt HTTP客户端
QMap<QNetworkReply*, int> m_downloadingGifs;  // 下载中GIF: reply→网格索引

void MainDialog::slot_dealRecommendRs(STRU_RECOMMEND_RS* rs) {
    for(int i = 0; i < rs->count; i++) {
        QString gifUrl = QString("http://%1/%2").arg(m_ip).arg(rs->videoList[i].gif);
        QNetworkReply* reply = m_netManager->get(QNetworkRequest(QUrl(gifUrl)));
        m_downloadingGifs[reply] = i;  // 记录下载索引
    }
}

void MainDialog::slot_gifDownloadFinished(QNetworkReply* reply) {
    int index = m_downloadingGifs[reply];
    QByteArray data = reply->readAll();
    // 保存GIF到本地缓存→设置QMovie→显示缩略图
}
```

> **面试官追问**：为什么GIF用HTTP下载而不是TCP二进制协议传输？
> - GIF是文件（几百KB），不适合用4KB分块的FILEBLOCK_RQ协议（那是上传用的）
> - HTTP下载更简单：QNetworkAccessManager一行代码搞定，无需自己处理粘包
> - **双协议设计**：业务数据用TCP二进制协议（低延迟），文件传输用HTTP（简单可靠）

## 遇到的难点

### 难点1：FFmpeg抽帧命令行参数复杂

- **问题**：ffmpeg抽I帧的-vf select滤镜参数写法复杂，不同版本语法不同
- **解决**：改用FFmpeg C API解码→avcodec_send_packet/avcodec_receive_frame→检查frame->key_frame判断I帧

### 难点2：GIF缩略图下载失败（网络超时）

- **问题**：推荐列表15个GIF同时下载→HTTP并发请求→部分请求超时
- **解决**：QNetworkAccessManager默认6个并发连接→15个请求排队→前6个立即下载，后9个等待→超时概率降低

> **面试官追问**：如果流量增加10倍，GIF下载哪里会先出问题？
> - **HTTP服务器并发**：10万用户×15个GIF=150万并发HTTP请求→nginx需要调大worker_connections
> - **GIF文件大小**：15个×360KB=5.4MB/用户→10万用户=540GB带宽→需要CDN缓存
> - **优化**：GIF改为静态jpg封面（几十KB）→鼠标悬停时才下载GIF→减少90%带宽

## 模拟面试题

### Q1：你的GIF生成和抖音的封面图有什么区别？
**回答要点**：
- **抖音**：上传时服务端用ffmpeg生成多张封面图→用户选择一张作为封面→封面是静态jpg
- **本项目**：客户端生成GIF动图→上传到服务端→推荐列表显示GIF动图
- **抖音更优**：静态封面加载更快（几KB vs 几百KB），用户选择更灵活
- **本项目GIF的优势**：动图更吸引眼球→点击率更高

### Q2：如果GIF生成耗时很长（10秒），用户体验怎么优化？
**回答要点**：
- 当前：用户点击"浏览"→SaveVideoJpg耗时5~10秒→界面卡住
- **优化1**：在子线程中生成GIF（类似UploadWorker），UI显示"正在生成缩略图..."
- **优化2**：先显示视频第一帧作为临时封面→GIF生成完成后替换为动图
- **优化3**：预生成——用户选择视频后立即开始生成，不等点击"开始上传"

### Q3：QProcess调用ffmpeg和FFmpeg C API的区别？各有什么优劣？
**回答要点**：
- **QProcess**：调用ffmpeg命令行→简单（一行命令）→但需要ffmpeg可执行文件在系统PATH中→部署时需要附带ffmpeg.exe
- **FFmpeg C API**：直接调用libavcodec/libavformat→无需可执行文件→但代码复杂（需要自己写解码循环）
- **本项目**：抽帧用C API（更可控），合成GIF用QProcess（ffmpeg命令行更简单）
- **生产级**：全部用C API→不依赖外部可执行文件→部署更简单
