
/******************************************************************************
        (c) COPYRIGHT 2012- by Dinstar technologies Co.,Ltd
                          All rights reserved.
File:relay_rtp.c
Desc: 本文件将realy的包正确的发送出去

Modification history(no, author, date, desc)
spark 16-11-22create file
******************************************************************************/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/sysctl.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/netfilter.h>
#include <linux/kthread.h>
#include <linux/net.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include "relay.h"



static void csum_udp_magic(struct sk_buff *skb)
{
	struct iphdr *iphdr;
	struct udphdr *uh;	
	U16 udp_len;

	iphdr = (struct iphdr*)skb_network_header(skb);
	uh = (struct udphdr*)skb_transport_header(skb);

	uh->check = 0; 
	udp_len = ntohs(uh->len);                               
	skb->csum = csum_partial( skb_transport_header(skb), udp_len, 0 );
	uh->check = csum_tcpudp_magic(iphdr->saddr, iphdr->daddr, udp_len, IPPROTO_UDP, skb->csum);
	skb->ip_summed = CHECKSUM_NONE;
	

#if 0
	if (udp_len == 66)
	{
		U8 *buf;
		buf = skb_transport_header(skb);
		MUXDBG(MUXDBG_LEVEL_WARNING,"header:0x%x 0x%x udp_len:0x%x csum:0x%x  skb->csum:0x%x uh->check:0x%x\n", *buf, *(buf+1), udp_len, csum, skb->csum, uh->check);
	}
#endif

	if (0 == uh->check)
		uh->check = CSUM_MANGLED_0;
}

// 替换ip 头
static int relay_rtp_turn(struct sk_buff * skb,unsigned long data)
{
	struct iphdr	*iph;
	struct udphdr *uh;
	struct ethhdr *eth;
	u32 temp;
	struct rm_media_conn *media_info = (struct rm_media_conn *)data;

	iph = ip_hdr(skb);
	#if 0
	temp = iph->saddr;
	iph->saddr = iph->daddr;
	iph->daddr = temp;
	#else
	iph->saddr = media_info->local_ip.ip;
	iph->daddr = media_info->remote_ip.ip;
	iph->check = 0;
	//iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);
	#endif
	uh = (struct udphdr*)skb_transport_header(skb);
	uh->source = media_info->local_port;
	uh->dest   = media_info->remote_port;
	csum_udp_magic(skb);
	iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);
	eth = eth_hdr(skb);
	memcpy(eth->h_source,media_info->local_mac,ETH_ALEN);
	memcpy(eth->h_dest,media_info->remote_mac,ETH_ALEN);
	
	#if 0
	iph = ip_hdr(skb);
	memcpy(iph,data,sizeof(struct iphdr)+sizeof(struct udphdr));
	#endif
	return RELAY_ACCEPT;
}


int relay_rtp_register(struct relay_info *cb,struct rm_media_conn *media_info,int dir)
{
	struct pipeline_info *info;

	// turn
	info = (struct pipeline_info *)kmalloc(sizeof(struct pipeline_info), GFP_KERNEL);
	if(info == NULL)
	{
		return -1;
	}

	info->function = relay_rtp_turn;
	#if 0
	info->data = (struct pipeline_info *)kmalloc(sizeof(struct iphdr)+sizeof(struct udphdr), GFP_KERNEL);
	// 填充data
	if(dir == PIPELINE_DIR_IN)
	{
		
	}
	else
	{
	}
	if(dir == PIPELINE_DIR_IN)
	{
		
	}
	// 填充data
	else
	{
	}
	#else
	info->data = kmalloc(sizeof(struct rm_media_conn), GFP_KERNEL);
	memcpy(info->data,media_info,sizeof(struct rm_media_conn));
	#endif
	relay_core_add_pipline(cb,info,dir);
	//out
	relay_out_register(cb,RELAY_OUT_TO_REMOTE,dir);
	return 1;
}


int __init relay_rtp_init(void)
{
	return 1;
}


void __exit relay_rtp_exit(void)
{
	return;
}


