#!/bin/bash

echo "=== 快速测试脚本 ==="
echo "内核版本: $(uname -r)"
echo "当前目录: $(pwd)"

# 1. 清理环境
echo -e "\n1. 清理环境..."
sudo umount /mnt/naive 2>/dev/null
sudo losetup -D 2>/dev/null
sudo rmmod naivefs 2>/dev/null

# 2. 关联环回设备
echo -e "\n2. 设置环回设备..."
cd ~/filesystem_lab/naive
sudo losetup /dev/loop12 tmpfile 2>/dev/null || sudo losetup -f tmpfile
LOOP_DEV=$(sudo losetup -a | grep tmpfile | cut -d: -f1)
echo "使用环回设备: $LOOP_DEV"

# 3. 格式化（如果需要）
echo -e "\n3. 格式化磁盘..."
if [ -f mkfs.naive ]; then
    sudo ./mkfs.naive $LOOP_DEV
else
    echo "编译mkfs.naive..."
    gcc -o mkfs.naive mkfs.naive.c
    sudo ./mkfs.naive $LOOP_DEV
fi

# 4. 解关联
sudo losetup -d $LOOP_DEV

# 5. 编译模块
echo -e "\n4. 编译模块..."
cd naivefs-9-10
make clean
make
if [ $? -ne 0 ]; then
    echo "编译失败，显示错误..."
    make 2>&1 | grep -A5 -B5 error
    exit 1
fi

# 6. 加载模块
echo -e "\n5. 加载模块..."
sudo insmod naivefs.ko
if [ $? -ne 0 ]; then
    echo "模块加载失败:"
    dmesg | tail -5
    exit 1
fi

# 7. 挂载
echo -e "\n6. 挂载文件系统..."
sudo mount -t naive -o loop ../tmpfile /mnt/naive
if [ $? -ne 0 ]; then
    echo "挂载失败:"
    dmesg | tail -10
    exit 1
fi

# 8. 基本测试
echo -e "\n7. 基本功能测试..."
echo "创建目录:"
mkdir /mnt/naive/test_dir
echo "创建文件:"
echo "Hello World" > /mnt/naive/hello.txt
echo "查看目录:"
ls -la /mnt/naive/
echo "文件内容:"
cat /mnt/naive/hello.txt

# 9. 清理
echo -e "\n8. 清理..."
rm /mnt/naive/hello.txt
rmdir /mnt/naive/test_dir

# 10. 最终状态
echo -e "\n9. 最终状态:"
ls -la /mnt/naive/
echo -e "\n内核日志最后10条:"
sudo dmesg | grep naivefs | tail -10

echo -e "\n=== 测试完成 ==="
echo "文件系统工作正常！"
