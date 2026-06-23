#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include "err_str.h"
#include <malloc.h>

#include<iostream>
#include<map>
#include<list>


//边界值
#define _DEF_SIZE           45
#define _DEF_BUFFERSIZE     1000
#define _DEF_PORT           8000
#define _DEF_SERVERIP       "0.0.0.0"
#define _DEF_LISTEN         128
#define _DEF_EPOLLSIZE      4096
#define _DEF_IPSIZE         16
#define _DEF_COUNT          10
#define _DEF_TIMEOUT        10
#define _DEF_SQLIEN         400
#define TRUE                true
#define FALSE               false



/*-------------数据库信息-----------------*/
#define _DEF_DB_NAME    "NetDisk"
#define _DEF_DB_IP      "localhost"
#define _DEF_DB_USER    "root"
#define _DEF_DB_PWD     "colin123"
/*--------------------------------------*/
#define _MAX_PATH           (260)
#define _DEF_BUFFER         (4096)
#define _DEF_CONTENT_SIZE	(1024)
#define _MAX_SIZE           (40)
#define _MAX_FILE_PATH      (260)

//自定义协议   先写协议头 再写协议结构
//登录 注册 获取好友信息 添加好友 聊天 发文件 下线请求
#define _DEF_PACK_BASE	(10000)
#define _DEF_PACK_COUNT (100)

//注册
#define _DEF_PACK_REGISTER_RQ	(_DEF_PACK_BASE + 0 )
#define _DEF_PACK_REGISTER_RS	(_DEF_PACK_BASE + 1 )
//登录
#define _DEF_PACK_LOGIN_RQ	(_DEF_PACK_BASE + 2 )
#define _DEF_PACK_LOGIN_RS	(_DEF_PACK_BASE + 3 )
//上传
#define _DEF_PACK_UPLOAD_RQ		(_DEF_PACK_BASE + 4 )
#define _DEF_PACK_UPLOAD_RS		(_DEF_PACK_BASE + 5 )
//文件块
#define _DEF_PACK_FILEBLOCK_RQ	(_DEF_PACK_BASE + 6 )
#define _DEF_PACK_FILEBLOCK_RS	(_DEF_PACK_BASE + 7 )
//推荐
#define _DEF_PACK_RECOMMEND_RQ	(_DEF_PACK_BASE + 8 )
#define _DEF_PACK_RECOMMEND_RS	(_DEF_PACK_BASE + 9 )
//播放
#define _DEF_PACK_PLAY_RQ		(_DEF_PACK_BASE + 10 )
#define _DEF_PACK_PLAY_RS		(_DEF_PACK_BASE + 11 )


//返回的结果
//注册请求的结果
#define user_is_exist		(0)
#define register_success	(1)
//登录请求的结果
#define user_not_exist		(0)
#define password_error		(1)
#define login_success		(2)
//上传请求的结果
#define upload_refuse		(0)
#define upload_accept		(1)
#define video_already_exist	(2)   // 视频已存在（MD5重复）
//文件块的结果
#define fileblock_fail		(0)
#define fileblock_success	(1)
//播放请求的结果
#define play_fail			(0)
#define play_success		(1)

//Hobby分类标签结构体 8个char 对应界面8个复选框
#define DEF_HOBBY_COUNT (8)
typedef struct Hobby
{
    char food;     // 美食
    char funny;    // 搞笑
    char ennergy;  // 正能量
    char dance;    // 舞蹈
    char music;    // 歌曲
    char video;    // 影视
    char outside;  // 户外
    char edu;      // 教育
}Hobby;


typedef int PackType;

//协议结构
//注册
typedef struct STRU_REGISTER_RQ
{
    STRU_REGISTER_RQ():type(_DEF_PACK_REGISTER_RQ)
    {
        memset( tel  , 0, sizeof(tel));
        memset( name  , 0, sizeof(name));
        memset( password , 0, sizeof(password) );
    }
    //需要手机号码 , 密码, 昵称
    PackType type;
    char tel[_MAX_SIZE];
    char name[_MAX_SIZE];
    char password[_MAX_SIZE];

}STRU_REGISTER_RQ;

typedef struct STRU_REGISTER_RS
{
    //回复结果
    STRU_REGISTER_RS(): type(_DEF_PACK_REGISTER_RS) , result(register_success)
    {
    }
    PackType type;
    int result;

}STRU_REGISTER_RS;

//登录
typedef struct STRU_LOGIN_RQ
{
    //登录需要: 手机号 密码
    STRU_LOGIN_RQ():type(_DEF_PACK_LOGIN_RQ)
    {
        memset( tel , 0, sizeof(tel) );
        memset( password , 0, sizeof(password) );
    }
    PackType type;
    char tel[_MAX_SIZE];
    char password[_MAX_SIZE];

}STRU_LOGIN_RQ;

typedef struct STRU_LOGIN_RS
{
    // 需要 结果 , 用户的id
    STRU_LOGIN_RS(): type(_DEF_PACK_LOGIN_RS) , result(login_success),userid(0)
    {
    }
    PackType type;
    int result;
    int userid;

}STRU_LOGIN_RS;


//上传请求
typedef struct STRU_UPLOAD_RQ
{
    STRU_UPLOAD_RQ()
    {
        m_nType = _DEF_PACK_UPLOAD_RQ;
        m_nFileId = 0;
        m_UserId = 0;
        m_nFileSize = 0;
        memset(m_szFileName, 0, sizeof(m_szFileName));
        memset(m_szGifName, 0, sizeof(m_szGifName));
        memset(m_szFileType, 0, sizeof(m_szFileType));
        memset(m_szHobby, 0, sizeof(m_szHobby));
        memset(m_szFileHash, 0, sizeof(m_szFileHash));
    }
    PackType m_nType;          // 数据包类型
    int m_UserId;              // 当前登录用户ID
    int m_nFileId;             // 文件唯一随机ID 区分上传任务
    int64_t m_nFileSize;       // 文件总字节大小
    char m_szHobby[DEF_HOBBY_COUNT]; // 8字节视频分类标签
    char m_szFileName[_MAX_FILE_PATH];     // 原始视频文件名
    char m_szGifName[_MAX_FILE_PATH];      // GIF动图文件名
    char m_szFileType[_MAX_SIZE];     // 文件后缀类型 mp4/flv
    char m_szFileHash[33];           // 文件MD5哈希（32字符+'\0'）用于去重
}STRU_UPLOAD_RQ;

//上传回复
typedef struct STRU_UPLOAD_RS
{
    STRU_UPLOAD_RS() : type(_DEF_PACK_UPLOAD_RS), result(upload_refuse)
    {
    }
    PackType type;
    int result;  // 0=拒绝 1=同意 2=视频已存在
}STRU_UPLOAD_RS;

//文件块请求
typedef struct STRU_FILEBLOCK_RQ
{
    STRU_FILEBLOCK_RQ()
    {
        type = _DEF_PACK_FILEBLOCK_RQ;
        m_nFileId = 0;
        m_nBlockIndex = 0;
        m_nBlockSize = 0;
        memset(m_szBlockData, 0, sizeof(m_szBlockData));
    }
    PackType type;
    int m_nFileId;            // 文件ID 对应上传请求
    int m_nBlockIndex;        // 文件块序号
    int m_nBlockSize;         // 实际数据大小
    char m_szBlockData[_DEF_BUFFER]; // 文件块数据
}STRU_FILEBLOCK_RQ;

//文件块回复
typedef struct STRU_FILEBLOCK_RS
{
    STRU_FILEBLOCK_RS() : type(_DEF_PACK_FILEBLOCK_RS), m_nFileId(0), m_nBlockIndex(0), result(fileblock_fail)
    {
    }
    PackType type;
    int m_nFileId;
    int m_nBlockIndex;
    int result;  // 0=失败 1=成功
}STRU_FILEBLOCK_RS;

//单个视频信息
typedef struct STRU_VIDEO_INFO
{
    STRU_VIDEO_INFO()
    {
        id = 0;
        userid = 0;
        memset(name, 0, sizeof(name));
        memset(url, 0, sizeof(url));
        memset(gif, 0, sizeof(gif));
        memset(hobby, 0, sizeof(hobby));
    }
    int id;
    int userid;
    char name[_MAX_FILE_PATH];
    char url[_MAX_FILE_PATH];
    char gif[_MAX_FILE_PATH];
    char hobby[DEF_HOBBY_COUNT];
}STRU_VIDEO_INFO;

//推荐请求
typedef struct STRU_RECOMMEND_RQ
{
    STRU_RECOMMEND_RQ() : type(_DEF_PACK_RECOMMEND_RQ), m_UserId(0)
    {
    }
    PackType type;
    int m_UserId;
}STRU_RECOMMEND_RQ;

//推荐回复
typedef struct STRU_RECOMMEND_RS
{
    STRU_RECOMMEND_RS() : type(_DEF_PACK_RECOMMEND_RS), count(0)
    {
    }
    PackType type;
    int count;                         // 视频数量
    STRU_VIDEO_INFO videoList[15];     // 最多15个视频 3行x5列
}STRU_RECOMMEND_RS;

//播放请求
typedef struct STRU_PLAY_RQ
{
    STRU_PLAY_RQ() : type(_DEF_PACK_PLAY_RQ), m_videoId(0), m_UserId(0)
    {
    }
    PackType type;
    int m_videoId;
    int m_UserId;
}STRU_PLAY_RQ;

//播放回复
typedef struct STRU_PLAY_RS
{
    STRU_PLAY_RS() : type(_DEF_PACK_PLAY_RS), result(play_fail)
    {
        memset(url, 0, sizeof(url));
    }
    PackType type;
    int result;
    char url[_MAX_FILE_PATH]; // RTMP播放地址
}STRU_PLAY_RS;


