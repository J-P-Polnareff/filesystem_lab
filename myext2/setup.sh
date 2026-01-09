#!/bin/bash
# MYEXT2一键安装脚本

echo "=== MYEXT2文件系统实验一键安装 ==="
echo "内核版本: $(uname -r)"
echo "工作目录: $(pwd)"

# 安装依赖
echo "安装依赖..."
sudo apt-get update
sudo apt-get install -y build-essential libncurses-dev flex bison libssl-dev libelf-dev
sudo apt-get install -y linux-headers-$(uname -r)

# 创建必要的挂载点
sudo mkdir -p /mnt/myext2 /mnt/myext2_test

# 设置权限
chmod +x *.sh
chmod +x fs/myext2/*.sh 2>/dev/null || true

echo ""
echo "安装完成！"
echo ""
echo "可用命令:"
echo "  ./build.sh      - 编译MYEXT2模块"
echo "  ./test.sh       - 完整测试"
echo "  ./mkfs.myext2.sh <file> - 创建MYEXT2文件系统"
echo ""
echo "手动测试步骤:"
echo "  1. 创建镜像: dd if=/dev/zero of=test.img bs=1M count=10"
echo "  2. 格式化为EXT2: mkfs.ext2 test.img"
echo "  3. 修改魔数: ./changeMN test.img"
echo "  4. 编译模块: cd fs/myext2 && make"
echo "  5. 加载模块: sudo insmod myext2.ko"
echo "  6. 挂载: sudo mount -t myext2 -o loop test.img /mnt/myext2"
echo "  7. 测试: ls /mnt/myext2"
echo "  8. 卸载: sudo umount /mnt/myext2"
echo "  9. 卸载模块: sudo rmmod myext2"
