#!/bin/bash

echo "=== NaiveFS 实验环境一键安装 ==="

# 1. 检查内核版本
echo "1. 检查内核版本..."
echo "当前内核: $(uname -r)"
echo "内核头文件:"
ls /lib/modules/$(uname -r)/build 2>/dev/null || echo "❌ 内核头文件未安装"

# 2. 创建目录结构
echo -e "\n2. 创建目录结构..."
mkdir -p ~/filesystem_lab/naive/naivefs-9-10
mkdir -p /mnt/naive

# 3. 创建虚拟磁盘
echo -e "\n3. 创建虚拟磁盘..."
cd ~/filesystem_lab/naive
if [ ! -f tmpfile ]; then
    dd if=/dev/zero of=tmpfile bs=1K count=200
    echo "✅ 创建200KB虚拟磁盘"
else
    echo "⚠️  虚拟磁盘已存在，跳过创建"
fi

# 4. 关联环回设备
echo -e "\n4. 关联环回设备..."
sudo losetup -d /dev/loop12 2>/dev/null
sudo losetup /dev/loop12 tmpfile
if [ $? -eq 0 ]; then
    echo "✅ 环回设备关联成功: /dev/loop12"
else
    echo "❌ 环回设备关联失败"
    exit 1
fi

# 5. 格式化磁盘
echo -e "\n5. 格式化磁盘..."
if [ ! -f mkfs.naive ]; then
    echo "编译mkfs.naive..."
    gcc -o mkfs.naive mkfs.naive.c
fi
sudo ./mkfs.naive /dev/loop12

# 6. 解关联环回设备
sudo losetup -d /dev/loop12

# 7. 准备内核模块
echo -e "\n7. 准备内核模块..."
cd naivefs-9-10

# 8. 检查必要文件
echo -e "\n8. 检查文件..."
FILES=("naivefs.h" "naivefs_main.c" "naivefs_super.c" "naivefs_dir.c" "Makefile")
for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "✅ $file 存在"
    else
        echo "❌ $file 不存在"
    fi
done

echo -e "\n=== 安装完成 ==="
echo "接下来可以运行:"
echo "1. cd ~/filesystem_lab/naive/naivefs-9-10"
echo "2. make"
echo "3. sudo insmod naivefs.ko"
echo "4. sudo mount -t naive -o loop ../tmpfile /mnt/naive"
echo "5. ./test_all.sh  # 运行完整测试"
