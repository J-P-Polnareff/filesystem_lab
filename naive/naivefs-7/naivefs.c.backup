#include <linux/module.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/namei.h>
#include <linux/uaccess.h>
#include <linux/iversion.h>

#define NAIVE_MAGIC 0x990717
#define NAIVE_BLOCK_SIZE 512
#define NAIVE_MAX_FILESIZE (8 * NAIVE_BLOCK_SIZE)  // 4KB
#define NAIVE_DIRECT_BLOCKS 8
#define NAIVE_MAX_FILENAME_LEN 255
#define NAIVE_ROOT_INO 1
#define NAIVE_SUPERBLOCK_BLOCK 1
#define NAIVE_BLOCK_BITMAP_BLOCK 2
#define NAIVE_INODE_BITMAP_BLOCK 3

// 磁盘上的超级块结构
struct naive_super_block {
    __le32 magic;
    __le32 inode_total;
    __le32 block_total;
    __le32 inode_table_block_no;
    __le32 data_block_no;
    __le32 padding[123];  // 填充到512字节
};

// 磁盘上的inode结构
struct naive_disk_inode {
    __le32 i_mode;           // 文件模式
    __le32 i_ino;            // inode编号
    __le32 i_blocks;         // 使用的块数
    __le32 i_block[NAIVE_DIRECT_BLOCKS];  // 直接块指针
    __le32 i_size;           // 文件大小（字节）
    __le32 i_uid;            // 用户ID
    __le32 i_gid;            // 组ID
    __le32 i_links_count;    // 链接数
    __le32 i_atime;          // 访问时间
    __le32 i_ctime;          // 创建时间
    __le32 i_mtime;          // 修改时间
    __u8 padding[452];       // 填充到512字节
};

// 内存中的超级块信息
struct naive_sb_info {
    struct buffer_head *sb_bh;
    struct buffer_head *block_bitmap_bh;
    struct buffer_head *inode_bitmap_bh;
    __u8 *block_bitmap;
    __u8 *inode_bitmap;
    __le32 inode_table_start;
    __le32 data_blocks_start;
    __le32 inodes_count;
    __le32 blocks_count;
};

// 内存中的inode信息
struct naive_inode_info {
    struct naive_disk_inode *disk_inode;
    struct buffer_head *inode_bh;
    struct inode vfs_inode;
};

// 转换宏
#define NAIVE_SB(sb) ((struct naive_sb_info *)(sb->s_fs_info))
#define NAIVE_I(inode) container_of(inode, struct naive_inode_info, vfs_inode)

// 位操作函数
static inline void naive_set_bit(int nr, void *addr)
{
    __u8 *p = (__u8 *)addr + (nr >> 3);
    *p |= (1 << (nr & 7));
}

static inline void naive_clear_bit(int nr, void *addr)
{
    __u8 *p = (__u8 *)addr + (nr >> 3);
    *p &= ~(1 << (nr & 7));
}

static inline int naive_test_bit(int nr, const void *addr)
{
    const __u8 *p = (const __u8 *)addr + (nr >> 3);
    return (*p >> (nr & 7)) & 1;
}

// 函数声明
static int naive_create(struct mnt_idmap *idmap, 
                       struct inode *dir, 
                       struct dentry *dentry,
                       umode_t mode, bool excl);
static int naive_write_inode(struct inode *inode, struct writeback_control *wbc);
static void naive_destroy_inode(struct inode *inode);

// 查找空闲块
static int naive_find_free_block(struct naive_sb_info *sbi)
{
    int i;
    __u8 *bitmap = sbi->block_bitmap;
    int total_blocks = le32_to_cpu(sbi->blocks_count);
    
    // 跳过前几个块（超级块、位图、inode表）
    int start_block = le32_to_cpu(sbi->data_blocks_start);
    
    for (i = start_block; i < total_blocks; i++) {
        if (!naive_test_bit(i, bitmap)) {
            naive_set_bit(i, bitmap);
            mark_buffer_dirty(sbi->block_bitmap_bh);
            return i;
        }
    }
    
    return -1;  // 没有空闲块
}

// 查找空闲inode
static int naive_find_free_inode(struct naive_sb_info *sbi)
{
    int i;
    
    for (i = 0; i < le32_to_cpu(sbi->inodes_count); i++) {
        if (!naive_test_bit(i, sbi->inode_bitmap)) {
            naive_set_bit(i, sbi->inode_bitmap);
            mark_buffer_dirty(sbi->inode_bitmap_bh);
            return i;
        }
    }
    
    return -1;  // 没有空闲inode
}

// ==================== 文件读写操作 ====================

// 读取文件
static ssize_t naive_file_read(struct file *filp, char __user *buf,
                              size_t len, loff_t *ppos)
{
    struct inode *inode = file_inode(filp);
    struct naive_inode_info *nii = NAIVE_I(inode);
    struct naive_disk_inode *disk_inode = nii->disk_inode;
    struct super_block *sb = inode->i_sb;
    __le32 file_size = le32_to_cpu(disk_inode->i_size);
    __le32 block_index, block_offset;
    loff_t pos = *ppos;
    ssize_t bytes_read = 0;
    struct buffer_head *bh;
    
    printk(KERN_INFO "naivefs: read inode %lu, size %u, pos %lld, len %zu\n",
           inode->i_ino, file_size, pos, len);
    
    // 检查是否超出文件范围
    if (pos >= file_size)
        return 0;
    
    // 调整读取长度
    if (pos + len > file_size)
        len = file_size - pos;
    
    while (len > 0) {
        // 计算块索引和块内偏移
        block_index = pos / NAIVE_BLOCK_SIZE;
        block_offset = pos % NAIVE_BLOCK_SIZE;
        
        // 检查块指针是否有效
        if (block_index >= NAIVE_DIRECT_BLOCKS || 
            disk_inode->i_block[block_index] == 0) {
            printk(KERN_WARNING "naivefs: invalid block pointer at index %u\n", block_index);
            break;
        }
        
        // 读取数据块
        bh = sb_bread(sb, le32_to_cpu(disk_inode->i_block[block_index]));
        if (!bh) {
            printk(KERN_ERR "naivefs: failed to read block %u\n",
                   le32_to_cpu(disk_inode->i_block[block_index]));
            break;
        }
        
        // 计算本次可以读取的字节数
        size_t bytes_to_copy = len;
        if (bytes_to_copy > NAIVE_BLOCK_SIZE - block_offset)
            bytes_to_copy = NAIVE_BLOCK_SIZE - block_offset;
        if (bytes_to_copy > file_size - pos)
            bytes_to_copy = file_size - pos;
        
        // 复制到用户空间
        if (copy_to_user(buf + bytes_read, bh->b_data + block_offset, bytes_to_copy)) {
            brelse(bh);
            bytes_read = -EFAULT;
            break;
        }
        
        brelse(bh);
        
        // 更新位置和计数器
        bytes_read += bytes_to_copy;
        pos += bytes_to_copy;
        len -= bytes_to_copy;
    }
    
    // 更新文件访问时间
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    inode_set_atime_to_ts(inode, ts);
    
    *ppos = pos;
    printk(KERN_INFO "naivefs: read completed, read %zd bytes\n", bytes_read);
    
    return bytes_read;
}

// 写入文件
static ssize_t naive_file_write(struct file *filp, const char __user *buf,
                               size_t len, loff_t *ppos)
{
    struct inode *inode = file_inode(filp);
    struct naive_inode_info *nii = NAIVE_I(inode);
    struct naive_disk_inode *disk_inode = nii->disk_inode;
    struct super_block *sb = inode->i_sb;
    struct naive_sb_info *sbi = NAIVE_SB(sb);
    struct buffer_head *bh;
    __le32 file_size = le32_to_cpu(disk_inode->i_size);
    loff_t pos = *ppos;
    ssize_t bytes_written = 0;
    __le32 block_index, block_offset;
    int new_block_needed;
    
    printk(KERN_INFO "naivefs: write inode %lu, size %u, pos %lld, len %zu\n",
           inode->i_ino, file_size, pos, len);
    
    // 检查文件大小限制
    if (pos + len > NAIVE_MAX_FILESIZE) {
        printk(KERN_ERR "naivefs: file size exceeds limit\n");
        return -EFBIG;
    }
    
    while (len > 0) {
        // 计算块索引和块内偏移
        block_index = pos / NAIVE_BLOCK_SIZE;
        block_offset = pos % NAIVE_BLOCK_SIZE;
        
        // 检查是否需要分配新块
        new_block_needed = 0;
        if (block_index >= NAIVE_DIRECT_BLOCKS) {
            printk(KERN_ERR "naivefs: exceeded direct block limit\n");
            break;
        }
        
        if (disk_inode->i_block[block_index] == 0) {
            // 需要分配新块
            int new_block = naive_find_free_block(sbi);
            if (new_block < 0) {
                printk(KERN_ERR "naivefs: no free blocks available\n");
                break;
            }
            disk_inode->i_block[block_index] = cpu_to_le32(new_block);
            disk_inode->i_blocks = cpu_to_le32(le32_to_cpu(disk_inode->i_blocks) + 1);
            new_block_needed = 1;
            printk(KERN_INFO "naivefs: allocated new block %d for inode %lu\n",
                   new_block, inode->i_ino);
        }
        
        // 读取或创建数据块
        bh = sb_bread(sb, le32_to_cpu(disk_inode->i_block[block_index]));
        if (!bh) {
            // 如果分配了块但读取失败，需要清理
            if (new_block_needed) {
                naive_clear_bit(le32_to_cpu(disk_inode->i_block[block_index]), 
                               sbi->block_bitmap);
                disk_inode->i_block[block_index] = 0;
                disk_inode->i_blocks = cpu_to_le32(le32_to_cpu(disk_inode->i_blocks) - 1);
            }
            printk(KERN_ERR "naivefs: failed to read block %u\n",
                   le32_to_cpu(disk_inode->i_block[block_index]));
            break;
        }
        
        // 计算本次可以写入的字节数
        size_t bytes_to_copy = len;
        if (bytes_to_copy > NAIVE_BLOCK_SIZE - block_offset)
            bytes_to_copy = NAIVE_BLOCK_SIZE - block_offset;
        
        // 复制数据
        if (copy_from_user(bh->b_data + block_offset, buf + bytes_written, bytes_to_copy)) {
            brelse(bh);
            bytes_written = -EFAULT;
            break;
        }
        
        // 标记缓冲区为脏
        mark_buffer_dirty(bh);
        brelse(bh);
        
        // 更新位置和计数器
        bytes_written += bytes_to_copy;
        pos += bytes_to_copy;
        len -= bytes_to_copy;
        
        // 更新文件大小
        if (pos > file_size) {
            file_size = pos;
            disk_inode->i_size = cpu_to_le32(file_size);
            i_size_write(inode, file_size);
        }
    }
    
    // 更新文件修改时间和inode
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    inode_set_mtime_to_ts(inode, ts);
    inode_set_ctime_to_ts(inode, ts);
    
    // 标记inode缓冲区为脏
    mark_buffer_dirty(nii->inode_bh);
    
    *ppos = pos;
    printk(KERN_INFO "naivefs: write completed, wrote %zd bytes, new size %u\n",
           bytes_written, file_size);
    
    return bytes_written;
}

// 文件关闭操作
static int naive_file_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "naivefs: file release inode %lu\n", inode->i_ino);
    return 0;
}

// 文件打开操作
static int naive_file_open(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "naivefs: file open inode %lu\n", inode->i_ino);
    return 0;
}

// 文件操作结构
static const struct file_operations naive_file_operations = {
    .owner = THIS_MODULE,
    .read = naive_file_read,
    .write = naive_file_write,
    .open = naive_file_open,
    .release = naive_file_release,
    .llseek = generic_file_llseek,
};

// inode操作结构（目录）
static struct inode_operations naive_dir_inode_operations = {
    .create = naive_create,
    .lookup = simple_lookup,
};

// inode操作结构（文件）
static struct inode_operations naive_file_inode_operations = {
    .setattr = simple_setattr,
    .getattr = simple_getattr,
};

// ==================== 文件创建函数 ====================

// naive_create 函数
static int naive_create(struct mnt_idmap *idmap, 
                       struct inode *dir, 
                       struct dentry *dentry,
                       umode_t mode, bool excl)
{
    struct inode *inode;
    struct naive_sb_info *sbi = NAIVE_SB(dir->i_sb);
    struct super_block *sb = dir->i_sb;
    int ino;
    struct naive_inode_info *nii;
    struct buffer_head *bh;
    struct naive_disk_inode *disk_inode;
    int inode_table_block;
    
    printk(KERN_INFO "naivefs: create called for %s, mode %o\n", 
           dentry->d_name.name, mode);
    
    // 分配inode编号
    ino = naive_find_free_inode(sbi);
    if (ino < 0) {
        printk(KERN_ERR "naivefs: no free inodes\n");
        return -ENOSPC;
    }
    
    printk(KERN_INFO "naivefs: allocated inode %d\n", ino);
    
    // 创建新的VFS inode
    inode = new_inode(sb);
    if (!inode) {
        naive_clear_bit(ino, sbi->inode_bitmap);
        return -ENOMEM;
    }
    
    inode->i_ino = ino;
    inode_init_owner(&nop_mnt_idmap, inode, dir, mode);
    
    // 设置时间
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    inode_set_atime_to_ts(inode, ts);
    inode_set_mtime_to_ts(inode, ts);
    inode_set_ctime_to_ts(inode, ts);
    
    // 设置链接数
    set_nlink(inode, 1);
    
    // 计算inode在磁盘上的位置（每个块一个inode）
    inode_table_block = le32_to_cpu(sbi->inode_table_start) + ino;
    
    // 读取inode块
    bh = sb_bread(sb, inode_table_block);
    if (!bh) {
        printk(KERN_ERR "naivefs: failed to read inode table block %d\n", inode_table_block);
        iput(inode);
        naive_clear_bit(ino, sbi->inode_bitmap);
        return -EIO;
    }
    
    disk_inode = (struct naive_disk_inode *)bh->b_data;
    
    // 初始化磁盘inode
    memset(disk_inode, 0, sizeof(struct naive_disk_inode));
    disk_inode->i_mode = cpu_to_le32(inode->i_mode);
    disk_inode->i_ino = cpu_to_le32(ino);
    disk_inode->i_uid = cpu_to_le32(i_uid_read(inode));
    disk_inode->i_gid = cpu_to_le32(i_gid_read(inode));
    disk_inode->i_links_count = cpu_to_le32(1);
    disk_inode->i_atime = cpu_to_le32(ts.tv_sec);
    disk_inode->i_mtime = cpu_to_le32(ts.tv_sec);
    disk_inode->i_ctime = cpu_to_le32(ts.tv_sec);
    
    // 根据文件类型设置操作集
    if (S_ISDIR(mode)) {
        inode->i_op = &naive_dir_inode_operations;
        inode->i_fop = &simple_dir_operations;
        printk(KERN_INFO "naivefs: created directory inode %d\n", ino);
    } else {
        inode->i_op = &naive_file_inode_operations;
        inode->i_fop = &naive_file_operations;  // 使用我们定义的文件操作
        printk(KERN_INFO "naivefs: created file inode %d\n", ino);
    }
    
    // 分配内存inode信息结构
    nii = kmalloc(sizeof(struct naive_inode_info), GFP_KERNEL);
    if (!nii) {
        brelse(bh);
        iput(inode);
        naive_clear_bit(ino, sbi->inode_bitmap);
        return -ENOMEM;
    }
    
    nii->disk_inode = disk_inode;
    nii->inode_bh = bh;
    inode->i_private = nii;
    
    // 插入inode到VFS
    d_add(dentry, inode);
    
    // 标记缓冲区为脏
    mark_buffer_dirty(bh);
    mark_buffer_dirty(sbi->sb_bh);
    
    printk(KERN_INFO "naivefs: created inode %d for %s\n", ino, dentry->d_name.name);
    
    return 0;
}

// naive_write_inode 函数
static int naive_write_inode(struct inode *inode, struct writeback_control *wbc)
{
    struct naive_inode_info *nii = (struct naive_inode_info *)inode->i_private;
    
    if (!nii || !nii->disk_inode || !nii->inode_bh) {
        printk(KERN_ERR "naivefs: no disk inode for inode %lu\n", inode->i_ino);
        return -EIO;
    }
    
    struct naive_disk_inode *disk_inode = nii->disk_inode;
    
    printk(KERN_INFO "naivefs: write_inode called for inode %lu\n", inode->i_ino);
    
    // 更新磁盘inode信息
    disk_inode->i_mode = cpu_to_le32(inode->i_mode);
    disk_inode->i_size = cpu_to_le32(i_size_read(inode));
    disk_inode->i_uid = cpu_to_le32(i_uid_read(inode));
    disk_inode->i_gid = cpu_to_le32(i_gid_read(inode));
    disk_inode->i_links_count = cpu_to_le32(inode->i_nlink);
    
    // 更新时间
    disk_inode->i_atime = cpu_to_le32(inode_get_atime(inode).tv_sec);
    disk_inode->i_mtime = cpu_to_le32(inode_get_mtime(inode).tv_sec);
    disk_inode->i_ctime = cpu_to_le32(inode_get_ctime(inode).tv_sec);
    
    // 标记缓冲区为脏
    mark_buffer_dirty(nii->inode_bh);
    
    printk(KERN_INFO "naivefs: inode %lu written to disk, size %u\n",
           inode->i_ino, le32_to_cpu(disk_inode->i_size));
    
    return 0;
}

// 销毁inode
static void naive_destroy_inode(struct inode *inode)
{
    struct naive_inode_info *nii = (struct naive_inode_info *)inode->i_private;
    
    if (nii) {
        if (nii->inode_bh) {
            brelse(nii->inode_bh);
        }
        kfree(nii);
    }
}

// 超级块操作
static const struct super_operations naive_sops = {
    .statfs = simple_statfs,
    .drop_inode = generic_drop_inode,
    .write_inode = naive_write_inode,
    .destroy_inode = naive_destroy_inode,
};

// ==================== 挂载和初始化 ====================

static int naive_fill_super(struct super_block *sb, void *data, int silent)
{
    struct buffer_head *bh;
    struct naive_sb_info *sbi;
    struct naive_super_block *nsb;
    struct inode *root_inode = NULL;
    struct naive_inode_info *root_nii;
    int ret = -EINVAL;
    
    printk(KERN_INFO "naivefs: filling super block\n");
    
    // 分配超级块信息结构
    sbi = kzalloc(sizeof(struct naive_sb_info), GFP_KERNEL);
    if (!sbi)
        return -ENOMEM;
    
    sb->s_fs_info = sbi;
    
    // 读取超级块
    bh = sb_bread(sb, NAIVE_SUPERBLOCK_BLOCK);
    if (!bh) {
        printk(KERN_ERR "naivefs: cannot read superblock\n");
        ret = -EIO;
        goto out_free_sbi;
    }
    
    nsb = (struct naive_super_block *)bh->b_data;
    sbi->sb_bh = bh;
    
    // 验证魔数
    if (le32_to_cpu(nsb->magic) != NAIVE_MAGIC) {
        if (!silent)
            printk(KERN_ERR "naivefs: wrong magic number (got 0x%x, expected 0x%x)\n",
                   le32_to_cpu(nsb->magic), NAIVE_MAGIC);
        ret = -EINVAL;
        goto out_brelse_sb;
    }
    
    // 读取块位图
    bh = sb_bread(sb, NAIVE_BLOCK_BITMAP_BLOCK);
    if (!bh) {
        printk(KERN_ERR "naivefs: cannot read block bitmap\n");
        ret = -EIO;
        goto out_brelse_sb;
    }
    sbi->block_bitmap_bh = bh;
    sbi->block_bitmap = bh->b_data;
    
    // 读取inode位图
    bh = sb_bread(sb, NAIVE_INODE_BITMAP_BLOCK);
    if (!bh) {
        printk(KERN_ERR "naivefs: cannot read inode bitmap\n");
        ret = -EIO;
        goto out_brelse_block_bitmap;
    }
    sbi->inode_bitmap_bh = bh;
    sbi->inode_bitmap = bh->b_data;
    
    // 从超级块读取信息
    sbi->inodes_count = le32_to_cpu(nsb->inode_total);
    sbi->blocks_count = le32_to_cpu(nsb->block_total);
    sbi->inode_table_start = le32_to_cpu(nsb->inode_table_block_no);
    sbi->data_blocks_start = le32_to_cpu(nsb->data_block_no);
    
    // 设置超级块属性
    sb->s_magic = NAIVE_MAGIC;
    sb->s_blocksize = NAIVE_BLOCK_SIZE;
    sb->s_blocksize_bits = 9; // 2^9 = 512
    sb->s_op = &naive_sops;
    
    // 标记根inode已使用
    naive_set_bit(NAIVE_ROOT_INO - 1, sbi->inode_bitmap);
    mark_buffer_dirty(sbi->inode_bitmap_bh);
    
    // 创建根inode
    root_inode = new_inode(sb);
    if (!root_inode) {
        ret = -ENOMEM;
        goto out_brelse_inode_bitmap;
    }
    
    root_inode->i_ino = NAIVE_ROOT_INO;
    root_inode->i_mode = S_IFDIR | 0755;
    inode_init_owner(&nop_mnt_idmap, root_inode, NULL, root_inode->i_mode);
    
    // 设置时间
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    inode_set_atime_to_ts(root_inode, ts);
    inode_set_mtime_to_ts(root_inode, ts);
    inode_set_ctime_to_ts(root_inode, ts);
    
    set_nlink(root_inode, 2); // . 和 ..
    
    // 设置根inode的操作集
    root_inode->i_op = &naive_dir_inode_operations;
    root_inode->i_fop = &simple_dir_operations;
    
    // 分配根inode的磁盘结构
    bh = sb_bread(sb, le32_to_cpu(sbi->inode_table_start) + NAIVE_ROOT_INO - 1);
    if (!bh) {
        printk(KERN_ERR "naivefs: failed to read root inode block\n");
        iput(root_inode);
        ret = -EIO;
        goto out_brelse_inode_bitmap;
    }
    
    // 分配内存inode信息
    root_nii = kmalloc(sizeof(struct naive_inode_info), GFP_KERNEL);
    if (!root_nii) {
        brelse(bh);
        iput(root_inode);
        ret = -ENOMEM;
        goto out_brelse_inode_bitmap;
    }
    
    root_nii->disk_inode = (struct naive_disk_inode *)bh->b_data;
    root_nii->inode_bh = bh;
    root_inode->i_private = root_nii;
    
    // 初始化根磁盘inode
    memset(root_nii->disk_inode, 0, sizeof(struct naive_disk_inode));
    root_nii->disk_inode->i_mode = cpu_to_le32(root_inode->i_mode);
    root_nii->disk_inode->i_ino = cpu_to_le32(NAIVE_ROOT_INO);
    root_nii->disk_inode->i_uid = cpu_to_le32(i_uid_read(root_inode));
    root_nii->disk_inode->i_gid = cpu_to_le32(i_gid_read(root_inode));
    root_nii->disk_inode->i_links_count = cpu_to_le32(2);
    root_nii->disk_inode->i_atime = cpu_to_le32(ts.tv_sec);
    root_nii->disk_inode->i_mtime = cpu_to_le32(ts.tv_sec);
    root_nii->disk_inode->i_ctime = cpu_to_le32(ts.tv_sec);
    
    mark_buffer_dirty(bh);
    
    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        printk(KERN_ERR "naivefs: failed to create root dentry\n");
        kfree(root_nii);
        brelse(bh);
        iput(root_inode);
        ret = -ENOMEM;
        goto out_brelse_inode_bitmap;
    }
    
    printk(KERN_INFO "naivefs: super block filled successfully\n");
    printk(KERN_INFO "naivefs: inodes=%u, blocks=%u, inode_table_start=%u, data_start=%u\n",
           sbi->inodes_count, sbi->blocks_count, sbi->inode_table_start, sbi->data_blocks_start);
    
    return 0;
    
out_brelse_inode_bitmap:
    brelse(sbi->inode_bitmap_bh);
out_brelse_block_bitmap:
    brelse(sbi->block_bitmap_bh);
out_brelse_sb:
    brelse(sbi->sb_bh);
out_free_sbi:
    kfree(sbi);
    return ret;
}

static void naive_put_super(struct super_block *sb)
{
    struct naive_sb_info *sbi = NAIVE_SB(sb);
    
    printk(KERN_INFO "naivefs: put_super called\n");
    
    if (sbi) {
        brelse(sbi->sb_bh);
        brelse(sbi->block_bitmap_bh);
        brelse(sbi->inode_bitmap_bh);
        kfree(sbi);
        sb->s_fs_info = NULL;
    }
}

static struct dentry *naive_mount(struct file_system_type *fs_type,
                                  int flags, const char *dev_name,
                                  void *data)
{
    printk(KERN_INFO "naivefs: mount called for %s\n", dev_name);
    return mount_bdev(fs_type, flags, dev_name, data, naive_fill_super);
}

static void naive_kill_sb(struct super_block *sb)
{
    struct naive_sb_info *sbi = NAIVE_SB(sb);
    
    printk(KERN_INFO "naivefs: kill_sb called\n");
    
    if (sbi) {
        brelse(sbi->sb_bh);
        brelse(sbi->block_bitmap_bh);
        brelse(sbi->inode_bitmap_bh);
        kfree(sbi);
    }
    
    kill_block_super(sb);
}

static struct file_system_type naive_fs_type = {
    .owner = THIS_MODULE,
    .name = "naive",
    .mount = naive_mount,
    .kill_sb = naive_kill_sb,
    .fs_flags = FS_REQUIRES_DEV,
};

static int __init init_naivefs(void)
{
    int err = register_filesystem(&naive_fs_type);
    if (err) {
        printk(KERN_ERR "naivefs: register failed, error %d\n", err);
        return err;
    }
    printk(KERN_INFO "naivefs: module loaded\n");
    return 0;
}

static void __exit exit_naivefs(void)
{
    unregister_filesystem(&naive_fs_type);
    printk(KERN_INFO "naivefs: module unloaded\n");
}

module_init(init_naivefs);
module_exit(exit_naivefs);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Naive Filesystem with Read/Write Support");
