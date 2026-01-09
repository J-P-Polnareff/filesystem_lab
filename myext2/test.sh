#!/bin/bash
# MYEXT2测试脚本

IMAGE="myext2_test.img"
MOUNT_POINT="/mnt/myext2_test"

echo "=== MYEXT2完整测试 ==="

# 1. 清理旧文件
echo "1. 清理旧文件..."
sudo umount "$MOUNT_POINT" 2>/dev/null || true
rm -f "$IMAGE"
sudo rmmod myext2 2>/dev/null || true

# 2. 编译
echo "2. 编译模块和工具..."
./build.sh

# 3. 创建MYEXT2文件系统
echo "3. 创建MYEXT2文件系统..."
./mkfs.myext2.sh "$IMAGE"

# 4. 加载模块
echo "4. 加载MYEXT2模块..."
sudo insmod fs/myext2/myext2.ko

# 检查模块是否加载
if lsmod | grep -q myext2; then
    echo "✓ 模块加载成功"
else
    echo "✗ 模块加载失败"
    exit 1
fi

# 5. 创建挂载点
echo "5. 准备挂载点..."
sudo mkdir -p "$MOUNT_POINT"

# 6. 挂载
echo "6. 挂载MYEXT2文件系统..."
sudo mount -t myext2 -o loop "$IMAGE" "$MOUNT_POINT"

if mount | grep -q "$MOUNT_POINT"; then
    echo "✓ 挂载成功"
    
    # 7. 测试基本操作
    echo "7. 测试基本操作..."
    sudo touch "$MOUNT_POINT/test_file.txt"
    sudo mkdir "$MOUNT_POINT/test_dir"
    sudo echo "Hello MYEXT2" > "$MOUNT_POINT/test_file.txt"
    
    echo "文件列表:"
    ls -la "$MOUNT_POINT/"
    
    echo "文件内容:"
    cat "$MOUNT_POINT/test_file.txt"
    
    # 8. 卸载
    echo "8. 卸载..."
    sudo umount "$MOUNT_POINT"
    echo "✓ 卸载成功"
else
    echo "✗ 挂载失败"
fi

# 9. 查看内核日志
echo ""
echo "9. 内核日志:"
sudo dmesg | tail -20 | grep -i myext2

# 10. 清理
echo ""
echo "10. 清理..."
sudo rmmod myext2 2>/dev/null || true
echo "测试完成！"
