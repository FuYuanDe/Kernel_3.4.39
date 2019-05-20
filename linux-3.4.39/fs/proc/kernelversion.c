#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>

extern char kernelversion[32];

static int kernelversion_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", kernelversion);
	return 0;
}

static int kernelversion_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, kernelversion_proc_show, NULL);
}

static const struct file_operations kernelversion_proc_fops = {
	.open		= kernelversion_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init kernel_version_init(void)
{
	proc_create("kernelversion", 0, NULL, &kernelversion_proc_fops);
	return 0;
}
fs_initcall(kernel_version_init);
