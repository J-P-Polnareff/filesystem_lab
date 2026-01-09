#include <linux/module.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/statfs.h>
#include <linux/time.h>

#define NAIVE_MAGIC 0x990717
#define NAIVE_BLOCK_SIZE 512
#define NAIVE_SUPER_BLOCK_BLOCK 1
#define NAIVE_ROOT_INODE_NO 1

// 简化的超级块结构
struct naive_super_block {
    __le32 magic;
    __le32 block_total;
    __le32 inode_total;
    __le32 inode_table_block_no;
    __le32 data_block_no;
    __u8 padding[492];
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
    buf->f_namelen = 30;
    
    return 0;
}

static struct super_operations naive_sops = {
    .statfs = naive_statfs,
    .drop_inode = generic_drop_inode,
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
    
    // 打印调试信息
    printk(KERN_INFO "naivefs: raw magic bytes: %02x %02x %02x %02x\n",
           ((__u8 *)&nsb->magic)[0], ((__u8 *)&nsb->magic)[1],
           ((__u8 *)&nsb->magic)[2], ((__u8 *)&nsb->magic)[3]);
    printk(KERN_INFO "naivefs: read magic: 0x%x\n", le32_to_cpu(nsb->magic));
    printk(KERN_INFO "naivefs: expected magic: 0x%x\n", NAIVE_MAGIC);
    
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
    
    // 使用简单的目录操作（避免函数签名问题）
    root_inode->i_op = &simple_dir_inode_operations;
    root_inode->i_fop = &simple_dir_operations;
    root_inode->i_mode = S_IFDIR | 0755;
    
    // 设置时间（内核6.8.0的API）
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
MODULE_DESCRIPTION("Naive File System");
