#!/bin/bash

echo "========================================="
echo "    NaiveFS 第8步：文件删除测试"
echo "========================================="

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_success() {
    echo -e "${GREEN}[✓] $1${NC}"
}

print_error() {
    echo -e "${RED}[✗] $1${NC}"
}

print_info() {
    echo -e "${YELLOW}[i] $1${NC}"
}

# 检查是否在正确目录
if [ ! -f "naivefs.c" ]; then
    print_error "请在 naivefs-8 目录中运行此脚本"
    exit 1
fi

# 步骤1：编译模块
print_info "步骤1: 编译内核模块..."
make clean > /dev/null 2>&1
make > /dev/null 2>&1
if [ $? -eq 0 ]; then
    print_success "编译成功"
else
    print_error "编译失败"
    exit 1
fi

# 步骤2：重新格式化磁盘
print_info "步骤2: 重新格式化磁盘..."
cd ..
./mkfs.naive tmpfile 2>&1 | grep -v "already" || true
cd naivefs-8

# 步骤3：加载模块
print_info "步骤3: 加载内核模块..."
sudo rmmod naivefs 2>/dev/null
sudo insmod naivefs.ko 2>&1
if [ $? -eq 0 ]; then
    print_success "模块加载成功"
else
    print_error "模块加载失败"
    exit 1
fi

# 步骤4：挂载文件系统
print_info "步骤4: 挂载文件系统..."
sudo umount /mnt/naive 2>/dev/null
sudo mkdir -p /mnt/naive
sudo mount -t naive -o loop ../tmpfile /mnt/naive 2>&1
if [ $? -eq 0 ]; then
    print_success "挂载成功"
else
    print_error "挂载失败"
    exit 1
fi

echo ""
echo "========================================="
echo "           开始删除功能测试"
echo "========================================="

# 测试1：创建测试文件
print_info "测试1: 创建测试文件"
echo "Test file content" > /mnt/naive/test1.txt
echo "Another test file" > /mnt/naive/test2.txt
echo "Third file" > /mnt/naive/test3.txt
echo "目录内容："
ls -la /mnt/naive/

# 测试2：删除单个文件
print_info "测试2: 删除单个文件"
rm /mnt/naive/test1.txt
if [ ! -f /mnt/naive/test1.txt ]; then
    print_success "文件删除成功"
else
    print_error "文件删除失败"
fi
echo "删除后目录内容："
ls -la /mnt/naive/

# 测试3：删除多个文件
print_info "测试3: 删除多个文件"
rm /mnt/naive/test2.txt /mnt/naive/test3.txt
FILE_COUNT=$(ls /mnt/naive/*.txt 2>/dev/null | wc -l)
if [ "$FILE_COUNT" -eq 0 ]; then
    print_success "多个文件删除成功"
else
    print_error "多个文件删除失败"
fi
echo "目录内容："
ls -la /mnt/naive/

# 测试4：创建并删除大文件
print_info "测试4: 创建并删除大文件"
dd if=/dev/urandom of=/mnt/naive/bigfile.bin bs=1024 count=2 2>/dev/null
echo "创建大文件大小："
ls -lh /mnt/naive/bigfile.bin
rm /mnt/naive/bigfile.bin
if [ ! -f /mnt/naive/bigfile.bin ]; then
    print_success "大文件删除成功"
else
    print_error "大文件删除失败"
fi

# 测试5：创建目录并删除
print_info "测试5: 创建并删除目录"
mkdir /mnt/naive/testdir
echo "file in dir" > /mnt/naive/testdir/file.txt
echo "创建目录结构："
ls -la /mnt/naive/
ls -la /mnt/naive/testdir/
rmdir /mnt/naive/testdir 2>&1
if [ ! -d /mnt/naive/testdir ]; then
    print_success "目录删除成功"
else
    print_error "目录删除失败（可能非空）"
    # 如果目录非空，先删除文件再删除目录
    rm -f /mnt/naive/testdir/file.txt
    rmdir /mnt/naive/testdir
fi

# 测试6：持久化测试
print_info "测试6: 持久化测试"
echo "创建新文件..."
echo "Persistent test" > /mnt/naive/persist.txt
echo "文件内容："
cat /mnt/naive/persist.txt

echo "卸载文件系统..."
sudo umount /mnt/naive

echo "重新挂载..."
sudo mount -t naive -o loop ../tmpfile /mnt/naive

echo "验证文件存在："
if [ -f /mnt/naive/persist.txt ]; then
    print_success "文件持久化成功"
    cat /mnt/naive/persist.txt
else
    print_error "文件持久化失败"
fi

# 测试7：删除持久化文件
print_info "测试7: 删除持久化文件"
rm /mnt/naive/persist.txt
sudo umount /mnt/naive
sudo mount -t naive -o loop ../tmpfile /mnt/naive
if [ ! -f /mnt/naive/persist.txt ]; then
    print_success "删除操作持久化成功"
else
    print_error "删除操作持久化失败"
fi

echo ""
echo "========================================="
echo "           资源释放测试"
echo "========================================="

# 创建多个文件占满空间
print_info "创建多个文件测试资源释放..."
for i in {1..10}; do
    echo "File $i" > /mnt/naive/file$i.txt
done
echo "创建10个文件后的状态："
ls -la /mnt/naive/*.txt 2>/dev/null | wc -l

# 删除一半文件
print_info "删除一半文件..."
for i in {1..5}; do
    rm /mnt/naive/file$i.txt
done
echo "删除后剩余文件数："
ls -la /mnt/naive/*.txt 2>/dev/null | wc -l

# 再创建新文件（应该使用释放的空间）
print_info "再创建新文件（使用释放的空间）..."
echo "New file after deletion" > /mnt/naive/newfile.txt
if [ -f /mnt/naive/newfile.txt ]; then
    print_success "资源释放后可以创建新文件"
else
    print_error "资源释放后无法创建新文件"
fi

echo ""
echo "========================================="
echo "           内核日志摘要"
echo "========================================="
sudo dmesg | grep naivefs | tail -20

echo ""
echo "========================================="
echo "           清理"
echo "========================================="
sudo umount /mnt/naive 2>/dev/null
sudo rmmod naivefs 2>/dev/null

echo ""
echo "测试完成！如果需要重新测试，请运行此脚本。"
