/******************************************************************************
        (c) COPYRIGHT 2016- by Dinstar technologies Co.,Ltd
                          All rights reserved.
File:relay_ioctl.c
Desc: 本文件用于与用户交互，完成用户的请求，目前用户的请求主要有
1:
Modification history(no, author, date, desc)
spark 16-11-22create file
******************************************************************************/
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/sysctl.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/netfilter.h>
#include <linux/kthread.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <net/udp.h>
#include <linux/net.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include "relay.h"

#define NETLINK_RELAY		23	/* Crypto layer */

#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[3]


//static DEFINE_MUTEX(muxioctl_mutex);
static struct sock *nlfd = NULL;

static int relay_netlink_create(struct rm_media_conn * media_infoa,struct rm_media_conn * media_infob)
{
	struct relay_info *cb;
	struct relay_inet_info in;
	struct relay_inet_info out;
	__u16 temp16;
	__u32 temp32;

	RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"dir in loacl port is %d,local mac is %x-%x-%x-%x-%x-%x,local ip is %d.%d.%d.%d\n"
		,media_infoa->local_port,media_infoa->local_mac[0],media_infoa->local_mac[1],media_infoa->local_mac[2]
		,media_infoa->local_mac[3],media_infoa->local_mac[4],media_infoa->local_mac[5],NIPQUAD(media_infoa->local_ip.ip));
	
	RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"dir in remote port is %d,remote mac is %x-%x-%x-%x-%x-%x,remote ip is %d.%d.%d.%d\n"
		,media_infoa->remote_port,media_infoa->remote_mac[0],media_infoa->remote_mac[1],media_infoa->remote_mac[2]
		,media_infoa->remote_mac[3],media_infoa->remote_mac[4],media_infoa->remote_mac[5],NIPQUAD(media_infoa->remote_ip.ip));

	RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"dir in crypto crypto is %s, key is %s\n"
		,media_infoa->crypto.crypto,media_infoa->crypto.key);

	RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"dir in vlanid is 0x%x,dscp 0x%x,protoc0l 0x%x,ip type is 0x%x,media type %s\n"
		,media_infoa->vlanid,media_infoa->dscp,media_infoa->protocol,media_infoa->ip_type,media_infoa->media_type);

	RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"dir in rtp info:encode name is %s,param %s,payload %d,slience %d,dtmf detect %d\
		,dtmf action %d,bitrate %d,max ptime %d\n"
		,media_infoa->media_data.rtp.encode_name,media_infoa->media_data.rtp.param,media_infoa->media_data.rtp.payload
		,media_infoa->media_data.rtp.slience_supp,media_infoa->media_data.rtp.dtmf_detect,media_infoa->media_data.rtp.dtmf_action
		,media_infoa->media_data.rtp.bitrate,media_infoa->media_data.rtp.max_ptime);


	RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"dir out loacl port is %d,local mac is %x-%x-%x-%x-%x-%x,local ip is %d.%d.%d.%d\n"
		,media_infob->local_port,media_infob->local_mac[0],media_infob->local_mac[1],media_infob->local_mac[2]
		,media_infob->local_mac[3],media_infob->local_mac[4],media_infob->local_mac[5],NIPQUAD(media_infob->local_ip.ip));
	
	RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"dir out remote port is %d,remote mac is %x-%x-%x-%x-%x-%x,remote ip is %d.%d.%d.%d\n"
		,media_infob->remote_port,media_infob->remote_mac[0],media_infob->remote_mac[1],media_infob->remote_mac[2]
		,media_infob->remote_mac[3],media_infob->remote_mac[4],media_infob->remote_mac[5],NIPQUAD(media_infob->remote_ip.ip));

	RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"dir out crypto crypto is %s, key is %s\n"
		,media_infob->crypto.crypto,media_infob->crypto.key);

	RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"dir out vlanid is 0x%x,dscp 0x%x,protoc0l 0x%x,ip type is 0x%x,media type %s\n"
		,media_infob->vlanid,media_infob->dscp,media_infob->protocol,media_infob->ip_type,media_infob->media_type);

	RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"dir out rtp info:encode name is %s,param %s,payload %d,slience %d,dtmf detect %d\
		,dtmf action %d,bitrate %d,max ptime %d\n"
		,media_infob->media_data.rtp.encode_name,media_infob->media_data.rtp.param,media_infob->media_data.rtp.payload
		,media_infob->media_data.rtp.slience_supp,media_infob->media_data.rtp.dtmf_detect,media_infob->media_data.rtp.dtmf_action
		,media_infob->media_data.rtp.bitrate,media_infob->media_data.rtp.max_ptime);


	#if 0
	RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"dir in remote ip is %d.%d.%d.%d,port %d,,local ip is %d.%d.%d.%d,port is %d\n"
		,NIPQUAD(media_infoa->remote_ip.ip),media_infoa->remote_port
		,NIPQUAD(media_infoa->local_ip.ip),media_infoa->local_port);
	
	RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"dir out remote ip is %d.%d.%d.%d,port %d,,local ip is %d.%d.%d.%d,port is %d\n"
		,NIPQUAD(media_infob->remote_ip.ip),media_infob->remote_port
		,NIPQUAD(media_infob->local_ip.ip),media_infob->local_port);
	#endif

	#if 1
	//temp32 = htonl(media_infoa->local_ip.ip);
	//media_infoa->local_ip.ip = temp32;
	//temp32 = htonl(media_infoa->remote_ip.ip);
	//media_infoa->remote_ip.ip = temp32;
	temp16  = htons(media_infoa->local_port);
	media_infoa->local_port = temp16;
	temp16 = htons(media_infoa->remote_port);
	media_infoa->remote_port = temp16;

	//temp32 = htonl(media_infob->local_ip.ip);
	//media_infob->local_ip.ip = temp32;
	//temp32 = htonl(media_infob->remote_ip.ip);
	//media_infob->remote_ip.ip = temp32;
	temp16  = htons(media_infob->local_port);
	media_infob->local_port = temp16;
	temp16= htons(media_infob->remote_port);
	media_infob->remote_port = temp16;
	#endif
	in.ip_daddr = media_infoa->local_ip.ip;
	in.ip_saddr = media_infoa->remote_ip.ip;
	in.port_dest = media_infoa->local_port;
	in.port_source = media_infoa->remote_port;

	out.ip_daddr = media_infob->local_ip.ip;
	out.ip_saddr = media_infob->remote_ip.ip;
	out.port_dest = media_infob->local_port;
	out.port_source = media_infob->remote_port;
	cb = relay_core_add_example(&in,&out);
	if(cb)
	{
		relay_rtp_register(cb,media_infoa,PIPELINE_DIR_OUT);
		relay_rtp_register(cb,media_infob,PIPELINE_DIR_IN);
	}
}

static void
relay_netlink_rcv_skb(struct sk_buff *skb)
{
	struct nlmsghdr *nlh = NULL;

	if( skb->len >= sizeof(struct nlmsghdr) )
	{
		nlh = (struct nlmsghdr *)skb->head;

		RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"\r\ntype:%d pid:%d len:%d, seq:%u\n"
					, nlh->nlmsg_type
				, nlh->nlmsg_pid
					, nlh->nlmsg_len
				, nlh->nlmsg_seq);

	//	lost_pkt_num = fwd_pkt_num - nlh->nlmsg_seq;
		
		if((nlh->nlmsg_len >= sizeof(struct nlmsghdr)) 
			&& (skb->len >= nlh->nlmsg_len)) 
		{
			switch(nlh->nlmsg_type)
			{
				case RM_MEDIA_TBL_CREATE:
				{
					struct rm_media_tbl *rm_media_tbl_info;

					rm_media_tbl_info = (struct rm_media_tbl *)NLMSG_DATA(nlh);
					relay_netlink_create(&rm_media_tbl_info->aconn,&rm_media_tbl_info->bconn);
				}
					break;
				case RM_MEDIA_TBL_DELETE:
				{
					__u16 port_dest;

					port_dest = (__u16)NLMSG_DATA(nlh);
					relay_core_del(port_dest);
				}
					break;
				case RM_MEDIA_TBL_SETCFG:
				{
					struct rm_init_cfg *cfg;

					cfg = (struct rm_init_cfg *)NLMSG_DATA(nlh);
					relay_core_set_baseport((__u16)(cfg ->port_min));
				}
					break;
				default:
					break;
			}
		}	
		else
		{
			RELAYDBG(RELAYDBG_LEVEL_ERROR,"nlmsghdr size:%d, skb->len:%d\n"
						, sizeof(struct nlmsghdr)
						, skb->len);					
		}
	}
	else
	{
		RELAYDBG(RELAYDBG_LEVEL_ERROR,"nlmsghdr size:%d, skb->len:%d\n"
						, sizeof(struct nlmsghdr)
						, skb->len);	
	}
	return;
}




int __init relay_netlink_init(void)
{

	RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"relay_netlink_init\n");
	
	nlfd = netlink_kernel_create(&init_net,NETLINK_RELAY, 0, relay_netlink_rcv_skb,NULL, THIS_MODULE);
	if( !nlfd )
	{
	//	net_hook_info("%s: create netlink socket fail.\n", __FUNCTION__);
		return -1;
	}
	nlfd->sk_sndbuf = 16*1024;
	nlfd->sk_rcvbuf = 16*1024;
	//nlfd->sk_sndtimeo = 3;	//30ms 3000/HZ

	// debug
	{
		struct relay_inet_info in;
		struct relay_inet_info out;

		
		in.ip_daddr = htonl(0xc0a80b3f);
		in.ip_saddr = htonl(0xc0a80b03);
		in.port_dest = htons(520);
		in.port_source = htons(520);

		out.ip_daddr = htonl(0xc0a80b3f);
		out.ip_saddr = htonl(0xc0a80b04);
		out.port_dest =  htons(520);
		out.port_source =  htons(521);
		relay_core_add_example(&in,&out);
	}

	return 1;
}


void __exit relay_netlink_exit(void)
{
	if( nlfd )
	{
		sock_release(nlfd->sk_socket);
	}
	return;
}




