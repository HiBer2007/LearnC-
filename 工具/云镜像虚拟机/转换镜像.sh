#!/bin/bash
# 把 Ubuntu 云镜像从 qcow2 转成 Hyper-V 能用的 VHDX，并扩容。
set -e

SRC_DIR="/mnt/k/系统镜像"
SRC="$SRC_DIR/noble-server-cloudimg-amd64.img"
DST="$SRC_DIR/Ubuntu-Debug.vhdx"
TARGET_GB=60

echo "源文件: $SRC"
if [ ! -f "$SRC" ]; then echo "找不到源文件"; exit 1; fi

rm -f "$DST"

echo
echo "=== 转换为 VHDX（动态分配）==="
START=$(date +%s)
qemu-img convert -f qcow2 -O vhdx -o subformat=dynamic "$SRC" "$DST"
END=$(date +%s)
echo "转换用时 $((END-START)) 秒"

echo
echo "=== 扩容到 ${TARGET_GB} GB ==="
qemu-img resize "$DST" "${TARGET_GB}G"

echo
echo "=== 结果 ==="
qemu-img info "$DST"
echo
ls -la "$DST"
echo
echo "实际占用: $(du -h --apparent-size "$DST" | cut -f1) 表观 / $(du -h "$DST" | cut -f1) 实占"
