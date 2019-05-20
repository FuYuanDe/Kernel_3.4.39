#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>

extern int board_hardid;

static int hardid_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%x\n", board_hardid);
	return 0;
}

static int hardid_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, hardid_proc_show, NULL);
}

static const struct file_operations hardid_proc_fops = {
	.open		= hardid_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init hardid_init(void)
{
	proc_create("hardid", 0, NULL, &hardid_proc_fops);
	return 0;
}
fs_initcall(hardid_init);

