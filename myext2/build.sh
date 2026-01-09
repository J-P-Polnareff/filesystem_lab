#!/bin/bash
# MYEXT2编译脚本

echo "=== 编译MYEXT2内核模块 ==="

# 检查内核版本
echo "内核版本: $(uname -r)"
echo "内核目录: /lib/modules/$(uname -r)/build"

# 检查必要工具
command -v make >/dev/null 2>&1 || { echo "需要make工具"; exit 1; }
command -v gcc >/dev/null 2>&1 || { echo "需要gcc工具"; exit 1; }

# 编译changeMN
echo "编译changeMN工具..."
gcc -o changeMN changeMN.c
if [ $? -eq 0 ]; then
    echo "✓ changeMN编译成功"
else
    echo "✗ changeMN编译失败"
    exit 1
fi

# 编译内核模块
echo "编译MYEXT2内核模块..."
cd fs/myext2
make clean
make

if [ -f myext2.ko ]; then
    echo "✓ MYEXT2模块编译成功"
    echo "模块大小: $(du -h myext2.ko | cut -f1)"
else
    echo "✗ MYEXT2模块编译失败"
    exit 1
fi

cd ../..

echo ""
echo "编译完成！"
echo "可用命令:"
echo "  加载模块: sudo insmod fs/myext2/myext2.ko"
echo "  查看模块: lsmod | grep myext2"
echo "  卸载模块: sudo rmmod myext2"
