/*
 * [http://www.tldp.org/LDP/lkmpg/2.6/html/lkmpg.html#AEN189 The Linux Kernel Module Programming Guide]
 * https://stackoverflow.com/questions/16920238/reliability-of-linux-kernel-add-timer-at-resolution-of-one-jiffy/17055867#17055867
 * https://stackoverflow.com/questions/8516021/proc-create-example-for-kernel-module/18924359#18924359
 * http://lxr.free-electrons.com/source/samples/hw_breakpoint/data_breakpoint.c
 */


#include <linux/module.h>   /* Needed by all modules */
#include <linux/kernel.h>   /* Needed for KERN_INFO */
#include <linux/init.h>     /* Needed for the macros */
#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/proc_fs.h>  /* /proc entry */
#include <linux/seq_file.h> /* /proc entry */
#define ARRSIZE 5
#define MAXRUNS 2*ARRSIZE

#include <linux/hrtimer.h>

#define HWDEBUG_STACK 1

#if (HWDEBUG_STACK == 1)
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>

struct perf_event * __percpu *sample_hbp;
static char ksym_name[KSYM_NAME_LEN] = "testhrarr_arr";
module_param_string(ksym, ksym_name, KSYM_NAME_LEN, S_IRUGO);
MODULE_PARM_DESC(ksym, "Kernel symbol to monitor; this module will report any"
      " write operations on the kernel symbol");
#endif

static volatile int testhrarr_runcount = 0;
static volatile int testhrarr_isRunning = 0;

static unsigned long period_ms;
static unsigned long period_ns;
static ktime_t ktime_period_ns;
static struct hrtimer my_hrtimer;

static int* testhrarr_arr;
static int* testhrarr_arr_first;

static enum hrtimer_restart testhrarr_timer_function(struct hrtimer *timer)
{
  unsigned long tjnow;
  ktime_t kt_now;
  int ret_overrun;

  printk(KERN_INFO
    " %s: testhrarr_runcount %d \n",
    __func__, testhrarr_runcount);

  if (testhrarr_runcount < MAXRUNS) {
    tjnow = jiffies;
    kt_now = hrtimer_cb_get_time(&my_hrtimer);
    ret_overrun = hrtimer_forward(&my_hrtimer, kt_now, ktime_period_ns);
    printk(KERN_INFO
      " testhrarr jiffies %lu ; ret: %d ; ktnsec: %lld\n",
      tjnow, ret_overrun, ktime_to_ns(kt_now));
    testhrarr_arr[(testhrarr_runcount % ARRSIZE)] += testhrarr_runcount;
    testhrarr_runcount++;
    return HRTIMER_RESTART;
  }
  else {
    int i;
    testhrarr_isRunning = 0;
    // do not use KERN_DEBUG etc, if printk buffering until newline is desired!
    printk("testhrarr_arr [ ");
    for(i=0; i<ARRSIZE; i++) {
      printk("%d, ", testhrarr_arr[i]);
    }
    printk("]\n");
    return HRTIMER_NORESTART;
  }
}

static void testhrarr_startup(void)
{
  if (testhrarr_isRunning == 0) {
    testhrarr_isRunning = 1;
    testhrarr_runcount = 0;
    testhrarr_arr[0] = 0; //just the first element
    hrtimer_start(&my_hrtimer, ktime_period_ns, HRTIMER_MODE_REL);
  }
}


static int testhrarr_proc_show(struct seq_file *m, void *v) {
  if (testhrarr_isRunning == 0) {
    seq_printf(m, "testhrarr proc: startup\n");
    testhrarr_startup();
  } else {
    seq_printf(m, "testhrarr proc: (is running, %d)\n", testhrarr_runcount);
  }
  return 0;
}

static int testhrarr_proc_open(struct inode *inode, struct  file *file) {
  return single_open(file, testhrarr_proc_show, NULL);
}

static const struct file_operations testhrarr_proc_fops = {
  .owner = THIS_MODULE,
  .open = testhrarr_proc_open,
  .read = seq_read,
  .llseek = seq_lseek,
  .release = single_release,
};


#if (HWDEBUG_STACK == 1)
static void sample_hbp_handler(struct perf_event *bp,
             struct perf_sample_data *data,
             struct pt_regs *regs)
{
  printk(KERN_INFO "%s value is changed\n", ksym_name);
  dump_stack();
  printk(KERN_INFO "Dump stack from sample_hbp_handler\n");
}
#endif

static int __init testhrarr_init(void)
{
  struct timespec tp_hr_res;
  #if (HWDEBUG_STACK == 1)
  struct perf_event_attr attr;
  #endif

  period_ms = 1000/HZ;
  hrtimer_get_res(CLOCK_MONOTONIC, &tp_hr_res);
  printk(KERN_INFO
    "Init testhrarr: %d ; HZ: %d ; 1/HZ (ms): %ld ; hrres: %lld.%.9ld\n",
               testhrarr_runcount,      HZ,        period_ms, (long long)tp_hr_res.tv_sec, tp_hr_res.tv_nsec );

  testhrarr_arr = (int*)kcalloc(ARRSIZE, sizeof(int), GFP_ATOMIC);
  testhrarr_arr_first = &testhrarr_arr[0];

  hrtimer_init(&my_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  my_hrtimer.function = &testhrarr_timer_function;
  period_ns = period_ms*( (unsigned long)1E6L );
  ktime_period_ns = ktime_set(0,period_ns);

  printk(KERN_INFO
    " Addresses: _runcount 0x%p ; _arr 0x%p ; _arr[0] 0x%p (0x%p) ; _timer_function 0x%p ; my_hrtimer 0x%p; my_hrt.f 0x%p\n",
    &testhrarr_runcount, &testhrarr_arr, &(testhrarr_arr[0]), testhrarr_arr_first, &testhrarr_timer_function, &my_hrtimer, &my_hrtimer.function);


  proc_create("testhrarr_proc", 0, NULL, &testhrarr_proc_fops);


  #if (HWDEBUG_STACK == 1)
  hw_breakpoint_init(&attr);
  if (strcmp(ksym_name, "testhrarr_arr_first") == 0) {
    // just for testhrarr_arr_first - interpret the found symbol address
    // as int*, and dereference it to get the "real" address it points to
    attr.bp_addr = *((int*)kallsyms_lookup_name(ksym_name));
  } else {
    // the usual - address is kallsyms_lookup_name result
    attr.bp_addr = kallsyms_lookup_name(ksym_name);
  }
  attr.bp_len = HW_BREAKPOINT_LEN_1;
  attr.bp_type = HW_BREAKPOINT_W ; //| HW_BREAKPOINT_R;

  sample_hbp = register_wide_hw_breakpoint(&attr, (perf_overflow_handler_t)sample_hbp_handler);
  if (IS_ERR((void __force *)sample_hbp)) {
    int ret = PTR_ERR((void __force *)sample_hbp);
    printk(KERN_INFO "Breakpoint registration failed\n");
    return ret;
  }

  // explicit cast needed to show 64-bit bp_addr as 32-bit address
  // https://stackoverflow.com/questions/11796909/how-to-resolve-cast-to-pointer-from-integer-of-different-size-warning-in-c-co/11797103#11797103
  printk(KERN_INFO "HW Breakpoint for %s write installed (0x%p)\n", ksym_name, (void*)(uintptr_t)attr.bp_addr);
  #endif

  return 0;
}

static void __exit testhrarr_exit(void)
{
  int ret_cancel = 0;
  kfree(testhrarr_arr);
  while( hrtimer_callback_running(&my_hrtimer) ) {
    ret_cancel++;
  }
  if (ret_cancel != 0) {
    printk(KERN_INFO " testhrarr Waited for hrtimer callback to finish (%d)\n", ret_cancel);
  }
  if (hrtimer_active(&my_hrtimer) != 0) {
    ret_cancel = hrtimer_cancel(&my_hrtimer);
    printk(KERN_INFO " testhrarr active hrtimer cancelled: %d (%d)\n", ret_cancel, testhrarr_runcount);
  }
  if (hrtimer_is_queued(&my_hrtimer) != 0) {
    ret_cancel = hrtimer_cancel(&my_hrtimer);
    printk(KERN_INFO " testhrarr queued hrtimer cancelled: %d (%d)\n", ret_cancel, testhrarr_runcount);
  }
  remove_proc_entry("testhrarr_proc", NULL);
  #if (HWDEBUG_STACK == 1)
  unregister_wide_hw_breakpoint(sample_hbp);
  printk(KERN_INFO "HW Breakpoint for %s write uninstalled\n", ksym_name);
  #endif
  printk(KERN_INFO "Exit testhrarr\n");
}

module_init(testhrarr_init);
module_exit(testhrarr_exit);

MODULE_LICENSE("GPL");