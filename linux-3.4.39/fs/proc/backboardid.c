#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>

extern int back_board_id;

static int backboardid_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", back_board_id);
	return 0;
}

static int backboardid_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, backboardid_proc_show, NULL);
}

static const struct file_operations backboardid_proc_fops = {
	.open		= backboardid_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init backboardid_init(void)
{
	proc_create("back_board_id", 0, NULL, &backboardid_proc_fops);
	return 0;
}
fs_initcall(backboardid_init);
