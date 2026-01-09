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

// ==================== 全局文件列表（内存中） ====================
struct naive_file_entry {
    char name[NAIVE_MAX_FILENAME_LEN];
    int inode_no;
    int is_dir;
    struct list_head list;
};

static LIST_HEAD(file_list);
static int next_inode_no = 100; // 从100开始分配inode号

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
    const char *content = "Hello from naive filesystem!\nThis is a test file.\n";
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
    struct naive_file_entry *entry;
    
    printk(KERN_INFO "naivefs: readdir inode %lu, pos %lld\n",
           inode->i_ino, ctx->pos);
    
    // 首先输出 . 和 ..
    if (ctx->pos == 0) {
        if (!dir_emit(ctx, ".", 1, inode->i_ino, DT_DIR))
            return 0;
        ctx->pos++;
    }
    
    if (ctx->pos == 1) {
        if (!dir_emit(ctx, "..", 2, inode->i_ino, DT_DIR))
            return 0;
        ctx->pos++;
    }
    
    // 输出内存中的文件列表
    list_for_each_entry(entry, &file_list, list) {
        if (ctx->pos < 2) {
            ctx->pos++;
            continue;
        }
        
        if (!dir_emit(ctx, entry->name, strlen(entry->name),
                      entry->inode_no, entry->is_dir ? DT_DIR : DT_REG))
            return 0;
        
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
    struct naive_file_entry *entry;
    
    printk(KERN_INFO "naivefs: create %s (mode: %o)\n",
           dentry->d_name.name, mode);
    
    // 检查是否已存在
    list_for_each_entry(entry, &file_list, list) {
        if (strcmp(entry->name, dentry->d_name.name) == 0) {
            printk(KERN_WARNING "naivefs: file %s already exists\n", 
                   dentry->d_name.name);
            return -EEXIST;
        }
    }
    
    // 创建新的inode
    inode = new_inode(dir->i_sb);
    if (!inode)
        return -ENOMEM;
    
    inode->i_ino = next_inode_no++;
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
    
    // 添加到内存文件列表
    entry = kmalloc(sizeof(*entry), GFP_KERNEL);
    if (entry) {
        strncpy(entry->name, dentry->d_name.name, NAIVE_MAX_FILENAME_LEN-1);
        entry->name[NAIVE_MAX_FILENAME_LEN-1] = '\0';
        entry->inode_no = inode->i_ino;
        entry->is_dir = S_ISDIR(mode);
        INIT_LIST_HEAD(&entry->list);
        list_add_tail(&entry->list, &file_list);
    }
    
    d_instantiate(dentry, inode);
    dget(dentry);
    
    return 0;
}

static int naive_unlink(struct inode *dir, struct dentry *dentry)
{
    struct naive_file_entry *entry, *tmp;
    
    printk(KERN_INFO "naivefs: unlink %s\n", dentry->d_name.name);
    
    // 从内存文件列表中删除
    list_for_each_entry_safe(entry, tmp, &file_list, list) {
        if (strcmp(entry->name, dentry->d_name.name) == 0) {
            list_del(&entry->list);
            kfree(entry);
            break;
        }
    }
    
    return 0;
}

static int naive_mkdir(struct mnt_idmap *idmap, struct inode *dir,
                       struct dentry *dentry, umode_t mode)
{
    return naive_create(idmap, dir, dentry, S_IFDIR | mode, false);
}

static int naive_rmdir(struct inode *dir, struct dentry *dentry)
{
    return naive_unlink(dir, dentry);
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
    
    // 清空内存文件列表
    {
        struct naive_file_entry *entry, *tmp;
        list_for_each_entry_safe(entry, tmp, &file_list, list) {
            list_del(&entry->list);
            kfree(entry);
        }
        next_inode_no = 100;
    }
    
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
    root_inode->i_op = &naive_dir_iops;
    root_inode->i_fop = &naive_dir_ops;
    root_inode->i_mode = S_IFDIR | 0755;
    
    // 正确设置所有者
    inode_init_owner(&nop_mnt_idmap, root_inode, NULL, S_IFDIR | 0755);
    
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
    
    // 预创建几个示例文件和目录用于测试
    {
        struct dentry *d;
        struct qstr name;
        
        // 创建一个示例文件
        name.name = "README";
        name.len = strlen(name.name);
        name.hash = full_name_hash(sb->s_root, name.name, name.len);
        d = d_alloc(sb->s_root, &name);
        if (d) {
            naive_create(&nop_mnt_idmap, root_inode, d, S_IFREG | 0644, false);
            dput(d);
        }
        
        // 创建一个示例目录
        name.name = "example";
        name.len = strlen(name.name);
        name.hash = full_name_hash(sb->s_root, name.name, name.len);
        d = d_alloc(sb->s_root, &name);
        if (d) {
            naive_mkdir(&nop_mnt_idmap, root_inode, d, 0755);
            dput(d);
        }
    }
    
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
    // 清理内存文件列表
    struct naive_file_entry *entry, *tmp;
    list_for_each_entry_safe(entry, tmp, &file_list, list) {
        list_del(&entry->list);
        kfree(entry);
    }
    
    unregister_filesystem(&naive_fs_type);
    printk(KERN_INFO "naivefs: module unloaded\n");
}

module_init(init_naivefs);
module_exit(exit_naivefs);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Naive File System - Final Implementation");
