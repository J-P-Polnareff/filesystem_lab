#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>

#define NAIVE_MAGIC 0x990717

// 定义文件系统类型结构
static struct file_system_type naive_fs_type = {
    .owner = THIS_MODULE,
    .name = "naive",
    .fs_flags = FS_REQUIRES_DEV,
};

// 模块初始化 - 注册文件系统
static int __init init_naivefs(void)
{
    int ret = register_filesystem(&naive_fs_type);
    if (ret != 0) {
        pr_err("Failed to register filesystem\n");
        return ret;
    }
    printk(KERN_INFO "naivefs: filesystem registered\n");
    return 0;
}

// 模块退出 - 注销文件系统
static void __exit exit_naivefs(void)
{
    unregister_filesystem(&naive_fs_type);
    printk(KERN_INFO "naivefs: filesystem unregistered\n");
}

module_init(init_naivefs);
module_exit(exit_naivefs);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Naive File System");
