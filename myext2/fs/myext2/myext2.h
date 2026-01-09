#ifndef _MYEXT2_H
#define _MYEXT2_H

#include <linux/types.h>
#include <linux/fs.h>

#define MYEXT2_SUPER_MAGIC 0x6666

/* 简化版的MYEXT2超级块结构 */
struct myext2_super_block {
    __le32  s_inodes_count;
    __le32  s_blocks_count;
    __le32  s_r_blocks_count;
    __le32  s_free_blocks_count;
    __le32  s_free_inodes_count;
    __le32  s_first_data_block;
    __le32  s_log_block_size;
    __le32  s_blocks_per_group;
    __le32  s_inodes_per_group;
    __le32  s_mtime;
    __le32  s_wtime;
    __le16  s_mnt_count;
    __le16  s_max_mnt_count;
    __le16  s_magic;        /* 魔数：0x6666 */
    __le16  s_state;
    __le16  s_errors;
    __u8    padding[1000];  /* 填充到1024字节 */
};

#endif /* _MYEXT2_H */
