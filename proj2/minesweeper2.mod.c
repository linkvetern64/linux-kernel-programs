#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x78902ab7, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x63c0b650, __VMLINUX_SYMBOL_STR(param_ops_bool) },
	{ 0xe7051e03, __VMLINUX_SYMBOL_STR(cs421net_disable) },
	{ 0x266fd8a3, __VMLINUX_SYMBOL_STR(misc_deregister) },
	{ 0xf20dabd8, __VMLINUX_SYMBOL_STR(free_irq) },
	{ 0x999e8297, __VMLINUX_SYMBOL_STR(vfree) },
	{ 0xdf26a863, __VMLINUX_SYMBOL_STR(misc_register) },
	{ 0xd6b8e852, __VMLINUX_SYMBOL_STR(request_threaded_irq) },
	{ 0xf98f620c, __VMLINUX_SYMBOL_STR(cs421net_enable) },
	{ 0xd6ee688f, __VMLINUX_SYMBOL_STR(vmalloc) },
	{ 0xad15acf0, __VMLINUX_SYMBOL_STR(cs421net_get_data) },
	{ 0x9166fada, __VMLINUX_SYMBOL_STR(strncpy) },
	{ 0x4ca9669f, __VMLINUX_SYMBOL_STR(scnprintf) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x6c1988ba, __VMLINUX_SYMBOL_STR(_raw_spin_unlock) },
	{ 0xb5419b40, __VMLINUX_SYMBOL_STR(_copy_from_user) },
	{ 0xdbbee5cd, __VMLINUX_SYMBOL_STR(_raw_spin_lock) },
	{ 0xdb7305a1, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0x79aa04a2, __VMLINUX_SYMBOL_STR(get_random_bytes) },
	{ 0x23b38720, __VMLINUX_SYMBOL_STR(remap_pfn_range) },
	{ 0x3744cf36, __VMLINUX_SYMBOL_STR(vmalloc_to_pfn) },
	{ 0x4971e093, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0xc671e369, __VMLINUX_SYMBOL_STR(_copy_to_user) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=xt_cs421net";


MODULE_INFO(srcversion, "576B47528DAB1D91F33000D");