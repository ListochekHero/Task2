#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/nospec.h>
#include <linux/kprobes.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/hw_breakpoint.h>
#include <linux/kallsyms.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <linux/ptrace.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

static unsigned long watch_address = 0x5560c2f3f2a0;

module_param(watch_address, ulong, S_IRUGO);

struct perf_event *__percpu *hbp_for_write_perf;

struct kobject *sysfs_kobj;

static void hbp_handler_for_write_perf(struct perf_event *bp,
									   struct perf_sample_data *data,
									   struct pt_regs *regs)
{
	// struct perf_event_attr attr = bp->attr;
	// struct hw_perf_event hw = bp->hw;
	// char hwirep[8];

	// if (hw.interrupts == HW_BREAKPOINT_W)
	// {
	// 	strcpy(hwirep, "_W_");
	// }
	// else if (hw.interrupts == HW_BREAKPOINT_R)
	// {
	// 	strcpy(hwirep, "_R_");
	// }
	// else
	// {
	// 	strcpy(hwirep, "___");
	// }
	printk(KERN_INFO "-----> %lx value is accessed <-----\n", watch_address);
	dump_stack();
	printk(KERN_INFO "-----> Dump stack from hbp_handler_for_write_perf <----- \n");
}

static int register_newhw_breakpoint(unsigned long watch_address)
{
	int ret;
	struct perf_event_attr attr_for_write_perf;
	hw_breakpoint_init(&attr_for_write_perf);
	attr_for_write_perf.bp_addr = watch_address;
	attr_for_write_perf.bp_len = HW_BREAKPOINT_LEN_4;
	attr_for_write_perf.bp_type = HW_BREAKPOINT_RW;

	hbp_for_write_perf = register_wide_hw_breakpoint(&attr_for_write_perf, hbp_handler_for_write_perf, NULL);
	if (IS_ERR((void __force *)hbp_for_write_perf))
	{
		ret = PTR_ERR((void __force *)hbp_for_write_perf);
		return PTR_ERR(hbp_for_write_perf);
	}
	printk(KERN_INFO "HW Breakpoint for %lx read/write installed\n", watch_address);
	return 0;
}

static void unregister_oldhw_breakpoint(void)
{
	if (hbp_for_write_perf)
	{
		unregister_wide_hw_breakpoint(hbp_for_write_perf);
		hbp_for_write_perf = NULL;
	}
}

static ssize_t watch_address_show(struct kobject *sysfs_kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%lx\n", watch_address);
}

static ssize_t watch_address_store(struct kobject *sysfs_kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	if (kstrtoul(buf, 0, &watch_address) == 0)
	{
		int ret; 
		unregister_oldhw_breakpoint();
		ret = register_newhw_breakpoint(watch_address);
		if (ret)
		{
			printk(KERN_INFO "Breakpoint registration failed\n");
			return ret;
		}
		return count;
	}
	return -EINVAL;
}

static struct kobj_attribute watch_address_attribute =
	__ATTR(watch_address, S_IRUGO | S_IWUSR, watch_address_show, watch_address_store);

static int __init hw_break_module_init(void)
{
	int ret;

	sysfs_kobj = kobject_create_and_add("hello_kernel_module", kernel_kobj);
	if (!sysfs_kobj)
	{
		return -ENOMEM;
	}

	ret = sysfs_create_file(sysfs_kobj, &watch_address_attribute.attr);
	if (ret)
	{
		kobject_put(sysfs_kobj);
		return ret;
	}

	ret = register_newhw_breakpoint(watch_address);
	if (ret)
	{
		printk(KERN_INFO "Breakpoint registration failed\n");
		return ret;
	}
	return 0;
}

static void __exit hw_break_module_exit(void)
{
	unregister_oldhw_breakpoint();
	kobject_put(sysfs_kobj);
	printk(KERN_INFO "HW Breakpoint for %lx read/write uninstalled\n", watch_address);
}

module_init(hw_break_module_init);
module_exit(hw_break_module_exit);

MODULE_LICENSE("GPL");

// static unsigned long watch_address = 0x55584782f2a0;

// module_param(watch_address, ulong, 0644);

// static int watchpoint_handler(struct kprobe *p, struct pt_regs *regs){
//     pr_info("watchpoint triggered at address: 0x%lu\n", watch_address);
//     dump_stack();

//     return 0;
// }

// static struct kprobe kp = {
//     .addr = (kprobe_opcode_t*)watch_address,
//     .pre_handler = watchpoint_handler,
// };

// static int __init watchpoint_init(void){
//     int ret = register_kprobe(&kp);
//     if(ret<0){
//         pr_err("Failed to register kprobe\n");
//         return ret;
//     }

//     pr_info("Watchppoint module loaded\n");
//     return 0;
// }

// static void __exit watchpoint_exit(void){
//     unregister_kprobe(&kp);
//     pr_info("Watchpoint module unloaded\n");
// }

// module_init(watchpoint_init);
// module_exit(watchpoint_exit);

// MODULE_LICENSE("GPL");