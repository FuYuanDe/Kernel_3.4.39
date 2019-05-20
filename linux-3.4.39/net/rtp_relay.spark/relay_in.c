/******************************************************************************
        (c) COPYRIGHT 2012- by Dinstar technologies Co.,Ltd
                          All rights reserved.
File:relay_in.c
Desc: ���ļ������ռ���Ҫmux�İ���mux����

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
#include <linux/ip.h>
#include <net/ip.h>
#include <net/udp.h>
#include <linux/net.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include "relay.h"


static unsigned int
relay_in_appout_hook(unsigned int hooknum, struct sk_buff *pskb,
	 const struct net_device *in, const struct net_device *out,
	 int (*okfn)(struct sk_buff *));

static unsigned int
relay_in_wanin_hook(unsigned int hooknum, struct sk_buff *pskb,
	 const struct net_device *in, const struct net_device *out,
	 int (*okfn)(struct sk_buff *));


/* app �·�����. */
static struct nf_hook_ops relay_in_appout_ops = {
	.hook		= relay_in_appout_hook,
	.owner		= THIS_MODULE,
	.pf		= PF_INET,
	.hooknum        = NF_INET_LOCAL_OUT,
	.priority       = NF_IP_PRI_FIRST+1,
};




/*���ⲿ���յ��ı��ģ�����app������rtp����*/
static struct nf_hook_ops relay_in_wanin_ops = {
	.hook		= relay_in_wanin_hook,
	.owner		= THIS_MODULE,
	.pf		= PF_INET,
	.hooknum        = NF_INET_PRE_ROUTING,
	.priority       = NF_IP_PRI_FIRST+2,
};



static unsigned int
relay_in_appout_hook(unsigned int hooknum, struct sk_buff *pskb,
	 const struct net_device *in, const struct net_device *out,
	 int (*okfn)(struct sk_buff *))
{
	struct sk_buff	*skb = pskb;
	struct iphdr	*iph;
	struct udphdr *uh;
	U16    dst_port;
	int i,j;
	int ret = -1;

	iph = ip_hdr(skb);
	//MUXDBG(MUXDBG_LEVEL_INFOPRINT,
	//	"app out recv packet type=%d proto=%d daddr=%d.%d.%d.%d,saddr=%d.%d.%d.%d\n",
	//	skb->pkt_type,
	//	ip_hdr(skb)->protocol,
	//	NIPQUAD(ip_hdr(skb)->daddr),
	//	NIPQUAD(ip_hdr(skb)->saddr));
	//Ŀǰ ֻ����udpЭ��
	if (unlikely(iph->protocol != IPPROTO_UDP)) 
	{
		return NF_ACCEPT;
	}

	//ƥ��
	uh = udp_hdr(skb);

	//sctp ���������bug
	if(uh == NULL)
	{
		return NF_ACCEPT;
	}
	//���������upd�˿���ʶ��rtp������
	dst_port = ntohs(uh->source);
	#if 0
	i = dst_port>>5; // /32
	j = dst_port&0x1f;   //%32
//	RTPM_IN_DBG(13, "vaid udp port %x\n",dst_port);
	//�ж��Ƿ��ǵ�ǰ��Ҫ��udp����
	//ƥ��ɹ�
	if((muxin_session_hash[i]&(1<<j)) > 0)
	{
		ret = muxcore_session_recvpacket(skb);
		if(ret == 1)
		{
			return NF_STOLEN;
		}
	}
	#endif
	return NF_ACCEPT;
}






static unsigned int
relay_in_wanin_hook(unsigned int hooknum, struct sk_buff *pskb,
	 const struct net_device *in, const struct net_device *out,
	 int (*okfn)(struct sk_buff *))
{
	int ret;
	struct ethhdr *eth;

	//printk(KERN_ERR"relay_in_wanin_hook\n");
	//����������Ҫip��udp��Ϣ���˴�Ԥ�����������������
	eth = eth_hdr(pskb);
	if(ntohs(eth->h_proto) == ETH_P_IP)
	{
		skb_set_transport_header(pskb,sizeof(struct iphdr));
	}
	else
	{
		skb_set_transport_header(pskb,sizeof(struct ipv6hdr));
	}
	ret = relay_core_recv(pskb,ntohs(eth->h_proto));
	if(ret > 0)
		return NF_STOLEN;
	else if(ret < 0)
	{
		return NF_DROP;
	}
	else	
		return NF_ACCEPT;
}





int __init relay_in_init(void)
{
	int i;
	int ret;

	#if 0
	//ע��udp����
	ret = nf_register_hook(&relay_in_appout_ops);
	if (ret < 0) {
		printk(KERN_ERR"relay in can't register app out hook.\n");
		return -1;
	}
	#endif

	ret = nf_register_hook(&relay_in_wanin_ops);
	if (ret < 0) {
		printk(KERN_ERR"relay in can't register wan in hook.\n");
		//nf_unregister_hook(&relay_in_appout_ops);
		return -1;
	}
	printk(KERN_ERR"relay_in_init ok.\n");
	return 1;
}

void __exit relay_in_exit(void)
{
	//ע��udp����
	//nf_unregister_hook(&relay_in_appout_ops);
	nf_unregister_hook(&relay_in_wanin_ops);
	return;
}



