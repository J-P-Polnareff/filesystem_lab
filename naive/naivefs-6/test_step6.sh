#!/bin/bash
echo "=== NaiveFS 第6步测试 ==="

# 设置颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_success() { echo -e "${GREEN}[✓] $1${NC}"; }
print_error() { echo -e "${RED}[✗] $1${NC}"; }
print_info() { echo -e "${YELLOW}[i] $1${NC}"; }

# 检查root权限
if [ "$EUID" -ne 0 ]; then
    print_info "需要使用sudo运行"
    exec sudo "$0" "$@"
fi

cd ~/filesystem_lab/naive/naivefs-6

echo ""
print_info "1. 编译内核模块..."
make clean 2>/dev/null
make 2>&1
if [ $? -eq 0 ]; then
    print_success "编译成功"
    ls -la *.ko
else
    print_error "编译失败"
    exit 1
fi

echo ""
print_info "2. 卸载旧模块..."
rmmod naivefs 2>/dev/null && print_success "已卸载" || print_info "没有旧模块"

echo ""
print_info "3. 加载模块..."
insmod naivefs.ko
if [ $? -eq 0 ]; then
    print_success "模块加载成功"
else
    print_error "模块加载失败"
    exit 1
fi

echo ""
print_info "4. 检查模块..."
if lsmod | grep -q naive; then
    print_success "模块已加载:"
    lsmod | grep naive
else
    print_error "模块未加载"
    exit 1
fi

echo ""
print_info "5. 准备挂载点..."
mkdir -p /mnt/naive
umount /mnt/naive 2>/dev/null && print_success "已卸载旧挂载" || print_info "没有旧挂载"

echo ""
print_info "6. 挂载文件系统..."
mount -t naive -o loop ../tmpfile /mnt/naive
if [ $? -eq 0 ]; then
    print_success "挂载成功"
else
    print_error "挂载失败"
    exit 1
fi

echo ""
print_info "7. 显示挂载状态..."
mount | grep naive

echo ""
print_info "8. 显示根目录..."
ls -la /mnt/naive

echo ""
print_info "9. 测试创建空文件..."
touch /mnt/naive/testfile1
touch /mnt/naive/testfile2
echo "创建了 testfile1 和 testfile2"
ls -la /mnt/naive

echo ""
print_info "10. 使用cat创建文件..."
echo "Hello from echo" > /mnt/naive/hello.txt
cat /mnt/naive/hello.txt

echo ""
print_info "11. 查看内核日志..."
dmesg | grep naivefs | tail -10

echo ""
print_info "12. 卸载并重新挂载..."
umount /mnt/naive
mount -t naive -o loop ../tmpfile /mnt/naive

echo ""
print_info "13. 检查文件是否持久化..."
echo "重新挂载后的目录内容："
ls -la /mnt/naive

echo ""
print_info "14. 清理..."
rm -f /mnt/naive/testfile1 /mnt/naive/testfile2 /mnt/naive/hello.txt
umount /mnt/naive
rmmod naivefs

echo ""
print_info "15. 最终检查..."
if ! lsmod | grep -q naive && ! mount | grep -q naive; then
    print_success "清理完成"
else
    print_error "清理不完全"
fi

echo ""
echo "=== 第6步测试完成 ==="
