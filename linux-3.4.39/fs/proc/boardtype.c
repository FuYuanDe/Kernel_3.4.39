#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>

extern int board_type_val;

static int boardtype_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%x\n", board_type_val);
	return 0;
}

static int boardtype_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, boardtype_proc_show, NULL);
}

static const struct file_operations boardtype_proc_fops = {
	.open		= boardtype_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init boardtype_init(void)
{
	proc_create("boardtype", 0, NULL, &boardtype_proc_fops);
	return 0;
}
fs_initcall(boardtype_init);
