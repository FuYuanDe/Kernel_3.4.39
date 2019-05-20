/******************************************************************************
        (c) COPYRIGHT 2012- by Dinstar technologies Co.,Ltd
                          All rights reserved.
File:relay_out.c
Desc: relay 五元组匹配skb报文

Modification history(no, author, date, desc)
spark 16-11-22create file
******************************************************************************/
#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/kernel.h>
	
#include <linux/crypto.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/types.h>
	
#include <net/ip.h>
#include <linux/kthread.h>
#include <net/route.h>
#include "relay.h"	
	


/*
返回NULL 为match失败
*/
struct relay_info *relay_match_ipv4_hander(struct sk_buff *skb,struct relay_info *global_cb)
{
	struct iphdr	*iph;
	struct udphdr *uh;
	__u16 udp_dest;
	struct relay_info * cb;
	
	iph = ip_hdr(skb);
	if (unlikely(iph->protocol != IPPROTO_UDP)) 
	{
		return NULL;
	}
	uh = udp_hdr(skb);
	udp_dest = ntohs(uh->dest);
	cb = &global_cb[udp_dest%RELAY_RTP_MUM_MAX];

	//RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"relay_match_ipv4_hander dest %x,port dest %x\n",uh->dest,cb->inet_in_info.port_dest);
	spin_lock(&cb->lock);
	if(uh->dest == cb->inet_in_info.port_dest)
	{

		//RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"source %x,saddr %x,daddr %x\n",uh->source,iph->saddr,iph->daddr);
		//RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"in source %x,saddr %x,daddr %x\n",cb->inet_in_info.port_source
			//,cb->inet_in_info.ip_saddr,cb->inet_in_info.ip_daddr);
		//RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"out source %x,saddr %x,daddr %x,dest %x\n",cb->inet_out_info.port_source
			//,cb->inet_out_info.ip_saddr,cb->inet_out_info.ip_daddr,cb->inet_out_info.port_dest);
		// 五元组匹配校验
		if(uh->source == cb->inet_in_info.port_source 
			&& iph->saddr == cb->inet_in_info.ip_saddr
			&& iph->daddr == cb->inet_in_info.ip_daddr)
		{
			cb->cur_dir = PIPELINE_DIR_IN;
			return cb;
		}
		else if(uh->source == cb->inet_out_info.port_source 
			&& iph->saddr == cb->inet_out_info.ip_saddr
			&& iph->daddr == cb->inet_out_info.ip_daddr)
		{
			cb->cur_dir = PIPELINE_DIR_OUT;
			return cb;
		}
		
	}
	spin_unlock(&cb->lock);
	return NULL;
}

int __init relay_match_init(void)
{
	int ret = 1;

	
	return ret;
}

void __exit relay_match_exit(void)
{
	return;
}


