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
  ����:  ��⻺�����Ƿ�Ϊ��

  �������:
                buffer - ��������ַ

  �������:
                ��

  ����:

  ����ֵ:
                 1: ������Ϊ��
                 0: ��������Ϊ��

  ����:
                 buffer_is_empty(&QuadFALCDev.aos_buf_send[0])
                 ���ͨ��0 �ķ��ͻ������Ƿ�Ϊ��
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
  ����:  ��⻺�����Ƿ�Ϊ��

  �������:
                buffer - ��������ַ

  �������:
                ��

  ����:

  ����ֵ:
                 1: ������Ϊ��
                 0: ��������Ϊ��

  ����:
                 buffer_is_full(&QuadFALCDev.aos_buf_receive[0])
                 ���ͨ��0 �Ľ��ջ������Ƿ�Ϊ��
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
  ����:  ����Ϣ��ӵ���������

  �������:
                buffer - ��������ַ
                srcbuf - ���ݻ�����
                length - ���ݳ���
                type - ��������

  �������:
                ��

  ����:

  ����ֵ:
                 0: �����ɹ�
                 -1: ����ʧ��

  ����:
                 add_to_buffer(&QuadFALCDev.aos_buf_receive[0], buf, 240, 0)
                 ��buf�е���Ϣ��ӵ�ͨ��0�Ľ��ջ���������Ϣ
                 ����Ϊ240����ָ������
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

    
	/*��黺�����Ƿ�Ϊ��*/
	if( quadFALC_buffer_is_full(buffer) )
	{
		printk(KERN_ERR "buffer is full\n");
		return -1;
	}

	/*����Ϣ��ӵ�������β��*/
	buf_tail->len = length;
	//buf_tail->type = type;
	buf_tail->cur = 0;
	memcpy(buf_tail->buf, srcbuf, length);

	/*���»�����βָ�룬��ָ�뵽�˻�����ĩβ��
	   ��������βָ��ָ�򻺳�����ʼ��ַ*/
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
  ����:  �ӻ�������ȡ��һ����Ϣ

  �������:
                buffer - ��������ַ

  �������:
                decbuf - ��Ϣȡ����Ĵ�ŵ�ַ

  ����:

  ����ֵ:
                 0: �����ɹ�
                 -1: ����ʧ��

  ����:
                 fetch_from_buffer(&QuadFALCDev.aos_buf_receive[0], buf)
                 ��ͨ��0 �Ľ��ջ������е�������Ϣȡ����
                 ��ŵ�buf ָ���ĵ�ַ
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

	/*��⻺�����Ƿ�Ϊ��*/
	if( quadFALC_buffer_is_empty(buffer) )
	{
		return -1;
	}

	/*����������ָ��ָ�����Ϣ���Ƶ�decbuf ��*/
	decbuf->len = buf_head->len;
	//decbuf->type = buf_head->type;
	memcpy(decbuf->buf, buf_head->buf, buf_head->len);
	/*���»�������ָ�룬��ָ�뵽�˻�����ĩβ��
	   ����������ָ��ָ�򻺳�����ʼ��ַ*/
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
  ����:  ��ջ�����

  �������:
                buffer - ��������ַ

  �������:

  ����:

  ����ֵ:
                 0: �����ɹ�
                 -1: ����ʧ��

  ����:
                 clear_buffer(&QuadFALCDev.aos_buf_receive[0])
                 ���ͨ��0�Ļ�����
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

	/*���»�����βָ�룬��ָ�뵽�˻�����ĩβ��
      ��������βָ��ָ�򻺳�����ʼ��ַ*/
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
  ����:  Ϊÿ��e1�ڷ���һ�����ͻ������ͽ��ջ�����

  �������:
                ��

  �������:
                ��

  ����:

  ����ֵ:
                 0:����
                 -EBUSY:����

  ����:
-------------------------------------------------------------------------*/
int quadFALC_buffer_init(void)
{
    int i, chipno;

    //printk(KERN_ERR "%s\n", __FUNCTION__);

	/*��ʼ����ͨ������ʱ���ջ�����*/
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

	/*��ʼ����ͨ���Ľ��ջ������ͷ��ͻ�����*/
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
  ����:  �ͷ��豸���еķ��ͻ������ͽ��ջ�����

  �������:
                ��

  �������:
                ��

  ����:

  ����ֵ:
                 0:����

  ����:
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


