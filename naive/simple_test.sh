#!/bin/bash

echo "=== NaiveFS 简化测试 ==="
echo "内核版本: $(uname -r)"

# 1. 清理环境
echo -e "\n1. 清理环境..."
sudo umount /mnt/naive 2>/dev/null
sudo losetup -D 2>/dev/null
sudo rmmod naivefs 2>/dev/null

# 2. 创建虚拟磁盘
echo -e "\n2. 创建虚拟磁盘..."
cd ~/filesystem_lab/naive
if [ ! -f tmpfile ]; then
    dd if=/dev/zero of=tmpfile bs=1K count=200
    echo "✅ 创建200KB虚拟磁盘"
fi

# 3. 编译模块
echo -e "\n3. 编译内核模块..."
make clean
make
if [ $? -ne 0 ]; then
    echo "❌ 编译失败"
    exit 1
fi

# 4. 加载模块
echo -e "\n4. 加载内核模块..."
sudo insmod naivefs.ko
if [ $? -ne 0 ]; then
    echo "❌ 模块加载失败"
    sudo dmesg | tail -5
    exit 1
fi

# 5. 创建环回设备
echo -e "\n5. 设置环回设备..."
sudo losetup -f tmpfile --show > /tmp/loopdev.txt
LOOP_DEV=$(cat /tmp/loopdev.txt)
echo "使用环回设备: $LOOP_DEV"

# 6. 格式化
echo -e "\n6. 格式化磁盘..."
if [ -f mkfs.naive ]; then
    sudo ./mkfs.naive $LOOP_DEV
else
    gcc -o mkfs.naive mkfs.naive.c
    sudo ./mkfs.naive $LOOP_DEV
fi

# 7. 解关联
sudo losetup -d $LOOP_DEV

# 8. 挂载
echo -e "\n7. 挂载文件系统..."
sudo mount -t naive -o loop tmpfile /mnt/naive
if [ $? -ne 0 ]; then
    echo "❌ 挂载失败"
    sudo dmesg | tail -10
    exit 1
fi

# 9. 基本测试
echo -e "\n8. 基本功能测试..."
echo "创建目录:"
mkdir /mnt/naive/test_dir
echo "创建文件:"
echo "Hello" > /mnt/naive/hello.txt
echo "查看目录:"
ls -la /mnt/naive/
echo "查看内核日志:"
sudo dmesg | grep naivefs | tail -10

# 10. 清理
echo -e "\n9. 清理..."
sudo umount /mnt/naive
sudo rmmod naivefs
echo "✅ 测试完成"
