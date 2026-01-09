#!/bin/bash

echo "========== NaiveFS 第8步简单测试 =========="

# 1. 清理环境
echo "1. 清理环境..."
sudo umount /mnt/naive 2>/dev/null
sudo rmmod naivefs 2>/dev/null

# 2. 编译
echo "2. 编译模块..."
make clean
make
if [ $? -ne 0 ]; then
    echo "编译失败！"
    exit 1
fi

# 3. 加载模块
echo "3. 加载模块..."
sudo insmod naivefs.ko
if [ $? -ne 0 ]; then
    echo "模块加载失败！"
    exit 1
fi

echo "模块已加载："
lsmod | grep naive

# 4. 格式化磁盘
echo "4. 格式化磁盘..."
cd ..
./mkfs.naive tmpfile
cd naivefs-8

# 5. 挂载
echo "5. 挂载文件系统..."
sudo mkdir -p /mnt/naive
sudo mount -t naive -o loop ../tmpfile /mnt/naive
if [ $? -ne 0 ]; then
    echo "挂载失败！"
    exit 1
fi

echo "挂载成功："
mount | grep naive

# 6. 测试创建文件
echo "6. 测试创建文件..."
echo "这是一个测试文件" > /mnt/naive/file1.txt
echo "这是另一个测试文件" > /mnt/naive/file2.txt
echo "这是第三个文件" > /mnt/naive/file3.txt

echo "创建文件后的目录内容："
ls -la /mnt/naive/
echo "文件内容："
cat /mnt/naive/file1.txt
cat /mnt/naive/file2.txt

# 7. 测试删除文件
echo "7. 测试删除文件..."
echo "删除 file1.txt..."
rm /mnt/naive/file1.txt
echo "删除后的目录内容："
ls -la /mnt/naive/

echo "删除 file2.txt..."
rm /mnt/naive/file2.txt
echo "删除后的目录内容："
ls -la /mnt/naive/

# 8. 测试删除所有文件
echo "8. 删除所有剩余文件..."
rm /mnt/naive/file3.txt
echo "最终目录内容："
ls -la /mnt/naive/

# 9. 测试重新创建文件
echo "9. 测试资源释放后重新创建..."
echo "新文件内容" > /mnt/naive/newfile.txt
ls -la /mnt/naive/
cat /mnt/naive/newfile.txt

# 10. 查看内核日志
echo "10. 内核日志："
sudo dmesg | grep naivefs | tail -20

# 11. 清理
echo "11. 清理环境..."
sudo umount /mnt/naive
sudo rmmod naivefs

echo "========== 测试完成 =========="
