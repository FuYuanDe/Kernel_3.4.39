/******************************************************************************
        (c) COPYRIGHT 2002-2003 by Shenzhen Allywll Information Co.,Ltd
                          All rights reserved.
File: pef22554.c
Desc:the source file of user config
Modification history(no, author, date, desc)
1.Holy 2003-04-02 create file
2.luke 2006-04-20 for AOS
******************************************************************************/



#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/semaphore.h>
#include <asm/uaccess.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include <mach/platform.h>

#include "pef22554_drv.h"
#include "pef22554_reg.h"

#define QUADFALC_GEN_NAME           "quadFALCV"
#define QUADFALC_DEV_NAME           "quadFALC"
#define QUADFALC_VERSION            "0.8.1"


static u32   quadFALC_gen_major = 220;
static dev_t quadFALC_gen_devno;
struct cdev  quadFALC_gen_cdev;
struct class *quadFALC_gen_class;

static u32   quadFALC_dev_major = 221;
static dev_t quadFALC_dev_devno;
struct cdev  quadFALC_dev_cdev;
struct class *quadFALC_dev_class;


struct quadFALC_dev QuadFALCDev;
ST_E1_RECEIVE_DATA  stE1BufferRec[MAX_PEF22554_NUM*NUM_OF_FALC];
u8 casrs[MAX_PEF22554_NUM][NUM_OF_FALC][16];
u8 casxs[MAX_PEF22554_NUM][NUM_OF_FALC][16];
volatile int sendflag[MAX_PEF22554_NUM*NUM_OF_FALC];
QFALC_REG_MAP *ptrreg[MAX_PEF22554_NUM*NUM_OF_FALC];


static const u8 RTRTS[MAX_TS_NUM] = {RTR1_TS0,RTR1_TS1,RTR1_TS2,RTR1_TS3,RTR1_TS4,RTR1_TS5,RTR1_TS6,RTR1_TS7,
                                          RTR2_TS8,RTR2_TS9,RTR2_TS10,RTR2_TS11,RTR2_TS12,RTR2_TS13,RTR2_TS14,RTR2_TS15,
                                          RTR3_TS16,RTR3_TS17,RTR3_TS18,RTR3_TS19,RTR3_TS20,RTR3_TS21,RTR3_TS22,RTR3_TS23,
                                          RTR4_TS24,RTR4_TS25,RTR4_TS26,RTR4_TS27,RTR4_TS28,RTR4_TS29,RTR4_TS30,RTR4_TS31};
static const u8 TTRTS[MAX_TS_NUM] = {TTR1_TS0,TTR1_TS1,TTR1_TS2,TTR1_TS3,TTR1_TS4,TTR1_TS5,TTR1_TS6,TTR1_TS7,
                                          TTR2_TS8,TTR2_TS9,TTR2_TS10,TTR2_TS11,TTR2_TS12,TTR2_TS13,TTR2_TS14,TTR2_TS15,
                                          TTR3_TS16,TTR3_TS17,TTR3_TS18,TTR3_TS19,TTR3_TS20,TTR3_TS21,TTR3_TS22,TTR3_TS23,
                                          TTR4_TS24,TTR4_TS25,TTR4_TS26,TTR4_TS27,TTR4_TS28,TTR4_TS29,TTR4_TS30,TTR4_TS31};



static u8 PEF22554_CHIPMAP;


extern int board_cpuid;


void quadFALC_mem_read8(u8 *a, u8 *d_ptr)
{
    u32 addr;

    addr = a - (u8 *)NULL;
    
    quadFALC_pef_read_reg(addr, d_ptr);
}

void quadFALC_mem_write8(u8 *a, u8 d)
{
    u32 addr;

    addr = a - (u8 *)NULL;

    quadFALC_pef_write_reg(addr, d);
}


void quadFALC_set_bit8(u8 *a, u8 b)
{
    u8 value;

    quadFALC_mem_read8(a, &value);
    quadFALC_mem_write8(a, (u8)(value | (b)));

    return;
}

void quadFALC_clear_bit8(u8 *a, u8 b)
{
    u8 value;

    quadFALC_mem_read8(a, &value);
    quadFALC_mem_write8(a, (u8)(value & ~(b)));
}


static int quadFALC_select_cpld_clk(u8 type)
{
    QFALC_REG_MAP   *falc;
    int i;
    u8  data;
    

    switch(type)
    {
        case CLK_PEF1:
            quadFALC_cpld_write_reg(0x1, 1);
            quadFALC_cpld_read_reg(0x1, &data);
            printk(KERN_ERR "%s: type:%d data:%d\n", __FUNCTION__, type, data);
            
            if(data != 1)
            {
                return -1;
            }
            
            for(i=0; i<NUM_OF_FALC*MAX_PEF22554_NUM; i++)
            {
                falc = ptrreg[i];
                CLEAR_BIT8(&falc->rdWr.lim0 , LIM0_MAS);
            }
            break;
        case CLK_MASTER:
            quadFALC_cpld_write_reg(0x1, 0);
            quadFALC_cpld_read_reg(0x1, &data);
            printk(KERN_ERR "%s: type:%d data:%d\n", __FUNCTION__, type, data);
            
            if(data != 0)
            {
                return -1;
            }
                
            for(i=0; i<NUM_OF_FALC*MAX_PEF22554_NUM; i++)
            {
                falc = ptrreg[i];
                SET_BIT8(&falc->rdWr.lim0 , LIM0_MAS);
            }
            break;
    }

    return 0;
}


static int quadFALC_open(struct inode *nod, struct file *filp)
{
    dev_t devno = nod->i_rdev;
    ST_FILE_PRIVATE *file_priv;
    int e1no, chipno;
    
    filp->private_data = kmalloc(sizeof(ST_FILE_PRIVATE), GFP_ATOMIC);
    if(!filp->private_data)
	{
		return -1;
	}

    file_priv = filp->private_data;
    file_priv->channel = MINOR(devno);
    file_priv->major = MAJOR(devno);
    file_priv->minor = MINOR(devno);

    if( quadFALC_gen_major == MAJOR(devno) )
    {
        return 0;
    }
    
	e1no   = (file_priv->channel) % NUM_OF_FALC;
	chipno = (file_priv->channel) / NUM_OF_FALC;

	QuadFALCDev.cnt    = 0;
	QuadFALCDev.intcnt = 0;
	QuadFALCDev.errcnt = 0;
	QuadFALCDev.rpfcnt = 0;
	QuadFALCDev.rmecnt = 0;
    QuadFALCDev.pef22554info[chipno].e1_use_state[e1no] = FDUNUSED;
	QuadFALCDev.pef22554info[chipno].sendfrmerr[e1no]   = 0;
	QuadFALCDev.pef22554info[chipno].sendfrmfail[e1no]  = 0;
	QuadFALCDev.pef22554info[chipno].recvfrmerr[e1no]   = 0;
	QuadFALCDev.pef22554info[chipno].recvfrmfail[e1no]  = 0;
	QuadFALCDev.pef22554info[chipno].intdeadcnt[e1no]   = 0;
    
    quadFALC_buffer_clear(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no]);

	return 0;
}

static int quadFALC_release(struct inode *nod, struct file *filp)
{
    dev_t devno = nod->i_rdev;
    int e1no, chipno;


    if(filp->private_data)
	{
		kfree(filp->private_data);
	}

    if( quadFALC_gen_major == MAJOR(devno) )
    {
        return 0;
    }
    
	e1no   = MINOR(devno) % NUM_OF_FALC;
	chipno = MINOR(devno) / NUM_OF_FALC;

    QuadFALCDev.pef22554info[chipno].e1_use_state[e1no] = FDUNUSED;
    quadFALC_buffer_clear(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no]);
    
	return 0;
}

static ssize_t quadFALC_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
    QFALC_REG_MAP   *falc;
    int readCount = 0;
    ST_FILE_PRIVATE *file_priv;
    int e1no, chipno;
    struct st_data buftmp;

    
    file_priv = filp->private_data;

    if(quadFALC_gen_major == file_priv->major)
    {
        return 0;
    }

    falc = ptrreg[file_priv->channel];
    e1no   = file_priv->channel % NUM_OF_FALC;
    chipno = file_priv->channel / NUM_OF_FALC;
    
    /* 检查接收缓冲区是否为空 */
    if( quadFALC_buffer_is_empty(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no]) )
    {
        printk(KERN_ERR "receive buffer is empty\n");
        return 0;
    }
    
    /* 检查用户缓冲区长度是否足够 */
    if( count < QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no].head->len )
    {
        printk(KERN_ERR "user space buffer is not enough, channel %d len=%d\n",
               file_priv->channel, 
               QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no].head->len);
        
        return 0;
    }

    /* 将设备中的数据拷贝到用户缓冲区中，并将接收缓冲区的头指针指向下一个单元，表示已成功读取完一条消息 */
    quadFALC_buffer_fetch(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no], &buftmp);
    if(copy_to_user(buf, buftmp.buf, buftmp.len))
    {
        printk(KERN_ERR "copy to user error\n");
    }
    readCount = buftmp.len;
    
    return readCount;
}

static ssize_t quadFALC_write(struct file *filp, const char __user *buf, size_t count, loff_t *offset)
{
    QFALC_REG_MAP   *falc;
    int i,len,ret;
    ST_FILE_PRIVATE *file_priv;
    unsigned long sendcount = 0;
    int e1no, chipno, port;
    struct st_data *buf_head = NULL, *buf_tail = NULL;

    
    file_priv = filp->private_data;

    if(quadFALC_gen_major == file_priv->major)
    {
        return 0;
    }

    falc = ptrreg[file_priv->channel];
    port = file_priv->channel;
    e1no = file_priv->channel % NUM_OF_FALC;
    chipno = file_priv->channel / NUM_OF_FALC;

    /* 检查是否进行阻塞的写，若为阻塞的写，则先获取设备写同步信号量 */
#if NONBLOCK
    if( !(filp->f_flags & O_NONBLOCK) )
    {
        if( down_interruptible(&QuadFALCDev.pef22554info[chipno].channel_sem[e1no]) )
        {
            return -ERESTARTSYS;
        }
    }
#endif

    if( count > DATA_LEN_LIMIT || count <= 0 )
    {
#if NONBLOCK
        up(&QuadFALCDev.pef22554info[chipno].channel_sem[e1no]);
#endif
        return 0;
    }

    /* 将消息添加到发送缓冲区中 */
    ret = quadFALC_buffer_add(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no], (char *)buf, count, EN_END);
    if( ret < 0)
    {
        /* 若返回失败，表示缓冲区满，XPR中断出现故障，恢复XPR中断 */
        quadFALC_buffer_clear(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no]);
        SET_BIT8(&falc->wr.cmdr, CMDR_SRES);
#if NONBLOCK
        up(&QuadFALCDev.pef22554info[chipno].channel_sem[e1no]);
#endif
        return 0;
    }

    sendcount = count;

    /* 若在添加新消息之前设备缓冲区已为空，则开始首次发送，每次发送最大字节数为32 */
    buf_head = QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].head;
    buf_tail = QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].tail;

    if( 0 == sendflag[port] )
    {
        /* 判断设备对应通道的发送缓冲区是否为空 */
        if( !quadFALC_buffer_is_empty(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no]) )
        {
            //printk(KERN_WARNING "kernel send data\n");
            /* 等待设备处于就绪状态 */

            /* 开始发送，若等待发送的字节数大于32，则发送32字节；
               若等待发送的字节数不大于32，则发送最后的字节数，并通知接收端本条消息发送完毕 */
            if( (buf_head->len - buf_head->cur ) > 32 )
            {
                /* 发送数据 */
                for(i=0; i<32; i++)
                {
                    MEM_WRITE8((u8 *)&falc->wr.xfifo,(u8)buf_head->buf[buf_head->cur++]);
                }
                SET_BIT8(&falc->wr.cmdr, CMDR_XHF);
            }
            else
            {
                len = buf_head->len - buf_head->cur;
                if(len < 0 || len > MAX_SEND_LEN)
                {
                    printk(KERN_WARNING "#################%s %d\n", __FUNCTION__, __LINE__);
                }
                //debug("write send data len = %d\n",len);
                for(i=0; i<len; i++)
                {
                    MEM_WRITE8((u8 *)&falc->wr.xfifo,(u8)buf_head->buf[buf_head->cur++]);
                }
                if( buf_head == (QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].databuf + QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].size) )
                {
                    QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].head = QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].databuf;
                }
                else
                {
                    QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].head++;
                }

                SET_BIT8(&falc->wr.cmdr, CMDR_XHF | CMDR_XME);
                wake_up_interruptible(&QuadFALCDev.pef22554info[chipno].wq[e1no]);
            }
        }
    }

    /*若为阻塞的写，则释放获得的设备写同步信号量*/
#if NONBLOCK
    if( !(filp->f_flags & O_NONBLOCK) )
    {
        up(&QuadFALCDev.pef22554info[chipno].channel_sem[e1no]);
    }
#endif

    return sendcount;
}

static int quadFALC_ioctl_major(unsigned int cmd, unsigned long args)
{

    printk(KERN_ERR "%s cmd:%x\n", __FUNCTION__, cmd);

    switch(cmd)
    {
        case E1NUM:
            *(u8 *)args = PEF22554_CHIPMAP;
            break;
            
        case E1ACTNUM:
            *(u8 *)args = quadFALC_e1_get_active_num();
            break;
            
        case E1GETBOARDVER:
            *(u8 *)args = quadFALC_gpio_get_board_ver();
            break;
            
        case E1GETCPLDVER:
            *(u8 *)args = quadFALC_gpio_get_cpld_ver();
            break;
            
        case CHIPMODE:
            {
                ST_CHIP_MODE_CMD chipcmd;
                int ret;
                int err;
                int i;

                ret = copy_from_user(&chipcmd, (void *)args, sizeof(chipcmd));
                if(ret)
                {
                    return -1;
                }

                if(chipcmd.chipno >= MAX_PEF22554_NUM)
                {
                    return -1;
                }

                switch(chipcmd.chipmode)
                {
                    case MODE_E1:
                        ret = quadFALC_e1_chip_init(chipcmd.chipno);
                        if(ret)
                        {
                            return -1;
                        }
                        
                        QuadFALCDev.pef22554info[chipcmd.chipno].chipmode = MODE_E1;
                        quadFALC_irq_cfg(chipcmd.chipno);
                        for(i=0; i<NUM_OF_FALC; i++)
                        {
                            QuadFALCDev.pef22554info[chipcmd.chipno].mode[i] = PRI;
                        }
                        
                        quadFALC_irq_thread_destroy();
                        err = quadFALC_irq_thread_create();
                        if( err )
                        {
                            return -1;
                        }
                        wake_up_process(QuadFALCDev.quadFALC_task);
                        break;
                    case MODE_T1:
                        ret = quadFALC_t1_chip_init(chipcmd.chipno);
                        if(ret)
                        {
                            return -1;
                        }
                        QuadFALCDev.pef22554info[chipcmd.chipno].chipmode = MODE_T1;
                        quadFALC_irq_cfg(chipcmd.chipno);
                        for(i=0; i<NUM_OF_FALC; i++)
                        {
                            QuadFALCDev.pef22554info[chipcmd.chipno].mode[i] = PRI;
                        }
                        quadFALC_irq_thread_destroy();
                        err = quadFALC_irq_thread_create();
                        if( err )
                        {
                            return -1;
                        }
                        wake_up_process(QuadFALCDev.quadFALC_task);
                        break;
                }
            }
            break;
            
        case E1RESET:
            {
                int i;

                for(i=0; i<MAX_PEF22554_NUM; i++)
                {
                    quadFALC_gpio_reset_chip(i);
                }
            }
            break;
            
        case E1SETCLKSRC:
            quadFALC_select_cpld_clk(args);
            break;
            
        case E1GETMODVER:
            {
                u32 modversion = MODVERSION;

                if(copy_to_user( (void *)args, &modversion, sizeof(modversion)))
                {
                    return -1;
                }
            }
            break;
            
        default:
            break;
    }
    
    return 0;
}


static int quadFALC_ioctl_minor(struct file *filp, unsigned int cmd, unsigned long args)
{
    ST_FILE_PRIVATE *file_pri;
    QFALC_REG_MAP   *falc;
    int port, chipno;
    u32 e1no = 0;

    
    file_pri = filp->private_data;

	falc = ptrreg[file_pri->channel];
	port = file_pri->channel;
	e1no = port % NUM_OF_FALC;
	chipno = port / NUM_OF_FALC;

    
    //printk(KERN_ERR "%s cmd:%x port:%d e1no:%d chipno:%d\n", __FUNCTION__, cmd, port, e1no, chipno);

	if( e1no > 3 )
	{
		debug("Invalid E1 no: %d\n", e1no);
	 	return -1;
	}


    switch(cmd)
    {
        case E1SENDDATA:
            {
                int i,len,ret;
                unsigned long sendcount = 0;
                struct st_data *buf_head = NULL, *buf_tail = NULL;
                ST_IO_SEND *sendmsg = NULL;

                sendmsg = (ST_IO_SEND *)args;

            	if( sendmsg->len > DATA_LEN_LIMIT || sendmsg->len <= 0 )
            	{
            		QuadFALCDev.pef22554info[chipno].sendfrmerr[e1no]++;
            		return 0;
            	}

            	/* 将消息添加到发送缓冲区中 */
            	ret = quadFALC_buffer_add(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no], sendmsg->buf, sendmsg->len, EN_END);
            	if( ret < 0)
            	{
            	    /* 若返回失败，表示缓冲区满，XPR中断出现故障，恢复XPR中断 */
                    quadFALC_buffer_clear(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no]);
                    SET_BIT8(&falc->wr.cmdr, CMDR_SRES);
                    QuadFALCDev.pef22554info[chipno].sendfrmfail[e1no] += MAX_DATA_NUM;

                    #if NONBLOCK
            		up(&QuadFALCDev.pef22554info[chipno].channel_sem[e1no]);
                    #endif
                    
            		return 0;
            	}

        	    sendcount = sendmsg->len;

            	/* 若在添加新消息之前设备缓冲区已为空，则开始首次发送，每次发送最大字节数为32 */
            	buf_head = QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].head;
            	buf_tail = QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].tail;


                if( 0 == sendflag[port] )
            	{
            		/* 判断设备对应通道的发送缓冲区是否为空 */
            		if( !quadFALC_buffer_is_empty(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no]) )
            		{
            			//printk(KERN_WARNING "kernel send data\n");
            			/* 等待设备处于就绪状态 */

                        sendflag[port] = 1;

            			/*开始发送，若等待发送的字节数大于32，则发送32字节；若
            			   等待发送的字节数不大于32，则发送最后的字节数，并通知
            			   接收端本条消息发送完毕*/
            			if( (buf_head->len - buf_head->cur ) > MAX_SEND_LEN )
            			{
            				/*发送数据*/
            				for(i=0; i<MAX_SEND_LEN; i++)
            				{
            					MEM_WRITE8((u8 *)&falc->wr.xfifo,(u8)buf_head->buf[buf_head->cur++]);
            				}
            				SET_BIT8(&falc->wr.cmdr, CMDR_XHF);
            			}
             			else
             			{
             				len = buf_head->len - buf_head->cur;
             				if(len < 0 || len > MAX_SEND_LEN)
             				{
             				    QuadFALCDev.pef22554info[chipno].sendfrmerr[e1no]++;
                                 printk(KERN_WARNING "#################%s %d\n", __FUNCTION__, __LINE__);
                                 return 0;
             				}
             				//debug("write send data len = %d\n",len);
             				for(i=0; i<len; i++)
             				{
             					MEM_WRITE8((u8 *)&falc->wr.xfifo,(u8)buf_head->buf[buf_head->cur++]);
             				}
             				mb();
             				if( buf_head == (QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].databuf + QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].size) )
                 			{
                     			QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].head = QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].databuf;
                 			}
                 			else
                 			{
                     			QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].head++;
                 			}
 
             				SET_BIT8(&falc->wr.cmdr, CMDR_XHF | CMDR_XME);
             				wake_up_interruptible(&QuadFALCDev.pef22554info[chipno].wq[e1no]);
             			}
            		}
            	}

                sendmsg->ret = sendcount;
            }
            break;
            
        case E1SENDR2SIG:
            {
                int ret;
                unsigned long sendcount = 0;
                ST_IO_SEND *sendmsg = NULL;

                sendmsg = (ST_IO_SEND *)args;

                if( sendmsg->len > sizeof(R2_MSG_ST) || sendmsg->len <= 0 )
                {
                    //up(&QuadFALCDev.channel_sem[e1no]);
                    QuadFALCDev.pef22554info[chipno].sendfrmerr[e1no]++;
                    return 0;
                }

                /* 将消息添加到发送缓冲区中,XMB中断每隔2ms产生 */
                ret = quadFALC_buffer_add(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no], sendmsg->buf, sendmsg->len, EN_END);
                if( ret < 0)
                {
                    /*若返回失败，表示缓冲区满*/
                    quadFALC_buffer_clear(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no]);
                    QuadFALCDev.pef22554info[chipno].sendfrmfail[e1no]++;

                    #if NONBLOCK
                    up(&QuadFALCDev.pef22554info[chipno].channel_sem[e1no]);
                    #endif
                    
                    return 0;
                }

                sendcount = sendmsg->len;
                sendmsg->ret = sendcount;
            }
            break;
            
        case E1RECVR2SIG:
            {
                READ_CAS_ST *p_read_cas_buf = (READ_CAS_ST *)args;
                R2_MSG_ST *p_r2_msg = NULL;
                struct st_data buftmp;

                p_read_cas_buf->ret_num = 0;
                p_r2_msg = (R2_MSG_ST *)p_read_cas_buf->buf;
                while(p_read_cas_buf->ret_num < p_read_cas_buf->max_num)
                {
                    /*检查接收缓冲区是否为空*/
                    if( quadFALC_buffer_is_empty(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no]) )
                    {
                        break;;
                    }

                    /*将设备中的数据拷贝到用户缓冲区中，并将接收缓冲区的
                       头指针指向下一个单元，表示已成功读取完一条消息*/
                    quadFALC_buffer_fetch(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no], &buftmp);
                    if(copy_to_user(p_r2_msg++, buftmp.buf, buftmp.len))
                    {
                        printk(KERN_WARNING "copy to user error\n");
                    }
                    p_read_cas_buf->ret_num++;
                }
            }
            break;
            
        case E1LOOP:
            {
                ST_E1_CMD_ARG *e1cmdp = (ST_E1_CMD_ARG *)args;
                u8  loop = 0, mode = 0;
                u32 tsno;

                
        		/*设置通道为非loop 模式*/
                if( NOLOOP == e1cmdp->mode )
                {
                    loop = E1_NO_LOOP;
                    quadFALC_e1_loop_ctrl_set((u8)port,(u8)loop, 0, 0);
                }
        		/*设置E1 的channel loop 模式*/
        		else if( PCM1 == e1cmdp->mode )
        		{
        		   // cmd mode 2
        	       loop = E1_CHANNEL_LOOP;
        	       if( E1_ON == e1cmdp->enable )
        		   {
        	           	mode = TRUE;
        		   }
        		   else if( E1_OFF == e1cmdp->enable )
        		   {
        	           	mode = FALSE;
        		   }
        		   else
        		   {
        	           	debug("enable must be ON or OFF\n");
        		   	    return -1;
        		   }
        	       tsno = e1cmdp->tsno;
        	       if( tsno > 31 )
        	       {
        	            debug("Invalid Ts no.\n");
        	            return -1;
        	       }
        	       quadFALC_e1_loop_ctrl_set((u8)port,(u8)loop, mode, (u8)tsno);
        		}
        		else
        		{
        		   /*设置通道为remote loop 模式*/
        		   if( E10 == e1cmdp->mode )
        		   {
        		       loop = E1_REMOTE_LOOP;
        		   }
        		   /*设置通道为local loop 模式*/
        		   else if( PCM0 == e1cmdp->mode )
        		   {
        		       loop = E1_LOCAL_LOOP;
        		   }
        		   /*设置通道为payload loop 模式*/
        		   else if( E11 == e1cmdp->mode )
        		   {
        		       loop = E1_PAYLOAD_LOOP;
        		   }
        		   else
        		   {
        		       debug("mode must be E10|E11|PCM0|PCM1\n");
        		       return -1;
        		   }

        		   if( E1_ON == e1cmdp->enable )
        		   {
        		       mode = TRUE;
        		   }
        		   else if( E1_OFF == e1cmdp->enable )
        		   {
        		       mode = FALSE;
        		   }
        		   else
        		   {
        		       debug("enable must be ON or OFF\n");
        		       return -1;
        		   }
        		   quadFALC_e1_loop_ctrl_set((u8)port,(u8)loop, mode, 0);

	            }
            }
            break;
            
        case E1FRAMEFORMAT:
            {
                ST_E1_CMD_ARG *e1cmdp = (ST_E1_CMD_ARG *)args;
                
        		switch( e1cmdp->mode )
        		{
        			case DOUBLEFRAME:
                    	quadFALC_e1_framer_cfg(port,FRAMING_DOUBLEFRAME,FRAMING_DOUBLEFRAME);
                    	break;

                	case MULTIFRAME:
                    	quadFALC_e1_framer_cfg(port,FRAMING_MULTIFRAME_TX,FRAMING_MULTIFRAME_RX);
                    	break;

                	case MODMULTIFRAME:
                    	quadFALC_e1_framer_cfg(port,FRAMING_MULTIFRAME_MOD,FRAMING_MULTIFRAME_MOD);
                    	break;

                	default:
                    	quadFALC_e1_framer_cfg(port,FRAMING_MULTIFRAME_TX,FRAMING_MULTIFRAME_RX);
                    	break;
        		}
            }
            break;
            
        case E1READDATA:
            {
                ST_E1_ERR_STATICS * pargs = (ST_E1_ERR_STATICS *)args;
                
        	    pargs->sendfrmerr  = QuadFALCDev.pef22554info[chipno].sendfrmerr[e1no];
        	    pargs->sendfrmfail = QuadFALCDev.pef22554info[chipno].sendfrmfail[e1no];
        	    pargs->recvfrmerr  = QuadFALCDev.pef22554info[chipno].recvfrmerr[e1no];
        	    pargs->recvfrmfail = QuadFALCDev.pef22554info[chipno].recvfrmfail[e1no];
        	    pargs->intdeadcnt  = QuadFALCDev.pef22554info[chipno].intdeadcnt[e1no];
        	    pargs->freezecnt   = QuadFALCDev.pef22554info[chipno].freezecnt[e1no];
        	    pargs->rpfcnt      = QuadFALCDev.pef22554info[chipno].rpfcnt[e1no];
        	    pargs->rmecnt      = QuadFALCDev.pef22554info[chipno].rmecnt[e1no];
        	    pargs->casccnt     = QuadFALCDev.pef22554info[chipno].casccnt[e1no];
        	    pargs->xmbcnt      = QuadFALCDev.pef22554info[chipno].xmbcnt[e1no];
            }
            break;
            
        case E1FRAMEERRCNT:
            {
                u8 lowbyte = 0, highbyte = 0;
                
                MEM_READ8(&falc->rd.fecl, &lowbyte);
                MEM_READ8(&falc->rd.fech, &highbyte);
                *(u16 *)args = ((highbyte << 8) & 0xff00) | lowbyte;
            }
            break;
         	
	    case E1CODEVIOCNT:
        	{
        		u8 lowbyte = 0, highbyte = 0;
                
                MEM_READ8(&falc->rd.cvcl, &lowbyte);
                MEM_READ8(&falc->rd.cvch, &highbyte);
                *(u16 *)args = ((highbyte << 8) & 0xff00) | lowbyte;
        	}
            break;

	    case E1FRAMECRCERRCNT:
        	{
        		u8 lowbyte = 0, highbyte = 0;
                
                MEM_READ8(&falc->rd.cec1l, &lowbyte);
                MEM_READ8(&falc->rd.cec1l, &highbyte);
                *(u16 *)args = ((highbyte << 8) & 0xff00) | lowbyte;
        	}
            break;

    	case E1EBITERRCNT:
        	{
        		u8 lowbyte = 0, highbyte = 0;
                
                MEM_READ8(&falc->rd.ebcl, &lowbyte);
                MEM_READ8(&falc->rd.ebcl, &highbyte);
                *(u16 *)args = ((highbyte << 8) & 0xff00) | lowbyte;
        	}
            break;

    	case E1LINKSTATUS:
        	{
                u8 reg_val;

                
        		MEM_READ8(&falc->rd.frs0, &reg_val);
        		if( reg_val & 0x80 )
        		{
                    *(u32 *)args = CHANNEL_DOWN;
        		}
        		else
        		{
        			*(u32 *)args = CHANNEL_UP;
        		}
        	}
            break;
            
    	case E1INFOCNTALL:
            {   
        		*(u32 *)args = QuadFALCDev.infocntall;
            }
            break;

	    case E1INFOCNT:
        	{
        		*(u32 *)args = QuadFALCDev.pef22554info[chipno].infocnt[e1no];
        	}
            break;
            
    	case E1GETSTATUS:
        	{
        		MEM_READ8(&falc->rd.frs0, (u8 *)args);
        	}
            break;
            
	    case E1GETSA4STATUS:
        	{
        		MEM_READ8(&falc->rd.rsa4_rdl1, (u8 *)args);
        	}
            break;
            
	    case E1GETSA5STATUS:
        	{
        		MEM_READ8(&falc->rd.rsa5_rdl2, (u8 *)args);
        	}
            break;
            
    	case E1GETSA6STATUS:
        	{
        		MEM_READ8(&falc->rd.rsa6_rdl3, (u8 *)args);
        	}
            break;
            
    	case E1GETSA7STATUS:
        	{
        		MEM_READ8(&falc->rd.rsa7, (u8 *)args);
        	}
            break;
	    case E1GETSA8STATUS:
        	{
        		MEM_READ8(&falc->rd.rsa8, (u8 *)args);
        	}
            break;

	    case E1WRITECAS:
        	{
                u8 *casp;
                int i;
                
        		casp = (u8 *)args;
                
        		for(i=0; i<16; i++)
        		{
        			MEM_WRITE8(&falc->wr.xs[i], casp[i]);
        		}
        	}
            break;

    	case E1READCAS:
        	{
                u8 *casp;
                
        		casp = (u8 *)args;
        		memcpy(casp, casrs[chipno][e1no], 16);
        	}
            break;
    
        case E1CHANGETS:
        	{
                ST_E1_CMD_ARG *e1cmdp = (ST_E1_CMD_ARG *)args;
                
        		MEM_WRITE8(&falc->rdWr.rtr1, 0);
                MEM_WRITE8(&falc->rdWr.ttr1, 0);
                MEM_WRITE8(&falc->rdWr.rtr2, 0);
                MEM_WRITE8(&falc->rdWr.ttr2, 0);
                MEM_WRITE8(&falc->rdWr.rtr3, 0);
                MEM_WRITE8(&falc->rdWr.ttr3, 0);
                MEM_WRITE8(&falc->rdWr.rtr4, 0);
                MEM_WRITE8(&falc->rdWr.ttr4, 0);
                if(e1cmdp->tsno >= 0 && e1cmdp->tsno < 8)
                {
        		    MEM_WRITE8(&falc->rdWr.rtr1, RTRTS[e1cmdp->tsno]);
        		    MEM_WRITE8(&falc->rdWr.ttr1, TTRTS[e1cmdp->tsno]);
        		}
        		else if(e1cmdp->tsno >= 8 && e1cmdp->tsno < 16)
        		{
                    MEM_WRITE8(&falc->rdWr.rtr2, RTRTS[e1cmdp->tsno]);
        		    MEM_WRITE8(&falc->rdWr.ttr2, TTRTS[e1cmdp->tsno]);
        		}
        		else if(e1cmdp->tsno >= 16 && e1cmdp->tsno < 24)
        		{
                    MEM_WRITE8(&falc->rdWr.rtr3, RTRTS[e1cmdp->tsno]);
        		    MEM_WRITE8(&falc->rdWr.ttr3, TTRTS[e1cmdp->tsno]);
        		}
        		else if(e1cmdp->tsno >= 24 && e1cmdp->tsno < 32)
        		{
                    MEM_WRITE8(&falc->rdWr.rtr4, RTRTS[e1cmdp->tsno]);
        		    MEM_WRITE8(&falc->rdWr.ttr4, TTRTS[e1cmdp->tsno]);
        		}

        	}
            break;
            
    	case E1CLOSETS:
        	{
        		MEM_WRITE8(&falc->rdWr.rtr1, 0);
                MEM_WRITE8(&falc->rdWr.ttr1, 0);
                MEM_WRITE8(&falc->rdWr.rtr2, 0);
                MEM_WRITE8(&falc->rdWr.ttr2, 0);
                MEM_WRITE8(&falc->rdWr.rtr3, 0);
                MEM_WRITE8(&falc->rdWr.ttr3, 0);
                MEM_WRITE8(&falc->rdWr.rtr4, 0);
                MEM_WRITE8(&falc->rdWr.ttr4, 0);
        	}
            break;
            
        case E1RESET:
        	{
        		quadFALC_gpio_reset_chip(chipno);
        	}
            break;
            
	    case E1NUM:
        	{
                *(unsigned char *)args = PEF22554_CHIPMAP;
        	}
            break;

        case E1ISSURGE:
        	{
                if( TRUE == quadFALC_e1_rclk_is_surge() )
                {
                    *(unsigned char *)args = 1;
                }
                else
                {
                    *(unsigned char *)args = 0;
                }
        	}
            break;

        case E1SSMCFG:
        	{
                ST_SSM_CFG *ssmcfgp;
                
                ssmcfgp = (ST_SSM_CFG *)args;
                quadFALC_e1_ssm_cfg(ssmcfgp->port, ssmcfgp->mode, ssmcfgp->sabit, ssmcfgp->ssm);
        	}
            break;

	    case E1PORTISCRC4:
            {
                if( TRUE == quadFALC_e1_port_is_crc4(port) )
                {
                    *(unsigned char *)args = 1;
                }
                else
                {
                    *(unsigned char *)args = 0;
                }
            }
            break;

        case E1RCLKSEL:
            {
                quadFALC_e1_rclk_select(port);
            }
            break;

        case E1RCLKSELSURGE:
            {
                quadFALC_e1_rclk_select_surge();
            }
            break;

        case E1CLOCKCFG:
            {
                quadFALC_e1_clocking_cfg(port);
            }
            break;

        case E1LOOPMODE:
            {
                *(u8 *)args = quadFALC_e1_loop_ctrl_read(port);
            }
            break;

        case E1RCLKRDPORT:
            {
                *(u8 *)args = quadFALC_e1_rclk_read_port();
            }
            break;

        case E1STARTFISU:
            {
                //MEM_WRITE8(&falc->rdWr.mode, MODE_MDS0 | MODE_HRAC);
                //SET_BIT8(&falc->rdWr.ccr2, CCR2_RADD);
                SET_BIT8(&falc->rdWr.ccr5, CCR5_AFX);
            }
            break;
        

        case E1STOPFISU:
            {
                //CLEAR_BIT8(&falc->rdWr.ccr2, CCR2_RADD);
                CLEAR_BIT8(&falc->rdWr.ccr5, CCR5_AFX);
                //MEM_WRITE8(&falc->rdWr.mode, MODE_MDS2 | MODE_HRAC);
            }
            break;

        case E1FDSTAT:
            {
                switch( args )
                {
                    case 0:
                        QuadFALCDev.pef22554info[chipno].e1_use_state[e1no] = FDUNUSED;
                        quadFALC_buffer_clear(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no]);
                        quadFALC_buffer_clear(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no]);
                        break;
                    case 1:
                        QuadFALCDev.pef22554info[chipno].e1_use_state[e1no] = FDUSED;
                        break;
                    default:
                        break;
                }
            }
            break;

        case E1LED:
            {
                quadFALC_gpio_led_cfg(port, args);
            }
            break;

        case E1SETREGVALUE:
            {
                ST_E1_REG cmd;
                u32 addr;

                memcpy(&cmd, (void *)args, sizeof(cmd));

                addr = e1no * 0x100 + cmd.regaddr;
                MEM_WRITE8((u8 *)addr, cmd.regvalue);
            }
            break;

        case E1READREGVALUE:
            {
                ST_E1_REG *cmd = NULL;
                u32 addr;

                cmd = (ST_E1_REG *)args;

                addr = e1no * 0x100 + cmd->regaddr;
                MEM_READ8((u8 *)addr, &cmd->regvalue);
            }
            break;

        case E1GETMODE:
            {
                if(0 != copy_to_user((void *)args, &QuadFALCDev.pef22554info[chipno].mode[e1no], sizeof(unsigned long)))
                {
                    printk(KERN_WARNING "copy to user failed\n");
                    return -1;
                }
            }
            break;

    	case E1MODE:
        	{
                ST_E1_CMD_ARG *e1cmdp = (ST_E1_CMD_ARG *)args;
                int i, ret;
                
        		if( QuadFALCDev.pef22554info[chipno].chipmode != e1cmdp->chipmode)
        		{
        			quadFALC_gpio_reset_chip(chipno);
                    
        			switch(e1cmdp->chipmode)
        			{
                        case MODE_E1:
                            ret = quadFALC_e1_chip_init(0);
                            if(ret)
                            {
                                return -1;
                            }
                            QuadFALCDev.pef22554info[chipno].chipmode = MODE_E1;
                            quadFALC_irq_cfg(0);
                            break;
                        case MODE_T1:
                            ret = quadFALC_t1_chip_init(0);
                            if(ret)
                            {
                                return -1;
                            }
                            QuadFALCDev.pef22554info[chipno].chipmode = MODE_T1;
                            quadFALC_irq_cfg(0);
                            break;

        			}
        			QuadFALCDev.pef22554info[chipno].mode[e1no] = e1cmdp->chipmode;
        		}

        		switch( e1cmdp->mode )
        		{
        			case SS7:
        				printk(KERN_WARNING "set mode ss7\n");
                        
        				CLEAR_BIT8(&falc->rdWr.ccr2, CCR2_RADD);
        				CLEAR_BIT8(&falc->rdWr.ccr5, CCR5_AFX);
        				CLEAR_BIT8(&falc->rdWr.mode, MODE_MDS0 | MODE_HRAC);
        				MEM_WRITE8(&falc->rdWr.mode, MODE_MDS0 | MODE_HRAC);
        				SET_BIT8(&falc->rdWr.ccr2, CCR2_RADD);
        				SET_BIT8(&falc->rdWr.ccr1, CCR1_EITS);
                        quadFALC_buffer_clear(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no]);
                        quadFALC_buffer_clear(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no]);
        			    //SET_BIT8(&falc->rdWr.ccr2, CCR2_RCRC);
        			    //SET_BIT8(&falc->rdWr.ccr5, CCR5_AFX);
        			    QuadFALCDev.pef22554info[chipno].mode[e1no] = SS7;
        			    CLEAR_BIT8(&falc->rdWr.xsp_fmr5, XSP_CASEN);
        			    if(MODE_E1 == QuadFALCDev.pef22554info[chipno].chipmode)
        			    {
            			    SET_BIT8(&falc->rdWr.imr0,IMR0_CASC);
            		        SET_BIT8(&falc->rdWr.imr1,IMR1_XMB);
        		        }
        		        else if(MODE_T1 == QuadFALCDev.pef22554info[chipno].chipmode)
        		        {
                            SET_BIT8(&falc->rdWr.imr0, IMR0_RSC);
                            SET_BIT8(&falc->rdWr.imr1, IMR1_CASE);
        		        }
        				break;
        			case SS1:
        				printk(KERN_WARNING "set mode ss1\n");
        				MEM_WRITE8(&falc->rdWr.mode, MODE_MDS2 | MODE_HRAC);
        				mb();
        				//SET_BIT8(&falc->rdWr.sic3, SIC3_DAF);
        				CLEAR_BIT8(&falc->rdWr.ccr1, CCR1_EITS);
        				mb();
        				QuadFALCDev.pef22554info[chipno].mode[e1no] = SS1;
        				SET_BIT8(&falc->rdWr.xsp_fmr5, XSP_CASEN);
        				mb();
        				if(MODE_E1 == QuadFALCDev.pef22554info[chipno].chipmode)
        				{
        				    u8  tmp, rsp1, rsp2;
                            u32 i;
                                                        
                            MEM_READ8(&falc->rd.isr0, &tmp);
                            mb();
                            MEM_READ8(&falc->rd.rsp1, &rsp1);
                            mb();
                            MEM_READ8(&falc->rd.rsp2, &rsp2);
                            mb();
            				CLEAR_BIT8(&falc->rdWr.imr0,IMR0_CASC);
            				mb();
            		        CLEAR_BIT8(&falc->rdWr.imr1,IMR1_XMB);
            		        mb();
            		        MEM_WRITE8(&falc->wr.xs[0], 0x0b);
            		        mb();
            		        MEM_WRITE8(&falc->wr.xs[0], 0x0b);
            		        mb();
            		        casxs[chipno][e1no][0] = 0x0b;
                            
                            for(i=1; i<16; i++)
                            {
                                MEM_WRITE8(&falc->wr.xs[i], 0x99);
                                mb();
                                MEM_WRITE8(&falc->wr.xs[i], 0x99);
                                mb();
                		        casxs[chipno][e1no][i] = 0x99;
                            }
                        }
                        else if(MODE_T1 == QuadFALCDev.pef22554info[chipno].chipmode)
                        {
                            u32 i;
                            
                            CLEAR_BIT8(&falc->rdWr.imr0, IMR0_RSC);
                            CLEAR_BIT8(&falc->rdWr.imr1, IMR1_CASE);
                            for(i=0; i<12; i++)
                            {
                                MEM_WRITE8(&falc->wr.xs[i], 0x99);
                                casxs[chipno][e1no][i] = 0x99;
                            }
                        }
                        
                        mb();
                        quadFALC_buffer_clear(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no]);
                        quadFALC_buffer_clear(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no]);
                        for(i=0; i<16; i++)
                        {
                            casrs[chipno][e1no][i] = 0x99;
                        }
        				break;
        			case PRI:
        				printk(KERN_WARNING "set mode PRI\n");
        				MEM_WRITE8(&falc->rdWr.mode, MODE_MDS2|MODE_HRAC);
        				SET_BIT8(&falc->rdWr.ccr1, CCR1_EITS);
        				QuadFALCDev.pef22554info[chipno].mode[e1no] = PRI;
        				CLEAR_BIT8(&falc->rdWr.xsp_fmr5, XSP_CASEN);
                        if(MODE_E1 == QuadFALCDev.pef22554info[chipno].chipmode)
                        {
            				SET_BIT8(&falc->rdWr.imr0,IMR0_CASC);
            		        SET_BIT8(&falc->rdWr.imr1,IMR1_XMB);
        		        }
        		        else if(MODE_T1 == QuadFALCDev.pef22554info[chipno].chipmode)
        		        {
                            SET_BIT8(&falc->rdWr.imr0, IMR0_RSC);
                            SET_BIT8(&falc->rdWr.imr1, IMR1_CASE);
        		        }
        				break;
        			default:
        				MEM_WRITE8(&falc->rdWr.mode, MODE_MDS2|MODE_HRAC);
        				SET_BIT8(&falc->rdWr.ccr1, CCR1_EITS);
        				QuadFALCDev.pef22554info[chipno].mode[e1no] = PRI;
        				CLEAR_BIT8(&falc->rdWr.xsp_fmr5, XSP_CASEN);
        				if(MODE_E1 == QuadFALCDev.pef22554info[chipno].chipmode)
        				{
            				SET_BIT8(&falc->rdWr.imr0,IMR0_CASC);
            		        SET_BIT8(&falc->rdWr.imr1,IMR1_XMB);
        		        }
        		        else if(MODE_T1 == QuadFALCDev.pef22554info[chipno].chipmode)
        		        {
                            SET_BIT8(&falc->rdWr.imr0, IMR0_RSC);
                            SET_BIT8(&falc->rdWr.imr1, IMR1_CASE);
        		        }
        				break;
	            }
            }
            break;
        case E1LINECODE:
            {
                ST_E1_CMD_ARG *e1cmdp = (ST_E1_CMD_ARG *)args;

                quadFALC_e1_lineinterface_cfg(port, SLAVE_MODE, e1cmdp->mode);

                // After changing transmit line code, a transmitter software reset is required
                SET_BIT8(&falc->wr.cmdr, CMDR_XRES);

                // After changing receive line code, a receiver software reset is required
                SET_BIT8(&falc->wr.cmdr, CMDR_RRES);
            }
            break;
        default:
            break;
    }

    return 0;
}

static long quadFALC_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
    ST_FILE_PRIVATE *file_pri;

    file_pri = filp->private_data;
    
    //printk(KERN_ERR "%s major:%d\n", __FUNCTION__, file_pri->major);

    if(quadFALC_gen_major == file_pri->major)
    {
        quadFALC_ioctl_major(cmd, args);
    }
    else
    {
        quadFALC_ioctl_minor(filp, cmd, args);
    }
    
	return 0;
}

static unsigned int quadFALC_poll(struct file *filp, struct poll_table_struct *table)
{
	ST_FILE_PRIVATE *file_priv;
	int e1no, chipno;
	unsigned int mask = 0;

    
	file_priv = filp->private_data;

    if(quadFALC_gen_major == file_priv->major)
    {
        return 0;
    }
    
	e1no   = file_priv->channel % NUM_OF_FALC;
	chipno = file_priv->channel / NUM_OF_FALC;
    
	if(chipno >= MAX_PEF22554_NUM || e1no >= NUM_OF_FALC)
	{
        printk(KERN_WARNING "%s %d\n", __FUNCTION__, __LINE__);
        return 0;
	}

	//schedule();
	/* 将设备加入内核等待队列 */
	poll_wait(filp, &QuadFALCDev.pef22554info[chipno].rq[e1no], table);
	poll_wait(filp, &QuadFALCDev.pef22554info[chipno].wq[e1no], table);

	/* 若接收缓冲区不为空，设备可读；若发送缓冲区不为满，设备可写 */
	if( !quadFALC_buffer_is_empty(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no]) )
	{
		mask |= POLLIN;
	}
	if( !quadFALC_buffer_is_full(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no]) )
	{
		mask |= POLLOUT;
	}

	return mask;

}

static struct file_operations quadFALC_ops =
{
	.open = quadFALC_open,
	.read = quadFALC_read,
	.write = quadFALC_write,
	.poll = quadFALC_poll,
	.unlocked_ioctl = quadFALC_ioctl,
	.release = quadFALC_release,
};

static int quadFALC_addr_remap(void)
{
    int i;
    

    for(i=0; i<MAX_PEF22554_NUM; i++)
    {
        PEF22554_CHIPMAP |= (1 << i) & E1_NUM_MASK;
        
        QuadFALCDev.pef22554info[i].virtual_base_source = 0x0;
    }

    return 0;
}

static void quadFALC_led_init(void)
{
    int i;
    for(i=0; i<MAX_PEF22554_NUM*NUM_OF_FALC; i++)
    {
        quadFALC_gpio_led_cfg(i, 0);
    }
}

static int quadFALC_chip_reset(void)
{
    int err, i, chipno;
    

    //printk(KERN_ERR "%s\n", __FUNCTION__);
    
    for(chipno=0; chipno<MAX_PEF22554_NUM; chipno++)
    {
        u8 data;
        
        quadFALC_gpio_reset_chip(chipno);
        
        quadFALC_pef_write_reg(0x1c, 0xc);
        quadFALC_pef_read_reg(0x1c, &data);      
        if(0xc != data)
        {
            debug("reset error\n");
            err = -ENODEV;
            return err;
        }
        
        quadFALC_gpio_reset_chip(chipno);
        
        quadFALC_pef_read_reg(0x1c, &data);      
        if(0 != data)
        {
            debug("reset error\n");
            err = -ENODEV;
            return err;
        }
    }

    for(chipno=0; chipno<MAX_PEF22554_NUM; chipno++)
    {
        for (i = 0; i < NUM_OF_FALC; i++)
        {
            ptrreg[NUM_OF_FALC*chipno+i] = (QFALC_REG_MAP*)(NULL + i*0x100);
        }
    }

    //return 0;

    /*初始化22554，为兼容以前的应用层软件，暂时不去除*/
    for(chipno=0; chipno<MAX_PEF22554_NUM; chipno++)
    {
        err = quadFALC_e1_chip_init(chipno);
        if( err < 0 )
        {
            debug("quadFALC register failed\n");
            err = -ENODEV;
            return err;
        }

        QuadFALCDev.pef22554info[chipno].chipmode = MODE_E1;
        for(i=0; i<NUM_OF_FALC; i++)
        {
            QuadFALCDev.pef22554info[chipno].mode[i] = PRI;
        }
    }

    return 0;
}

static int quadFALC_list_init(void)
{
    int i, chipno;

    //printk(KERN_ERR "%s\n", __FUNCTION__);

    for(chipno=0; chipno<MAX_PEF22554_NUM; chipno++)
    {
        /*初始化设备写同步信号量，初始化定时器*/
        for( i=0; i<NUM_OF_FALC; i++)
        {
            sema_init(&QuadFALCDev.pef22554info[chipno].sem[i], 1);
            sema_init(&QuadFALCDev.pef22554info[chipno].channel_sem[i], 1);
    	}



    	for(i=0; i<NUM_OF_FALC; i++)
    	{
        	init_waitqueue_head(&QuadFALCDev.pef22554info[chipno].rq[i]);
        	init_waitqueue_head(&QuadFALCDev.pef22554info[chipno].wq[i]);
    	}
	}

	/*初始化等待队列*/
	init_waitqueue_head(&QuadFALCDev.q);
	spin_lock_init(&QuadFALCDev.lock);

    return 0;
}


static int quadFALC_drv_init(void)
{
    int ret, i, j;
	struct device *class_device;

#if defined(PRODUCT_MTG2500USER)
    if(board_cpuid == 1)
    {
        return 0;
    }
#endif

    /*自动分配的设备节点号与ubi冲突，这里还是选择固定的*/
#if 0
    /* 注册通用信息处理设备 */
    ret = alloc_chrdev_region(&quadFALC_gen_devno, 0, 1, QUADFALC_GEN_NAME);
    if(ret < 0)
    {
        printk(KERN_ERR "%s alloc_chrdev_region error!\n", __FUNCTION__);
        goto err_0;
    }
    quadFALC_gen_major = MAJOR(quadFALC_gen_devno);
    printk(KERN_ERR "%s alloc major:%d\n", __FUNCTION__, MAJOR(quadFALC_gen_devno));
#else
    quadFALC_gen_devno = MKDEV(quadFALC_gen_major, 0);
    ret = register_chrdev_region(quadFALC_gen_devno, 1, QUADFALC_GEN_NAME);
    if(ret < 0)
    {
        printk(KERN_ERR "%s alloc_chrdev_region error!\n", __FUNCTION__);
        goto err_0;
    }
#endif
    
    cdev_init(&quadFALC_gen_cdev, &quadFALC_ops);
    ret = cdev_add(&quadFALC_gen_cdev, quadFALC_gen_devno, 1);
    if(ret < 0)
    {
        printk(KERN_ERR "%s cdev_add error!\n", __FUNCTION__);
        goto err_1;
    }
    
    quadFALC_gen_class = class_create(THIS_MODULE, QUADFALC_GEN_NAME);
    if(IS_ERR(quadFALC_gen_class))
    {
        printk(KERN_ERR "%s class_create error!\n", __FUNCTION__);
        ret = -EFAULT;
        goto err_2;
    }
    
    class_device = device_create(quadFALC_gen_class, NULL, quadFALC_gen_devno, NULL, QUADFALC_GEN_NAME);
    if(IS_ERR(class_device))
    {
        printk(KERN_ERR "%s device_create error!\n", __FUNCTION__);
        ret = -EFAULT;
        goto err_3;
    }

#if 0
    /* 注册通道信息处理设备 */
    ret = alloc_chrdev_region(&quadFALC_dev_devno, 0, 4, QUADFALC_DEV_NAME);
    if(ret < 0)
    {
        printk(KERN_ERR "%s alloc_chrdev_region error!\n", __FUNCTION__);
        goto err_4;
    }
    quadFALC_dev_major = MAJOR(quadFALC_dev_devno);
    printk(KERN_ERR "%s alloc major:%d\n", __FUNCTION__, MAJOR(quadFALC_dev_devno));
#else
    quadFALC_dev_devno = MKDEV(quadFALC_dev_major, 0);
    ret = register_chrdev_region(quadFALC_dev_devno, 4, QUADFALC_DEV_NAME);
    if(ret < 0)
    {
        printk(KERN_ERR "%s alloc_chrdev_region error!\n", __FUNCTION__);
        goto err_4;
    }
#endif
    
    cdev_init(&quadFALC_dev_cdev, &quadFALC_ops);
    ret = cdev_add(&quadFALC_dev_cdev, quadFALC_dev_devno, 4);
    if(ret < 0)
    {
        printk(KERN_ERR "%s cdev_add error!\n", __FUNCTION__);
        ret = -EFAULT;
        goto err_5;
    }
    
    quadFALC_dev_class = class_create(THIS_MODULE, QUADFALC_DEV_NAME);
    if(IS_ERR(quadFALC_dev_class))
    {
        printk(KERN_ERR "%s class_create error!\n", __FUNCTION__);
        ret = -EFAULT;
        goto err_6;
    }

    for(i=0; i<4; i++)
    {
        class_device = device_create(quadFALC_dev_class, NULL, MKDEV(quadFALC_dev_major, i), NULL, QUADFALC_DEV_NAME"%d", i);
        if(IS_ERR(class_device))
        {
            printk(KERN_ERR "%s device_create %s%d error!\n", __FUNCTION__, QUADFALC_DEV_NAME, i);
            ret = -EFAULT;
            goto err_7;
        }
    }

    /* 设备初始化 */
    quadFALC_addr_remap();
    
    ret = quadFALC_gpio_init();
    if(ret)
    {
        goto err_7;
    }
    
    quadFALC_led_init();

    ret = quadFALC_chip_reset();
    if(ret)
    {
        goto err_8;
    }

    for(j=0; j<MAX_PEF22554_NUM; j++)
	{
    	quadFALC_irq_cfg(j);
	}

    
    quadFALC_list_init();

    ret = quadFALC_buffer_init();
    if(ret)
    {
        goto err_8;
    }

    ret = quadFALC_irq_thread_create();
    if(ret)
    {
        debug("quadFALC register failed\n");
        goto err_8;
    }

    quadFALC_gpio_get_cpld_ver();

    
    printk(KERN_ERR "%s version:%s\n", __FUNCTION__, QUADFALC_VERSION);
    return 0;

err_8:
    quadFALC_gpio_exit();
err_7:
    if(i > 0)
    {
        for(j=0; j<i; j++)
        {
            device_destroy(quadFALC_dev_class, MKDEV(quadFALC_dev_major, j));
        }
    }
    class_destroy(quadFALC_dev_class);
err_6:
    cdev_del(&quadFALC_dev_cdev);
err_5:
    unregister_chrdev_region(quadFALC_dev_devno, 4);
err_4:
    device_destroy(quadFALC_gen_class, quadFALC_gen_devno);
err_3:
    class_destroy(quadFALC_gen_class);
err_2:
    cdev_del(&quadFALC_gen_cdev);
err_1:
    unregister_chrdev_region(quadFALC_gen_devno, 1);
err_0:
    return ret;
}

/*------------------------------------------------------------------------
  描述:  设备驱动模块注销

  输入参数:
                无

  输出参数:
                无

  配置:

  返回值:
                 无

  举例:
-------------------------------------------------------------------------*/
static void quadFALC_drv_exit(void)
{
    int i;

#if defined(PRODUCT_MTG2500USER)
    if(board_cpuid == 1)
    {
        return;
    }
#endif

    quadFALC_gpio_exit();
    quadFALC_buffer_free();
    quadFALC_irq_thread_destroy();
    

    /* 注销通用信息处理设备 */
    for(i=0; i<4; i++)
    {
        device_destroy(quadFALC_dev_class, MKDEV(quadFALC_dev_major, i));
    }
    class_destroy(quadFALC_dev_class);
    cdev_del(&quadFALC_dev_cdev);
    unregister_chrdev_region(quadFALC_dev_devno, 4);

    /* 注销通道信息处理设备 */
    device_destroy(quadFALC_gen_class, quadFALC_gen_devno);
    class_destroy(quadFALC_gen_class);
    cdev_del(&quadFALC_gen_cdev);
    unregister_chrdev_region(quadFALC_gen_devno, 1);

    
    printk(KERN_ERR "%s\n", __FUNCTION__);
}

module_init(quadFALC_drv_init);
module_exit(quadFALC_drv_exit);
MODULE_LICENSE("GPL");
MODULE_VERSION(QUADFALC_VERSION);

