/*
 * DECnet       An implementation of the DECnet protocol suite for the LINUX
 *              operating system.  DECnet is implemented using the  BSD Socket
 *              interface as the means of communication with the user level.
 *
 *              DECnet Routing Message Grabulator
 *
 *              (C) 2000 ChyGwyn Limited  -  http://www.chygwyn.com/
 *              This code may be copied under the GPL v.2 or at your option
 *              any later version.
 *
 * Author:      Steven Whitehouse <steve@chygwyn.com>
 *
 */
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


typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef short  S16;

#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[3]


static struct timer_list relayin_ticktimer;
static struct sock *dnrmg = NULL;
//static struct rtable *test_rt;
//static struct rtable *test_rt1;

static DEFINE_PER_CPU(int, recv_cnt);
static DEFINE_PER_CPU(int, print_flag);
static DEFINE_PER_CPU(struct rtable *,test_rt);
static DEFINE_PER_CPU(struct rtable *,test_rt1);





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



static inline int ip_finish_output2(struct sk_buff *skb)
{
	struct dst_entry *dst = skb_dst(skb);
	struct rtable *rt = (struct rtable *)dst;
	struct net_device *dev = dst->dev;
	unsigned int hh_len = LL_RESERVED_SPACE(dev);
	struct neighbour *neigh;

	if (rt->rt_type == RTN_MULTICAST) {
		IP_UPD_PO_STATS(dev_net(dev), IPSTATS_MIB_OUTMCAST, skb->len);
	} else if (rt->rt_type == RTN_BROADCAST)
		IP_UPD_PO_STATS(dev_net(dev), IPSTATS_MIB_OUTBCAST, skb->len);

	/* Be paranoid, rather than too clever. */
	if (unlikely(skb_headroom(skb) < hh_len && dev->header_ops)) {
		struct sk_buff *skb2;

		skb2 = skb_realloc_headroom(skb, LL_RESERVED_SPACE(dev));
		if (skb2 == NULL) {
			kfree_skb(skb);
			return -ENOMEM;
		}
		if (skb->sk)
			skb_set_owner_w(skb2, skb->sk);
		kfree_skb(skb);
		skb = skb2;
	}

	rcu_read_lock();
	neigh = dst_get_neighbour_noref(dst);

	//printk(KERN_ERR "nud state %d,hh len %d!\n",neigh->nud_state,neigh->hh.hh_len);
	if (neigh) {
		int res = neigh_output(neigh, skb);

		rcu_read_unlock();
		return res;
	}
	rcu_read_unlock();

	if (net_ratelimit())
		printk(KERN_DEBUG "ip_finish_output2: No header cache and no neighbour!\n");
	kfree_skb(skb);
	return -EINVAL;
}


static unsigned int wanin_hook(unsigned int hook,
			struct sk_buff *pskb,
			const struct net_device *in,
			const struct net_device *out,
			int (*okfn)(struct sk_buff *))
{
	struct sk_buff	*skb = pskb;
	struct iphdr	*iph;
	struct udphdr *uh;
	U16    dst_port;
	int ret = -1;
	u32 temp;
	int *cnt;
	int *local_print_flag;


	local_print_flag = &__get_cpu_var(print_flag);
#if 0
	if(*local_print_flag == 0)
	printk(KERN_ERR
		"wan in recv packet type=%d proto=%d daddr=%d.%d.%d.%d,saddr=%d.%d.%d.%d\n",
		skb->pkt_type,
		ip_hdr(skb)->protocol,
		NIPQUAD(ip_hdr(skb)->daddr),
		NIPQUAD(ip_hdr(skb)->saddr));
#endif
	//判断当前的报文来源
	if (unlikely(skb->pkt_type != PACKET_HOST
			 || skb->dev == init_net.loopback_dev)) 
	//if(skb->dev != wlan_netdev)
	{
		return NF_ACCEPT;
	}

	iph = ip_hdr(skb);
	if (unlikely(iph->protocol != IPPROTO_UDP)) 
	{
		return NF_ACCEPT;
	}

	//__skb_pull(skb, ip_hdrlen(skb));
	
	/* Point into the IP datagram, just past the header. */
	//skb_reset_transport_header(skb);

	skb_set_transport_header(skb,sizeof(struct iphdr));
	//匹配
	uh = udp_hdr(skb);

	//根据输入的upd端口来识别rtp数据流
	dst_port = ntohs(uh->dest);
	if(dst_port == 520)
	{
		cnt = &__get_cpu_var(recv_cnt);
		(*cnt)++;
		if(*local_print_flag == 0)
		{
			printk(KERN_ERR"wanin_hook recv udp package %d,cnt %d.\n",dst_port,*cnt);	
			*local_print_flag = 1;
		}
		temp = iph->saddr;
		iph->saddr = iph->daddr;
		iph->daddr = temp;
		csum_udp_magic(skb);
		#if 1
		if(iph->daddr == htonl(0xc0a80b03))
			skb_dst_set(skb, dst_clone(&__get_cpu_var(test_rt)->dst));
		else
			skb_dst_set(skb, dst_clone(&__get_cpu_var(test_rt1)->dst));
		//ret= nf_hook(NFPROTO_IPV4, NF_INET_LOCAL_OUT, skb, NULL,
		      // skb_dst(skb)->dev, dst_output);
		//udelay(5);
		
		ret = ip_finish_output2(skb);//dst_output(skb);
		#endif
		//kfree_skb(skb);
		//if(ret > 0)
			return NF_STOLEN;
	}
	return NF_ACCEPT;
}



/*
 * 
 * handler for tick_timer
 */
static void relayin_tick_timer(unsigned long date)
{ 
	int cnt = 0;
	static int old_cnt = 0;
	int i;
	relayin_ticktimer.expires	= jiffies + 100;
	add_timer(&relayin_ticktimer);
	for_each_online_cpu(i)
		cnt += per_cpu(recv_cnt, i);
	per_cpu(print_flag, 0) = 0;
	per_cpu(print_flag, 1) = 0;
	per_cpu(print_flag, 2) = 0;
	per_cpu(print_flag, 3) = 0;
	per_cpu(print_flag, 4) = 0;
	per_cpu(print_flag, 5) = 0;
	per_cpu(print_flag, 6) = 0;
	per_cpu(print_flag, 7) = 0;
	printk(KERN_ERR"relay in recv %d,total %d. cpu0:%d,1:%d,2:%d,3:%d,4:%d,5:%d,6:%d,7:%d at time %d.\n"
		,cnt - old_cnt,cnt,per_cpu(recv_cnt, 0),per_cpu(recv_cnt, 1)
		,per_cpu(recv_cnt, 2),per_cpu(recv_cnt, 3),per_cpu(recv_cnt, 4)
		,per_cpu(recv_cnt, 5),per_cpu(recv_cnt, 6),per_cpu(recv_cnt, 7),jiffies);
	old_cnt = cnt;
    return;
}


static struct nf_hook_ops wanin_ops __read_mostly = {
	.hook		= wanin_hook,
	.owner		= THIS_MODULE,
	.pf		= PF_INET,
	.hooknum	= NF_INET_PRE_ROUTING,
	.priority	= NF_IP_PRI_FIRST+2,
};

static int __init udp_relay_init(void)
{
	int rv = 0;

	init_timer(&relayin_ticktimer);
    relayin_ticktimer.data     = 0;
	relayin_ticktimer.expires  = jiffies + 100;		// 1000ms
	relayin_ticktimer.function = relayin_tick_timer;
	add_timer(&relayin_ticktimer);
	
	//获取rt信息
	{	
		struct flowi4 fl4;
		int i;
		
		
		memset(&fl4, 0, sizeof(fl4));
		fl4.daddr = htonl(0xc0a80b03);  // 192.168.11.3
		fl4.saddr = htonl(0xc0a80b3f);  // 192.168.11.63
		fl4.flowi4_tos = 0;
		fl4.flowi4_proto = IPPROTO_IP;

		for_each_online_cpu(i)
		{
			per_cpu(test_rt, i) = ip_route_output_key(&init_net, &fl4);
			if(IS_ERR(per_cpu(test_rt, i)))
			{
				printk(KERN_ERR"get test_rt fail\n");
			}
			else
			{
				printk(KERN_ERR"get test_rt dev %s\n",per_cpu(test_rt, i)->dst.dev->name);
			}
		}
	}


	//获取rt信息
	{	
		struct flowi4 fl4;
		int i;

		memset(&fl4, 0, sizeof(fl4));
		fl4.daddr = htonl(0xc0a80bc9);  // 192.168.11.201
		fl4.saddr = htonl(0xc0a80b3f);  // 192.168.11.63
		fl4.flowi4_tos = 0;
		fl4.flowi4_proto = IPPROTO_IP;

		for_each_online_cpu(i)
		{
			per_cpu(test_rt1, i) = ip_route_output_key(&init_net, &fl4);
			if(IS_ERR(per_cpu(test_rt1, i)))
			{
				printk(KERN_ERR"get test_rt1 fail\n");
			}
			else
			{
				printk(KERN_ERR"get test_rt1 dev %s\n",per_cpu(test_rt1, i)->dst.dev->name);
			}
		}
	}
	rv = nf_register_hook(&wanin_ops);
	recv_cnt = 0;
	return rv;
}

static void __exit udp_relay_fini(void)
{
	nf_unregister_hook(&wanin_ops);
	del_timer(&relayin_ticktimer);
}


MODULE_DESCRIPTION("DECnet Routing Message Grabulator");
MODULE_AUTHOR("Steven Whitehouse <steve@chygwyn.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS_NET_PF_PROTO(PF_NETLINK, NETLINK_DNRTMSG);

module_init(udp_relay_init);
module_exit(udp_relay_fini);

