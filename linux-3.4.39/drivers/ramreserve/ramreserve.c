#include <linux/module.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include "ramreserve.h"

#define RAMRESERVE_DISK_NAME        "ramreserve"
#define RAMRESERVE_PRIV_NAME        "ramreservepriv"
#define RAMRESERVE_CHAR_NAME        "ramreservechar"
#define RAMRESERVE_MAJOR            COMPAQ_SMART2_MAJOR //抢个不用的

#define RAMRESERVE_HEAD_REGION_SIZE 0x400
#define RAMRESERVE_CHAR_SIZE        0x500000

extern unsigned long num_physpages;

static struct ramreserve_dev_t ramreserve_dev;
static dev_t ramreserve_priv_devno, ramreserve_char_devno;
static u32 ramreserve_char_major = 157, ramreserve_char_minor = 0;
static u32 ramreserve_priv_major = 158, ramreserve_priv_minor = 0;
static u32 ramreserve_head_region_addr = 0, ramreserve_size_all = 0;
static u32 ramreserve_phy_start_addr =0, ramreserve_size = 0;
static u32 ramreserve_char_start_addr = 0;

static void ramreserve_do_request(struct request_queue *q)
{
    struct request *req;
    struct req_iterator ri;
    struct bio_vec *bvec;
    char *buffer;
    char *disk_mem;

    while((req = blk_fetch_request(q)) != NULL)
    {
        if(((blk_rq_pos(req) + blk_rq_cur_sectors(req)) << 9) > ramreserve_size)
        {
            printk(KERN_WARNING "%s :bad request: block=%llu, count=%u\n",
                RAMRESERVE_DISK_NAME, (unsigned long long)blk_rq_pos(req),
                blk_rq_cur_sectors(req));
			__blk_end_request_all(req, -EIO);
            continue;
        }

        disk_mem = ramreserve_dev.virtual_blk_start_addr + (blk_rq_pos(req)<<9);

        switch(rq_data_dir(req))
        {
            case READ:
                rq_for_each_segment(bvec, req, ri)
                {
                    //buffer = kmap(bvec->bv_page) + bvec->bv_offset;
                    buffer = page_address(bvec->bv_page) + bvec->bv_offset;
                    //memcpy(req->buffer, ramreserve_dev.virtual_blk_start_addr + (blk_rq_pos(req)<<9),
                    //    blk_rq_cur_sectors(req) << 9);
                    memcpy(buffer, disk_mem, bvec->bv_len);
                    //kunmap(bvec->bv_page);
                    disk_mem += bvec->bv_len;
                }
                __blk_end_request_all(req, 0);
                break;
            case WRITE:
                rq_for_each_segment(bvec, req, ri)
                {
                    //memcpy(ramreserve_dev.virtual_blk_start_addr + (blk_rq_pos(req)<<9), req->buffer,
                    //    blk_rq_cur_sectors(req) << 9);
                    //buffer = kmap(bvec->bv_page) + bvec->bv_offset;
                    buffer = page_address(bvec->bv_page) + bvec->bv_offset;
                    memcpy(disk_mem, buffer, bvec->bv_len);
                    //kunmap(bvec->bv_page);
                    disk_mem += bvec->bv_len;
                }
                __blk_end_request_all(req, 0);
                break;
            default:
                break;
        }
    }
}

static struct block_device_operations ramreserve_fops =
{
    .owner = THIS_MODULE,
};

static int set_file_length(u32 length)
{
    ramreserve_dev.virtual_priv_len_addr[0] = (ramchar_priv_len_t)length;
    return 0;
}

static int get_file_length(void)
{
    int length;

    length = (int)ramreserve_dev.virtual_priv_len_addr[0];
    return length;
}

static int rampriv_open(struct inode * node, struct file * filp)
{
    int ret = 0;

    if(filp->f_flags & O_TRUNC)
    {
        if(set_file_length(0) < 0)
        {
            ret = -EIO;
        }
    }
    return 0;
}

static ssize_t rampriv_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
    int file_len, ret;

    file_len = get_file_length();
    if(file_len < 0)
    {
        ret = -EIO;
        goto out;
    }
    if(file_len > ramreserve_dev.rampriv_total_size)
    {
        ret = -EIO;
        goto out;
    }
    if((*offset) + count >= file_len)
    {
        count = file_len - (*offset);
    }
    ret = copy_to_user(buf,
        ((char *)ramreserve_dev.virtual_priv_start_addr) + (*offset), count);
    ret = count - ret;
    *offset += ret;
out:
    return ret;
}

static ssize_t rampriv_write(struct file *filp, const char __user *buf, size_t count, loff_t *offset)
{
    int ret = 0;
    loff_t dataoffset;
    int file_len;

    down_interruptible(&ramreserve_dev.sem);

    file_len = get_file_length();
    if(file_len < 0)
    {
        ret = -EIO;
        goto out0;
    }

    dataoffset = *offset;
    if(dataoffset >= ramreserve_dev.rampriv_total_size)
    {
        ret = -ENOSPC;
        goto out0;
    }
    if((dataoffset + count) >= ramreserve_dev.rampriv_total_size)
    {
        count = ramreserve_dev.rampriv_total_size - dataoffset;
    }

    ret = copy_from_user(((char *)ramreserve_dev.virtual_priv_start_addr) + dataoffset,
        buf, count);
    if(ret < count)
    {
        *offset += count - ret;
        if((*offset) >= file_len)
        {
            if(set_file_length(*offset) < 0)
            {
                ret = -EIO;
                goto out0;
            }
        }
    }
    ret = count- ret;

out0:
    up(&ramreserve_dev.sem);
    return ret;
}

static loff_t rampriv_llseek(struct file *filp, loff_t offset, int origin)
{
    int file_len;
    int ret = 0;

    down_interruptible(&ramreserve_dev.sem);

    file_len = get_file_length();
    if(file_len < 0)
    {
        ret = -EIO;
        goto out0;
    }

    switch(origin)
    {
        case SEEK_SET:
            if(offset >= ramreserve_dev.rampriv_total_size)
            {
                offset = ramreserve_dev.rampriv_total_size;
            }
            filp->f_pos = offset;
            break;
        case SEEK_CUR:
            offset += filp->f_pos;
            if(offset >= ramreserve_dev.rampriv_total_size)
            {
                offset = ramreserve_dev.rampriv_total_size;
            }
            filp->f_pos = offset;
            break;
        case SEEK_END:
            offset += file_len;
            if(offset >= ramreserve_dev.rampriv_total_size)
            {
                offset = ramreserve_dev.rampriv_total_size;
            }
            filp->f_pos = offset;
            break;
    }
    ret = filp->f_pos;

out0:
    up(&ramreserve_dev.sem);
    return ret;
}

static int rampriv_close(struct inode * node, struct file * filp)
{
    return 0;
}

static struct file_operations rampriv_fops =
{
    .owner = THIS_MODULE,
    .open = rampriv_open,
    .read = rampriv_read,
    .write = rampriv_write,
    .llseek = rampriv_llseek,
    .release = rampriv_close,
};

static int ramchar_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long page;
	unsigned long start = (unsigned long)vma->vm_start;
	unsigned long size =  (unsigned long)(vma->vm_end - vma->vm_start);

    if(size > RAMRESERVE_CHAR_SIZE)
    {
        printk(KERN_ERR "size can not exceed 0x%lx\n", RAMRESERVE_CHAR_SIZE);
        return -EINVAL;
    }

	/* Switch virtual address to physical address */
    page = ramreserve_char_start_addr;
    
	vma->vm_flags |= VM_IO;
	vma->vm_flags |= (VM_DONTEXPAND | VM_NODUMP);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if(remap_pfn_range(vma, start, page>>PAGE_SHIFT, size, vma->vm_page_prot))
	{
		printk(KERN_ERR "remap_pfn_range failed\n");
		return -1;
	}

    return 0;
}

static long ramchar_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    switch(cmd)
    {
        case 0:
            {
                unsigned char *start;
                start = (unsigned char *)ramreserve_dev.virtual_char_start_addr;
                printk("%02x %02x %02x %02x %02x %02x %02x %02x\n", start[0], start[1], start[2],
                        start[3], start[4], start[5], start[6], start[7]);
                start = (unsigned char *)ramreserve_dev.virtual_char_start_addr + RAMRESERVE_CHAR_SIZE/2;
                printk("%02x %02x %02x %02x %02x %02x %02x %02x\n", start[0], start[1], start[2],
                        start[3], start[4], start[5], start[6], start[7]);
                start = (unsigned char *)ramreserve_dev.virtual_char_start_addr + RAMRESERVE_CHAR_SIZE - 8;
                printk("%02x %02x %02x %02x %02x %02x %02x %02x\n", start[0], start[1], start[2],
                        start[3], start[4], start[5], start[6], start[7]);
            }
            break;
    }

    return 0;
}

static struct file_operations ramchar_fops =
{
    .owner = THIS_MODULE,
    mmap:           ramchar_mmap,
    unlocked_ioctl: ramchar_ioctl,
};

static __init int ramreserve_init(void)
{
    int ret;
    struct gendisk *p_gendisk;
    struct device *class_device;
    uint32_t try_size = 0x4000000;

    /*正常mem为2的n次方大小，如果传进来的mem仍然为2的n次方，说明没有保留
      内存*/
    for(; try_size <= (num_physpages<<PAGE_SHIFT); try_size<<=1);
    if((try_size>>1) == (num_physpages<<PAGE_SHIFT))
    {
        printk(KERN_ERR "no mem reserved\n");
        return -EIO;
    }
    /*try_size为实际内存大小，num_physpages<<PAGE_SHIFT为内核管理内存大小*/
    ramreserve_size_all = try_size - (num_physpages<<PAGE_SHIFT);
    /*ag主控板的u-boot的保留内存是5MB，ag上暂没有用到内存作为log缓存，仅
      SBC会用到保留内存做log缓存*/
#if defined(PRODUCT_SBCMAIN) || defined(PRODUCT_SBCUSER) || defined(PRODUCT_MTG2500MAIN) || \
    defined(PRODUCT_UC200) || defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER) || defined(PRODUCT_SBC1000MAIN)
    /*私有区域作为特殊用途，这里用作判断系统是软复位还是硬复位，位置固定在保留内存的
      最前面部分*/
    ramreserve_head_region_addr = PHYS_OFFSET + (num_physpages<<PAGE_SHIFT);
    /*log缓存区域*/
    ramreserve_phy_start_addr = ramreserve_head_region_addr + RAMRESERVE_HEAD_REGION_SIZE;
    /*log缓存区域大小，log缓存区夹在私有区和ramreservechar区之间*/
    ramreserve_size = ramreserve_size_all - RAMRESERVE_HEAD_REGION_SIZE - RAMRESERVE_CHAR_SIZE;
#endif
    /*ramreserve_char固定是实际内存的高端5M位置*/
    ramreserve_char_start_addr = PHYS_OFFSET + try_size - RAMRESERVE_CHAR_SIZE;
    printk(KERN_INFO "num_physpages:%x ramreserve_head_region_addr:%x ramreserve_size_all:%x\n",
        num_physpages, ramreserve_head_region_addr, ramreserve_size_all);
///-
#if 1
    ramreserve_dev.virtual_start_addr = ioremap(ramreserve_head_region_addr, ramreserve_size_all);
    if(NULL == ramreserve_dev.virtual_start_addr)
    {
        ret = -ENOMEM;
        goto err_ioremap;
    }
#if defined(PRODUCT_SBCMAIN) || defined(PRODUCT_SBCUSER) || defined(PRODUCT_MTG2500MAIN) || \
    defined(PRODUCT_UC200) || defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER) || defined(PRODUCT_SBC1000MAIN)
    ramreserve_dev.virtual_priv_len_addr = (ramchar_priv_len_t *)ramreserve_dev.virtual_start_addr;
    ramreserve_dev.virtual_priv_start_addr = ramreserve_dev.virtual_start_addr + sizeof(ramchar_priv_len_t);
    ramreserve_dev.rampriv_total_size = RAMRESERVE_HEAD_REGION_SIZE - sizeof(ramchar_priv_len_t);
    ramreserve_dev.virtual_blk_start_addr = ramreserve_dev.virtual_start_addr + RAMRESERVE_HEAD_REGION_SIZE;
#endif
    ramreserve_dev.virtual_char_start_addr = ramreserve_dev.virtual_start_addr + ramreserve_size_all - RAMRESERVE_CHAR_SIZE;

    spin_lock_init(&ramreserve_dev.spinlock);
#if defined(PRODUCT_SBCMAIN) || defined(PRODUCT_SBCUSER) || defined(PRODUCT_MTG2500MAIN) || \
    defined(PRODUCT_UC200) || defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER) || defined(PRODUCT_SBC1000MAIN)
    ramreserve_dev.ramreserve_queue = blk_init_queue(ramreserve_do_request, &ramreserve_dev.spinlock);
    if(NULL == ramreserve_dev.ramreserve_queue)
    {
        ret = -ENOMEM;
        goto err_init_queue;
    }

    ramreserve_dev.old_e = ramreserve_dev.ramreserve_queue->elevator;
	elevator_exit(ramreserve_dev.ramreserve_queue->elevator);
	ramreserve_dev.ramreserve_queue->elevator = NULL;
    if(IS_ERR_VALUE(elevator_init(ramreserve_dev.ramreserve_queue, "noop")))
    {
        printk(KERN_WARNING "ramreserve switch elevator failed, using default\n");
    }
    else
    {
        //elevator_exit(ramreserve_dev.old_e);
    }

    ramreserve_dev.blkdev_disk = alloc_disk(1);
    if(NULL == ramreserve_dev.blkdev_disk)
    {
        ret = -ENOMEM;
        goto err_alloc_disk;
    }

    p_gendisk = ramreserve_dev.blkdev_disk;
    strncpy(p_gendisk->disk_name, RAMRESERVE_DISK_NAME, sizeof(p_gendisk->disk_name));
    p_gendisk->major = RAMRESERVE_MAJOR;
    p_gendisk->first_minor = 0;
    p_gendisk->fops = &ramreserve_fops;
    p_gendisk->queue = ramreserve_dev.ramreserve_queue;
    set_capacity(p_gendisk, ramreserve_size >> 9);

    add_disk(ramreserve_dev.blkdev_disk);
    printk(KERN_WARNING "ramreserve blk driver init success\n");
#endif

    sema_init(&ramreserve_dev.sem, 1);

#if defined(PRODUCT_SBCMAIN) || defined(PRODUCT_SBCUSER) || defined(PRODUCT_MTG2500MAIN) || \
    defined(PRODUCT_UC200) || defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER) || defined(PRODUCT_SBC1000MAIN)
    /*ramreservepriv设备注册*/
    ramreserve_priv_devno = MKDEV(ramreserve_priv_major, ramreserve_priv_minor);
    ret = register_chrdev_region(ramreserve_priv_devno, 1, RAMRESERVE_PRIV_NAME);
    if(ret < 0)
    {
        ret = alloc_chrdev_region(&ramreserve_priv_devno, 0, 1, RAMRESERVE_PRIV_NAME);
        if(ret < 0)
        {
            printk(KERN_WARNING "register ramreservepriv device error\n");
            goto err_priv_register_chrdev;
        }
    }

    cdev_init(&ramreserve_dev.priv_cdev, &rampriv_fops);
    ret = cdev_add(&ramreserve_dev.priv_cdev, ramreserve_priv_devno, 1);
    if(ret < 0)
    {
        printk(KERN_WARNING "cdev_add ramreservepriv failed\n");
        goto err_priv_cdev_add;
    }

    ramreserve_dev.rampriv_class = class_create(THIS_MODULE, RAMRESERVE_PRIV_NAME);
    if(IS_ERR(ramreserve_dev.rampriv_class))
    {
        printk(KERN_WARNING "class_create error\n");
        goto err_priv_class_create;
    }
    class_device = device_create(ramreserve_dev.rampriv_class, NULL, ramreserve_priv_devno, NULL, RAMRESERVE_PRIV_NAME);
    if(IS_ERR(class_device))
    {
        printk(KERN_WARNING "device_create error\n");
        goto err_priv_device_create;
    }
#endif

    /*ramreservechar设备注册*/
    ramreserve_char_devno = MKDEV(ramreserve_char_major, ramreserve_char_minor);
    ret = register_chrdev_region(ramreserve_char_devno, 1, RAMRESERVE_CHAR_NAME);
    if(ret < 0)
    {
        ret = alloc_chrdev_region(&ramreserve_char_devno, 0, 1, RAMRESERVE_CHAR_NAME);
        if(ret < 0)
        {
            printk(KERN_WARNING "register ramreservechar device error\n");
            goto err_char_register_chrdev;
        }
    }

    cdev_init(&ramreserve_dev.char_cdev, &ramchar_fops);
    ret = cdev_add(&ramreserve_dev.char_cdev, ramreserve_char_devno, 1);
    if(ret < 0)
    {
        printk(KERN_WARNING "cdev_add ramreservechar failed\n");
        goto err_char_cdev_add;
    }

    ramreserve_dev.ramchar_class = class_create(THIS_MODULE, RAMRESERVE_CHAR_NAME);
    if(IS_ERR(ramreserve_dev.ramchar_class))
    {
        printk(KERN_WARNING "class_create error\n");
        goto err_char_class_create;
    }
    class_device = device_create(ramreserve_dev.ramchar_class, NULL, ramreserve_char_devno, NULL, RAMRESERVE_CHAR_NAME);
    if(IS_ERR(class_device))
    {
        printk(KERN_WARNING "device_create error\n");
        goto err_char_device_create;
    }
#endif

    printk(KERN_WARNING "ramreservechar register success\n");

    return 0;
    
err_char_device_create:
    class_destroy(ramreserve_dev.ramchar_class);
err_char_class_create:
    cdev_del(&ramreserve_dev.char_cdev);
err_char_cdev_add:
    unregister_chrdev_region(ramreserve_char_devno, 1);
err_char_register_chrdev:
#if defined(PRODUCT_SBCMAIN) || defined(PRODUCT_SBCUSER) || defined(PRODUCT_MTG2500MAIN) || \
    defined(PRODUCT_UC200) || defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER) || defined(PRODUCT_SBC1000MAIN)
    device_destroy(ramreserve_dev.rampriv_class, ramreserve_priv_devno);
err_priv_device_create:
    class_destroy(ramreserve_dev.rampriv_class);
err_priv_class_create:
    cdev_del(&ramreserve_dev.priv_cdev);
err_priv_cdev_add:
    unregister_chrdev_region(ramreserve_priv_devno, 1);
err_priv_register_chrdev:
    del_gendisk(ramreserve_dev.blkdev_disk);
    put_disk(ramreserve_dev.blkdev_disk);
err_alloc_disk:
    blk_cleanup_queue(ramreserve_dev.ramreserve_queue);
err_init_queue:
    iounmap(ramreserve_dev.virtual_start_addr);
#endif
err_ioremap:
    return ret;
}
static __exit void ramreserve_exit(void)
{
    device_destroy(ramreserve_dev.ramchar_class, ramreserve_char_devno);
    class_destroy(ramreserve_dev.ramchar_class);
    cdev_del(&ramreserve_dev.char_cdev);
    unregister_chrdev_region(ramreserve_char_devno, 1);

#if defined(PRODUCT_SBCMAIN) || defined(PRODUCT_SBCUSER) || defined(PRODUCT_MTG2500MAIN) || \
    defined(PRODUCT_UC200) || defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER) || defined(PRODUCT_SBC1000MAIN)
    device_destroy(ramreserve_dev.rampriv_class, ramreserve_priv_devno);
    class_destroy(ramreserve_dev.rampriv_class);
    cdev_del(&ramreserve_dev.priv_cdev);
    unregister_chrdev_region(ramreserve_priv_devno, 1);

    if(ramreserve_dev.blkdev_disk)
    {
        del_gendisk(ramreserve_dev.blkdev_disk);
        put_disk(ramreserve_dev.blkdev_disk);
        blk_cleanup_queue(ramreserve_dev.ramreserve_queue);
        iounmap(ramreserve_dev.virtual_start_addr);
    }
#endif

    printk(KERN_WARNING "ramreserve exit\n");
}

MODULE_LICENSE("GPL");
module_init(ramreserve_init);
module_exit(ramreserve_exit);
