#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kprobes.h>

static unsigned long wath_address = 0x0;

module_param(wathc_address, ulong, 0644);

static int watchpoint_handler(struct kprobe *p, struct pt_regs *regs){
    pr_info("watchpoint triggered at address: 0x%ln\n", wath_address);
    dump_stack();

    return 0;
}

static struct kprobe kp = {
    .symbol_name = "function_to_watch",
    .pre_handler = watchpoint_handler,
};

static int __init watchpoint_init(void){
    int ret = register_kprobe(&kp);
    if(ret<0){
        pr_err("Failed to register kprobe\n");
        return ret;
    }

    pr_info("Watchppoint module loaded\n");
    return 0;
}

static void __exit watchpoint_exit(void){
    unregister_kprobe(&kp);
    pr_info("Watchpoint module unloaded\n");
}

module_init(watchpoint_init);
module_exit(watchpoint_exit);

MODULE_LICENSE("GPL")