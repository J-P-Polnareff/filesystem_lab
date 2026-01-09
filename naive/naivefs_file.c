#include "naivefs.h"

#include <linux/pagemap.h>
#include <linux/uio.h>

/* 文件打开函数 */
int naive_file_open(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "naivefs: file_open called for inode %lu\n", inode->i_ino);
    return 0;
}

/* 文件释放函数 */
int naive_file_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "naivefs: file_release called for inode %lu\n", inode->i_ino);
    return 0;
}

/* 文件读取函数 */
ssize_t naive_file_read(struct file *filp, char __user *buf, size_t len, loff_t *pos)
{
    struct inode *inode = file_inode(filp);
    struct naive_inode_info *nii = NAIVE_I(inode);
    struct super_block *sb = inode->i_sb;
    struct buffer_head *bh;
    ssize_t ret = 0;
    size_t remaining;
    int block_index;
    
    printk(KERN_INFO "naivefs: file_read called for inode %lu, len=%zu, pos=%lld\n",
           inode->i_ino, len, *pos);
    
    /* 检查边界 */
    if (*pos >= inode->i_size)
        return 0;
    
    remaining = min_t(size_t, len, inode->i_size - *pos);
    
    /* 逐块读取 */
    while (remaining > 0) {
        block_index = *pos / NAIVE_BLOCK_SIZE;
        if (block_index >= nii->block_count || nii->block_pointers[block_index] == 0)
            break;
        
        bh = sb_bread(sb, nii->block_pointers[block_index]);
        if (!bh) {
            ret = -EIO;
            break;
        }
        
        size_t offset_in_block = *pos % NAIVE_BLOCK_SIZE;
        size_t to_copy = min_t(size_t, remaining, NAIVE_BLOCK_SIZE - offset_in_block);
        
        if (copy_to_user(buf + ret, bh->b_data + offset_in_block, to_copy)) {
            brelse(bh);
            ret = -EFAULT;
            break;
        }
        
        brelse(bh);
        
        *pos += to_copy;
        remaining -= to_copy;
        ret += to_copy;
    }
    
    return ret;
}

/* 文件写入函数 */
ssize_t naive_file_write(struct file *filp, const char __user *buf, size_t len, loff_t *pos)
{
    struct inode *inode = file_inode(filp);
    struct naive_inode_info *nii = NAIVE_I(inode);
    struct naive_sb_info *sbi = NAIVE_SB(inode->i_sb);
    struct buffer_head *bh;
    ssize_t ret = 0;
    size_t remaining = len;
    int block_index;
    
    printk(KERN_INFO "naivefs: file_write called for inode %lu, len=%zu, pos=%lld\n",
           inode->i_ino, len, *pos);
    
    /* 检查文件大小限制 */
    if (*pos + len > NAIVE_MAX_FILE_SIZE)
        return -EFBIG;
    
    /* 需要分配新的块吗？ */
    if (*pos + len > inode->i_size) {
        int needed_blocks = ((*pos + len + NAIVE_BLOCK_SIZE - 1) / NAIVE_BLOCK_SIZE) -
                           (inode->i_size / NAIVE_BLOCK_SIZE);
        
        for (int i = 0; i < needed_blocks; i++) {
            int new_block = naive_alloc_block(sbi);
            if (new_block < 0) {
                ret = -ENOSPC;
                goto out;
            }
            
            if (nii->block_count < NAIVE_BLOCK_PER_FILE) {
                nii->block_pointers[nii->block_count++] = new_block;
            } else {
                naive_free_block(sbi, new_block);
                ret = -ENOSPC;
                goto out;
            }
        }
    }
    
    /* 逐块写入 */
    while (remaining > 0) {
        block_index = *pos / NAIVE_BLOCK_SIZE;
        if (block_index >= nii->block_count) {
            ret = -ENOSPC;
            break;
        }
        
        bh = sb_bread(inode->i_sb, nii->block_pointers[block_index]);
        if (!bh) {
            ret = -EIO;
            break;
        }
        
        size_t offset_in_block = *pos % NAIVE_BLOCK_SIZE;
        size_t to_copy = min_t(size_t, remaining, NAIVE_BLOCK_SIZE - offset_in_block);
        
        if (copy_from_user(bh->b_data + offset_in_block, buf + ret, to_copy)) {
            brelse(bh);
            ret = -EFAULT;
            break;
        }
        
        mark_buffer_dirty(bh);
        brelse(bh);
        
        *pos += to_copy;
        remaining -= to_copy;
        ret += to_copy;
    }
    
    /* 更新文件大小 */
    if (*pos > inode->i_size) {
        inode->i_size = *pos;
        mark_inode_dirty(inode);
    }
    
    /* 更新时间 */
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    inode_set_mtime_to_ts(inode, ts);
    inode_set_ctime_to_ts(inode, ts);
    
out:
    return ret;
}
