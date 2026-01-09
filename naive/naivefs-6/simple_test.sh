#!/bin/bash
echo "简单测试第6步"
cd ~/filesystem_lab/naive/naivefs-6

echo "1. 编译..."
make clean
make

echo "2. 加载..."
sudo rmmod naivefs 2>/dev/null
sudo insmod naivefs.ko

echo "3. 挂载..."
sudo umount /mnt/naive 2>/dev/null
sudo mount -t naive -o loop ../tmpfile /mnt/naive

echo "4. 测试创建文件..."
touch /mnt/naive/myfile.txt
ls -la /mnt/naive/

echo "5. 使用echo..."
echo "test" > /mnt/naive/test.txt
cat /mnt/naive/test.txt

echo "6. 卸载清理..."
sudo umount /mnt/naive
sudo rmmod naivefs
echo "完成！"
