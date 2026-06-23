#include "clogic.h"

// 视频存储根目录
#define VIDEO_ROOT_PATH "/home/lmh/netdisk/videos/"

// SQL字符串转义：替换单引号防止SQL注入
static std::string EscapeSql(const char* input)
{
    std::string result;
    if(!input) return result;
    while(*input)
    {
        if(*input == '\'')
            result += "\\'";
        else if(*input == '\\')
            result += "\\\\";
        else
            result += *input;
        input++;
    }
    return result;
}

// 文件名安全检查：禁止路径分隔符和特殊字符，防止路径穿越
static bool IsSafeFileName(const char* name)
{
    if(!name || strlen(name) == 0) return false;
    // 禁止路径分隔符、空字节、上级目录引用
    return strpbrk(name, "/\\:\0") == NULL && strstr(name, "..") == NULL;
}

void CLogic::setNetPackMap()
{
    NetPackMap(_DEF_PACK_REGISTER_RQ)    = &CLogic::RegisterRq;
    NetPackMap(_DEF_PACK_LOGIN_RQ)       = &CLogic::LoginRq;
    NetPackMap(_DEF_PACK_UPLOAD_RQ)      = &CLogic::UploadRq;
    NetPackMap(_DEF_PACK_FILEBLOCK_RQ)   = &CLogic::FileBlockRq;
    NetPackMap(_DEF_PACK_RECOMMEND_RQ)   = &CLogic::RecommendRq;
    NetPackMap(_DEF_PACK_PLAY_RQ)        = &CLogic::PlayRq;
}

#define _DEF_COUT_FUNC_    cout << "clientfd:"<< clientfd << " " << __func__ << endl;

//注册
void CLogic::RegisterRq(sock_fd clientfd, char* szbuf, int nlen)
{
    _DEF_COUT_FUNC_
    STRU_REGISTER_RQ * rq = (STRU_REGISTER_RQ*)szbuf;
    STRU_REGISTER_RS rs;

    string strTel = EscapeSql(rq->tel);
    string strPwd = EscapeSql(rq->password);
    string strName = EscapeSql(rq->name);

    char sqlstr[2048] = "";
    sprintf(sqlstr, 
        "insert into t_UserData(tel, password, name, food, funny, ennergy, dance, music, video, outside, edu) "
        "values('%s', '%s', '%s', %d, %d, %d, %d, %d, %d, %d, %d);",
        strTel.c_str(), strPwd.c_str(), strName.c_str(),
        (int)rq->hobby[0], (int)rq->hobby[1], (int)rq->hobby[2], (int)rq->hobby[3],
        (int)rq->hobby[4], (int)rq->hobby[5], (int)rq->hobby[6], (int)rq->hobby[7]);
    bool res = m_sql->UpdataMysql(sqlstr);

    if(res) {
        rs.result = register_success;
    } else {
        rs.result = user_is_exist;
    }

    SendData(clientfd, (char*)&rs, sizeof(rs));
}

//登录
void CLogic::LoginRq(sock_fd clientfd, char* szbuf, int nlen)
{
    _DEF_COUT_FUNC_
    STRU_LOGIN_RQ *rq = (STRU_LOGIN_RQ*)szbuf;
    STRU_LOGIN_RS rs;

    string strTel = EscapeSql(rq->tel);

    char sqlstr[1000] = "";
    list<string> lstRes;
    sprintf(sqlstr, "select id,password from t_UserData where tel='%s';", strTel.c_str());
    m_sql->SelectMysql(sqlstr, 2, lstRes);

    if(lstRes.size() == 0)
    {
        rs.result = user_not_exist;
    }
    else
    {
        int id = atoi(lstRes.front().c_str());
        lstRes.pop_front();
        string strPasswd = lstRes.front();
        lstRes.pop_front();

        if(strcmp(rq->password, strPasswd.c_str()) != 0)
        {
            rs.result = password_error;
        }
        else
        {
            rs.result = login_success;
            rs.userid = id;
        }
    }

    SendData(clientfd, (char*)&rs, sizeof(rs));
}

//上传请求处理
void CLogic::UploadRq(sock_fd clientfd, char* szbuf, int nlen)
{
    _DEF_COUT_FUNC_
    STRU_UPLOAD_RQ* rq = (STRU_UPLOAD_RQ*)szbuf;
    STRU_UPLOAD_RS rs;

    // 文件名安全检查：防止路径穿越攻击
    if(!IsSafeFileName(rq->m_szFileName))
    {
        cout << "  非法文件名: " << rq->m_szFileName << endl;
        rs.result = upload_refuse;
        SendData(clientfd, (char*)&rs, sizeof(rs));
        return;
    }

    // 判断是否为GIF缩略图（GIF不做重复检查）
    bool isGifUpload = (strcmp(rq->m_szFileType, "gif") == 0);

    // 视频文件：通过MD5哈希检查是否已存在（去重）
    if(!isGifUpload && strlen(rq->m_szFileHash) > 0)
    {
        string strHash = EscapeSql(rq->m_szFileHash);
        char sqlBuf[4096] = "";
        sprintf(sqlBuf, "SELECT id FROM t_VideoData WHERE hash='%s';", strHash.c_str());
        list<string> lstRes;
        if(m_sql->SelectMysql(sqlBuf, 1, lstRes) && lstRes.size() > 0)
        {
            // 视频已存在，拒绝上传
            rs.result = video_already_exist;
            SendData(clientfd, (char*)&rs, sizeof(rs));
            cout << "  视频已存在(MD5重复): " << rq->m_szFileHash << endl;
            return;
        }
    }

    // 创建视频存储目录
    system("mkdir -p " VIDEO_ROOT_PATH);

    // 保存上传状态
    UploadInfo info;
    info.fileId = rq->m_nFileId;
    info.clientfd = clientfd;
    info.userId = rq->m_UserId;
    info.fileSize = rq->m_nFileSize;
    strcpy(info.fileName, rq->m_szFileName);
    strcpy(info.gifName, rq->m_szGifName);
    strcpy(info.fileType, rq->m_szFileType);
    strcpy(info.fileHash, rq->m_szFileHash);
    memcpy(info.hobby, rq->m_szHobby, 8);

    // 创建临时文件（先写.tmp，完成后rename为正式文件名）
    char tmpPath[520] = "";
    sprintf(tmpPath, VIDEO_ROOT_PATH "%d_%s.tmp", rq->m_nFileId, rq->m_szFileName);
    info.fd = open(tmpPath, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if(info.fd < 0)
    {
        cout << "  创建临时文件失败: " << tmpPath << endl;
        rs.result = upload_refuse;
        SendData(clientfd, (char*)&rs, sizeof(rs));
        return;
    }

    // 记录上传状态（key=fileId，视频和GIF各自独立）
    {
        pthread_mutex_lock(&m_mapMutex);
        m_mapUpload[rq->m_nFileId] = info;
        pthread_mutex_unlock(&m_mapMutex);
    }

    rs.result = upload_accept;
    SendData(clientfd, (char*)&rs, sizeof(rs));
    cout << "  同意上传 fileId=" << rq->m_nFileId
         << " fileName=" << rq->m_szFileName
         << " fileSize=" << rq->m_nFileSize
         << " fileType=" << rq->m_szFileType << endl;
}

//文件块上传处理
void CLogic::FileBlockRq(sock_fd clientfd, char* szbuf, int nlen)
{
    STRU_FILEBLOCK_RQ* rq = (STRU_FILEBLOCK_RQ*)szbuf;
    STRU_FILEBLOCK_RS rs;
    rs.m_nFileId = rq->m_nFileId;
    rs.m_nBlockIndex = rq->m_nBlockIndex;

    // 通过fileId查找上传状态
    UploadInfo info;
    {
        pthread_mutex_lock(&m_mapMutex);
        auto it = m_mapUpload.find(rq->m_nFileId);
        if(it == m_mapUpload.end())
        {
            pthread_mutex_unlock(&m_mapMutex);
            rs.result = fileblock_fail;
            SendData(clientfd, (char*)&rs, sizeof(rs));
            return;
        }
        info = it->second;  // 拷贝出来，锁外做IO
        pthread_mutex_unlock(&m_mapMutex);
    }

    // 写入文件块（锁外做IO，避免持锁阻塞其他客户端）
    int writeLen = write(info.fd, rq->m_szBlockData, rq->m_nBlockSize);
    if(writeLen != rq->m_nBlockSize)
    {
        cout << "  文件块写入失败 fileId=" << rq->m_nFileId
             << " blockIndex=" << rq->m_nBlockIndex << endl;
        rs.result = fileblock_fail;
        SendData(info.clientfd, (char*)&rs, sizeof(rs));
        return;
    }

    info.uploadedSize += writeLen;
    info.blockIndex++;
    rs.result = fileblock_success;
    SendData(info.clientfd, (char*)&rs, sizeof(rs));

    // 上传完成判定
    if(info.uploadedSize >= info.fileSize)
    {
        // 关闭临时文件
        close(info.fd);

        bool isGifUpload = (strcmp(info.fileType, "gif") == 0);

        // 拼接临时文件路径和目标文件路径
        char tmpPath[520] = "";
        char dstPath[520] = "";
        sprintf(tmpPath, VIDEO_ROOT_PATH "%d_%s.tmp", info.fileId, info.fileName);
        sprintf(dstPath, VIDEO_ROOT_PATH "%d_%s", info.fileId, info.fileName);

        // 重命名临时文件为正式文件名
        rename(tmpPath, dstPath);

        if(isGifUpload)
        {
            // GIF上传完成：更新对应视频记录的GIF路径
            char actualGifPath[520] = "";
            sprintf(actualGifPath, VIDEO_ROOT_PATH "%d_%s", info.fileId, info.fileName);

            string strGifPath = EscapeSql(actualGifPath);
            char sqlBuf[4096] = "";
            sprintf(sqlBuf,
                "UPDATE t_VideoData SET gif='%s' "
                "WHERE userid=%d AND gif='' ORDER BY id DESC LIMIT 1;",
                strGifPath.c_str(), info.userId);
            m_sql->UpdataMysql(sqlBuf);

            cout << "  GIF上传完成: " << dstPath
                 << " 已更新视频GIF路径" << endl;
        }
        else
        {
            // 视频上传完成：拼接RTMP地址 + 写入数据库
            // GIF路径暂存为空，等GIF上传完成后UPDATE
            char rtmpUrl[520] = "";
            sprintf(rtmpUrl, "rtmp://192.168.18.129:1935/vod/%d_%s", info.fileId, info.fileName);

            string strName = EscapeSql(info.fileName);
            string strUrl = EscapeSql(rtmpUrl);
            string strHash = EscapeSql(info.fileHash);

            // INSERT INTO t_VideoData（gif字段暂为空，GIF上传后UPDATE）
            char sqlBuf[8192] = "";
            sprintf(sqlBuf,
                "INSERT INTO t_VideoData(userid, name, url, gif, hash, "
                "food, funny, ennergy, dance, music, video, outside, edu) "
                "VALUES(%d, '%s', '%s', '', '%s', %d, %d, %d, %d, %d, %d, %d, %d);",
                info.userId, strName.c_str(), strUrl.c_str(), strHash.c_str(),
                (int)info.hobby[0], (int)info.hobby[1], (int)info.hobby[2], (int)info.hobby[3],
                (int)info.hobby[4], (int)info.hobby[5], (int)info.hobby[6], (int)info.hobby[7]);
            m_sql->UpdataMysql(sqlBuf);

            cout << "  视频上传完成: " << dstPath
                 << " rtmpUrl=" << rtmpUrl << endl;
        }

        // 移除上传状态
        pthread_mutex_lock(&m_mapMutex);
        m_mapUpload.erase(info.fileId);
        pthread_mutex_unlock(&m_mapMutex);
    }
    else
    {
        // 未完成，更新进度
        pthread_mutex_lock(&m_mapMutex);
        auto it = m_mapUpload.find(rq->m_nFileId);
        if(it != m_mapUpload.end())
        {
            it->second.uploadedSize = info.uploadedSize;
            it->second.blockIndex = info.blockIndex;
        }
        pthread_mutex_unlock(&m_mapMutex);
    }
}

//推荐请求处理（精准评分算法）
void CLogic::RecommendRq(sock_fd clientfd, char* szbuf, int nlen)
{
    _DEF_COUT_FUNC_
    STRU_RECOMMEND_RQ* rq = (STRU_RECOMMEND_RQ*)szbuf;
    STRU_RECOMMEND_RS rs;
    int userId = rq->m_UserId;

    // 1. 获取用户爱好向量（从数据库读取注册兴趣）
    double myVec[8] = {0};
    char sqlHobby[512] = "";
    sprintf(sqlHobby, "SELECT food,funny,ennergy,dance,music,video,outside,edu FROM t_UserData WHERE id=%d;", userId);
    list<string> lstHobby;
    if(m_sql->SelectMysql(sqlHobby, 8, lstHobby) && lstHobby.size() >= 8) {
        for(int i = 0; i < 8; i++) {
            myVec[i] = atoi(lstHobby.front().c_str()) * 10.0; // 赋予基准分
            lstHobby.pop_front();
        }
    }

    // 2. 综合评分排序：(标签匹配度 * 100) + 热度 (注意：若表中无hotdegree则默认为0)
    char sqlBuf[8192] = "";
    sprintf(sqlBuf,
        "SELECT id, userid, name, url, gif, food, funny, ennergy, dance, music, video, outside, edu, "
        "((IFNULL(food,0)*%f + IFNULL(funny,0)*%f + IFNULL(ennergy,0)*%f + IFNULL(dance,0)*%f + "
        "IFNULL(music,0)*%f + IFNULL(video,0)*%f + IFNULL(outside,0)*%f + IFNULL(edu,0)*%f) * 100 + IFNULL(hotdegree,0)) as score "
        "FROM t_VideoData ORDER BY score DESC, id DESC LIMIT 15;",
        myVec[0], myVec[1], myVec[2], myVec[3], myVec[4], myVec[5], myVec[6], myVec[7]);

    list<string> lstRes;
    if(m_sql->SelectMysql(sqlBuf, 14, lstRes))
    {
        int count = 0;
        while(lstRes.size() >= 14 && count < 15)
        {
            rs.videoList[count].id = atoi(lstRes.front().c_str()); lstRes.pop_front();
            rs.videoList[count].userid = atoi(lstRes.front().c_str()); lstRes.pop_front();

            string strName = lstRes.front(); lstRes.pop_front();
            strncpy(rs.videoList[count].name, strName.c_str(), _MAX_FILE_PATH - 1);

            string strUrl = lstRes.front(); lstRes.pop_front();
            strncpy(rs.videoList[count].url, strUrl.c_str(), _MAX_FILE_PATH - 1);

            string strGif = lstRes.front(); lstRes.pop_front();
            string gifUrl = strGif;
            if(!strGif.empty()) {
                size_t lastSlash = strGif.find_last_of('/');
                if(lastSlash != string::npos) {
                    gifUrl = "http://192.168.18.129/gif/" + strGif.substr(lastSlash + 1);
                }
            }
            strncpy(rs.videoList[count].gif, gifUrl.c_str(), _MAX_FILE_PATH - 1);

            for(int i=0; i<8; i++) {
                rs.videoList[count].hobby[i] = (char)atoi(lstRes.front().c_str()); 
                lstRes.pop_front();
            }
            lstRes.pop_front(); // score

            count++;
        }
        rs.count = count;
    }

    SendData(clientfd, (char*)&rs, sizeof(rs));
}

//播放请求处理
void CLogic::PlayRq(sock_fd clientfd, char* szbuf, int nlen)
{
    _DEF_COUT_FUNC_
    STRU_PLAY_RQ* rq = (STRU_PLAY_RQ*)szbuf;
    STRU_PLAY_RS rs;

    // 根据videoId查询RTMP播放地址
    char sqlBuf[512] = "";
    sprintf(sqlBuf, "SELECT url FROM t_VideoData WHERE id=%d;", rq->m_videoId);
    list<string> lstRes;
    if(m_sql->SelectMysql(sqlBuf, 1, lstRes) && lstRes.size() > 0)
    {
        rs.result = play_success;
        string strUrl = lstRes.front();
        strncpy(rs.url, strUrl.c_str(), _MAX_FILE_PATH - 1);
        cout << "  播放地址: " << rs.url << endl;
    }
    else
    {
        rs.result = play_fail;
        cout << "  视频不存在: id=" << rq->m_videoId << endl;
    }

    SendData(clientfd, (char*)&rs, sizeof(rs));
}
