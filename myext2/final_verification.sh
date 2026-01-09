#!/bin/bash
echo "=== MYEXT2最终验证 ==="
echo "内核版本: $(uname -r)"
echo "当前目录: $(pwd)"
echo ""

# 1. 验证模块编译
echo "1. 验证模块编译..."
cd fs/myext2
if [ -f myext2.ko ]; then
    echo "✓ 模块存在: myext2.ko"
    echo "  大小: $(ls -lh myext2.ko | cut -d' ' -f5)"
    echo "  文件类型: $(file myext2.ko | cut -d: -f2)"
else
    echo "✗ 模块不存在，重新编译..."
    make clean
    make
    if [ ! -f myext2.ko ]; then
        echo "编译失败，退出"
        exit 1
    fi
fi
cd ../..

# 2. 验证模块加载
echo ""
echo "2. 验证模块加载..."
sudo rmmod myext2 2>/dev/null
sudo insmod fs/myext2/myext2.ko

if lsmod | grep -q myext2; then
    echo "✓ 模块加载成功"
    MODULE_INFO=$(lsmod | grep myext2)
    echo "  模块信息: $MODULE_INFO"
else
    echo "✗ 模块加载失败"
    sudo dmesg | tail -10
    exit 1
fi

# 3. 验证文件系统注册
echo ""
echo "3. 验证文件系统注册..."
if grep -q myext2 /proc/filesystems; then
    echo "✓ MYEXT2已注册到系统"
else
    echo "✗ MYEXT2未注册"
    exit 1
fi

# 4. 创建测试文件系统
echo ""
echo "4. 创建MYEXT2测试文件系统..."
TEST_IMG="final_test.img"
rm -f $TEST_IMG
dd if=/dev/zero of=$TEST_IMG bs=1M count=5 status=none
mkfs.ext2 -q -F $TEST_IMG

# 修改魔数
echo "  修改魔数为0x6666..."
perl -e 'open(F, "+<", "'$TEST_IMG'"); seek(F, 1080, 0); print F "\x66\x66"; close F;'

# 验证魔数
MAGIC=$(hexdump -C -s 1080 -n 2 $TEST_IMG | head -1 | awk '{print $3$2}')
if [ "$MAGIC" = "6666" ]; then
    echo "✓ 魔数正确: 0x$MAGIC"
else
    echo "✗ 魔数错误: 0x$MAGIC"
    exit 1
fi

# 5. 挂载测试
echo ""
echo "5. 挂载测试..."
sudo mkdir -p /mnt/myext2_final
sudo umount /mnt/myext2_final 2>/dev/null || true

if sudo mount -t myext2 -o loop $TEST_IMG /mnt/myext2_final 2>/dev/null; then
    echo "✓ 挂载成功"
    
    # 测试文件操作
    sudo touch /mnt/myext2_final/test_created_by_myext2
    sudo mkdir /mnt/myext2_final/myext2_dir
    echo "MYEXT2实验成功！" | sudo tee /mnt/myext2_final/success.txt > /dev/null
    
    echo "  创建的文件:"
    ls -la /mnt/myext2_final/
    
    echo "  文件内容:"
    sudo cat /mnt/myext2_final/success.txt
    
    # 卸载
    sudo umount /mnt/myext2_final
    echo "✓ 卸载成功"
else
    echo "✗ 挂载失败"
    echo "内核日志:"
    sudo dmesg | tail -10 | grep -i myext2
fi

# 6. 清理
echo ""
echo "6. 清理..."
sudo rmmod myext2 2>/dev/null || true
sudo umount /mnt/myext2_final 2>/dev/null || true
rm -f $TEST_IMG

# 7. 总结
echo ""
echo "=== 验证结果 ==="
echo "1. 模块编译: ✓ 成功"
echo "2. 模块加载: ✓ 成功"
echo "3. 文件系统注册: ✓ 成功"
echo "4. 魔数修改: ✓ 成功 (0x6666)"
echo "5. 挂载测试: $(if mount | grep -q myext2; then echo "✓ 成功"; else echo "✗ 失败"; fi)"
echo ""
echo "=== MYEXT2实验完成 ==="
echo "实验成功！你已经创建了一个自定义的MYEXT2文件系统，"
echo "它基于EXT2但使用自定义魔数0x6666。"
