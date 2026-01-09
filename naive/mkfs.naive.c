#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#define NAIVE_MAGIC 0x990717
#define NAIVE_BLOCK_SIZE 512
#define NAIVE_INODE_SIZE 512
#define NAIVE_ROOT_INODE_NO 1

/* 磁盘数据结构 - 与内核一致 */
struct naive_super_block {
    unsigned int magic;
    unsigned int inode_total;
    unsigned int block_total;
    unsigned int inode_table_block_no;
    unsigned int data_block_no;
    unsigned char padding[492];
};

struct naive_inode {
    unsigned int mode;
    unsigned int i_ino;
    unsigned int block_count;
    unsigned int block[8];
    union {
        unsigned int file_size;
        unsigned int dir_children_count;
    };
    unsigned int i_uid;
    unsigned int i_gid;
    unsigned int i_nlink;
    unsigned int i_atime;
    unsigned int i_ctime;
    unsigned int i_mtime;
    unsigned char padding[352];
};

struct naive_dir_record {
    unsigned int i_ino;
    char filename[128];
};

void format_disk(int fd, const char *path)
{
    struct stat stat_;
    struct naive_super_block nsb;
    unsigned char *bmap, *imap;
    struct naive_inode root_inode;
    struct naive_dir_record dir_dot, dir_dotdot;
    int disk_size, inode_table_size;
    
    stat(path, &stat_);
    disk_size = stat_.st_size;
    
    printf("Formatting disk: %s (size: %d bytes)\n", path, disk_size);
    
    // 初始化超级块
    memset(&nsb, 0, sizeof(nsb));
    nsb.magic = NAIVE_MAGIC;
    nsb.block_total = disk_size / NAIVE_BLOCK_SIZE;
    nsb.inode_total = 128;
    
    // 计算布局
    inode_table_size = (nsb.inode_total * NAIVE_INODE_SIZE + NAIVE_BLOCK_SIZE - 1) / NAIVE_BLOCK_SIZE;
    nsb.inode_table_block_no = 4;  // 块0:引导, 块1:超级块, 块2:数据位图, 块3:inode位图
    nsb.data_block_no = nsb.inode_table_block_no + inode_table_size;
    
    printf("  Block total: %d\n", nsb.block_total);
    printf("  Inode total: %d\n", nsb.inode_total);
    printf("  Inode table starts at block: %d\n", nsb.inode_table_block_no);
    printf("  Data blocks start at block: %d\n", nsb.data_block_no);
    
    // 分配位图
    bmap = (unsigned char*)calloc(NAIVE_BLOCK_SIZE, 1);
    imap = (unsigned char*)calloc(NAIVE_BLOCK_SIZE, 1);
    
    // 标记前几个块为已使用（引导块、超级块、位图）
    bmap[0] = 0x1F;  // 前5位为1（块0-4）
    
    // 标记inode位图：根inode已使用
    imap[0] = 0x01;  // 第一位为1（inode 1）
    
    // 写入引导块（全零）
    unsigned char zero_block[NAIVE_BLOCK_SIZE] = {0};
    write(fd, zero_block, NAIVE_BLOCK_SIZE);
    
    // 写入超级块
    write(fd, &nsb, sizeof(nsb));
    lseek(fd, NAIVE_BLOCK_SIZE, SEEK_SET);  // 填充到块边界
    
    // 写入数据块位图
    write(fd, bmap, NAIVE_BLOCK_SIZE);
    
    // 写入inode位图
    write(fd, imap, NAIVE_BLOCK_SIZE);
    
    // 创建根目录inode
    memset(&root_inode, 0, sizeof(root_inode));
    root_inode.mode = 040755;  // S_IFDIR | 0755
    root_inode.i_ino = NAIVE_ROOT_INODE_NO;
    root_inode.block_count = 1;
    root_inode.block[0] = nsb.data_block_no;
    root_inode.dir_children_count = 2;  // . 和 ..
    root_inode.i_uid = getuid();
    root_inode.i_gid = getgid();
    root_inode.i_nlink = 2;
    root_inode.i_atime = root_inode.i_mtime = root_inode.i_ctime = time(NULL);
    
    // 写入根inode（在inode表的第一个位置）
    lseek(fd, nsb.inode_table_block_no * NAIVE_BLOCK_SIZE, SEEK_SET);
    write(fd, &root_inode, sizeof(root_inode));
    
    // 创建.和..目录项
    memset(&dir_dot, 0, sizeof(dir_dot));
    dir_dot.i_ino = NAIVE_ROOT_INODE_NO;
    strcpy(dir_dot.filename, ".");
    
    memset(&dir_dotdot, 0, sizeof(dir_dotdot));
    dir_dotdot.i_ino = NAIVE_ROOT_INODE_NO;
    strcpy(dir_dotdot.filename, "..");
    
    // 写入根目录的数据块
    lseek(fd, nsb.data_block_no * NAIVE_BLOCK_SIZE, SEEK_SET);
    write(fd, &dir_dot, sizeof(dir_dot));
    write(fd, &dir_dotdot, sizeof(dir_dotdot));
    
    // 标记数据块为已使用
    bmap[nsb.data_block_no / 8] |= (1 << (nsb.data_block_no % 8));
    
    // 更新位图
    lseek(fd, 2 * NAIVE_BLOCK_SIZE, SEEK_SET);
    write(fd, bmap, NAIVE_BLOCK_SIZE);
    
    free(bmap);
    free(imap);
    
    printf("Format completed successfully!\n");
}

int main(int argc, char *argv[])
{
    int fd;
    
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <device>\n", argv[0]);
        exit(1);
    }
    
    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        perror("open device failed");
        exit(1);
    }
    
    format_disk(fd, argv[1]);
    close(fd);
    
    return 0;
}
