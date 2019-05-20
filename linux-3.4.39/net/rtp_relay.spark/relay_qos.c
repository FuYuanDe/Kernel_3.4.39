 /******************************************************************************
        (c) COPYRIGHT 2016- by Dinstar technologies Co.,Ltd
                          All rights reserved.
File:relay_qos.c
Desc: qos 处理考虑网络抖动延时，暂定qos处理时间为1s
note:此处qos简单处理防止单个rtp大量异常报文耗费cpu。

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
获取qos统计信息
*/
int relay_qos_statictis()
{
}

/*
qos 流水线
skb:待处理报文
cb:core 控制管理块
*/
int relay_qos_handler(struct sk_buff *skb,struct relay_info *cb)
{
	
	if(cb->cur_dir == PIPELINE_DIR_IN)
	{
		//流量统计
		cb->totalin_cnt++;
		cb->curin_cnt++;
		// 流量异常，丢弃报文
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
		else	// 流量正常
		{
			return RELAY_ACCEPT;	
		}	
	}
	else
	{
		//流量统计
		cb->totalout_cnt++;
		cb->curout_cnt++;
		// 流量异常，丢弃报文
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
		else	// 流量正常
		{
			return RELAY_ACCEPT;	
		}	
	}
	
	return RELAY_ACCEPT;	
}

/*
注册一个qos实例
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
注销一个qos实例
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
	// 刷新接收统计
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

