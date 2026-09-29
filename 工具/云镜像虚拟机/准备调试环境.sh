#!/bin/bash
# 在 Ubuntu-Debug 虚拟机中准备远程调试环境。
set -e

echo "=== 系统概况 ==="
echo "主机名 : $(hostname)"
echo "内核   : $(uname -r)"
echo "内存   : $(free -h | awk '/Mem:/{print $2}')"
echo "磁盘   : $(df -h / | awk 'NR==2{print $2" 总 / "$4" 可用"}')"
echo "CPU    : $(nproc) 核"
echo

echo "=== 测试各镜像源的下载速度 ==="
BEST=""
BEST_SPEED=0
for m in mirrors.ustc.edu.cn mirrors.tuna.tsinghua.edu.cn mirrors.aliyun.com mirrors.huaweicloud.com archive.ubuntu.com; do
    URL="https://$m/ubuntu/dists/noble/Release"
    # 取 200 KB 计时
    T=$( { /usr/bin/time -f "%e" curl -s -r 0-204799 -o /dev/null "$URL" ; } 2>&1 )
    if [ -z "$T" ]; then
        printf "  %-34s 不可达\n" "$m"
        continue
    fi
    SPEED=$(awk -v t="$T" 'BEGIN{ if (t>0) printf "%.0f", 200/t; else print 0 }')
    printf "  %-34s %6s KB/s\n" "$m" "$SPEED"
    if [ "$SPEED" -gt "$BEST_SPEED" ]; then BEST_SPEED=$SPEED; BEST=$m; fi
done

if [ -z "$BEST" ]; then
    echo "  没有可用镜像，保持原配置"
else
    echo
    echo "=== 选用 $BEST（$BEST_SPEED KB/s）==="
    sudo cp /etc/apt/sources.list.d/ubuntu.sources /etc/apt/sources.list.d/ubuntu.sources.bak 2>/dev/null || true
    sudo tee /etc/apt/sources.list.d/ubuntu.sources > /dev/null <<EOF
Types: deb
URIs: https://$BEST/ubuntu/
Suites: noble noble-updates noble-backports
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: https://$BEST/ubuntu/
Suites: noble-security
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF
    echo "已切换"
fi

echo
echo "=== 更新软件包索引 ==="
sudo apt-get update -qq 2>&1 | tail -3
echo "完成"

echo
echo "=== 安装调试与构建工具链 ==="
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
    build-essential gdb cmake ninja-build rsync pkg-config git curl \
    2>&1 | tail -5
echo "完成"

echo
echo "=== 版本确认 ==="
printf "  %-12s %s\n" "gcc"   "$(gcc --version | head -1)"
printf "  %-12s %s\n" "g++"   "$(g++ --version | head -1)"
printf "  %-12s %s\n" "gdb"   "$(gdb --version | head -1)"
printf "  %-12s %s\n" "cmake" "$(cmake --version | head -1)"
printf "  %-12s %s\n" "ninja" "$(ninja --version)"
printf "  %-12s %s\n" "make"  "$(make --version | head -1)"

echo
echo "=== gdb 的调试目标支持（关键）==="
gdb --configuration | grep -E 'target|host' | head -4
echo
echo "（对照：Windows 版 GDB 只有 x86_64-w64-mingw32 目标，无法调试 Linux 程序）"

echo
echo "=== 磁盘占用 ==="
df -h / | tail -1
