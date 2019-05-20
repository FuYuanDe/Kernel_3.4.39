#include <linux/init.h>
#include <linux/timer.h>
#include <linux/kern_watchdog.h>
#include <asm/io.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
#include <mach/platform.h>


#define WDOG0_CTRL_REG     (SUNXI_TIMER_VBASE + 0xB0) 
#define WDOG0_CFG_REG    (SUNXI_TIMER_VBASE + 0xB4)
#define WDOG0_MODE_REG     (SUNXI_TIMER_VBASE + 0xB8)

struct timer_list wdtk_time_a;
unsigned int kerm_wdt_status = 0;
spinlock_t	kern_wdt_lock;

/**启动看门狗，定时10s*/
void kern_watchdog_enable(void)
{    
    writel(0x1, WDOG0_CFG_REG);
    writel(0x80, WDOG0_MODE_REG);
    writel(readl(WDOG0_MODE_REG) |(1<<0), WDOG0_MODE_REG);
    
	writel((0xA57 << 1) | (1 << 0), WDOG0_CTRL_REG);   
	printk (KERN_ERR "watchdog enable\n");
}

void kern_watchdog_reload(void)
{
	writel((0xA57 << 1) | (1 << 0), WDOG0_CTRL_REG);   
}


static void watchdog_kernel_timer(unsigned long data)
{	
	wdtk_time_a.expires  = jiffies +4*HZ;
	add_timer(&wdtk_time_a);
	kern_watchdog_reload();	
}

static void kern_init_timer(void)
{
	unsigned long data = 0;
	init_timer(&wdtk_time_a);
	wdtk_time_a.expires  = jiffies + 4*HZ;
	wdtk_time_a.function = watchdog_kernel_timer;
	wdtk_time_a.data = data;
	add_timer(&wdtk_time_a);
}

void kern_watchdog_disable(void)
{
	writel(readl(WDOG0_MODE_REG) & 0xFE, WDOG0_MODE_REG);
}

void wdt_del_timer(void)
{
	spin_lock(&kern_wdt_lock);	
	del_timer(&wdtk_time_a);	
	spin_unlock(&kern_wdt_lock);
}
void wdt_del_timer_and_disable(void)
{
	spin_lock(&kern_wdt_lock);

	if(!kerm_wdt_status)
	{
		del_timer(&wdtk_time_a);
		kern_watchdog_disable();
		kerm_wdt_status=1;
    }
    spin_unlock(&kern_wdt_lock);
}
EXPORT_SYMBOL(wdt_del_timer);
EXPORT_SYMBOL(wdt_del_timer_and_disable);

static int wdts_proc_show(struct seq_file *m, void *v)
{
	char wdts_status[4];
	if(kerm_wdt_status)
		strncpy(wdts_status, "off", sizeof(wdts_status));
	else
		strncpy(wdts_status, "on", sizeof(wdts_status));
	seq_printf(m, "%s\n",wdts_status);
	return 0;
}
static int wdts_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, wdts_proc_show, NULL);
}

static const struct file_operations wdts_proc_fops = {
	.open		= wdts_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init wdts_proc_init(void)
{	
	proc_create("wdts_timer_status", 0, NULL, &wdts_proc_fops);
	return 0;
}
static int __init watchdog_kernel_init(void)
{
	wdts_proc_init();
	spin_lock_init(&kern_wdt_lock);
	kern_init_timer();
	kern_watchdog_enable();
	printk("watchdog_kernel_init START\n");
    return 0;
}

pure_initcall(watchdog_kernel_init);
