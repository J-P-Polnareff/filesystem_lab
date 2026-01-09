#!/bin/bash
# mkfs.myext2 - 创建MYEXT2文件系统
# 用法: ./mkfs.myext2.sh <设备或文件>

set -e

if [ $# -ne 1 ]; then
    echo "用法: $0 <设备或文件>"
    echo "示例: $0 myext2.img"
    exit 1
fi

DEVICE="$1"
SIZE_MB=10

# 检查设备是否存在，如果不存在则创建
if [ ! -e "$DEVICE" ]; then
    echo "创建 $DEVICE (${SIZE_MB}MB)..."
    dd if=/dev/zero of="$DEVICE" bs=1M count=$SIZE_MB status=progress
fi

echo "第一步：格式化为EXT2..."
mkfs.ext2 -q -F "$DEVICE"

echo "第二步：修改魔数为MYEXT2 (0x6666)..."
./changeMN "$DEVICE"

echo "第三步：验证魔数..."
MAGIC=$(hexdump -C -s 1024 -n 2 "$DEVICE" | head -1 | awk '{print $3$2}')
if [ "$MAGIC" = "6666" ]; then
    echo "✓ 成功创建MYEXT2文件系统 (magic=0x$MAGIC)"
else
    echo "✗ 魔数验证失败: 0x$MAGIC"
    exit 1
fi

echo "完成！现在可以加载MYEXT2模块并挂载了。"
echo "加载模块: sudo insmod fs/myext2/myext2.ko"
echo "挂载: sudo mount -t myext2 -o loop $DEVICE /mnt/myext2"
