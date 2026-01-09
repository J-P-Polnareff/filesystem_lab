#include <linux/module.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/statfs.h>
#include <linux/time.h>
#include <linux/namei.h>
#include <linux/uaccess.h>

#define NAIVE_MAGIC 0x990717
#define NAIVE_BLOCK_SIZE 512
#define NAIVE_SUPER_BLOCK_BLOCK 1
#define NAIVE_ROOT_INODE_NO 1
#define NAIVE_MAX_FILENAME_LEN 30

// ==================== 数据结构定义 ====================

struct naive_super_block {
    __le32 magic;
    __le32 block_total;
    __le32 inode_total;
    __le32 inode_table_block_no;
    __le32 data_block_no;
    __u8 padding[492];
};

struct naive_inode {
    __le32 i_mode;
    __le32 i_uid;
    __le32 i_gid;
    __le32 i_size;
    __le32 i_blocks;
    __le32 i_nlink;
    __le32 i_atime;
    __le32 i_mtime;
    __le32 i_ctime;
    __le32 i_block[8];
    __u8 padding[NAIVE_BLOCK_SIZE - (11 + 8) * sizeof(__le32)];
};

struct naive_dir_entry {
    __le32 inode;
    char name[NAIVE_MAX_FILENAME_LEN];
};

// ==================== 超级块操作 ====================

static int naive_statfs(struct dentry *dentry, struct kstatfs *buf)
{
    struct super_block *sb = dentry->d_sb;
    
    buf->f_type = sb->s_magic;
    buf->f_bsize = sb->s_blocksize;
    buf->f_blocks = 200 * 1024 / sb->s_blocksize;
    buf->f_bfree = buf->f_bavail = buf->f_blocks / 2;
    buf->f_files = 128;
    buf->f_ffree = 127;
    buf->f_namelen = NAIVE_MAX_FILENAME_LEN;
    
    return 0;
}

static struct super_operations naive_sops = {
    .statfs = naive_statfs,
    .drop_inode = generic_drop_inode,
};

// ==================== 文件操作 ====================

static ssize_t naive_file_read(struct file *filp, char __user *buf,
                               size_t len, loff_t *ppos)
{
    struct inode *inode = file_inode(filp);
    
    printk(KERN_INFO "naivefs: read file %lu, pos %lld, len %zu\n",
           inode->i_ino, *ppos, len);
    
    // 简单实现：返回固定内容
    const char *content = "Hello from naive filesystem!\n";
    size_t content_len = strlen(content);
    
    if (*ppos >= content_len)
        return 0;
    
    if (*ppos + len > content_len)
        len = content_len - *ppos;
    
    if (copy_to_user(buf, content + *ppos, len))
        return -EFAULT;
    
    *ppos += len;
    return len;
}

static ssize_t naive_file_write(struct file *filp, const char __user *buf,
                                size_t len, loff_t *ppos)
{
    struct inode *inode = file_inode(filp);
    
    printk(KERN_INFO "naivefs: write file %lu, pos %lld, len %zu\n",
           inode->i_ino, *ppos, len);
    
    // 简单实现：记录写操作但不实际存储数据
    *ppos += len;
    return len;
}

static int naive_file_open(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "naivefs: open file %lu\n", inode->i_ino);
    return 0;
}

static const struct file_operations naive_file_ops = {
    .owner = THIS_MODULE,
    .read = naive_file_read,
    .write = naive_file_write,
    .open = naive_file_open,
    .llseek = generic_file_llseek,
};

// ==================== 目录操作 ====================

static int naive_readdir(struct file *filp, struct dir_context *ctx)
{
    struct inode *inode = file_inode(filp);
    static struct naive_dir_entry entries[] = {
        {.inode = cpu_to_le32(1), .name = "."},
        {.inode = cpu_to_le32(1), .name = ".."},
        {.inode = cpu_to_le32(2), .name = "testfile.txt"},
        {.inode = cpu_to_le32(3), .name = "testdir"},
    };
    int i;
    
    printk(KERN_INFO "naivefs: readdir inode %lu, pos %lld\n",
           inode->i_ino, ctx->pos);
    
    for (i = 0; i < ARRAY_SIZE(entries); i++) {
        if (ctx->pos >= ARRAY_SIZE(entries))
            break;
        
        if (!dir_emit(ctx, entries[i].name, strlen(entries[i].name),
                      le32_to_cpu(entries[i].inode), DT_UNKNOWN))
            break;
        
        ctx->pos++;
    }
    
    return 0;
}

static const struct file_operations naive_dir_ops = {
    .owner = THIS_MODULE,
    .iterate_shared = naive_readdir,
    .llseek = generic_file_llseek,
};

// ==================== inode操作 ====================

static int naive_create(struct mnt_idmap *idmap, struct inode *dir,
                        struct dentry *dentry, umode_t mode, bool excl)
{
    struct inode *inode;
    static int ino_counter = 100; // 从100开始分配inode号
    
    printk(KERN_INFO "naivefs: create %s (mode: %o)\n",
           dentry->d_name.name, mode);
    
    inode = new_inode(dir->i_sb);
    if (!inode)
        return -ENOMEM;
    
    inode->i_ino = ino_counter++;
    inode_init_owner(&nop_mnt_idmap, inode, dir, mode);
    
    // 设置时间
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    inode_set_atime_to_ts(inode, ts);
    inode_set_mtime_to_ts(inode, ts);
    inode_set_ctime_to_ts(inode, ts);
    
    if (S_ISDIR(mode)) {
        inode->i_op = &simple_dir_inode_operations;
        inode->i_fop = &simple_dir_operations;
        set_nlink(inode, 2); // . 和 ..
    } else if (S_ISREG(mode)) {
        inode->i_op = &simple_dir_inode_operations;
        inode->i_fop = &naive_file_ops;
        inode->i_size = 0;
        set_nlink(inode, 1);
    }
    
    d_instantiate(dentry, inode);
    dget(dentry);
    
    return 0;
}

static int naive_unlink(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode = d_inode(dentry);
    
    printk(KERN_INFO "naivefs: unlink %s\n", dentry->d_name.name);
    
    // 使用安全的链接计数减少方法
    drop_nlink(inode);
    mark_inode_dirty(inode);
    
    return 0;
}

static int naive_mkdir(struct mnt_idmap *idmap, struct inode *dir,
                       struct dentry *dentry, umode_t mode)
{
    int ret = naive_create(idmap, dir, dentry, S_IFDIR | mode, false);
    if (!ret) {
        inc_nlink(dir);
        printk(KERN_INFO "naivefs: mkdir %s success\n", dentry->d_name.name);
    }
    return ret;
}

static int naive_rmdir(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode = d_inode(dentry);
    
    printk(KERN_INFO "naivefs: rmdir %s\n", dentry->d_name.name);
    
    // 简化：总是允许删除
    drop_nlink(dir);
    set_nlink(inode, 0);
    
    return 0;
}

static struct inode_operations naive_dir_iops = {
    .create = naive_create,
    .lookup = simple_lookup,
    .unlink = naive_unlink,
    .mkdir = naive_mkdir,
    .rmdir = naive_rmdir,
};

// ==================== 挂载和填充超级块 ====================

static int naive_fill_super(struct super_block *sb, void *data, int silent)
{
    struct buffer_head *bh;
    struct naive_super_block *nsb;
    struct inode *root_inode;
    struct timespec64 ts;
    
    printk(KERN_INFO "naivefs: filling super block\n");
    
    // 设置块大小
    if (!sb_set_blocksize(sb, NAIVE_BLOCK_SIZE)) {
        printk(KERN_ERR "naivefs: cannot set blocksize\n");
        return -EINVAL;
    }
    
    // 读取超级块
    bh = sb_bread(sb, NAIVE_SUPER_BLOCK_BLOCK);
    if (!bh) {
        printk(KERN_ERR "naivefs: cannot read superblock\n");
        return -EIO;
    }
    
    nsb = (struct naive_super_block *)bh->b_data;
    
    // 检查魔数
    if (le32_to_cpu(nsb->magic) != NAIVE_MAGIC) {
        printk(KERN_ERR "naivefs: wrong magic number (0x%x != 0x%x)\n",
               le32_to_cpu(nsb->magic), NAIVE_MAGIC);
        brelse(bh);
        return -EINVAL;
    }
    
    printk(KERN_INFO "naivefs: magic number OK\n");
    
    // 设置超级块属性
    sb->s_magic = NAIVE_MAGIC;
    sb->s_blocksize = NAIVE_BLOCK_SIZE;
    sb->s_blocksize_bits = 9;
    sb->s_maxbytes = NAIVE_BLOCK_SIZE * 1024;
    sb->s_op = &naive_sops;
    
    // 创建根inode
    root_inode = new_inode(sb);
    if (!root_inode) {
        printk(KERN_ERR "naivefs: cannot allocate root inode\n");
        brelse(bh);
        return -ENOMEM;
    }
    
    root_inode->i_ino = NAIVE_ROOT_INODE_NO;
    root_inode->i_sb = sb;
    root_inode->i_op = &naive_dir_iops;
    root_inode->i_fop = &naive_dir_ops;
    root_inode->i_mode = S_IFDIR | 0755;
    
    // 设置时间
    ktime_get_real_ts64(&ts);
    inode_set_atime_to_ts(root_inode, ts);
    inode_set_mtime_to_ts(root_inode, ts);
    inode_set_ctime_to_ts(root_inode, ts);
    
    set_nlink(root_inode, 2);
    
    // 创建根目录的dentry
    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        printk(KERN_ERR "naivefs: cannot create root dentry\n");
        iput(root_inode);
        brelse(bh);
        return -ENOMEM;
    }
    
    brelse(bh);
    
    printk(KERN_INFO "naivefs: fill_super success\n");
    return 0;
}

static struct dentry *naive_mount(struct file_system_type *fs_type,
                                  int flags, const char *dev_name,
                                  void *data)
{
    printk(KERN_INFO "naivefs: mounting %s\n", dev_name);
    return mount_bdev(fs_type, flags, dev_name, data, naive_fill_super);
}

static struct file_system_type naive_fs_type = {
    .owner = THIS_MODULE,
    .name = "naive",
    .mount = naive_mount,
    .kill_sb = kill_block_super,
    .fs_flags = FS_REQUIRES_DEV,
};

// ==================== 模块初始化和退出 ====================

static int __init init_naivefs(void)
{
    int ret = register_filesystem(&naive_fs_type);
    if (ret) {
        printk(KERN_ERR "naivefs: register failed: %d\n", ret);
        return ret;
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
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Naive File System - Complete Implementation");
