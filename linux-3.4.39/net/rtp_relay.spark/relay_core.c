 /******************************************************************************
        (c) COPYRIGHT 2016- by Dinstar technologies Co.,Ltd
                          All rights reserved.
File:relay_core.c
Desc: relay��ˮ��ģʽ����core

Modification history(no, author, date, desc)
spark 16-11-22create file
******************************************************************************/

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/netfilter.h>
#include <linux/spinlock.h>
#include <linux/netlink.h>
#include <linux/net.h>
#include <linux/netfilter_ipv4.h>
#include <net/sock.h>
#include <net/flow.h>
#include <net/dn.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <net/udp.h>
#include <net/route.h>
#include <linux/if_ether.h>
#include "relay.h"

extern int dinstar_dispatch_register(__u16 dstport);
extern int dinstar_dispatch_unregister(__u16 dstport);
extern int dinstar_dispatch_setbase(__u16 dstport);


static struct relay_info g_cb[RELAY_RTP_MUM_MAX];
static __u16 udp_port_base;
int g_relay_debug_level = RELAYDBG_LEVEL_MAX;
/*
����1Ϊ���ܣ�0��������-1������
*/
int relay_core_recv(struct sk_buff *skb,int protocol)
{
	struct pipeline_info *pipeline;
	struct relay_info *cb;
	int ret = 0;

	//RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"core recv protoco %x\n",protocol);
	// ƥ��ʧ�ܣ�����Э��ջ����
	if(protocol == ETH_P_IP)
	{
		if((cb = relay_match_ipv4_hander(skb,g_cb)) == NULL)
		{
			return 0;
		}
	}
	else		//IP V6
	{
	}
	//RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"core recv type %d,cur in %d,out %d\n",cb->cur_dir,cb->curin_cnt,cb->curout_cnt);
	#if 0
	// qos �쳣��Ҫ����
	if(relay_qos_handler(skb,cb) != RELAY_ACCEPT)
	{
		ret = -1;
		goto end;
	}
	#endif
	// ���� cb��ע�������
	if(cb->cur_dir == PIPELINE_DIR_IN)
	{
		pipeline = cb->pipeline_in_head;
	}
	else
	{
		pipeline = cb->pipeline_out_head;
	}
	for(;pipeline!= NULL;pipeline=pipeline->next)
	{
		ret = pipeline->function(skb,pipeline->data);
		if(ret == RELAY_STOLEN)
		{
			ret = 1;
			goto end;
		}
		else if(ret == RELAY_DROP)
		{
			ret = -1;
			goto end;
		}
	}

	end:
	spin_unlock(&cb->lock);
	return ret;
}


/*
����һ��ʵ��ioctl  ����
*/
struct relay_info * relay_core_add_example(struct relay_inet_info *in,struct relay_inet_info *out)
{
	struct relay_info *cb;
	__u16 dest;

	dest = ntohs(in->port_dest);
	if(dest < udp_port_base || dest > udp_port_base + RELAY_RTP_MUM_MAX)
	{
		return NULL;
	}
	cb = &g_cb[dest%RELAY_RTP_MUM_MAX];

	
	spin_lock(&cb->lock);
	if(cb->inet_in_info.port_dest != 0)
	{
		spin_unlock(&cb->lock);
		return NULL;
	}
	// ��Ԫ��
	cb->inet_in_info.ip_saddr = in->ip_saddr;
	cb->inet_in_info.ip_daddr = in->ip_daddr;
	cb->inet_in_info.port_source = in->port_source;
	cb->inet_in_info.port_dest = in->port_dest;

	cb->inet_out_info.ip_saddr = out->ip_saddr;
	cb->inet_out_info.ip_daddr = out->ip_daddr;
	cb->inet_out_info.port_source = out->port_source;
	cb->inet_out_info.port_dest = out->port_dest;
	
	spin_unlock(&cb->lock);

	RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"in source %x,saddr %x,daddr %x,dest %x\n",cb->inet_in_info.port_source
			,cb->inet_in_info.ip_saddr,cb->inet_in_info.ip_daddr,cb->inet_out_info.port_dest);
	RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"out source %x,saddr %x,daddr %x,dest %x\n",cb->inet_out_info.port_source
		,cb->inet_out_info.ip_saddr,cb->inet_out_info.ip_daddr,cb->inet_out_info.port_dest);
	// dispatch ע��
	dinstar_dispatch_register(ntohs(cb->inet_in_info.port_dest));
	RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"add example dispatch register\n");
	return cb;
}

/*
ɾ��һ��ʵ��
*/
int relay_core_del(__u16	port_dest)
{
	struct pipeline_info *tmp1;
	struct pipeline_info *tmp2;
	struct relay_info *cb;

	cb = &g_cb[port_dest%RELAY_RTP_MUM_MAX];
	// dispatch ע��
	//dinstar_dispatch_unregister(port_dest);
	spin_lock(&cb->lock);
	// ��Ԫ��
	cb->inet_in_info.ip_saddr = 0;
	cb->inet_in_info.ip_daddr = 0;
	cb->inet_in_info.port_source = 0;
	cb->inet_in_info.port_dest = 0;

	cb->inet_out_info.ip_saddr = 0;
	cb->inet_out_info.ip_daddr = 0;
	cb->inet_out_info.port_source = 0;
	cb->inet_out_info.port_dest = 0;
	
	for(tmp1 = cb->pipeline_in_head;tmp1 != NULL&&tmp1->next != NULL;)
	{
		if(tmp1->data)
			kfree(tmp1->data);
		tmp2 = tmp1;
		tmp1 = tmp1->next;
		kfree(tmp2);
	}
	for(tmp1 = cb->pipeline_out_head;tmp1 != NULL&&tmp1->next != NULL;)
	{
		if(tmp1->data)
			kfree(tmp1->data);
		tmp2 = tmp1;
		tmp1 = tmp1->next;
		kfree(tmp2);
	}
	spin_unlock(&cb->lock);
}


int relay_core_add_pipline(struct relay_info *cb,struct pipeline_info *info,int dir)
{
	struct pipeline_info *tmp;
	
	spin_lock(&cb->lock);
	if(dir == PIPELINE_DIR_IN)
	{
		if(cb->pipeline_in_head == NULL)
		{
			cb->pipeline_in_head = info;
		}
		else
		{
			for(tmp = cb->pipeline_in_head;tmp->next != NULL;tmp = tmp->next);
			tmp->next = info;
		}
	}
	else
	{
		if(cb->pipeline_out_head == NULL)
		{
			cb->pipeline_out_head = info;
		}
		else
		{
			for(tmp = cb->pipeline_out_head;tmp->next != NULL;tmp = tmp->next);
			tmp->next = info;
		}
	}
	info->next = NULL;
	spin_unlock(&cb->lock);
	return 1;
}

int relay_core_set_baseport( __u16 port)
{
	RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"set baskport %d\n",port);
	udp_port_base = port;
	dinstar_dispatch_setbase(port);
}

int __init relay_core_init()
{
	int i;

	memset(g_cb, 0,sizeof(struct relay_info)*RELAY_RTP_MUM_MAX);
	for(i=0;i<RELAY_RTP_MUM_MAX;i++)
	{
		spin_lock_init(&g_cb[i].lock);
	}
	udp_port_base = 0;
    relay_dsp_init();
	relay_in_init();
	relay_ioctl_init();
	relay_netlink_init();
	relay_match_init();
	relay_out_init();
	relay_qos_init();
	return 0;
}

void __exit relay_core_exit()
{
	relay_dsp_exit();
	relay_in_exit();
	relay_ioctl_exit();
	relay_netlink_exit();
	relay_match_exit();
	relay_out_exit();
	relay_qos_exit();
}



MODULE_DESCRIPTION("rtp relay");
MODULE_AUTHOR("spark");
MODULE_LICENSE("GPL");
MODULE_ALIAS_NET_PF_PROTO(PF_NETLINK, NETLINK_DNRTMSG);

module_init(relay_core_init);
module_exit(relay_core_exit);

