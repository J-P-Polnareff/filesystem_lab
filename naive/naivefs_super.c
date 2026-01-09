#include "naivefs.h"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>
#include <linux/iversion.h>

/* 块管理函数 */

/* 查找空闲inode */
int naive_find_free_inode(struct naive_sb_info *sbi)
{
    int i, j;
    unsigned char *imap = sbi->inode_bitmap;
    int total_inodes = le32_to_cpu(sbi->disk_sb->inode_total);
    
    for (i = 0; i < total_inodes / 8; i++) {
        if (imap[i] != 0xFF) {
            for (j = 0; j < 8; j++) {
                if (!(imap[i] & (1 << j))) {
                    int ino = i * 8 + j + 1;
                    if (ino > total_inodes)
                        return 0;
                    return ino;
                }
            }
        }
    }
    return 0;
}

/* 分配数据块 */
int naive_alloc_block(struct naive_sb_info *sbi)
{
    int i, j;
    unsigned char *bmap = sbi->block_bitmap;
    int total_blocks = le32_to_cpu(sbi->disk_sb->block_total);
    int data_start = le32_to_cpu(sbi->disk_sb->data_block_no);
    
    int start_byte = data_start / 8;
    int start_bit = data_start % 8;
    
    for (i = start_byte; i < total_blocks / 8; i++) {
        unsigned char byte = bmap[i];
        int start = (i == start_byte) ? start_bit : 0;
        
        for (j = start; j < 8; j++) {
            if (!(byte & (1 << j))) {
                int block_no = i * 8 + j;
                if (block_no >= total_blocks)
                    return -ENOSPC;
                    
                bmap[i] |= (1 << j);
                return block_no;
            }
        }
    }
    return -ENOSPC;
}

/* 释放数据块 */
void naive_free_block(struct naive_sb_info *sbi, int block_no)
{
    int i = block_no / 8;
    int j = block_no % 8;
    
    if (i < sbi->block_bitmap_blocks * NAIVE_BLOCK_SIZE) {
        sbi->block_bitmap[i] &= ~(1 << j);
    }
}

/* 填充超级块 */
int naive_fill_super(struct super_block *sb, void *data, int silent)
{
    struct naive_sb_info *sbi;
    struct buffer_head *bh;
    struct naive_super_block *nsb;
    struct inode *root_inode;
    int ret = 0;
    
    printk(KERN_INFO "naivefs: filling super block\n");
    
    sbi = kzalloc(sizeof(struct naive_sb_info), GFP_KERNEL);
    if (!sbi)
        return -ENOMEM;
    
    sb->s_fs_info = sbi;
    
    /* 读取超级块 */
    bh = sb_bread(sb, NAIVE_SUPER_BLOCK_BLOCK);
    if (!bh) {
        printk(KERN_ERR "naivefs: sb_bread failed for block %d\n", 
               NAIVE_SUPER_BLOCK_BLOCK);
        ret = -EIO;
        goto free_sbi;
    }
    
    nsb = (struct naive_super_block *)bh->b_data;
    sbi->disk_sb = nsb;
    sbi->sb_bh = bh;
    
    /* 验证魔数 */
    printk(KERN_INFO "naivefs: read magic: 0x%x\n", le32_to_cpu(nsb->magic));
    printk(KERN_INFO "naivefs: expected magic: 0x%x\n", NAIVE_MAGIC);
    
    if (le32_to_cpu(nsb->magic) != NAIVE_MAGIC) {
        printk(KERN_ERR "naivefs: wrong magic number (0x%x != 0x%x)\n",
               le32_to_cpu(nsb->magic), NAIVE_MAGIC);
        ret = -EINVAL;
        goto release_sb_bh;
    }
    
    printk(KERN_INFO "naivefs: magic number OK\n");
    
    /* 设置超级块属性 */
    sb->s_magic = NAIVE_MAGIC;
    sb->s_blocksize = NAIVE_BLOCK_SIZE;
    sb->s_blocksize_bits = 9;
    sb->s_maxbytes = NAIVE_MAX_FILE_SIZE;
    sb->s_op = &naive_sops;
    
    /* 读取数据块位图 */
    bh = sb_bread(sb, 2); /* 数据块位图 */
    if (!bh) {
        ret = -EIO;
        goto release_sb_bh;
    }
    sbi->block_bitmap = kmalloc(NAIVE_BLOCK_SIZE, GFP_KERNEL);
    if (!sbi->block_bitmap) {
        ret = -ENOMEM;
        goto release_bh;
    }
    memcpy(sbi->block_bitmap, bh->b_data, NAIVE_BLOCK_SIZE);
    sbi->block_bitmap_blocks = 1;
    brelse(bh);
    
    /* 读取inode位图 */
    bh = sb_bread(sb, 3); /* inode位图 */
    if (!bh) {
        ret = -EIO;
        goto free_block_bitmap;
    }
    sbi->inode_bitmap = kmalloc(NAIVE_BLOCK_SIZE, GFP_KERNEL);
    if (!sbi->inode_bitmap) {
        ret = -ENOMEM;
        goto release_bh2;
    }
    memcpy(sbi->inode_bitmap, bh->b_data, NAIVE_BLOCK_SIZE);
    sbi->inode_bitmap_blocks = 1;
    brelse(bh);
    
    /* 创建根inode */
    root_inode = naive_alloc_inode(sb);
    if (!root_inode) {
        ret = -ENOMEM;
        goto free_inode_bitmap;
    }
    
    root_inode->i_ino = NAIVE_ROOT_INODE_NO;
    root_inode->i_sb = sb;
    root_inode->i_op = &naive_dir_iops;
    root_inode->i_fop = &simple_dir_operations;
    root_inode->i_mode = S_IFDIR | 0755;
    
    /* 设置时间 */
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    inode_set_atime_to_ts(root_inode, ts);
    inode_set_mtime_to_ts(root_inode, ts);
    inode_set_ctime_to_ts(root_inode, ts);
    
    set_nlink(root_inode, 2); /* . 和 .. */
    
    /* 创建根dentry */
    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        ret = -ENOMEM;
        goto free_inode_bitmap;
    }
    
    printk(KERN_INFO "naivefs: fill_super success\n");
    return 0;
    
free_inode_bitmap:
    kfree(sbi->inode_bitmap);
release_bh2:
    brelse(bh);
free_block_bitmap:
    kfree(sbi->block_bitmap);
release_bh:
    brelse(bh);
release_sb_bh:
    brelse(sbi->sb_bh);
free_sbi:
    kfree(sbi);
    return ret;
}

/* 清理超级块 */
void naive_put_super(struct super_block *sb)
{
    struct naive_sb_info *sbi = NAIVE_SB(sb);
    
    if (sbi) {
        kfree(sbi->block_bitmap);
        kfree(sbi->inode_bitmap);
        brelse(sbi->sb_bh);
        kfree(sbi);
        sb->s_fs_info = NULL;
    }
    printk(KERN_INFO "naivefs: put_super called\n");
}

/* 分配inode */
struct inode *naive_alloc_inode(struct super_block *sb)
{
    struct naive_inode_info *nii;
    
    nii = kmalloc(sizeof(struct naive_inode_info), GFP_KERNEL);
    if (!nii)
        return NULL;
    
    memset(nii, 0, sizeof(struct naive_inode_info));
    return &nii->vfs_inode;
}

/* 销毁inode */
void naive_destroy_inode(struct inode *inode)
{
    struct naive_inode_info *nii = NAIVE_I(inode);
    kfree(nii);
}

/* 写入inode */
int naive_write_inode(struct inode *inode, struct writeback_control *wbc)
{
    struct super_block *sb = inode->i_sb;
    struct naive_sb_info *sbi = NAIVE_SB(sb);
    struct naive_inode_info *nii = NAIVE_I(inode);
    struct buffer_head *bh;
    struct naive_inode *disk_inode;
    int inode_table_start = le32_to_cpu(sbi->disk_sb->inode_table_block_no);
    
    printk(KERN_INFO "naivefs: write_inode called for inode %lu\n", inode->i_ino);
    
    bh = sb_bread(sb, inode_table_start + (inode->i_ino - 1));
    if (!bh)
        return -EIO;
    
    disk_inode = (struct naive_inode *)bh->b_data;
    
    /* 填充磁盘inode */
    disk_inode->mode = cpu_to_le32(inode->i_mode);
    disk_inode->i_ino = cpu_to_le32(inode->i_ino);
    disk_inode->block_count = cpu_to_le32(nii->block_count);
    
    for (int i = 0; i < nii->block_count; i++) {
        disk_inode->block[i] = cpu_to_le32(nii->block_pointers[i]);
    }
    
    if (S_ISDIR(inode->i_mode)) {
        disk_inode->dir_children_count = cpu_to_le32(inode->i_size / NAIVE_DIR_RECORD_SIZE);
    } else {
        disk_inode->file_size = cpu_to_le32(inode->i_size);
    }
    
    disk_inode->i_uid = cpu_to_le32(i_uid_read(inode));
    disk_inode->i_gid = cpu_to_le32(i_gid_read(inode));
    disk_inode->i_nlink = cpu_to_le32(inode->i_nlink);
    
    /* 使用正确的时间访问函数 */
    struct timespec64 atime = inode_get_atime(inode);
    struct timespec64 mtime = inode_get_mtime(inode);
    struct timespec64 ctime = inode_get_ctime(inode);
    
    disk_inode->i_atime = cpu_to_le32(atime.tv_sec);
    disk_inode->i_mtime = cpu_to_le32(mtime.tv_sec);
    disk_inode->i_ctime = cpu_to_le32(ctime.tv_sec);
    
    mark_buffer_dirty(bh);
    brelse(bh);
    
    return 0;
}

/* 清除inode */
void naive_evict_inode(struct inode *inode)
{
    printk(KERN_INFO "naivefs: evict_inode called for inode %lu\n", inode->i_ino);
    
    truncate_inode_pages_final(&inode->i_data);
    clear_inode(inode);
}
