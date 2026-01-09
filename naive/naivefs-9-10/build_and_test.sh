#!/bin/bash

echo "开始第7步实验：文件读写功能"
echo "当前目录: $(pwd)"
echo "内核版本: $(uname -r)"

# 检查是否在正确目录
if [ ! -f "naivefs.c" ]; then
    echo "错误：请在 naivefs-7 目录中运行此脚本"
    exit 1
fi

# 编译模块
echo "编译内核模块..."
make clean
make

if [ $? -ne 0 ]; then
    echo "编译失败！"
    exit 1
fi

echo "编译成功！模块文件："
ls -lh *.ko

# 检查模块是否已加载
echo "检查当前加载的模块..."
lsmod | grep naive

echo "准备测试..."
echo "按 Enter 键继续..."
read

# 运行测试
./test_rw.sh
