#!/bin/bash
echo "=== MYEXT2调试模式 ==="

# 1. 清理内核日志
sudo dmesg -c > /dev/null

# 2. 重新加载模块
cd fs/myext2
sudo rmmod myext2 2>/dev/null
sudo insmod myext2.ko
cd ../..

# 3. 查看详细内核日志
echo "模块加载日志:"
sudo dmesg | grep -i myext2

# 4. 创建测试镜像
echo "创建测试镜像..."
dd if=/dev/zero of=debug.img bs=1M count=5 status=none
mkfs.ext2 -q -F debug.img

# 5. 修改魔数
echo "修改魔数为MYEXT2..."
# 方法1：使用dd
echo -ne '\x66\x66' | sudo dd of=debug.img bs=1 seek=1080 conv=notrunc status=none

# 方法2：使用perl（备用）
perl -e 'open(F, "+<", "debug.img"); seek(F, 1080, 0); print F "\x66\x66"; close F;'

# 6. 验证魔数
echo "验证魔数:"
hexdump -C -s 1024 -n 128 debug.img | head -5

# 7. 尝试挂载并查看详细错误
echo "尝试挂载..."
sudo mkdir -p /mnt/debug
sudo umount /mnt/debug 2>/dev/null || true

sudo mount -t myext2 -o loop debug.img /mnt/debug -v

echo "挂载结果: $?"
if mount | grep -q "/mnt/debug"; then
    echo "挂载成功！"
    ls -la /mnt/debug/
    sudo umount /mnt/debug
else
    echo "挂载失败"
    echo "详细错误信息:"
    sudo dmesg | tail -20
fi

# 8. 检查超级块内容
echo "超级块内容:"
hexdump -C -s 1024 -n 256 debug.img

# 9. 检查模块信息
echo "模块信息:"
modinfo fs/myext2/myext2.ko
