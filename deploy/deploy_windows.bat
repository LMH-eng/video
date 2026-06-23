@echo off
REM ============================================
REM Short Video Platform - Deploy to VM
REM Prerequisite: OpenSSH client installed (Win10+)
REM ============================================

set VM_IP=192.168.18.129
set VM_USER=lmh
set VM_SERVER_DIR=/home/lmh/netdisk/0611/NetDisk
set LOCAL_PROJECT_DIR=%~dp0..

echo ===== Deploy Server Code to VM =====
echo Target: %VM_USER%@%VM_IP%:%VM_SERVER_DIR%
echo.

echo [1/3] Sync header files...
scp %LOCAL_PROJECT_DIR%\include\packdef.h %VM_USER%@%VM_IP%:%VM_SERVER_DIR%/include/
scp %LOCAL_PROJECT_DIR%\include\clogic.h   %VM_USER%@%VM_IP%:%VM_SERVER_DIR%/include/

echo [2/3] Sync source files...
scp %LOCAL_PROJECT_DIR%\src\clogic.cpp          %VM_USER%@%VM_IP%:%VM_SERVER_DIR%/src/
scp %LOCAL_PROJECT_DIR%\src\block_epoll_net.cpp  %VM_USER%@%VM_IP%:%VM_SERVER_DIR%/src/

echo [3/3] Sync SQL script...
scp %LOCAL_PROJECT_DIR%\deploy\create_table.sql %VM_USER%@%VM_IP%:/tmp/create_table.sql

echo.
echo ===== Sync Complete =====
echo.
echo Next: SSH to VM and run:
echo   ssh %VM_USER%@%VM_IP%
echo   mkdir -p /home/lmh/netdisk/videos
echo   mysql -u root -pcolin123 ^< /tmp/create_table.sql
echo   cd %VM_SERVER_DIR%/src ^&^& make clean ^&^& make
echo   ./server
echo.
pause
