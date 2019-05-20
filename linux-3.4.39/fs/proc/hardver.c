#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>

extern int board_hardver;

static int hardver_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%x\n", board_hardver);
	return 0;
}

static int hardver_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, hardver_proc_show, NULL);
}

static const struct file_operations hardver_proc_fops = {
	.open		= hardver_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init hardver_init(void)
{
	proc_create("hardver", 0, NULL, &hardver_proc_fops);
	return 0;
}
fs_initcall(hardver_init);

