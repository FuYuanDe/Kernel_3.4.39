#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>

extern int board_slot;

static int slot_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", board_slot);
	return 0;
}

static int slot_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, slot_proc_show, NULL);
}

static const struct file_operations slot_proc_fops = {
	.open		= slot_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init slot_init(void)
{
	proc_create("slot", 0, NULL, &slot_proc_fops);
	return 0;
}
fs_initcall(slot_init);
