#!/bin/bash
# 验证MYEXT2实现

echo "=== 验证MYEXT2实现 ==="

echo "1. 检查魔数定义..."
grep -n "0x6666" include/linux/myext2_fs.h
if [ $? -eq 0 ]; then
    echo "✓ 魔数定义正确"
else
    echo "✗ 魔数定义错误"
fi

echo ""
echo "2. 检查模块注册..."
grep -n "register_filesystem" fs/myext2/super.c
if [ $? -eq 0 ]; then
    echo "✓ 模块注册代码存在"
else
    echo "✗ 模块注册代码缺失"
fi

echo ""
echo "3. 检查文件系统类型..."
grep -n "myext2_fs_type" fs/myext2/super.c
if [ $? -eq 0 ]; then
    echo "✓ 文件系统类型定义存在"
else
    echo "✗ 文件系统类型定义缺失"
fi

echo ""
echo "4. 检查编译配置..."
if [ -f fs/myext2/Makefile ]; then
    echo "✓ Makefile存在"
    grep "myext2-objs" fs/myext2/Makefile
else
    echo "✗ Makefile不存在"
fi

echo ""
echo "5. 检查工具..."
if [ -f changeMN ]; then
    echo "✓ changeMN工具存在"
    ./changeMN --help 2>&1 | head -5
else
    echo "✗ changeMN工具不存在"
fi

echo ""
echo "验证完成！"
echo "请运行 ./build.sh 进行编译测试"
