#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>

extern char bootversion[32];

static int bootversion_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", bootversion);
	return 0;
}

static int bootversion_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bootversion_proc_show, NULL);
}

static const struct file_operations bootversion_proc_fops = {
	.open		= bootversion_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init boot_version_init(void)
{
	proc_create("bootversion", 0, NULL, &bootversion_proc_fops);
	return 0;
}
fs_initcall(boot_version_init);
