#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

#define MYEXT2_SUPER_MAGIC 0x6666
#define EXT2_SUPER_MAGIC   0xEF53

int main(int argc, char *argv[]) {
    int fd;
    unsigned short magic;
    
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <device_or_file>\n", argv[0]);
        fprintf(stderr, "Example: %s myext2.img\n", argv[0]);
        return 1;
    }
    
    // 打开设备或文件
    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    
    // EXT2超级块在1024字节偏移处
    if (lseek(fd, 1024, SEEK_SET) < 0) {
        perror("lseek to superblock");
        close(fd);
        return 1;
    }
    
    // 读取魔数
    if (read(fd, &magic, 2) != 2) {
        perror("read magic");
        close(fd);
        return 1;
    }
    
    printf("Current magic: 0x%04x\n", magic);
    
    // 如果是EXT2魔数，改为MYEXT2魔数
    if (magic == EXT2_SUPER_MAGIC) {
        magic = MYEXT2_SUPER_MAGIC;
        lseek(fd, 1024, SEEK_SET);
        if (write(fd, &magic, 2) != 2) {
            perror("write magic");
            close(fd);
            return 1;
        }
        printf("Changed magic to: 0x%04x (MYEXT2)\n", magic);
    } else if (magic == MYEXT2_SUPER_MAGIC) {
        printf("Already MYEXT2 magic\n");
    } else {
        printf("Unknown filesystem magic: 0x%04x\n", magic);
    }
    
    close(fd);
    return 0;
}
