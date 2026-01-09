#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>

/* 定义文件系统类型 */
static struct file_system_type naive_fs_type = {
    .owner = THIS_MODULE,
    .name = "naive",
    .mount = naive_mount,
    .kill_sb = kill_block_super,
    .fs_flags = FS_REQUIRES_DEV,
};

/* 挂载函数 */
static struct dentry *naive_mount(struct file_system_type *fs_type,
                                 int flags, const char *dev_name,
                                 void *data)
{
    printk(KERN_INFO "naivefs: mount called for %s\n", dev_name);
    return mount_bdev(fs_type, flags, dev_name, data, naive_fill_super);
}

/* 填充超级块（简化版） */
static int naive_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *root_inode;
    int ret = 0;
    
    printk(KERN_INFO "naivefs: filling super block\n");
    
    /* 设置超级块属性 */
    sb->s_magic = 0x990717;
    sb->s_blocksize = 512;
    sb->s_blocksize_bits = 9;
    sb->s_maxbytes = 512 * 8; /* 4KB */
    
    /* 创建根inode */
    root_inode = new_inode(sb);
    if (!root_inode) {
        ret = -ENOMEM;
        goto out;
    }
    
    root_inode->i_ino = 1;
    root_inode->i_sb = sb;
    root_inode->i_op = &simple_dir_inode_operations;
    root_inode->i_fop = &simple_dir_operations;
    root_inode->i_mode = S_IFDIR | 0755;
    
    /* 设置时间 */
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    inode_set_atime_to_ts(root_inode, ts);
    inode_set_mtime_to_ts(root_inode, ts);
    inode_set_ctime_to_ts(root_inode, ts);
    
    set_nlink(root_inode, 2);
    
    /* 创建根dentry */
    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        ret = -ENOMEM;
        goto out;
    }
    
    printk(KERN_INFO "naivefs: fill_super success\n");
    return 0;
    
out:
    return ret;
}

/* 模块初始化 */
static int __init init_naivefs(void)
{
    int ret = register_filesystem(&naive_fs_type);
    if (ret)
        printk(KERN_ERR "naivefs: register failed, error %d\n", ret);
    else
        printk(KERN_INFO "naivefs: register success\n");
    return ret;
}

/* 模块退出 */
static void __exit exit_naivefs(void)
{
    unregister_filesystem(&naive_fs_type);
    printk(KERN_INFO "naivefs: unregistered\n");
}

module_init(init_naivefs);
module_exit(exit_naivefs);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chen Sihan");
MODULE_DESCRIPTION("Naive File System - Simplified");
MODULE_VERSION("1.0");
