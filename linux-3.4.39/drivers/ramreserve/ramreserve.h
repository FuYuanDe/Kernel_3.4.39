#ifndef _RAMRESERVE_H_
#define _RAMRESERVE_H_

typedef u32 ramchar_priv_len_t;

struct ramreserve_dev_t
{
    void *virtual_start_addr;
    void *virtual_priv_start_addr;
    void *virtual_blk_start_addr;
    void *virtual_char_start_addr;
    ramchar_priv_len_t *virtual_priv_len_addr;
    struct request_queue *ramreserve_queue;
    spinlock_t spinlock;
    struct elevator_queue *old_e;
    struct gendisk *blkdev_disk;
    struct cdev priv_cdev;
    struct cdev char_cdev;
    struct semaphore sem;
    u32 rampriv_total_size;
    u32 cur_pos;
    struct class *rampriv_class;
    struct class *ramchar_class;
};

#endif