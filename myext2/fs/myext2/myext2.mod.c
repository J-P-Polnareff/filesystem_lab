#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x5eca949d, "simple_dir_inode_operations" },
	{ 0x7a436b03, "new_inode" },
	{ 0x5c828131, "unregister_filesystem" },
	{ 0x5390c505, "d_make_root" },
	{ 0x31e313f6, "iput" },
	{ 0x4775ae65, "register_filesystem" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0xdafc406e, "kill_block_super" },
	{ 0x122c3a7e, "_printk" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x3495bbec, "simple_dir_operations" },
	{ 0x3e00db60, "__brelse" },
	{ 0xfdb5fcff, "set_nlink" },
	{ 0x9ec6ca96, "ktime_get_real_ts64" },
	{ 0x624e9b3a, "__bread_gfp" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xae817ade, "mount_bdev" },
	{ 0xe2fd41e5, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "C66EB2871380B6C18D96903");
