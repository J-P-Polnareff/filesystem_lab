#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/statfs.h>
#include <linux/time64.h>

#define MYEXT2_SUPER_MAGIC 0x6666

/* 填充超级块函数 */
static int myext2_fill_super(struct super_block *sb, void *data, int silent)
{
    struct buffer_head *bh;
    struct inode *root;
    unsigned short magic;
    int i;
    
    printk(KERN_INFO "MYEXT2: filling super block\n");
    
    /* 读取超级块（块1，偏移1024字节） */
    bh = sb_bread(sb, 1);
    if (!bh) {
        printk(KERN_ERR "MYEXT2: unable to read superblock\n");
        return -EIO;
    }
    
    /* 调试：打印前64字节 */
    printk(KERN_INFO "MYEXT2: super block first 64 bytes:");
    for (i = 0; i < 64; i++) {
        if (i % 16 == 0) printk(KERN_CONT "\n  ");
        printk(KERN_CONT "%02x ", ((__u8 *)bh->b_data)[i]);
    }
    printk(KERN_CONT "\n");
    
    /* 检查魔数（在超级块偏移56字节处） */
    magic = *(__le16 *)(bh->b_data + 56);
    printk(KERN_INFO "MYEXT2: read magic: 0x%04x\n", le16_to_cpu(magic));
    printk(KERN_INFO "MYEXT2: expected magic: 0x%04x\n", MYEXT2_SUPER_MAGIC);
    
    if (le16_to_cpu(magic) != MYEXT2_SUPER_MAGIC) {
        printk(KERN_ERR "MYEXT2: wrong magic number (0x%04x != 0x%04x)\n",
               le16_to_cpu(magic), MYEXT2_SUPER_MAGIC);
        brelse(bh);
        return -EINVAL;
    }
    
    printk(KERN_INFO "MYEXT2: magic number OK\n");
    
    /* 设置超级块属性 */
    sb->s_magic = MYEXT2_SUPER_MAGIC;
    sb->s_blocksize = 1024;  /* 默认块大小 */
    sb->s_blocksize_bits = 10;
    sb->s_maxbytes = MAX_LFS_FILESIZE;
    
    /* 创建根inode */
    root = new_inode(sb);
    if (!root) {
        printk(KERN_ERR "MYEXT2: new_inode failed\n");
        brelse(bh);
        return -ENOMEM;
    }
    
    root->i_ino = 2;  /* 根目录inode通常是2 */
    root->i_sb = sb;
    root->i_op = &simple_dir_inode_operations;
    root->i_fop = &simple_dir_operations;
    root->i_mode = S_IFDIR | 0755;
    
    /* 设置时间 */
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    inode_set_atime_to_ts(root, ts);
    inode_set_mtime_to_ts(root, ts);
    inode_set_ctime_to_ts(root, ts);
    
    set_nlink(root, 2);  /* . 和 .. */
    
    /* 创建根dentry */
    sb->s_root = d_make_root(root);
    if (!sb->s_root) {
        printk(KERN_ERR "MYEXT2: d_make_root failed\n");
        iput(root);
        brelse(bh);
        return -ENOMEM;
    }
    
    brelse(bh);
    printk(KERN_INFO "MYEXT2: fill_super success\n");
    return 0;
}

/* 挂载函数 */
static struct dentry *myext2_mount(struct file_system_type *fs_type,
                                  int flags, const char *dev_name,
                                  void *data)
{
    printk(KERN_INFO "MYEXT2: mount called for %s\n", dev_name);
    return mount_bdev(fs_type, flags, dev_name, data, myext2_fill_super);
}

/* 文件系统类型定义 */
static struct file_system_type myext2_fs_type = {
    .owner      = THIS_MODULE,
    .name       = "myext2",
    .mount      = myext2_mount,
    .kill_sb    = kill_block_super,
    .fs_flags   = FS_REQUIRES_DEV,
};

/* 模块初始化函数 */
static int __init init_myext2_fs(void)
{
    int ret = register_filesystem(&myext2_fs_type);
    if (ret) {
        printk(KERN_ERR "MYEXT2: register failed, error %d\n", ret);
    } else {
        printk(KERN_INFO "MYEXT2: register success (magic=0x%04x)\n", 
               MYEXT2_SUPER_MAGIC);
    }
    return ret;
}

/* 模块退出函数 */
static void __exit exit_myext2_fs(void)
{
    unregister_filesystem(&myext2_fs_type);
    printk(KERN_INFO "MYEXT2: unregistered\n");
}

module_init(init_myext2_fs)
module_exit(exit_myext2_fs)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dennis");
MODULE_DESCRIPTION("MYEXT2 Filesystem (Simplified)");
