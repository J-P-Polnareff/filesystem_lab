#!/bin/bash

echo "=== NaiveFS 完整功能测试 ==="
echo "当前内核版本: $(uname -r)"
echo "工作目录: $(pwd)"

# 1. 编译模块
echo "1. 编译内核模块..."
make clean
if make; then
    echo "✅ 编译成功"
else
    echo "❌ 编译失败"
    exit 1
fi

# 2. 检查模块
echo -e "\n2. 检查模块文件..."
ls -lh naivefs.ko

# 3. 加载模块
echo -e "\n3. 加载内核模块..."
sudo rmmod naivefs 2>/dev/null
if sudo insmod naivefs.ko; then
    echo "✅ 模块加载成功"
else
    echo "❌ 模块加载失败"
    exit 1
fi

# 4. 检查模块状态
echo -e "\n4. 检查模块状态..."
if lsmod | grep -q naive; then
    echo "✅ 模块已加载"
else
    echo "❌ 模块未加载"
    exit 1
fi

# 5. 查看内核日志
echo -e "\n5. 内核日志..."
sudo dmesg | grep naivefs | tail -5

# 6. 挂载文件系统
echo -e "\n6. 挂载文件系统..."
sudo umount /mnt/naive 2>/dev/null
if sudo mount -t naive -o loop ../tmpfile /mnt/naive; then
    echo "✅ 挂载成功"
    echo "挂载信息:"
    mount | grep naive
else
    echo "❌ 挂载失败"
    exit 1
fi

# 7. 测试目录功能
echo -e "\n7. 测试目录创建..."
mkdir /mnt/naive/testdir
if [ -d "/mnt/naive/testdir" ]; then
    echo "✅ 目录创建成功"
else
    echo "❌ 目录创建失败"
fi

# 8. 测试文件功能
echo -e "\n8. 测试文件创建..."
touch /mnt/naive/testdir/file1.txt
echo "Hello World" > /mnt/naive/testdir/file2.txt
if [ -f "/mnt/naive/testdir/file2.txt" ]; then
    echo "✅ 文件创建成功"
    echo "文件内容: $(cat /mnt/naive/testdir/file2.txt)"
else
    echo "❌ 文件创建失败"
fi

# 9. 测试目录非空检查
echo -e "\n9. 测试删除非空目录..."
rmdir /mnt/naive/testdir 2>&1 | grep -q "Directory not empty"
if [ $? -eq 0 ]; then
    echo "✅ 正确拒绝删除非空目录"
else
    echo "❌ 应该拒绝删除非空目录"
fi

# 10. 测试删除文件
echo -e "\n10. 测试文件删除..."
rm /mnt/naive/testdir/file1.txt /mnt/naive/testdir/file2.txt
if [ ! -f "/mnt/naive/testdir/file1.txt" ]; then
    echo "✅ 文件删除成功"
else
    echo "❌ 文件删除失败"
fi

# 11. 测试目录删除
echo -e "\n11. 测试目录删除..."
rmdir /mnt/naive/testdir
if [ ! -d "/mnt/naive/testdir" ]; then
    echo "✅ 目录删除成功"
else
    echo "❌ 目录删除失败"
fi

# 12. 测试持久化
echo -e "\n12. 测试持久化..."
mkdir /mnt/naive/persist_dir
echo "Persistent data" > /mnt/naive/persist_file.txt
sudo umount /mnt/naive
sudo mount -t naive -o loop ../tmpfile /mnt/naive
if [ -d "/mnt/naive/persist_dir" ] && [ -f "/mnt/naive/persist_file.txt" ]; then
    echo "✅ 持久化测试通过"
    echo "持久化文件内容: $(cat /mnt/naive/persist_file.txt)"
else
    echo "❌ 持久化测试失败"
fi

# 13. 清理测试
echo -e "\n13. 清理测试文件..."
rm -rf /mnt/naive/persist_dir
rm -f /mnt/naive/persist_file.txt

# 14. 最终检查
echo -e "\n14. 最终目录结构..."
ls -la /mnt/naive/

# 15. 查看内核操作日志
echo -e "\n15. 内核操作日志..."
sudo dmesg | grep -E "(mkdir|rmdir|create|unlink)" | grep naivefs

# 16. 卸载和清理
echo -e "\n16. 清理..."
sudo umount /mnt/naive
sudo rmmod naivefs
echo "✅ 清理完成"

echo -e "\n=== 所有测试完成 ==="
