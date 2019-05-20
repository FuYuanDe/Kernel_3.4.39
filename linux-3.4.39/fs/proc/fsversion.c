#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>

#if defined(PRODUCT_AG)
extern char fsversion[32];

static int fsversion_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", fsversion);
	return 0;
}

static int fsversion_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fsversion_proc_show, NULL);
}

static const struct file_operations fsversion_proc_fops = {
	.open		= fsversion_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif
static int __init fs_version_init(void)
{
#if defined(PRODUCT_AG)
	proc_create("fsversion", 0, NULL, &fsversion_proc_fops);
#endif
	return 0;
}
fs_initcall(fs_version_init);
