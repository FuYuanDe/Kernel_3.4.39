/******************************************************************************
        (c) COPYRIGHT 2012- by Dinstar technologies Co.,Ltd
                          All rights reserved.
File:relay_out.c
Desc: ���ļ���realy�İ���ȷ�ķ��ͳ�ȥ

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
#include <net/route.h>



/*�������ݵ�wan�ڣ��Խ����ݷ���Զ��
���ǵ�vpn��pppoe�����󣬴����ݷ��͵�ʱ����ip���·��֮ǰ���
*/
static int relay_out_toremote(struct sk_buff * skb,unsigned long data)
{
	int ret = -1;
	struct net_device *netdev;
	//struct rtable *rt;
	
	//rt = (struct rtable *)data;
	netdev = (struct net_device *)data;
	{
		struct iphdr	*iph;
		struct ethhdr *eth;

		iph = ip_hdr(skb);
		eth = eth_hdr(skb);
		//RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"out saddr %x,daddr %x\,rt is %x\n",iph->saddr,iph->daddr,data);
		//RELAYDBG(RELAYDBG_LEVEL_DEBUGINFO,"local mac is %x-%x-%x-%x-%x-%x,remote mac is %x-%x-%x-%x-%x-%x\n"
			//,eth->h_source[0],eth->h_source[1],eth->h_source[2]
			//,eth->h_source[3],eth->h_source[4],eth->h_source[5]
			//,eth->h_dest[0],eth->h_dest[1],eth->h_dest[2]
			//,eth->h_dest[3],eth->h_dest[4],eth->h_dest[5]);
	}
	#if 0
	skb_dst_set(skb, dst_clone(&(rt->dst)));
	//NF_IP_LOCAL_OUT ����д���
	ret = dst_output(skb);
	#endif
	skb->dev = netdev;
	skb_push(skb,sizeof(struct ethhdr));
	ret = dev_queue_xmit(skb);
	return RELAY_STOLEN;	
}


/*���ͱ��ĸ�app����
��app�ı��ģ����������ַ�ʽ��һֱֱ�ӽ��ɴ���㣬һ����ip��
����app���������٣����ύip���Ժ���Է������Ӹ���ip������
ʹ���ύip��ķ�ʽ
*/
static int realy_out_toapp(struct sk_buff * skb,unsigned long data)
{
	int ret = -1;
	if(skb == NULL)
	{
		return -1;
	}
	dst_release(skb_dst(skb));
	skb_dst_set(skb,NULL);
	nf_reset(skb);
	ret = netif_rx(skb);
	return ret;
}

int relay_out_register(struct relay_info *cb,int type,int dir)
{
	struct pipeline_info *info;
	struct rtable *rt;
	struct net_device *netdev;

	info = (struct pipeline_info *)kmalloc(sizeof(struct pipeline_info), GFP_KERNEL);
	if(info == NULL)
	{
		return -1;
	}
	if(type == RELAY_OUT_TO_REMOTE)
	{
		info->function = relay_out_toremote;
	}
	else //if(RELAY_OUT_TO_APP)
	{
		info->function = realy_out_toapp;
	}
	#if 0
	//��ȡrt��Ϣ
	{	
		struct flowi4 fl4;
		struct net_device *netdev;

		netdev = dev_get_by_name(&init_net,"eth0");
		if(netdev != NULL)
		{
			printk(KERN_ERR"get dev is %s\n",netdev->name);
		}
		else
		{
			printk(KERN_ERR"eth0 get dev fail\n",netdev->name);
		}
		memset(&fl4, 0, sizeof(fl4));
		if(dir == PIPELINE_DIR_IN)
		{
			fl4.saddr = cb->inet_in_info.ip_daddr;  
			fl4.daddr = cb->inet_in_info.ip_saddr;  
		}
		else
		{
			fl4.saddr = cb->inet_out_info.ip_daddr;  
			fl4.daddr = cb->inet_out_info.ip_saddr;
		}
		fl4.flowi4_oif = netdev->ifindex;
		fl4.flowi4_tos = 0;
		fl4.flowi4_proto = IPPROTO_IP;

		rt = ip_route_output_key(&init_net, &fl4);
		if(IS_ERR(rt))
		{
			printk(KERN_ERR"daddr %x,saddr %x get rt fail\n",fl4.daddr,fl4.saddr);
			rt = NULL;
		}
		else
		{
			printk(KERN_ERR"daddr %x,saddr %x get rt dev %s,addr %x\n",fl4.daddr,fl4.saddr,rt->dst.dev->name,(unsigned long)rt);
		}
	}
	info->data = (unsigned long)rt;
	#endif
	{
		struct net_device *netdev;

		netdev = dev_get_by_name(&init_net,"eth0");
		if(netdev != NULL)
		{
			printk(KERN_ERR"get dev is %s\n",netdev->name);
		}
		else
		{
			printk(KERN_ERR"eth0 get dev fail\n",netdev->name);
		}
		info->data = (unsigned long)netdev;
	}
	relay_core_add_pipline(cb,info,dir);
	return 1;
}

int __init relay_out_init(void)
{
	return 1;
}


void __exit relay_out_exit(void)
{
	return;
}


