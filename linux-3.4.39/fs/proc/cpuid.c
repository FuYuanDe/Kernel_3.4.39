#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>

extern int board_cpuid;

static int cpuid_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", board_cpuid);
	return 0;
}

static int cpuid_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, cpuid_proc_show, NULL);
}

static const struct file_operations cpuid_proc_fops = {
	.open		= cpuid_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init cpuid_init(void)
{
	proc_create("cpuid", 0, NULL, &cpuid_proc_fops);
	return 0;
}
fs_initcall(cpuid_init);
