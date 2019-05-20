 /******************************************************************************
        (c) COPYRIGHT 2016- by Dinstar technologies Co.,Ltd
                          All rights reserved.
File:relay_qos.c
Desc: qos ���������綶����ʱ���ݶ�qos����ʱ��Ϊ1s
note:�˴�qos�򵥴����ֹ����rtp�����쳣���ĺķ�cpu��

Modification history(no, author, date, desc)
spark 16-11-22create file
******************************************************************************/
#include <linux/sysctl.h>
#include <linux/errno.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/net.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include "relay.h"


 
//struct relay_qos_priv {
//	 struct timer_list qos_ticktimer;
//};

//static struct relay_qos_priv qos_priv;
//static atomic_t qos_timer_cnt;;


/*
��ȡqosͳ����Ϣ
*/
int relay_qos_statictis()
{
}

/*
qos ��ˮ��
skb:��������
cb:core ���ƹ����
*/
int relay_qos_handler(struct sk_buff *skb,struct relay_info *cb)
{
	
	if(cb->cur_dir == PIPELINE_DIR_IN)
	{
		//����ͳ��
		cb->totalin_cnt++;
		cb->curin_cnt++;
		// �����쳣����������
		if(cb->curin_cnt > cb->maxin_cnt)
		{
			if(jiffies - cb->pre_jiffies > 100)		// 1s
			{
				cb->curin_cnt = 0;
				cb->curout_cnt = 0;
				cb->pre_jiffies = jiffies;
				return RELAY_ACCEPT;	
			}
			else
			{
				return RELAY_DROP;
			}	
		}
		else	// ��������
		{
			return RELAY_ACCEPT;	
		}	
	}
	else
	{
		//����ͳ��
		cb->totalout_cnt++;
		cb->curout_cnt++;
		// �����쳣����������
		if(cb->curout_cnt > cb->maxout_cnt)
		{
			if(jiffies - cb->pre_jiffies > 100)
			{
				cb->curin_cnt = 0;
				cb->curout_cnt = 0;
				cb->pre_jiffies = jiffies;
				return RELAY_ACCEPT;	
			}
			else
			{
				return RELAY_DROP;
			}	
		}
		else	// ��������
		{
			return RELAY_ACCEPT;	
		}	
	}
	
	return RELAY_ACCEPT;	
}

/*
ע��һ��qosʵ��
*/
int relay_qos_register(struct relay_info *cb,int max_in,int max_out)
{
	cb->maxin_cnt = max_in;
	cb->curin_cnt = 0;
	cb->maxout_cnt = max_out;
	cb->curout_cnt = 0;
	cb->totalin_cnt = 0;
	cb->totalout_cnt = 0;
	cb->pre_jiffies = jiffies;
	
	return 1;
}

/*
ע��һ��qosʵ��
*/
int relay_qos_unregister()
{
	return 1;
}

#if 0
/*
 * 
 * handler for tick_timer
 */
static void relay_qos_tick_timer(unsigned long date)
{ 
	// ˢ�½���ͳ��
	atomic_inc(&qos_timer_cnt);
	qos_priv->qos_ticktimer.expires = jiffies + 100;
	add_timer(&qos_priv->qos_ticktimer);
}

#endif

int __init relay_qos_init(void)
{
	#if 0
    init_timer(&qos_priv.qos_ticktimer);
    qos_priv.qos_ticktimer.data     = 0;
	qos_priv.qos_ticktimer.expires  = jiffies + 100;		// 1000ms
	qos_priv.qos_ticktimer.function = relay_qos_tick_timer;
	atomic_set(&qos_timer_cnt,0);
	add_timer(&qos_priv->qos_ticktimer);
	#endif
}

void __exit relay_qos_exit(void)
{
	#if 0
	del_timer(&qos_priv.qos_ticktimer);
	#endif
}

