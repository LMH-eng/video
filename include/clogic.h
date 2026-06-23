#ifndef CLOGIC_H
#define CLOGIC_H

#include"TCPKernel.h"
#include<map>
#include<vector>
#include<fcntl.h>
#include<unistd.h>

// 上传文件状态结构体（key=fileId，视频和GIF各自独立）
typedef struct UploadInfo
{
    int fileId;
    int clientfd;      // 客户端socket fd（用于发送响应）
    int userId;
    int64_t fileSize;
    int64_t uploadedSize;
    int blockIndex;
    char fileName[260];
    char gifName[260];
    char fileType[40];
    char fileHash[33];          // 文件MD5哈希（用于去重）
    char hobby[8];
    int fd;                     // 临时文件描述符
    UploadInfo() : fileId(0), clientfd(-1), userId(0), fileSize(0),
                   uploadedSize(0), blockIndex(0), fd(-1) {
        memset(fileName, 0, sizeof(fileName));
        memset(gifName, 0, sizeof(gifName));
        memset(fileType, 0, sizeof(fileType));
        memset(fileHash, 0, sizeof(fileHash));
        memset(hobby, 0, sizeof(hobby));
    }
}UploadInfo;

class CLogic
{
public:
    CLogic( TcpKernel* pkernel )
    {
        m_pKernel = pkernel;
        m_sql = pkernel->m_sql;
        m_tcp = pkernel->m_tcp;
        pthread_mutex_init(&m_mapMutex, NULL);
    }
    ~CLogic()
    {
        pthread_mutex_destroy(&m_mapMutex);
    }
public:
    //设置协议映射
    void setNetPackMap();
    /************** 发送数据*********************/
    void SendData( sock_fd clientfd, char*szbuf, int nlen )
    {
        m_pKernel->SendData( clientfd ,szbuf , nlen );
    }
    /************** 网络处理 *********************/
    //注册
    void RegisterRq(sock_fd clientfd, char*szbuf, int nlen);
    //登录
    void LoginRq(sock_fd clientfd, char*szbuf, int nlen);
    //上传请求
    void UploadRq(sock_fd clientfd, char*szbuf, int nlen);
    //文件块上传
    void FileBlockRq(sock_fd clientfd, char*szbuf, int nlen);
    //推荐请求
    void RecommendRq(sock_fd clientfd, char*szbuf, int nlen);
    //播放请求
    void PlayRq(sock_fd clientfd, char*szbuf, int nlen);

    /*******************************************/

private:
    TcpKernel* m_pKernel;
    CMysql * m_sql;
    Block_Epoll_Net * m_tcp;

    // 上传任务映射：fileId -> UploadInfo
    std::map<int, UploadInfo> m_mapUpload;
    // 上传map互斥锁
    pthread_mutex_t m_mapMutex;
};

#endif // CLOGIC_H
