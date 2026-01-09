#!/bin/bash

echo "=== 第7步：测试文件读写功能 ==="

echo "1. 编译模块..."
make clean
make
if [ $? -ne 0 ]; then
    echo "编译失败!"
    exit 1
fi
echo "编译成功!"

echo "2. 加载模块..."
sudo rmmod naivefs 2>/dev/null
sudo insmod naivefs.ko
if [ $? -ne 0 ]; then
    echo "模块加载失败!"
    exit 1
fi
echo "模块加载成功!"

echo "3. 挂载文件系统..."
sudo mkdir -p /mnt/naive
sudo umount /mnt/naive 2>/dev/null
sudo mount -t naive -o loop ../tmpfile /mnt/naive
if [ $? -ne 0 ]; then
    echo "挂载失败!"
    exit 1
fi
echo "挂载成功!"

echo "4. 查看初始状态..."
ls -la /mnt/naive
df -h /mnt/naive

echo "5. 测试创建文件并写入..."
echo "创建文件 test.txt"
echo "Hello, Naive Filesystem!" > /mnt/naive/test.txt
ls -la /mnt/naive/test.txt

echo "6. 测试读取文件..."
echo "文件内容："
cat /mnt/naive/test.txt

echo "7. 测试追加写入..."
echo "This is a second line." >> /mnt/naive/test.txt
echo "追加后的文件内容："
cat /mnt/naive/test.txt

echo "8. 测试二进制文件写入..."
dd if=/dev/urandom of=/mnt/naive/random.bin bs=256 count=2 2>/dev/null
echo "二进制文件大小："
ls -lh /mnt/naive/random.bin

echo "9. 测试大文件（接近4KB限制）..."
echo "创建大文件..."
for i in {1..100}; do
    echo "Line $i: This is a test line for NaiveFS." >> /mnt/naive/bigfile.txt
done
ls -lh /mnt/naive/bigfile.txt
echo "文件行数："
wc -l /mnt/naive/bigfile.txt

echo "10. 测试持久化..."
echo "卸载文件系统..."
sudo umount /mnt/naive
echo "重新挂载..."
sudo mount -t naive -o loop ../tmpfile /mnt/naive

echo "11. 验证文件是否持久化..."
echo "目录内容："
ls -la /mnt/naive
echo "test.txt 内容："
cat /mnt/naive/test.txt
echo "bigfile.txt 行数："
wc -l /mnt/naive/bigfile.txt

echo "12. 查看内核日志..."
sudo dmesg | grep naivefs | tail -20

echo "=== 测试完成 ==="
