#include "naivefs.h"

#include <linux/namei.h>
#include <linux/pagemap.h>

/* 目录操作函数 */

/* 添加目录项 */
int naive_add_entry(struct inode *dir, struct dentry *dentry, int ino)
{
    struct naive_inode_info *nii = NAIVE_I(dir);
    struct super_block *sb = dir->i_sb;
    struct buffer_head *bh;
    struct naive_dir_record *record;
    int i, j;
    int found = 0;
    
    for (i = 0; i < nii->block_count; i++) {
        if (nii->block_pointers[i] == 0)
            continue;
            
        bh = sb_bread(sb, nii->block_pointers[i]);
        if (!bh)
            return -EIO;
        
        // 查找空闲目录项
        for (j = 0; j < NAIVE_DIR_RECORDS_PER_BLOCK; j++) {
            record = (struct naive_dir_record *)
                     (bh->b_data + j * NAIVE_DIR_RECORD_SIZE);
            
            if (le32_to_cpu(record->i_ino) == 0) {
                // 找到空闲位置
                record->i_ino = cpu_to_le32(ino);
                strncpy(record->filename, dentry->d_name.name, 
                       NAIVE_MAX_FILENAME_LEN);
                record->filename[NAIVE_MAX_FILENAME_LEN - 1] = '\0';
                
                mark_buffer_dirty(bh);
                brelse(bh);
                found = 1;
                break;
            }
        }
        brelse(bh);
        if (found)
            break;
    }
    
    if (!found) {
        // 需要分配新的数据块
        struct naive_sb_info *sbi = NAIVE_SB(sb);
        int new_block = naive_alloc_block(sbi);
        
        if (new_block < 0)
            return -ENOSPC;
        
        // 添加到inode块指针数组
        for (i = 0; i < NAIVE_BLOCK_PER_FILE; i++) {
            if (nii->block_pointers[i] == 0) {
                nii->block_pointers[i] = new_block;
                nii->block_count++;
                break;
            }
        }
        
        // 初始化新块
        bh = sb_bread(sb, new_block);
        if (!bh) {
            naive_free_block(sbi, new_block);
            return -EIO;
        }
        
        memset(bh->b_data, 0, NAIVE_BLOCK_SIZE);
        record = (struct naive_dir_record *)bh->b_data;
        record->i_ino = cpu_to_le32(ino);
        strncpy(record->filename, dentry->d_name.name, 
               NAIVE_MAX_FILENAME_LEN);
        record->filename[NAIVE_MAX_FILENAME_LEN - 1] = '\0';
        
        mark_buffer_dirty(bh);
        brelse(bh);
        
        // 更新inode大小
        dir->i_size += NAIVE_BLOCK_SIZE;
        mark_inode_dirty(dir);
    }
    
    return 0;
}

/* 从目录中移除条目 */
int naive_remove_entry(struct inode *dir, struct dentry *dentry)
{
    struct naive_inode_info *nii = NAIVE_I(dir);
    struct super_block *sb = dir->i_sb;
    struct buffer_head *bh;
    struct naive_dir_record *record;
    int i, j;
    
    for (i = 0; i < nii->block_count; i++) {
        if (nii->block_pointers[i] == 0)
            continue;
            
        bh = sb_bread(sb, nii->block_pointers[i]);
        if (!bh)
            return -EIO;
        
        for (j = 0; j < NAIVE_DIR_RECORDS_PER_BLOCK; j++) {
            record = (struct naive_dir_record *)
                     (bh->b_data + j * NAIVE_DIR_RECORD_SIZE);
            
            // 找到匹配的目录项
            if (le32_to_cpu(record->i_ino) != 0 &&
                strncmp(record->filename, dentry->d_name.name,
                       NAIVE_MAX_FILENAME_LEN) == 0) {
                
                // 清空目录项
                record->i_ino = 0;
                memset(record->filename, 0, NAIVE_MAX_FILENAME_LEN);
                
                mark_buffer_dirty(bh);
                brelse(bh);
                
                return 0;
            }
        }
        brelse(bh);
    }
    
    return -ENOENT;  // 未找到条目
}

/* 检查目录是否为空 */
int naive_is_empty_dir(struct inode *dir)
{
    struct naive_inode_info *nii = NAIVE_I(dir);
    struct buffer_head *bh;
    struct naive_dir_record *record;
    int i, j;
    int count = 0;
    
    for (i = 0; i < nii->block_count; i++) {
        if (nii->block_pointers[i] == 0)
            continue;
            
        bh = sb_bread(dir->i_sb, nii->block_pointers[i]);
        if (!bh)
            return -EIO;
        
        for (j = 0; j < NAIVE_DIR_RECORDS_PER_BLOCK; j++) {
            record = (struct naive_dir_record *)
                     (bh->b_data + j * NAIVE_DIR_RECORD_SIZE);
            
            if (le32_to_cpu(record->i_ino) != 0) {
                count++;
            }
        }
        brelse(bh);
    }
    
    // 目录应该只有.和..两个条目
    return (count == 2) ? 1 : 0;
}

/* 创建目录函数 */
static int naive_mkdir(struct mnt_idmap *idmap, struct inode *dir,
                      struct dentry *dentry, umode_t mode)
{
    struct inode *inode;
    struct super_block *sb = dir->i_sb;
    struct naive_sb_info *sbi = NAIVE_SB(sb);
    struct naive_inode_info *new_nii;
    struct naive_dir_record dot, dotdot;
    struct buffer_head *bh;
    int ret = 0;
    int block_no;
    int ino;
    
    printk(KERN_INFO "naivefs: mkdir called for %s\n", dentry->d_name.name);
    
    // 1. 分配新的inode编号
    ino = naive_find_free_inode(sbi);
    if (ino == 0) {
        ret = -ENOSPC;
        goto out;
    }
    
    // 2. 分配inode对象
    inode = new_inode(sb);
    if (!inode) {
        ret = -ENOMEM;
        goto out;
    }
    
    inode->i_ino = ino;
    
    // 3. 分配数据块
    block_no = naive_alloc_block(sbi);
    if (block_no < 0) {
        ret = -ENOSPC;
        goto fail_inode;
    }
    
    // 4. 初始化目录内容
    bh = sb_bread(sb, block_no);
    if (!bh) {
        ret = -EIO;
        naive_free_block(sbi, block_no);
        goto fail_inode;
    }
    
    memset(bh->b_data, 0, NAIVE_BLOCK_SIZE);
    
    // 创建.目录项
    dot.i_ino = cpu_to_le32(ino);
    strncpy(dot.filename, ".", NAIVE_MAX_FILENAME_LEN);
    memcpy(bh->b_data, &dot, sizeof(struct naive_dir_record));
    
    // 创建..目录项
    dotdot.i_ino = cpu_to_le32(dir->i_ino);
    strncpy(dotdot.filename, "..", NAIVE_MAX_FILENAME_LEN);
    memcpy(bh->b_data + sizeof(struct naive_dir_record), 
           &dotdot, sizeof(struct naive_dir_record));
    
    mark_buffer_dirty(bh);
    brelse(bh);
    
    // 5. 初始化inode
    inode_init_owner(idmap, inode, dir, S_IFDIR | (mode & 0777));
    inode->i_sb = sb;
    inode->i_op = &naive_dir_iops;
    inode->i_fop = &simple_dir_operations;
    
    // 设置时间
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    inode_set_atime_to_ts(inode, ts);
    inode_set_mtime_to_ts(inode, ts);
    inode_set_ctime_to_ts(inode, ts);
    
    // 设置链接数：.和..
    set_nlink(inode, 2);
    
    // 设置inode大小
    inode->i_size = NAIVE_BLOCK_SIZE;
    
    // 设置块指针
    new_nii = NAIVE_I(inode);
    new_nii->block_count = 1;
    new_nii->block_pointers[0] = block_no;
    
    // 6. 标记位图为已使用
    sbi->inode_bitmap[(ino - 1) / 8] |= (1 << ((ino - 1) % 8));
    
    // 7. 在父目录中添加目录项
    ret = naive_add_entry(dir, dentry, ino);
    if (ret < 0) {
        naive_free_block(sbi, block_no);
        goto fail_inode;
    }
    
    // 8. 更新父目录链接数
    inc_nlink(dir);
    mark_inode_dirty(dir);
    
    // 9. 关联inode和dentry
    d_instantiate(dentry, inode);
    
    printk(KERN_INFO "naivefs: directory %s created successfully, inode=%d\n",
           dentry->d_name.name, ino);
    return 0;
    
fail_inode:
    iput(inode);
out:
    return ret;
}

/* 删除目录函数 */
static int naive_rmdir(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode = d_inode(dentry);
    struct super_block *sb = dir->i_sb;
    struct naive_sb_info *sbi = NAIVE_SB(sb);
    struct naive_inode_info *nii = NAIVE_I(inode);
    int ret;
    int i;
    
    printk(KERN_INFO "naivefs: rmdir called for %s\n", dentry->d_name.name);
    
    // 1. 检查是否是目录
    if (!S_ISDIR(inode->i_mode)) {
        ret = -ENOTDIR;
        goto out;
    }
    
    // 2. 检查目录是否为空
    ret = naive_is_empty_dir(inode);
    if (ret <= 0) {
        if (ret == 0)
            ret = -ENOTEMPTY;
        goto out;
    }
    
    // 3. 从父目录中删除目录项
    ret = naive_remove_entry(dir, dentry);
    if (ret < 0) {
        goto out;
    }
    
    // 4. 释放目录的数据块
    for (i = 0; i < nii->block_count; i++) {
        if (nii->block_pointers[i] != 0) {
            naive_free_block(sbi, nii->block_pointers[i]);
            nii->block_pointers[i] = 0;
        }
    }
    nii->block_count = 0;
    
    // 5. 释放inode位图
    int ino = inode->i_ino;
    sbi->inode_bitmap[(ino - 1) / 8] &= ~(1 << ((ino - 1) % 8));
    
    // 6. 减少父目录链接数
    drop_nlink(dir);
    mark_inode_dirty(dir);
    
    // 7. 清除inode
    clear_nlink(inode);
    inode->i_size = 0;
    
    // 8. 删除dentry
    d_drop(dentry);
    
    printk(KERN_INFO "naivefs: directory %s removed successfully\n",
           dentry->d_name.name);
    return 0;
    
out:
    return ret;
}

/* 更新inode_operations结构 */
static struct inode_operations naive_dir_iops = {
    .create     = naive_create,
    .lookup     = naive_lookup,
    .unlink     = naive_unlink,
    .mkdir      = naive_mkdir,      // 新增
    .rmdir      = naive_rmdir,      // 新增
    .getattr    = simple_getattr,
};

/* 已有的其他函数... */
static int naive_create(struct mnt_idmap *idmap, struct inode *dir,
                       struct dentry *dentry, umode_t mode, bool excl)
{
    /* 已有的实现 */
    printk(KERN_INFO "naivefs: create called for %s\n", dentry->d_name.name);
    
    struct inode *inode;
    struct super_block *sb = dir->i_sb;
    struct naive_sb_info *sbi = NAIVE_SB(sb);
    int ino;
    int ret;
    
    ino = naive_find_free_inode(sbi);
    if (ino == 0)
        return -ENOSPC;
    
    inode = new_inode(sb);
    if (!inode)
        return -ENOMEM;
    
    inode->i_ino = ino;
    inode_init_owner(idmap, inode, dir, mode);
    inode->i_sb = sb;
    inode->i_op = &naive_file_iops;
    inode->i_fop = &naive_file_ops;
    
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    inode_set_atime_to_ts(inode, ts);
    inode_set_mtime_to_ts(inode, ts);
    inode_set_ctime_to_ts(inode, ts);
    
    set_nlink(inode, 1);
    inode->i_size = 0;
    
    sbi->inode_bitmap[(ino - 1) / 8] |= (1 << ((ino - 1) % 8));
    
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

static int naive_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
    /* 已有的实现 */
    struct naive_inode_info *nii = NAIVE_I(dir);
    struct super_block *sb = dir->i_sb;
    struct buffer_head *bh;
    struct naive_dir_record *record;
    struct inode *inode = NULL;
    int i, j;
    
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
    
    d_add(dentry, inode);
    return 0;
}

static int naive_unlink(struct inode *dir, struct dentry *dentry)
{
    /* 已有的实现 */
    struct inode *inode = d_inode(dentry);
    struct super_block *sb = dir->i_sb;
    struct naive_sb_info *sbi = NAIVE_SB(sb);
    struct naive_inode_info *nii = NAIVE_I(inode);
    int ret;
    int i;
    
    printk(KERN_INFO "naivefs: unlink called for %s\n", dentry->d_name.name);
    
    ret = naive_remove_entry(dir, dentry);
    if (ret < 0)
        return ret;
    
    for (i = 0; i < nii->block_count; i++) {
        if (nii->block_pointers[i] != 0) {
            naive_free_block(sbi, nii->block_pointers[i]);
            nii->block_pointers[i] = 0;
        }
    }
    nii->block_count = 0;
    
    int ino = inode->i_ino;
    sbi->inode_bitmap[(ino - 1) / 8] &= ~(1 << ((ino - 1) % 8));
    
    clear_nlink(inode);
    inode->i_size = 0;
    
    d_drop(dentry);
    
    printk(KERN_INFO "naivefs: file %s removed\n", dentry->d_name.name);
    return 0;
}
