-- ============================================
-- 短视频平台 - 数据库建表脚本
-- 数据库: NetDisk
-- 执行方式: mysql -u root -pcolin123 < create_table.sql
-- ============================================

USE NetDisk;

-- 用户数据表
CREATE TABLE IF NOT EXISTS t_UserData (
    id       INT AUTO_INCREMENT PRIMARY KEY,
    tel      VARCHAR(50)  NOT NULL UNIQUE COMMENT '手机号(注册查重)',
    password VARCHAR(50)  NOT NULL        COMMENT 'MD5加密密码',
    name     VARCHAR(50)  NOT NULL        COMMENT '用户昵称',
    food     INT DEFAULT 0 COMMENT '美食',
    funny    INT DEFAULT 0 COMMENT '搞笑',
    ennergy  INT DEFAULT 0 COMMENT '正能量',
    dance    INT DEFAULT 0 COMMENT '舞蹈',
    music    INT DEFAULT 0 COMMENT '歌曲',
    video    INT DEFAULT 0 COMMENT '影视',
    outside  INT DEFAULT 0 COMMENT '户外',
    edu      INT DEFAULT 0 COMMENT '教育'
) DEFAULT CHARSET=utf8;

-- 视频数据表
CREATE TABLE IF NOT EXISTS t_VideoData (
    id       INT AUTO_INCREMENT PRIMARY KEY,
    userid   INT NOT NULL,
    name     VARCHAR(260) NOT NULL  COMMENT '视频文件名',
    url      VARCHAR(520) NOT NULL  COMMENT 'RTMP播放地址',
    gif      VARCHAR(260) DEFAULT '' COMMENT 'GIF缩略图路径',
    hash     VARCHAR(33)  DEFAULT '' COMMENT 'MD5哈希(去重)',
    time     TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '上传时间',
    food     INT DEFAULT 0 COMMENT '美食',
    funny    INT DEFAULT 0 COMMENT '搞笑',
    ennergy  INT DEFAULT 0 COMMENT '正能量',
    dance    INT DEFAULT 0 COMMENT '舞蹈',
    music    INT DEFAULT 0 COMMENT '歌曲',
    video    INT DEFAULT 0 COMMENT '影视',
    outside  INT DEFAULT 0 COMMENT '户外',
    edu      INT DEFAULT 0 COMMENT '教育',
    hotdegree INT DEFAULT 0 COMMENT '热度值(播放+点赞累计)'
) DEFAULT CHARSET=utf8;

-- 验证建表结果
DESC t_UserData;
DESC t_VideoData;

-- ============================================
-- 如果数据库已存在，执行以下语句修复表结构
-- ============================================
-- ALTER TABLE t_UserData ADD COLUMN ennergy INT DEFAULT 0 COMMENT '正能量';
-- ALTER TABLE t_UserData ADD COLUMN dance INT DEFAULT 0 COMMENT '舞蹈';
-- ALTER TABLE t_UserData ADD COLUMN video INT DEFAULT 0 COMMENT '影视';
-- ALTER TABLE t_UserData ADD COLUMN outside INT DEFAULT 0 COMMENT '户外';
-- ALTER TABLE t_UserData ADD COLUMN edu INT DEFAULT 0 COMMENT '教育';
-- ALTER TABLE t_UserData ADD UNIQUE INDEX idx_tel (tel);
-- ALTER TABLE t_VideoData ADD COLUMN hotdegree INT DEFAULT 0 COMMENT '热度值(播放+点赞累计)';
