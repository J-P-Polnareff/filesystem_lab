#ifndef _NAIVEFS_H
#define _NAIVEFS_H

#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/mount.h>

#define NAIVE_MAGIC 0x990717
#define NAIVE_BLOCK_SIZE 512
#define NAIVE_SUPER_BLOCK_BLOCK 1
#define NAIVE_ROOT_INODE_NO 1
#define NAIVE_MAX_FILENAME_LEN 128
#define NAIVE_DIR_RECORD_SIZE 132  /* 4 + 128 */
#define NAIVE_DIR_RECORDS_PER_BLOCK 3
#define NAIVE_BLOCK_PER_FILE 8
#define NAIVE_MAX_FILE_SIZE (NAIVE_BLOCK_SIZE * NAIVE_BLOCK_PER_FILE)

/* 磁盘数据结构 */
struct naive_super_block {
    __le32 magic;
    __le32 inode_total;
    __le32 block_total;
    __le32 inode_table_block_no;
    __le32 data_block_no;
    __u8 padding[492];
};

struct naive_inode {
    __le32 mode;
    __le32 i_ino;
    __le32 block_count;
    __le32 block[NAIVE_BLOCK_PER_FILE];
    union {
        __le32 file_size;
        __le32 dir_children_count;
    };
    __le32 i_uid;
    __le32 i_gid;
    __le32 i_nlink;
    __le32 i_atime;
    __le32 i_ctime;
    __le32 i_mtime;
    __u8 padding[352];
};

struct naive_dir_record {
    __le32 i_ino;
    char filename[NAIVE_MAX_FILENAME_LEN];
};

/* 内存数据结构 */
struct naive_sb_info {
    struct naive_super_block *disk_sb;
    struct buffer_head *sb_bh;
    unsigned char *block_bitmap;
    unsigned char *inode_bitmap;
    int block_bitmap_blocks;
    int inode_bitmap_blocks;
};

struct naive_inode_info {
    struct naive_inode *disk_inode;
    struct buffer_head *inode_bh;
    int block_count;
    int block_pointers[NAIVE_BLOCK_PER_FILE];
    struct inode vfs_inode;
};

#define NAIVE_SB(sb) ((struct naive_sb_info *)(sb->s_fs_info))
#define NAIVE_I(inode) ((struct naive_inode_info *)(inode))

/* ========== 所有函数声明 ========== */

/* 超级块操作集 */
extern const struct super_operations naive_sops;
extern const struct inode_operations naive_dir_iops;
extern const struct inode_operations naive_file_iops;
extern const struct file_operations naive_file_ops;

/* 超级块函数 */
struct inode *naive_alloc_inode(struct super_block *sb);
void naive_destroy_inode(struct inode *inode);
void naive_put_super(struct super_block *sb);
int naive_write_inode(struct inode *inode, struct writeback_control *wbc);
void naive_evict_inode(struct inode *inode);
int naive_fill_super(struct super_block *sb, void *data, int silent);

/* 目录操作 */
int naive_create(struct mnt_idmap *idmap, struct inode *dir,
                struct dentry *dentry, umode_t mode, bool excl);
int naive_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);
int naive_unlink(struct inode *dir, struct dentry *dentry);
int naive_mkdir(struct mnt_idmap *idmap, struct inode *dir,
               struct dentry *dentry, umode_t mode);
int naive_rmdir(struct inode *dir, struct dentry *dentry);
struct inode *naive_iget(struct super_block *sb, unsigned long ino);

/* 文件操作 */
int naive_file_open(struct inode *inode, struct file *filp);
int naive_file_release(struct inode *inode, struct file *filp);
ssize_t naive_file_read(struct file *filp, char __user *buf, size_t len, loff_t *pos);
ssize_t naive_file_write(struct file *filp, const char __user *buf, size_t len, loff_t *pos);

/* 目录项操作 */
int naive_add_entry(struct inode *dir, struct dentry *dentry, int ino);
int naive_remove_entry(struct inode *dir, struct dentry *dentry);

/* 块管理 */
int naive_find_free_inode(struct naive_sb_info *sbi);
int naive_alloc_block(struct naive_sb_info *sbi);
void naive_free_block(struct naive_sb_info *sbi, int block_no);

#endif /* _NAIVEFS_H */
