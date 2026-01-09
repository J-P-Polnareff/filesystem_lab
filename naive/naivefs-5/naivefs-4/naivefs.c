#include <linux/module.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/init.h>
#include <linux/types.h>

#define NAIVE_MAGIC 0x990717
#define NAIVE_BLOCK_SIZE 512
#define NAIVE_SUPER_BLOCK_BLOCK 1
#define NAIVE_ROOT_INODE_NO 1

struct naive_super_block {
    __le32 magic;
    __le32 inode_total;
    __le32 block_total;
    __le32 inode_table_block_no;
    __le32 data_block_no;
    __u8 padding[492];  // 512 - 5*4 = 492
};

static int naive_fill_super(struct super_block *sb, void *data, int silent)
{
    struct buffer_head *bh;
    struct naive_super_block *nsb;
    struct inode *root_inode;
    int i;
    
    printk(KERN_INFO "naivefs: filling super block\n");
    
    // 打印设备信息
    if (sb->s_bdev) {
        printk(KERN_INFO "naivefs: device size: %llu bytes\n",
               (unsigned long long)i_size_read(sb->s_bdev->bd_inode));
    }
    
    // 设置块大小（非常重要！）
    if (!sb_set_blocksize(sb, NAIVE_BLOCK_SIZE)) {
        printk(KERN_ERR "naivefs: cannot set blocksize to %d\n", NAIVE_BLOCK_SIZE);
        return -EINVAL;
    }
    
    // 读取超级块
    printk(KERN_INFO "naivefs: reading super block at block %d\n", NAIVE_SUPER_BLOCK_BLOCK);
    bh = sb_bread(sb, NAIVE_SUPER_BLOCK_BLOCK);
    if (!bh) {
        printk(KERN_ERR "naivefs: sb_bread failed for block %d\n", NAIVE_SUPER_BLOCK_BLOCK);
        return -EIO;
    }
    
    nsb = (struct naive_super_block *)bh->b_data;
    
    // 打印超级块前32字节用于调试
    printk(KERN_INFO "naivefs: super block first 32 bytes:");
    for (i = 0; i < 32; i++) {
        if (i % 16 == 0) printk(KERN_CONT "\n ");
        printk(KERN_CONT "%02x ", ((__u8 *)nsb)[i]);
    }
    printk(KERN_CONT "\n");
    
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
    sb->s_maxbytes = NAIVE_BLOCK_SIZE * NAIVE_BLOCK_SIZE;
    
    // 创建根inode
    root_inode = new_inode(sb);
    if (!root_inode) {
        printk(KERN_ERR "naivefs: new_inode failed\n");
        brelse(bh);
        return -ENOMEM;
    }
    
    root_inode->i_ino = NAIVE_ROOT_INODE_NO;
    root_inode->i_sb = sb;
    root_inode->i_op = &simple_dir_inode_operations;
    root_inode->i_fop = &simple_dir_operations;
    root_inode->i_mode = S_IFDIR | 0755;
    
    // 设置时间（适应不同内核版本）
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
    inode_set_atime_to_ts(root_inode, current_time(root_inode));
    inode_set_mtime_to_ts(root_inode, current_time(root_inode));
    inode_set_ctime_to_ts(root_inode, current_time(root_inode));
    #else
    root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = current_time(root_inode);
    #endif
    
    set_nlink(root_inode, 2);  // . 和 ..
    
    // 创建根目录的dentry
    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        printk(KERN_ERR "naivefs: d_make_root failed\n");
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
    printk(KERN_INFO "naivefs: mount called for %s\n", dev_name);
    return mount_bdev(fs_type, flags, dev_name, data, naive_fill_super);
}

static struct file_system_type naive_fs_type = {
    .owner = THIS_MODULE,
    .name = "naive",
    .mount = naive_mount,
    .kill_sb = kill_block_super,
    .fs_flags = FS_REQUIRES_DEV,
};

static int __init init_naivefs(void)
{
    int ret = register_filesystem(&naive_fs_type);
    if (ret)
        printk(KERN_ERR "naivefs: register failed, error %d\n", ret);
    else
        printk(KERN_INFO "naivefs: register success\n");
    return ret;
}

static void __exit exit_naivefs(void)
{
    unregister_filesystem(&naive_fs_type);
    printk(KERN_INFO "naivefs: unregistered\n");
}

module_init(init_naivefs);
module_exit(exit_naivefs);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Naive File System");
