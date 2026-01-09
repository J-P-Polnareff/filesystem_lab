#!/bin/bash
echo "快速测试第8步：文件删除"

# 清理环境
sudo umount /mnt/naive 2>/dev/null
sudo rmmod naivefs 2>/dev/null

# 编译
make

# 加载模块
sudo insmod naivefs.ko

# 重新格式化
cd ..
./mkfs.naive tmpfile
cd naivefs-8

# 挂载
sudo mkdir -p /mnt/naive
sudo mount -t naive -o loop ../tmpfile /mnt/naive

# 简单测试
echo "=== 创建文件 ==="
echo "Hello World" > /mnt/naive/test.txt
ls -la /mnt/naive/

echo "=== 删除文件 ==="
rm /mnt/naive/test.txt
ls -la /mnt/naive/

echo "=== 内核日志 ==="
sudo dmesg | grep naivefs | tail -10

# 清理
sudo umount /mnt/naive
sudo rmmod naivefs
