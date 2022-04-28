#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(.gnu.linkonce.this_module) = {
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
__used __section(__versions) = {
	{ 0x581b1365, "module_layout" },
	{ 0x2c90ed6f, "alloc_pages_current" },
	{ 0x2d3385d3, "system_wq" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x1fdc7df2, "_mcount" },
	{ 0x2599e24, "blk_cleanup_queue" },
	{ 0xf721c4a0, "blk_mq_start_request" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0xc6f46339, "init_timer_key" },
	{ 0x409bcb62, "mutex_unlock" },
	{ 0xc0d6bdc4, "kthread_create_on_node" },
	{ 0x15ba50a6, "jiffies" },
	{ 0xf31a2e49, "__alloc_disk_node" },
	{ 0x7de00619, "blk_mq_init_queue" },
	{ 0x977f511b, "__mutex_init" },
	{ 0xc5850110, "printk" },
	{ 0xddf638cd, "kthread_stop" },
	{ 0xbba3d74e, "del_gendisk" },
	{ 0x42f567b8, "blk_mq_free_tag_set" },
	{ 0x2ab7989d, "mutex_lock" },
	{ 0x71a50dbc, "register_blkdev" },
	{ 0x24d273d1, "add_timer" },
	{ 0xfeef3c1a, "blk_update_request" },
	{ 0xb5a459dc, "unregister_blkdev" },
	{ 0xc7ef8c65, "__free_pages" },
	{ 0x9c1e5bf5, "queued_spin_lock_slowpath" },
	{ 0xdecd0b29, "__stack_chk_fail" },
	{ 0x1000e51, "schedule" },
	{ 0x536bbf76, "put_disk" },
	{ 0xf3ecac14, "cpu_hwcap_keys" },
	{ 0x758f41f7, "wake_up_process" },
	{ 0x974feb67, "blk_mq_alloc_tag_set" },
	{ 0xda51ce09, "blk_mq_end_request" },
	{ 0xb3f7646e, "kthread_should_stop" },
	{ 0x37a0cba, "kfree" },
	{ 0x4829a47e, "memcpy" },
	{ 0xc5b6f236, "queue_work_on" },
	{ 0xb90971d9, "device_add_disk" },
	{ 0x14b89635, "arm64_const_caps_ready" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "E0A6157740989DE16D56206");
