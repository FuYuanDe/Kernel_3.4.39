#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <mtd/mtd-abi.h>
#include <linux/mtd/mtd.h>
#include <linux/vmalloc.h>

struct configdev_s
{
    struct file * pfile;
    unsigned long page_size;
    unsigned long erase_size;
    unsigned long total_size;
    struct semaphore sem;
    unsigned char *write_buf;
    unsigned char dirty;
    struct cdev cdev;
};

#define DEV_FILE_PATH   "/dev/mtd1"
#define DEV_NAME        "configdata"

#define ALIGN_ADDR_DE(addr, align_size)  ((addr) & (~((align_size) - 1)))
#define ALIGN_ADDR_IN(addr, align_size)  (((addr) + (align_size)) & (~((align_size) - 1)))
#define ALIGN_SIZE_DE(size, align_size)  ((size) & (~((align_size) - 1)))
#define ALIGN_SIZE_IN(size, align_size)  (((size) + (align_size)) & (~((align_size) - 1)))

static struct configdev_s configdev;
static dev_t configdev_devno;
static u32 configdev_major = 156, configdev_minor = 0;
static struct class *configdata_class = NULL;

static ssize_t configdev_write_oneblock(struct file *filp, const char __user *buf, size_t count, loff_t offset);
static int set_file_length(u32 length);

static bool check_empty(u8 *buf, int size)
{
    int i;

    for(i=0; i<size; i++)
    {
        if(0xff != buf[i])
        {
            return false;
        }
    }
    return true;
}

static int configdev_open(struct inode * node, struct file * filp)
{
    int ret = 0;
    mm_segment_t old_fs = get_fs();

    if(filp->f_flags & O_TRUNC)
    {
        set_fs(get_ds());
        if(set_file_length(0) < 0)
        {
            ret = -EIO;
        }
        set_fs(old_fs);
    }
    return ret;
}

static int configdev_close(struct inode * node, struct file * filp)
{
    return 0;
}

static int get_file_length(void)
{
    int length;
    loff_t offset;

    offset = 0;
    if(sizeof(length) != vfs_read(configdev.pfile, (char *)&length, sizeof(length), &offset))
    {
        return -1;
    }

    if( -1 == length)
    {
        length = 0;
    }
    return length;
}

static int set_file_length(u32 length)
{
    if(sizeof(length) != configdev_write_oneblock(configdev.pfile, (char *)&length, sizeof(length), 0))
    {
        return -1;
    }
    return 0;
}

static ssize_t data_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
    loff_t devoffset;
    int ret;

    /*文件内容在文件的起始偏移4字节后开始*/
    devoffset = (*offset) + sizeof(u32);
    if(count != (ret = vfs_read(configdev.pfile, buf, count, &devoffset)))
    {
        if(ret > 0)
        {
            *offset += ret;
        }
        return ret;
    }
    else
    {
        *offset += count;
        return count;
    }
}

static ssize_t configdev_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
    int file_len, ret;
    mm_segment_t old_fs = get_fs();

    set_fs(get_ds());
    file_len = get_file_length();
    if(file_len < 0)
    {
        ret = -EIO;
        goto out;
    }
    if((*offset) + count >= file_len)
    {
        count = file_len - (*offset);
    }
    ret = data_read(configdev.pfile, buf, count, offset);
out:
    set_fs(old_fs);
    return ret;
}

static int non_region_erase(struct file *filp, u32 start, u32 count, int unlock)
{
    struct erase_info_user erase;

    erase.start = start;
    erase.length = configdev.erase_size;

    for(; count>0; count--)
    {
        if(unlock != 0)
        {
            if(configdev.pfile->f_op->unlocked_ioctl(configdev.pfile, MEMUNLOCK, (unsigned long)&erase))
            {
                return -1;
            }
        }
        if(configdev.pfile->f_op->unlocked_ioctl(configdev.pfile, MEMERASE, (unsigned long)&erase))
        {
            return -1;
        }
        erase.start += configdev.erase_size;
    }

    return 0;
}

static ssize_t configdev_write_oneblock(struct file *filp, const char __user *buf, size_t count, loff_t offset)
{
    u8 *tmp_buff;
    loff_t readoffset, modifyoffset, tmpoffset;
    size_t tmpcount;
    int ret;

    /*使用vmalloc分配大内存*/
    tmp_buff = vmalloc(configdev.erase_size);
    if(NULL == tmp_buff)
    {
        return -ENOMEM;
    }
    memset(tmp_buff, 0, configdev.erase_size);

    /*读取文件制定偏移处的内容，如果全为0xff，
      说明此段可以直接写入数据，否则，必须将
      整块的数据读出后，将原内容删除后再写入*/
    tmpoffset = ALIGN_ADDR_DE(offset, configdev.page_size);
    tmpcount = ALIGN_SIZE_IN(count, configdev.page_size);
    ret = vfs_read(configdev.pfile, tmp_buff, tmpcount, &tmpoffset);
    if(ret != tmpcount)
    {
        ret = -EIO;
    }
    if(check_empty(tmp_buff, tmpcount))
    {
        tmpoffset = ALIGN_ADDR_DE(offset, configdev.page_size);
        tmpcount = ALIGN_SIZE_IN(count, configdev.page_size);
        if(0 != copy_from_user(tmp_buff + (offset-tmpoffset), buf, count))
        {
            ret = -EIO;
        }
        ret = vfs_write(configdev.pfile, tmp_buff, tmpcount, &tmpoffset);
        if(ret != tmpcount)
        {
            ret = -EFAULT;
        }
        else
        {
            ret = count;
        }
    }
    else
    {
        readoffset = ALIGN_ADDR_DE(offset, configdev.erase_size);
        memset(tmp_buff, 0, configdev.erase_size);
        tmpoffset = readoffset;
        ret = vfs_read(configdev.pfile, tmp_buff, configdev.erase_size, &tmpoffset);
        if(ret != configdev.erase_size)
        {
            ret = -EIO;
        }

        modifyoffset = offset - readoffset;
        memset(tmp_buff + modifyoffset, 0, count);
        if(0 != copy_from_user(tmp_buff + modifyoffset, buf, count))
        {
            ret = -EIO;
        }

        if(non_region_erase(configdev.pfile, readoffset, 1, 0))
        {
            ret = -EIO;
        }

        tmpoffset = readoffset;
        ret = vfs_write(configdev.pfile, tmp_buff, configdev.erase_size, &tmpoffset);
        if(ret != configdev.erase_size)
        {
            ret = -EIO;
        }
        else
        {
            ret = count;
        }
    }

    vfree(tmp_buff);
    return ret;
}

static ssize_t configdev_write(struct file *filp, const char __user *buf, size_t count, loff_t *offset)
{
    int ret = 0;
    size_t write_count, old_count = count;
    loff_t write_offset, dataoffset;
    mm_segment_t old_fs = get_fs();
    struct erase_info_user mtd_lockinfo;
    int file_len;

    down_interruptible(&configdev.sem);

    set_fs(get_ds());
///-
    /*解开硬件锁*/
    mtd_lockinfo.start = 0;
    mtd_lockinfo.length = configdev.total_size;
    if(configdev.pfile->f_op->unlocked_ioctl(configdev.pfile, MEMUNLOCK, (unsigned long)&mtd_lockinfo) < 0)
    {
        printk(KERN_WARNING "unlock configdev failed\n");
    }

    file_len = get_file_length();
    if(file_len < 0)
    {
        ret = -EIO;
        goto out0;
    }

    dataoffset = (*offset) + sizeof(u32);
    if(dataoffset >= configdev.total_size)
    {
        ret = -ENOSPC;
        goto out0;
    }
    if((dataoffset + count) >= configdev.total_size)
    {
        count = configdev.total_size - dataoffset;
        old_count = count;
    }
    /*要写入的内容的起始和结束地址都在同一个块内部*/
    if((dataoffset + count) < ALIGN_ADDR_IN(dataoffset, configdev.erase_size))
    {
        ret = configdev_write_oneblock(configdev.pfile, buf, count, dataoffset);
        if(ret > 0)
        {
            *offset += ret;
            if((*offset) >= file_len)
            {
                if(set_file_length(*offset) < 0)
                {
                    ret = -EIO;
                    goto out0;
                }
            }
        }
    }
    else /*要写入的内容的起始地址和结束地址不在同一个块内部*/
    {
        while(count > 0)
        {
            write_count = ALIGN_ADDR_IN(dataoffset, configdev.erase_size) - dataoffset;
            if(write_count > count)
            {
                write_count = count;
            }
            write_offset = dataoffset;
            ret = configdev_write_oneblock(configdev.pfile, buf, write_count, write_offset);
            if(ret != write_count)
            {
                ret = -EIO;
                break;
            }
            else
            {
                *offset += write_count;
            }
            count -= write_count;
            buf += write_count;
            dataoffset = (*offset) + sizeof(u32);
        }
        if(ret > 0)
        {
            ret = old_count - count;
            if((*offset) >= file_len)
            {
                if(set_file_length(*offset) < 0)
                {
                    ret = -EIO;
                    goto out0;
                }
            }
        }
    }

out0:
///-
    /*加上硬件锁*/
    mtd_lockinfo.start = 0;
    mtd_lockinfo.length = configdev.total_size;
    if(configdev.pfile->f_op->unlocked_ioctl(configdev.pfile, MEMLOCK, (unsigned long)&mtd_lockinfo) < 0)
    {
        printk(KERN_WARNING "lock configdev failed\n");
    }
    set_fs(old_fs);
    up(&configdev.sem);
    return ret;
}

static loff_t configdev_llseek(struct file *filp, loff_t offset, int origin)
{
    int file_len;
    int ret = 0;
    mm_segment_t old_fs = get_fs();

    down_interruptible(&configdev.sem);

    set_fs(get_ds());

    file_len = get_file_length();
    if(file_len < 0)
    {
        ret = -EIO;
        goto out0;
    }

    switch(origin)
    {
        case SEEK_SET:
            if(offset >= (configdev.total_size - sizeof(u32)))
            {
                offset = configdev.total_size - sizeof(u32);
            }
            filp->f_pos = offset;
            break;
        case SEEK_CUR:
            offset += filp->f_pos;
            if(offset >= (configdev.total_size - sizeof(u32)))
            {
                offset = configdev.total_size - sizeof(u32);
            }
            filp->f_pos = offset;
            break;
        case SEEK_END:
            offset += file_len;
            if(offset >= (configdev.total_size - sizeof(u32)))
            {
                offset = configdev.total_size - sizeof(u32);
            }
            filp->f_pos = offset;
            break;
    }
    ret = filp->f_pos;

out0:
    set_fs(old_fs);
    up(&configdev.sem);
    return ret;
}

static int configdev_sync(struct file *filp, loff_t start, loff_t end, int datasync)
{
    return 0;
}

static struct file_operations configdev_ops =
{
    .owner = THIS_MODULE,
    .open = configdev_open,
    .read = configdev_read,
    .write = configdev_write,
    .llseek = configdev_llseek,
    .fsync = configdev_sync,
    .release = configdev_close,
};

static __init int configdev_init(void)
{
	mm_segment_t old_fs = get_fs();
    struct mtd_info_user meminfo;
    int ret;
    struct device *configdata_device;

    configdev.pfile = filp_open(DEV_FILE_PATH,  O_RDWR, 0755);
    if(NULL == configdev.pfile)
    {
        printk(KERN_WARNING "open %s failed!\n", DEV_FILE_PATH);
        return -ENODEV;
    }

    sema_init(&configdev.sem, 1);

    /*扩充系统调用可访问的空间到内核空间*/
	set_fs(get_ds());

    ret = configdev.pfile->f_op->unlocked_ioctl(configdev.pfile, MEMGETINFO, (unsigned long)&meminfo);
    if(ret < 0)
    {
        set_fs(old_fs);
        return -EBUSY;
    }
    set_fs(old_fs);
    configdev.page_size = meminfo.writesize;
    configdev.erase_size = meminfo.erasesize;
    configdev.total_size = meminfo.size;
    configdev.write_buf = NULL;
    configdev.dirty = 0;

    configdev_devno = MKDEV(configdev_major, configdev_minor);
    ret = register_chrdev_region(configdev_devno, 1, "configdata");
    if(ret < 0)
    {
        printk(KERN_WARNING "register configdata device error\n");
        goto fail0;
    }

    cdev_init(&configdev.cdev, &configdev_ops);
    ret = cdev_add(&configdev.cdev, configdev_devno, 1);
    if(ret < 0)
    {
        printk(KERN_WARNING "cdev_add configdev failed\n");
        goto fail1;
    }

    configdata_class = class_create(THIS_MODULE, DEV_NAME);
    if(IS_ERR(configdata_class))
    {
        ret = -EINVAL;
        goto fail2;
    }

    configdata_device = device_create(configdata_class, NULL, configdev_devno, NULL, DEV_NAME);
    if(IS_ERR(configdata_device))
    {
        ret = -EINVAL;
        goto fail3;
    }

    return 0;

fail3:
    class_destroy(configdata_class);
fail2:
    cdev_del(&configdev.cdev);
fail1:
    unregister_chrdev_region(configdev_devno, 1);
fail0:
    filp_close(configdev.pfile, NULL);
    return ret;
}

static __exit void configdev_exit(void)
{
    device_destroy(configdata_class, configdev_devno);
    class_destroy(configdata_class);
    cdev_del(&configdev.cdev);
    unregister_chrdev_region(configdev_devno, 1);
    filp_close(configdev.pfile, NULL);
}

module_init(configdev_init);
module_exit(configdev_exit);
MODULE_LICENSE("GPL");

