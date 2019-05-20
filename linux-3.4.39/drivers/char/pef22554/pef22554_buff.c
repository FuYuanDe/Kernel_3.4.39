/******************************************************************************
        (c) COPYRIGHT 2002-2003 by Shenzhen Allywll Information Co.,Ltd
                          All rights reserved.
File: pef22554.c
Desc:the source file of user config
Modification history(no, author, date, desc)
1.Holy 2003-04-02 create file
2.luke 2006-04-20 for AOS
******************************************************************************/
#include <linux/interrupt.h>
#include <linux/slab.h>

#include "pef22554_drv.h"


extern struct quadFALC_dev QuadFALCDev;
extern ST_E1_RECEIVE_DATA  stE1BufferRec[MAX_PEF22554_NUM*NUM_OF_FALC];
extern u8 casrs[MAX_PEF22554_NUM][NUM_OF_FALC][16];
extern u8 casxs[MAX_PEF22554_NUM][NUM_OF_FALC][16];
extern volatile int sendflag[MAX_PEF22554_NUM*NUM_OF_FALC];


/*------------------------------------------------------------------------
  描述:  检测缓冲区是否为空

  输入参数:
                buffer - 缓冲区地址

  输出参数:
                无

  配置:

  返回值:
                 1: 缓冲区为空
                 0: 缓冲区不为空

  举例:
                 buffer_is_empty(&QuadFALCDev.aos_buf_send[0])
                 检查通道0 的发送缓冲区是否为空
-------------------------------------------------------------------------*/
int quadFALC_buffer_is_empty(ST_BUFFER *buffer)
{

	if( buffer->head == buffer->tail)
	{
		return 1;
	}

	return 0;
}

/*------------------------------------------------------------------------
  描述:  检测缓冲区是否为满

  输入参数:
                buffer - 缓冲区地址

  输出参数:
                无

  配置:

  返回值:
                 1: 缓冲区为满
                 0: 缓冲区不为满

  举例:
                 buffer_is_full(&QuadFALCDev.aos_buf_receive[0])
                 检查通道0 的接收缓冲区是否为满
-------------------------------------------------------------------------*/
int quadFALC_buffer_is_full(ST_BUFFER *buffer)
{
	if( ((buffer->tail - buffer->head) == buffer->size) ||
		((buffer->head - buffer->tail) == 1) )
	{
		return 1;
	}

	return 0;
}

/*------------------------------------------------------------------------
  描述:  将消息添加到缓冲区中

  输入参数:
                buffer - 缓冲区地址
                srcbuf - 数据缓冲区
                length - 数据长度
                type - 数据类型

  输出参数:
                无

  配置:

  返回值:
                 0: 操作成功
                 -1: 操作失败

  举例:
                 add_to_buffer(&QuadFALCDev.aos_buf_receive[0], buf, 240, 0)
                 将buf中的消息添加到通道0的接收缓冲区，消息
                 长度为240，不指定类型
-------------------------------------------------------------------------*/
int quadFALC_buffer_add(ST_BUFFER *buffer, char *srcbuf, int length, EN_TYPE type)
{
	struct st_data *buf_tail = buffer->tail;


    if((buffer->size < 0) || (buffer->size > MAX_DATA_NUM))
    {
        printk(KERN_WARNING "buffer size is %d\n", buffer->size);
        return -1;
    }

    if((buffer->head < buffer->databuf) ||
        (buffer->head > buffer->databuf + buffer->size) ||
        (buffer->tail < buffer->databuf) ||
        (buffer->tail > buffer->databuf + buffer->size))
    {
        printk(KERN_WARNING "pointer error\n");
        return -1;
    }

    
	/*检查缓冲区是否为满*/
	if( quadFALC_buffer_is_full(buffer) )
	{
		printk(KERN_ERR "buffer is full\n");
		return -1;
	}

	/*将消息添加到缓冲区尾部*/
	buf_tail->len = length;
	//buf_tail->type = type;
	buf_tail->cur = 0;
	memcpy(buf_tail->buf, srcbuf, length);

	/*更新缓冲区尾指针，若指针到了缓冲区末尾，
	   将缓冲区尾指针指向缓冲区开始地址*/
	if( buffer->tail == (buffer->databuf + buffer->size) )
	{
		buffer->tail = buffer->databuf;
	}
	else
	{
		buffer->tail++;
	}
	buffer->len++;

	return 0;
}

/*------------------------------------------------------------------------
  描述:  从缓冲区中取出一条消息

  输入参数:
                buffer - 缓冲区地址

  输出参数:
                decbuf - 消息取出后的存放地址

  配置:

  返回值:
                 0: 操作成功
                 -1: 操作失败

  举例:
                 fetch_from_buffer(&QuadFALCDev.aos_buf_receive[0], buf)
                 将通道0 的接收缓冲区中的首条消息取出，
                 存放到buf 指定的地址
-------------------------------------------------------------------------*/
int quadFALC_buffer_fetch(ST_BUFFER *buffer, struct st_data *decbuf)
{
	struct st_data *buf_head = buffer->head;

    if((buffer->size < 0) || (buffer->size > MAX_DATA_NUM))
    {
        printk(KERN_WARNING "buffer size is %d\n", buffer->size);
        return -1;
    }

    if((buffer->head < buffer->databuf) ||
        (buffer->head > buffer->databuf + buffer->size) ||
        (buffer->tail < buffer->databuf) ||
        (buffer->tail > buffer->databuf + buffer->size))
    {
        printk(KERN_WARNING "pointer error\n");
        return -1;
    }

	/*检测缓冲区是否为空*/
	if( quadFALC_buffer_is_empty(buffer) )
	{
		return -1;
	}

	/*将缓冲区首指针指向的消息复制到decbuf 中*/
	decbuf->len = buf_head->len;
	//decbuf->type = buf_head->type;
	memcpy(decbuf->buf, buf_head->buf, buf_head->len);
	/*更新缓冲区首指针，若指针到了缓冲区末尾，
	   将缓冲区首指针指向缓冲区开始地址*/
	if( buffer->head == (buffer->databuf + buffer->size) )
	{
		buffer->head = buffer->databuf;
	}
	else
	{
		buffer->head++;
	}
	buffer->len--;

	return 0;
}

/*------------------------------------------------------------------------
  描述:  清空缓冲区

  输入参数:
                buffer - 缓冲区地址

  输出参数:

  配置:

  返回值:
                 0: 操作成功
                 -1: 操作失败

  举例:
                 clear_buffer(&QuadFALCDev.aos_buf_receive[0])
                 清空通道0的缓冲区
-------------------------------------------------------------------------*/
int quadFALC_buffer_clear(ST_BUFFER *buffer)
{
    buffer->head = buffer->databuf;
    buffer->tail = buffer->databuf;

    return 0;
}

struct st_data * quadFALC_buffer_get_empty_node(ST_BUFFER *buffer)
{
    struct st_data *buf_tail = buffer->tail;

    if(quadFALC_buffer_is_full(buffer))
    {
        return NULL;
    }

    return buf_tail;
}

void quadFALC_buffer_get_next_node(ST_BUFFER *buffer)
{
    if(quadFALC_buffer_is_full(buffer))
    {
        return;
    }

	/*更新缓冲区尾指针，若指针到了缓冲区末尾，
      将缓冲区尾指针指向缓冲区开始地址*/
	if( buffer->tail == (buffer->databuf + buffer->size) )
	{
		buffer->tail = buffer->databuf;
	}
	else
	{
		buffer->tail++;
	}
	buffer->len++;
}


/*------------------------------------------------------------------------
  描述:  为每个e1口分配一个发送缓冲区和接收缓冲区

  输入参数:
                无

  输出参数:
                无

  配置:

  返回值:
                 0:正常
                 -EBUSY:出错

  举例:
-------------------------------------------------------------------------*/
int quadFALC_buffer_init(void)
{
    int i, chipno;

    //printk(KERN_ERR "%s\n", __FUNCTION__);

	/*初始化各通道的临时接收缓冲区*/
	QuadFALCDev.infocntall = 0;
	for(chipno=0; chipno<MAX_PEF22554_NUM; chipno++)
	{
    	for (i=0; i<NUM_OF_FALC; i++)
    	{
    		QuadFALCDev.pef22554info[chipno].infocnt[i] = 0;
    		QuadFALCDev.pef22554info[chipno].e1_use_state[i] = FDUNUSED;

    		stE1BufferRec[i + NUM_OF_FALC * chipno].enBuffSta = E1_RX_BUFF_CLEAN;
            stE1BufferRec[i + NUM_OF_FALC * chipno].ptSend = 0;

            QuadFALCDev.pef22554info[chipno].aos_buf_send[i].databuf = NULL;
            QuadFALCDev.pef22554info[chipno].aos_buf_receive[i].databuf = NULL;

            sendflag[i + NUM_OF_FALC * chipno] = 0;
        }
    }

	/*初始化个通道的接收缓冲区和发送缓冲区*/
	for(chipno=0; chipno<MAX_PEF22554_NUM; chipno++)
	{
    	for(i=0; i<NUM_OF_FALC; i++)
    	{
    		QuadFALCDev.pef22554info[chipno].aos_buf_send[i].databuf = kmalloc((MAX_DATA_NUM + 1) * sizeof(struct st_data), GFP_KERNEL);
    		if( QuadFALCDev.pef22554info[chipno].aos_buf_send[i].databuf == NULL )
    		{
    			goto error0;
    		}
    		QuadFALCDev.pef22554info[chipno].aos_buf_send[i].head = QuadFALCDev.pef22554info[chipno].aos_buf_send[i].tail = QuadFALCDev.pef22554info[chipno].aos_buf_send[i].databuf;
    		QuadFALCDev.pef22554info[chipno].aos_buf_send[i].len = 0;
    		QuadFALCDev.pef22554info[chipno].aos_buf_send[i].size = MAX_DATA_NUM;

    		QuadFALCDev.pef22554info[chipno].aos_buf_receive[i].databuf = kmalloc((MAX_DATA_NUM + 1) * sizeof(struct st_data), GFP_KERNEL);
    		if( QuadFALCDev.pef22554info[chipno].aos_buf_receive[i].databuf == NULL )
    		{
    			goto error0;
    		}
    		QuadFALCDev.pef22554info[chipno].aos_buf_receive[i].head = QuadFALCDev.pef22554info[chipno].aos_buf_receive[i].tail = QuadFALCDev.pef22554info[chipno].aos_buf_receive[i].databuf;
    		QuadFALCDev.pef22554info[chipno].aos_buf_receive[i].len = 0;
    		QuadFALCDev.pef22554info[chipno].aos_buf_receive[i].size = MAX_DATA_NUM;
    	}
	}

	memset(casrs, 0, sizeof(casrs));
	memset(casxs, 0, sizeof(casxs));

	return 0;

error0:
    for(chipno=0; chipno<MAX_PEF22554_NUM; chipno++)
    {
        for(i=0; i<NUM_OF_FALC; i++)
        {
            if(NULL != QuadFALCDev.pef22554info[chipno].aos_buf_send[i].databuf)
            {
                kfree(QuadFALCDev.pef22554info[chipno].aos_buf_send[i].databuf);
            }
            if(NULL != QuadFALCDev.pef22554info[chipno].aos_buf_receive[i].databuf)
            {
                kfree(QuadFALCDev.pef22554info[chipno].aos_buf_receive[i].databuf);
            }
        }
    }
    return -EBUSY;
}


/*------------------------------------------------------------------------
  描述:  释放设备所有的发送缓冲区和接收缓冲区

  输入参数:
                无

  输出参数:
                无

  配置:

  返回值:
                 0:正常

  举例:
-------------------------------------------------------------------------*/
int quadFALC_buffer_free(void)
{
    int i, chipno;

    for(chipno=0; chipno<MAX_PEF22554_NUM; chipno++)
    {
        for(i=0; i<NUM_OF_FALC; i++)
        {
            kfree(QuadFALCDev.pef22554info[chipno].aos_buf_send[i].databuf);
            kfree(QuadFALCDev.pef22554info[chipno].aos_buf_receive[i].databuf);
        }
    }

    return 0;
}


