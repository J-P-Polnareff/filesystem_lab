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
        if (imap[i] != 0xFF) {  // 不是全1，说明有空闲位
            for (j = 0; j < 8; j++) {
                if (!(imap[i] & (1 << j))) {
                    int ino = i * 8 + j + 1;  // inode编号从1开始
                    if (ino > total_inodes)
                        return 0;
                    return ino;
                }
            }
        }
    }
    return 0;  // 没有空闲inode
}

/* 分配数据块 */
int naive_alloc_block(struct naive_sb_info *sbi)
{
    int i, j;
    unsigned char *bmap = sbi->block_bitmap;
    int total_blocks = le32_to_cpu(sbi->disk_sb->block_total);
    int data_start = le32_to_cpu(sbi->disk_sb->data_block_no);
    
    // 跳过元数据块
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
                    
                bmap[i] |= (1 << j);  // 标记为已分配
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
        sbi->block_bitmap[i] &= ~(1 << j);  // 清除位
    }
}

/* 读取inode的数据块 */
int naive_read_inode_block(struct inode *inode, int block_index, struct buffer_head **bh)
{
    struct naive_inode_info *nii = NAIVE_I(inode);
    struct super_block *sb = inode->i_sb;
    
    if (block_index < 0 || block_index >= nii->block_count)
        return -EIO;
        
    *bh = sb_bread(sb, nii->block_pointers[block_index]);
    if (!*bh)
        return -EIO;
        
    return 0;
}

/* 现有的超级块操作函数... */
/* 这里应该包含你已有的naive_fill_super, naive_put_super等函数 */
/* 为了简洁，我只展示新增部分 */

/* 填充超级块 - 新增位图读取 */
static int naive_fill_super(struct super_block *sb, void *data, int silent)
{
    struct naive_sb_info *sbi;
    struct buffer_head *bh;
    struct naive_super_block *nsb;
    struct inode *root_inode;
    int ret = 0;
    
    /* ... 现有的超级块读取代码 ... */
    
    // 读取数据块位图
    bh = sb_bread(sb, 2);  // 块2是数据块位图
    if (!bh) {
        ret = -EIO;
        goto release_sb;
    }
    sbi->block_bitmap = kmalloc(NAIVE_BLOCK_SIZE, GFP_KERNEL);
    if (!sbi->block_bitmap) {
        ret = -ENOMEM;
        goto release_bh;
    }
    memcpy(sbi->block_bitmap, bh->b_data, NAIVE_BLOCK_SIZE);
    brelse(bh);
    
    // 读取inode位图
    bh = sb_bread(sb, 3);  // 块3是inode位图
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
    brelse(bh);
    
    sbi->block_bitmap_blocks = 1;
    sbi->inode_bitmap_blocks = 1;
    
    /* ... 剩余的代码 ... */
    
free_block_bitmap:
    kfree(sbi->block_bitmap);
release_bh2:
    brelse(bh);
release_sb:
    kfree(sbi);
    return ret;
}

/* 清理函数 */
static void naive_put_super(struct super_block *sb)
{
    struct naive_sb_info *sbi = NAIVE_SB(sb);
    
    if (sbi) {
        kfree(sbi->block_bitmap);
        kfree(sbi->inode_bitmap);
        brelse(sbi->sb_bh);
        kfree(sbi);
        sb->s_fs_info = NULL;
    }
}

/* ... 其他现有函数 ... */
