#include "naivefs.h"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>

/* 挂载函数声明（放在文件开头） */
static struct dentry *naive_mount(struct file_system_type *fs_type,
                                 int flags, const char *dev_name,
                                 void *data);

/* 文件系统类型定义 */
static struct file_system_type naive_fs_type = {
    .owner      = THIS_MODULE,
    .name       = "naive",
    .mount      = naive_mount,
    .kill_sb    = kill_block_super,
    .fs_flags   = FS_REQUIRES_DEV,
};

/* 挂载函数实现 */
static struct dentry *naive_mount(struct file_system_type *fs_type,
                                 int flags, const char *dev_name,
                                 void *data)
{
    printk(KERN_INFO "naivefs: mount called for %s\n", dev_name);
    return mount_bdev(fs_type, flags, dev_name, data, naive_fill_super);
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
MODULE_DESCRIPTION("Naive File System");
MODULE_VERSION("1.0");
