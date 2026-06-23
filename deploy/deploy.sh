#!/bin/bash
# ============================================
# 短视频平台 - 服务端部署脚本
# 用法: 在Windows上通过scp将文件传到VM后，在VM上执行此脚本
# 或者: 直接在VM上执行（需先将文件复制过去）
# ============================================

set -e

VM_IP="192.168.18.129"
VM_USER="lmh"
VM_PASS="colin123"
SERVER_DIR="/home/lmh/netdisk/0611/NetDisk"
VIDEO_DIR="/home/lmh/netdisk/videos"

echo "===== 短视频平台服务端部署 ====="

# 1. 创建视频存储目录
echo "[1/5] 创建视频存储目录..."
mkdir -p ${VIDEO_DIR}
echo "  目录已创建: ${VIDEO_DIR}"

# 2. 复制源文件到服务端项目目录
echo "[2/5] 复制源文件..."
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_DIR=$(dirname "$SCRIPT_DIR")

# 复制头文件
cp -v ${PROJECT_DIR}/include/packdef.h      ${SERVER_DIR}/include/
cp -v ${PROJECT_DIR}/include/clogic.h        ${SERVER_DIR}/include/

# 复制源文件
cp -v ${PROJECT_DIR}/src/clogic.cpp          ${SERVER_DIR}/src/
cp -v ${PROJECT_DIR}/src/block_epoll_net.cpp  ${SERVER_DIR}/src/

echo "  文件复制完成"

# 3. 执行建表SQL
echo "[3/5] 创建数据库表..."
mysql -u root -pcolin123 < ${PROJECT_DIR}/deploy/create_table.sql
echo "  数据库表创建完成"

# 4. 编译服务端
echo "[4/5] 编译服务端..."
cd ${SERVER_DIR}/src
make clean 2>/dev/null || true
make
echo "  编译完成"

# 5. 配置Nginx静态文件服务（GIF缩略图HTTP下载）
echo "[5/5] 配置Nginx GIF静态文件服务..."
NGINX_CONF="/usr/local/nginx/conf/nginx.conf"
if [ -f "$NGINX_CONF" ]; then
    # 检查是否已配置gif location
    if ! grep -q "location /gif/" "$NGINX_CONF"; then
        # 在nginx.conf的http块中添加server块（如果不存在80端口server）
        # 使用sed在http块的最后一个}之前插入配置
        sed -i '/^[[:space:]]*}$/i\
    # GIF缩略图静态文件服务\
    server {\
        listen 80;\
        server_name 192.168.18.129;\
        location /gif/ {\
            alias /home/lmh/netdisk/videos/;\
            autoindex off;\
        }\
    }' "$NGINX_CONF"
        echo "  Nginx配置已更新"
        # 重载Nginx
        /usr/local/nginx/sbin/nginx -s reload 2>/dev/null || echo "  Nginx重载失败，请手动执行: /usr/local/nginx/sbin/nginx -s reload"
    else
        echo "  Nginx GIF配置已存在，跳过"
    fi
else
    echo "  未找到Nginx配置文件: $NGINX_CONF"
    echo "  请手动添加以下配置到nginx.conf的http块中:"
    echo ""
    echo "  server {"
    echo "      listen 80;"
    echo "      server_name 192.168.18.129;"
    echo "      location /gif/ {"
    echo "          alias /home/lmh/netdisk/videos/;"
    echo "          autoindex off;"
    echo "      }"
    echo "  }"
fi

echo ""
echo "===== 部署完成 ====="
echo "启动服务端: cd ${SERVER_DIR}/src && ./server"
echo "验证GIF服务: curl http://192.168.18.129/gif/"
echo ""
