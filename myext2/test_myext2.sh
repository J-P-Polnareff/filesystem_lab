#!/bin/bash
echo "=== MYEXT2文件系统测试 ==="

# 1. 清理旧文件
echo "1. 清理环境..."
sudo umount /mnt/myext2 2>/dev/null || true
rm -f myext2_test.img

# 2. 创建10MB测试镜像
echo "2. 创建10MB测试镜像..."
dd if=/dev/zero of=myext2_test.img bs=1M count=10 status=none

# 3. 格式化为EXT2
echo "3. 格式化为EXT2..."
mkfs.ext2 -q -F myext2_test.img

# 4. 查看EXT2魔数
echo "4. 查看原始EXT2魔数..."
echo "EXT2魔数应该在1080字节处（1024+56）:"
hexdump -C -s 1080 -n 2 myext2_test.img

# 5. 修改为MYEXT2魔数
echo "5. 修改为MYEXT2魔数（0x6666）..."
# 使用perl确保正确修改
perl -e 'open(F, "+<", "myext2_test.img"); seek(F, 1080, 0); print F "\x66\x66"; close F;'

# 6. 验证修改
echo "6. 验证魔数修改..."
echo "修改后魔数:"
hexdump -C -s 1080 -n 2 myext2_test.img
MAGIC=$(hexdump -C -s 1080 -n 2 myext2_test.img | head -1 | awk '{print $3$2}')
if [ "$MAGIC" = "6666" ]; then
    echo "✓ 魔数正确: 0x$MAGIC"
else
    echo "✗ 魔数错误: 0x$MAGIC"
    exit 1
fi

# 7. 加载模块（如果未加载）
echo "7. 加载MYEXT2模块..."
if ! lsmod | grep -q myext2; then
    cd fs/myext2
    sudo insmod myext2.ko
    cd ../..
fi

# 8. 挂载测试
echo "8. 挂载MYEXT2文件系统..."
sudo mkdir -p /mnt/myext2
sudo umount /mnt/myext2 2>/dev/null || true

echo "正在挂载..."
MOUNT_OUTPUT=$(sudo mount -t myext2 -o loop myext2_test.img /mnt/myext2 2>&1)

if mount | grep -q "/mnt/myext2"; then
    echo "✓ 挂载成功！"
    
    # 9. 测试基本操作
    echo "9. 测试基本操作..."
    sudo touch /mnt/myext2/testfile
    sudo mkdir /mnt/myext2/testdir
    echo "Hello MYEXT2 Filesystem!" | sudo tee /mnt/myext2/hello.txt > /dev/null
    
    echo "目录内容:"
    ls -la /mnt/myext2/
    
    echo "文件内容:"
    sudo cat /mnt/myext2/hello.txt
    
    # 10. 卸载
    echo "10. 卸载文件系统..."
    sudo umount /mnt/myext2
    echo "✓ 卸载成功"
    
    # 11. 重新挂载验证持久性
    echo "11. 重新挂载验证数据持久性..."
    sudo mount -t myext2 -o loop myext2_test.img /mnt/myext2
    if [ -f /mnt/myext2/hello.txt ]; then
        echo "✓ 数据持久化成功"
        echo "重新读取文件内容:"
        sudo cat /mnt/myext2/hello.txt
    fi
    sudo umount /mnt/myext2
else
    echo "✗ 挂载失败: $MOUNT_OUTPUT"
    echo "内核日志:"
    sudo dmesg | tail -15 | grep -i myext2
fi

# 12. 查看内核日志
echo ""
echo "=== 内核日志摘要 ==="
sudo dmesg | grep -i myext2 | tail -20

# 13. 清理
echo ""
echo "13. 清理环境..."
sudo umount /mnt/myext2 2>/dev/null || true

echo ""
echo "=== 测试完成 ==="
echo "如果看到'挂载成功'和数据持久化成功，那么MYEXT2实验就成功了！"
