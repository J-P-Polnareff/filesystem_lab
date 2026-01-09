#include "naivefs.h"

#include <linux/namei.h>
#include <linux/pagemap.h>

/* 创建文件 */
int naive_create(struct mnt_idmap *idmap, struct inode *dir,
                struct dentry *dentry, umode_t mode, bool excl)
{
    struct inode *inode;
    struct super_block *sb = dir->i_sb;
    struct naive_sb_info *sbi = NAIVE_SB(sb);
    int ino;
    int ret;
    
    printk(KERN_INFO "naivefs: create called for %s\n", dentry->d_name.name);
    
    /* 分配inode编号 */
    ino = naive_find_free_inode(sbi);
    if (ino == 0)
        return -ENOSPC;
    
    /* 分配inode */
    inode = new_inode(sb);
    if (!inode)
        return -ENOMEM;
    
    inode->i_ino = ino;
    inode_init_owner(idmap, inode, dir, mode);
    inode->i_sb = sb;
    inode->i_op = &naive_file_iops;
    inode->i_fop = &naive_file_ops;
    
    /* 设置时间 */
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    inode_set_atime_to_ts(inode, ts);
    inode_set_mtime_to_ts(inode, ts);
    inode_set_ctime_to_ts(inode, ts);
    
    set_nlink(inode, 1);
    inode->i_size = 0;
    
    /* 标记位图 */
    sbi->inode_bitmap[(ino - 1) / 8] |= (1 << ((ino - 1) % 8));
    
    /* 添加到目录 */
    ret = naive_add_entry(dir, dentry, ino);
    if (ret < 0) {
        iput(inode);
        return ret;
    }
    
    d_instantiate(dentry, inode);
    printk(KERN_INFO "naivefs: file %s created, inode=%d\n",
           dentry->d_name.name, ino);
    return 0;
}

/* 查找文件/目录 - 修正返回类型为 struct dentry* */
struct dentry *naive_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
    struct naive_inode_info *nii = NAIVE_I(dir);
    struct super_block *sb = dir->i_sb;
    struct buffer_head *bh;
    struct naive_dir_record *record;
    struct inode *inode = NULL;
    int i, j;
    
    printk(KERN_INFO "naivefs: lookup called for %s\n", dentry->d_name.name);
    
    /* 遍历目录项 */
    for (i = 0; i < nii->block_count; i++) {
        if (nii->block_pointers[i] == 0)
            continue;
            
        bh = sb_bread(sb, nii->block_pointers[i]);
        if (!bh)
            continue;
        
        for (j = 0; j < NAIVE_DIR_RECORDS_PER_BLOCK; j++) {
            record = (struct naive_dir_record *)
                     (bh->b_data + j * NAIVE_DIR_RECORD_SIZE);
            
            if (le32_to_cpu(record->i_ino) != 0 &&
                strncmp(record->filename, dentry->d_name.name,
                       NAIVE_MAX_FILENAME_LEN) == 0) {
                
                inode = naive_iget(sb, le32_to_cpu(record->i_ino));
                break;
            }
        }
        brelse(bh);
        if (inode)
            break;
    }
    
    if (!inode) {
        /* 未找到，返回NULL让VFS处理 */
        return NULL;
    }
    
    return d_splice_alias(inode, dentry);
}

/* 删除文件 */
int naive_unlink(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode = d_inode(dentry);
    struct super_block *sb = dir->i_sb;
    struct naive_sb_info *sbi = NAIVE_SB(sb);
    struct naive_inode_info *nii = NAIVE_I(inode);
    int ret;
    int i;
    
    printk(KERN_INFO "naivefs: unlink called for %s\n", dentry->d_name.name);
    
    /* 从目录中移除 */
    ret = naive_remove_entry(dir, dentry);
    if (ret < 0)
        return ret;
    
    /* 释放数据块 */
    for (i = 0; i < nii->block_count; i++) {
        if (nii->block_pointers[i] != 0) {
            naive_free_block(sbi, nii->block_pointers[i]);
            nii->block_pointers[i] = 0;
        }
    }
    nii->block_count = 0;
    
    /* 释放inode位图 */
    int ino = inode->i_ino;
    sbi->inode_bitmap[(ino - 1) / 8] &= ~(1 << ((ino - 1) % 8));
    
    /* 更新inode */
    clear_nlink(inode);
    inode->i_size = 0;
    
    printk(KERN_INFO "naivefs: file %s removed\n", dentry->d_name.name);
    return 0;
}

/* 获取inode */
struct inode *naive_iget(struct super_block *sb, unsigned long ino)
{
    struct inode *inode;
    struct naive_inode_info *nii;
    struct naive_sb_info *sbi = NAIVE_SB(sb);
    struct buffer_head *bh;
    struct naive_inode *disk_inode;
    int inode_table_start;
    
    printk(KERN_INFO "naivefs: iget called for inode %lu\n", ino);
    
    /* 首先检查inode是否已经在缓存中 */
    inode = iget_locked(sb, ino);
    if (!inode)
        return ERR_PTR(-ENOMEM);
    
    if (!(inode->i_state & I_NEW))
        return inode;
    
    nii = NAIVE_I(inode);
    
    /* 读取磁盘inode */
    inode_table_start = le32_to_cpu(sbi->disk_sb->inode_table_block_no);
    bh = sb_bread(sb, inode_table_start + (ino - 1));
    if (!bh) {
        iget_failed(inode);
        return ERR_PTR(-EIO);
    }
    
    disk_inode = (struct naive_inode *)bh->b_data;
    
    /* 填充inode信息 */
    inode->i_mode = le32_to_cpu(disk_inode->mode);
    inode->i_uid = le32_to_cpu(disk_inode->i_uid);
    inode->i_gid = le32_to_cpu(disk_inode->i_gid);
    inode->i_size = le32_to_cpu(disk_inode->file_size);
    set_nlink(inode, le32_to_cpu(disk_inode->i_nlink));
    
    inode->i_atime.tv_sec = le32_to_cpu(disk_inode->i_atime);
    inode->i_mtime.tv_sec = le32_to_cpu(disk_inode->i_mtime);
    inode->i_ctime.tv_sec = le32_to_cpu(disk_inode->i_ctime);
    inode->i_atime.tv_nsec = inode->i_mtime.tv_nsec = inode->i_ctime.tv_nsec = 0;
    
    /* 设置操作集 */
    if (S_ISDIR(inode->i_mode)) {
        inode->i_op = &naive_dir_iops;
        inode->i_fop = &simple_dir_operations;
    } else {
        inode->i_op = &naive_file_iops;
        inode->i_fop = &naive_file_ops;
    }
    
    /* 设置块指针 */
    nii->block_count = le32_to_cpu(disk_inode->block_count);
    for (int i = 0; i < nii->block_count; i++) {
        nii->block_pointers[i] = le32_to_cpu(disk_inode->block[i]);
    }
    
    brelse(bh);
    
    /* 解锁inode */
    unlock_new_inode(inode);
    
    return inode;
}
