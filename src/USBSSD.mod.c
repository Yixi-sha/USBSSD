#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

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
	{ 0xeeab4c1e, "module_layout" },
	{ 0x3c04396, "alloc_pages_current" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0xce43c9e0, "blk_cleanup_queue" },
	{ 0xd1333ce, "blk_mq_start_request" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0xc6f46339, "init_timer_key" },
	{ 0x720a27a7, "__register_blkdev" },
	{ 0x409bcb62, "mutex_unlock" },
	{ 0x97651e6c, "vmemmap_base" },
	{ 0x1a81ff9c, "pv_ops" },
	{ 0xe554678a, "kthread_create_on_node" },
	{ 0x15ba50a6, "jiffies" },
	{ 0xdf04b5eb, "__alloc_disk_node" },
	{ 0xd3753614, "blk_mq_init_queue" },
	{ 0x4d497d69, "current_task" },
	{ 0x977f511b, "__mutex_init" },
	{ 0xc5850110, "printk" },
	{ 0xf437babb, "kthread_stop" },
	{ 0xd93161ac, "del_gendisk" },
	{ 0x8bb9aed, "blk_mq_free_tag_set" },
	{ 0x2ab7989d, "mutex_lock" },
	{ 0xb9e21922, "set_capacity" },
	{ 0x24d273d1, "add_timer" },
	{ 0x18a0cfae, "blk_update_request" },
	{ 0xb5a459dc, "unregister_blkdev" },
	{ 0x7cd8d75e, "page_offset_base" },
	{ 0xc959d152, "__stack_chk_fail" },
	{ 0x1000e51, "schedule" },
	{ 0x4ec1dfb7, "put_disk" },
	{ 0x8427cc7b, "_raw_spin_lock_irq" },
	{ 0x2fbefc53, "wake_up_process" },
	{ 0x8be020a8, "blk_mq_alloc_tag_set" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x4c3b55b5, "blk_mq_end_request" },
	{ 0x4302d0eb, "free_pages" },
	{ 0xb3f7646e, "kthread_should_stop" },
	{ 0x37a0cba, "kfree" },
	{ 0x69acdf38, "memcpy" },
	{ 0x6236144d, "device_add_disk" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "02A68F8F616458437DF31D5");
