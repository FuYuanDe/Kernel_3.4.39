/* linux/net/ipv4/arp.c
 *
 * Copyright (C) 1994 by Florian  La Roche
 *
 * This module implements the Address Resolution Protocol ARP (RFC 826),
 * which is used to convert IP addresses (or in the future maybe other
 * high-level addresses) into a low-level hardware address (like an Ethernet
 * address).
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Fixes:
 *		Alan Cox	:	Removed the Ethernet assumptions in
 *					Florian's code
 *		Alan Cox	:	Fixed some small errors in the ARP
 *					logic
 *		Alan Cox	:	Allow >4K in /proc
 *		Alan Cox	:	Make ARP add its own protocol entry
 *		Ross Martin     :       Rewrote arp_rcv() and arp_get_info()
 *		Stephen Henson	:	Add AX25 support to arp_get_info()
 *		Alan Cox	:	Drop data when a device is downed.
 *		Alan Cox	:	Use init_timer().
 *		Alan Cox	:	Double lock fixes.
 *		Martin Seine	:	Move the arphdr structure
 *					to if_arp.h for compatibility.
 *					with BSD based programs.
 *		Andrew Tridgell :       Added ARP netmask code and
 *					re-arranged proxy handling.
 *		Alan Cox	:	Changed to use notifiers.
 *		Niibe Yutaka	:	Reply for this device or proxies only.
 *		Alan Cox	:	Don't proxy across hardware types!
 *		Jonathan Naylor :	Added support for NET/ROM.
 *		Mike Shaver     :       RFC1122 checks.
 *		Jonathan Naylor :	Only lookup the hardware address for
 *					the correct hardware type.
 *		Germano Caronni	:	Assorted subtle races.
 *		Craig Schlenter :	Don't modify permanent entry
 *					during arp_rcv.
 *		Russ Nelson	:	Tidied up a few bits.
 *		Alexey Kuznetsov:	Major changes to caching and behaviour,
 *					eg intelligent arp probing and
 *					generation
 *					of host down events.
 *		Alan Cox	:	Missing unlock in device events.
 *		Eckes		:	ARP ioctl control errors.
 *		Alexey Kuznetsov:	Arp free fix.
 *		Manuel Rodriguez:	Gratuitous ARP.
 *              Jonathan Layes  :       Added arpd support through kerneld
 *                                      message queue (960314)
 *		Mike Shaver	:	/proc/sys/net/ipv4/arp_* support
 *		Mike McLagan    :	Routing by source
 *		Stuart Cheshire	:	Metricom and grat arp fixes
 *					*** FOR 2.1 clean this up ***
 *		Lawrence V. Stefani: (08/12/96) Added FDDI support.
 *		Alan Cox	:	Took the AP1000 nasty FDDI hack and
 *					folded into the mainstream FDDI code.
 *					Ack spit, Linus how did you allow that
 *					one in...
 *		Jes Sorensen	:	Make FDDI work again in 2.1.x and
 *					clean up the APFDDI & gen. FDDI bits.
 *		Alexey Kuznetsov:	new arp state machine;
 *					now it is in net/core/neighbour.c.
 *		Krzysztof Halasa:	Added Frame Relay ARP support.
 *		Arnaldo C. Melo :	convert /proc/net/arp to seq_file
 *		Shmulik Hen:		Split arp_send to arp_create and
 *					arp_xmit so intermediate drivers like
 *					bonding can change the skb before
 *					sending (e.g. insert 8021q tag).
 *		Harald Welte	:	convert to make use of jenkins hash
 *		Jesper D. Brouer:       Proxy ARP PVLAN RFC 3069 support.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/capability.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/mm.h>
#include <linux/inet.h>
#include <linux/inetdevice.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/fddidevice.h>
#include <linux/if_arp.h>
#include <linux/trdevice.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/net.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#endif

#include <net/net_namespace.h>
#include <net/ip.h>
#include <net/icmp.h>
#include <net/route.h>
#include <net/protocol.h>
#include <net/tcp.h>
#include <net/sock.h>
#include <net/arp.h>
#include <net/ax25.h>
#include <net/netrom.h>

#include <linux/uaccess.h>

#include <linux/netfilter_arp.h>

/*
 *	Interface to generic neighbour cache.
 */
static u32 arp_hash(const void *pkey, const struct net_device *dev, __u32 *hash_rnd);
static int arp_constructor(struct neighbour *neigh);
static void arp_solicit(struct neighbour *neigh, struct sk_buff *skb);
static void arp_error_report(struct neighbour *neigh, struct sk_buff *skb);
static void parp_redo(struct sk_buff *skb);

static const struct neigh_ops arp_generic_ops = {
	.family =		AF_INET,
	.solicit =		arp_solicit,
	.error_report =		arp_error_report,
	.output =		neigh_resolve_output,
	.connected_output =	neigh_connected_output,
};


static const struct neigh_ops arp_hh_ops = {
	.family =		AF_INET,
	.solicit =		arp_solicit,
	.error_report =		arp_error_report,
	.output =		neigh_resolve_output,
	.connected_output =	neigh_resolve_output,
};

//设备不需要L2帧头
static const struct neigh_ops arp_direct_ops = {
	.family =		AF_INET,
	.output =		neigh_direct_output,
	.connected_output =	neigh_direct_output,
};

static const struct neigh_ops arp_broken_ops = {
	.family =		AF_INET,
	.solicit =		arp_solicit,
	.error_report =		arp_error_report,
	.output =		neigh_compat_output,
	.connected_output =	neigh_compat_output,
};

struct neigh_table arp_tbl = {
	.family		= AF_INET,
	.key_len	= 4,
	.hash		= arp_hash,					//计算hash值的一个函数
	.constructor	= arp_constructor, 		//邻居项初始化函数
	.proxy_redo	= parp_redo,				//处理arp代理的函数
	.id		= "arp_cache", 					//邻居项缓存池名
	.parms		= {
		.tbl			= &arp_tbl,
		.base_reachable_time	= 30 * HZ,	//只有在30秒内收到可到达性确认才承认reachable状态
		.retrans_time		= 1 * HZ,		//solicit请求重传时间
		.gc_staletime		= 60 * HZ,		//stale状态的最长持续时间
		.reachable_time		= 30 * HZ,		//reachable状态的最长时间
		.delay_probe_time	= 5 * HZ,		//delay状态的最长时间
		.queue_len_bytes	= 64*1024,
		.ucast_probes		= 3,			//单播地址探测次数
		.mcast_probes		= 3,			//多播地址探测次数
		.anycast_delay		= 1 * HZ,
		.proxy_delay		= (8 * HZ) / 10,
		.proxy_qlen		= 64,
		.locktime		= 1 * HZ,
	},
	.gc_interval	= 30 * HZ,				//垃圾回收定时器
	.gc_thresh1	= 128,						//保留
	.gc_thresh2	= 512,						//邻居项阈值
	.gc_thresh3	= 1024, 					//邻居项阈值
};
EXPORT_SYMBOL(arp_tbl);

FUNC_ARP_HANDLE *arp_proxy_handle = NULL;
EXPORT_SYMBOL(arp_proxy_handle);

static int arp_win_policy_flag = 1;

static unsigned char hw_mac_broadcast_source[ETH_ALEN];

int arp_mc_map(__be32 addr, u8 *haddr, struct net_device *dev, int dir)
{
	switch (dev->type) {
	case ARPHRD_ETHER:
	case ARPHRD_FDDI:
	case ARPHRD_IEEE802:
		ip_eth_mc_map(addr, haddr);
		return 0;
	case ARPHRD_IEEE802_TR:
		ip_tr_mc_map(addr, haddr);
		return 0;
	case ARPHRD_INFINIBAND:
		ip_ib_mc_map(addr, dev->broadcast, haddr);
		return 0;
	case ARPHRD_IPGRE:
		ip_ipgre_mc_map(addr, dev->broadcast, haddr);
		return 0;
	default:
		if (dir) {
			memcpy(haddr, dev->broadcast, dev->addr_len);
			return 0;
		}
	}
	return -EINVAL;
}


static u32 arp_hash(const void *pkey,
		    const struct net_device *dev,
		    __u32 *hash_rnd)
{
	return arp_hashfn(*(u32 *)pkey, dev, *hash_rnd);
}

//邻居初始化函数
static int arp_constructor(struct neighbour *neigh)
{
	__be32 addr = *(__be32 *)neigh->primary_key;
	struct net_device *dev = neigh->dev;
	struct in_device *in_dev;
	struct neigh_parms *parms;

	rcu_read_lock();
	//获取该邻居项使用的设备，失败则返回
	in_dev = __in_dev_get_rcu(dev);
	if (in_dev == NULL) {
		rcu_read_unlock();
		return -EINVAL;
	}

	//获取地址类型，比如单播、多播或者广播
	neigh->type = inet_addr_type(dev_net(dev), addr);

	//将配置参数改成设备的配置参数
	//先释放邻居表默认的参数引用
	//然后增加dev的参数引用
	parms = in_dev->arp_parms;
	__neigh_parms_put(neigh->parms);
	neigh->parms = neigh_parms_clone(parms);
	rcu_read_unlock();

	//根据设备能力设置操作函数集
	if (!dev->header_ops) {
		//如果设备不需要ARP的话，走这里
		neigh->nud_state = NUD_NOARP;
		neigh->ops = &arp_direct_ops;
		neigh->output = neigh_direct_output;
	} else {
		/* Good devices (checked by reading texts, but only Ethernet is
		   tested)

		   ARPHRD_ETHER: (ethernet, apfddi)
		   ARPHRD_FDDI: (fddi)
		   ARPHRD_IEEE802: (tr)
		   ARPHRD_METRICOM: (strip)
		   ARPHRD_ARCNET:
		   etc. etc. etc.

		   ARPHRD_IPDDP will also work, if author repairs it.
		   I did not it, because this driver does not work even
		   in old paradigm.
		 */

#if 1
		/* So... these "amateur" devices are hopeless.
		   The only thing, that I can say now:
		   It is very sad that we need to keep ugly obsolete
		   code to make them happy.

		   They should be moved to more reasonable state, now
		   they use rebuild_header INSTEAD OF hard_start_xmit!!!
		   Besides that, they are sort of out of date
		   (a lot of redundant clones/copies, useless in 2.1),
		   I wonder why people believe that they work.
		 */
		switch (dev->type) {
		default:
			break;
		case ARPHRD_ROSE:
#if IS_ENABLED(CONFIG_AX25)
		case ARPHRD_AX25:
#if IS_ENABLED(CONFIG_NETROM)
		case ARPHRD_NETROM:
#endif
			neigh->ops = &arp_broken_ops;
			neigh->output = neigh->ops->output;
			return 0;
#else
			break;
#endif
		}
#endif
		//多播mac地址可以计算出来，不需要ARP
		if (neigh->type == RTN_MULTICAST) {
			neigh->nud_state = NUD_NOARP;
			arp_mc_map(addr, neigh->ha, dev, 1);
		} else if (dev->flags & (IFF_NOARP | IFF_LOOPBACK)) {
			//环回接口也是不需要ARP
			neigh->nud_state = NUD_NOARP;
			memcpy(neigh->ha, dev->dev_addr, dev->addr_len);
		} else if (neigh->type == RTN_BROADCAST ||
			   (dev->flags & IFF_POINTOPOINT)) {
			//点对点或者广播的mac地址也是已知的   
			neigh->nud_state = NUD_NOARP;
			memcpy(neigh->ha, dev->broadcast, dev->addr_len);
		}

		//根据设备能力选择函数集，这些函数包括邻居项操作函数以及于L3层接口

		//如果设备提供L2帧头缓存则选择arp_hh_ops，否则选择一个通用的arp_generic_ops
		if (dev->header_ops->cache)
			neigh->ops = &arp_hh_ops;
		else
			neigh->ops = &arp_generic_ops;

		//根据邻居状态配置输出接口
		if (neigh->nud_state & NUD_VALID)
			neigh->output = neigh->ops->connected_output;
		else
			neigh->output = neigh->ops->output;
	}
	return 0;
}

static void arp_error_report(struct neighbour *neigh, struct sk_buff *skb)
{
	dst_link_failure(skb);
	kfree_skb(skb);
}

//发送arp solicit请求报文
static void arp_solicit(struct neighbour *neigh, struct sk_buff *skb)
{
	__be32 saddr = 0;
	u8  *dst_ha = NULL;
	struct net_device *dev = neigh->dev;

	//目的地址
	__be32 target = *(__be32 *)neigh->primary_key;
	int probes = atomic_read(&neigh->probes);
	struct in_device *in_dev;

	rcu_read_lock();
	//获取IPv4的设备配置，获取失败直接返回
	in_dev = __in_dev_get_rcu(dev);
	if (!in_dev) {
		rcu_read_unlock();
		return;
	}
	//当产生solicit请求的主机有多个地址，ANNOUNCE属性标识选择哪一个
	switch (IN_DEV_ARP_ANNOUNCE(in_dev)) {
	default:
	case 0:		/* By default announce any local IP */
		//0表示任意本地地址都可以
		if (skb && inet_addr_type(dev_net(dev),
					  ip_hdr(skb)->saddr) == RTN_LOCAL)
			saddr = ip_hdr(skb)->saddr;
		break;
	case 1:		/* Restrict announcements of saddr in same subnet */
		//选择和目的地址位于同一个子网内的地址
		if (!skb)
			break;
		saddr = ip_hdr(skb)->saddr;
		if (inet_addr_type(dev_net(dev), saddr) == RTN_LOCAL) {
			/* saddr should be known to target */
			if (inet_addr_onlink(in_dev, target, saddr))
				break;
		}
		saddr = 0;
		break;
	case 2:		/* Avoid secondary IPs, get a primary/preferred one */
		//优先使用主地址
		break;
	}
	rcu_read_unlock();

	//如果源地址不存在的话，选择本地设备上一个scope小于link的地址
	if (!saddr)
		saddr = inet_select_addr(dev, target, RT_SCOPE_LINK);

	
	probes -= neigh->parms->ucast_probes;
	if (probes < 0) {
		if (!(neigh->nud_state & NUD_VALID))
			printk(KERN_DEBUG
			       "trying to ucast probe in NUD_INVALID\n");
		dst_ha = neigh->ha;
		read_lock_bh(&neigh->lock);
	} else {
		probes -= neigh->parms->app_probes;
		if (probes < 0) {
#ifdef CONFIG_ARPD
			neigh_app_ns(neigh);
#endif
			return;
		}
	}

	//发送solicit请求
	arp_send(ARPOP_REQUEST, ETH_P_ARP, target, dev, saddr,
		 dst_ha, dev->dev_addr, NULL);
	if (dst_ha)
		read_unlock_bh(&neigh->lock);
}

static int arp_ignore(struct in_device *in_dev, __be32 sip, __be32 tip)
{
	int scope;

	switch (IN_DEV_ARP_IGNORE(in_dev)) {
	case 0:	/* Reply, the tip is already validated */
		return 0;
	case 1:	/* Reply only if tip is configured on the incoming interface */
		sip = 0;
		scope = RT_SCOPE_HOST;
		break;
	case 2:	/*
		 * Reply only if tip is configured on the incoming interface
		 * and is in same subnet as sip
		 */
		scope = RT_SCOPE_HOST;
		break;
	case 3:	/* Do not reply for scope host addresses */
		sip = 0;
		scope = RT_SCOPE_LINK;
		break;
	case 4:	/* Reserved */
	case 5:
	case 6:
	case 7:
		return 0;
	case 8:	/* Do not reply */
		return 1;
	default:
		return 0;
	}
	return !inet_confirm_addr(in_dev, sip, tip, scope);
}

static int arp_filter(__be32 sip, __be32 tip, struct net_device *dev)
{
	struct rtable *rt;
	int flag = 0;
	/*unsigned long now; */
	struct net *net = dev_net(dev);

	rt = ip_route_output(net, sip, tip, 0, 0);
	if (IS_ERR(rt))
		return 1;
	if (rt->dst.dev != dev) {
		NET_INC_STATS_BH(net, LINUX_MIB_ARPFILTER);
		flag = 1;
	}
	ip_rt_put(rt);
	return flag;
}

/* OBSOLETE FUNCTIONS */

/*
 *	Find an arp mapping in the cache. If not found, post a request.
 *
 *	It is very UGLY routine: it DOES NOT use skb->dst->neighbour,
 *	even if it exists. It is supposed that skb->dev was mangled
 *	by a virtual device (eql, shaper). Nobody but broken devices
 *	is allowed to use this function, it is scheduled to be removed. --ANK
 */

static int arp_set_predefined(int addr_hint, unsigned char *haddr,
			      __be32 paddr, struct net_device *dev)
{
	switch (addr_hint) {
	case RTN_LOCAL:
		printk(KERN_DEBUG "ARP: arp called for own IP address\n");
		memcpy(haddr, dev->dev_addr, dev->addr_len);
		return 1;
	case RTN_MULTICAST:
		arp_mc_map(paddr, haddr, dev, 1);
		return 1;
	case RTN_BROADCAST:
		memcpy(haddr, dev->broadcast, dev->addr_len);
		return 1;
	}
	return 0;
}


int arp_find(unsigned char *haddr, struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	__be32 paddr;
	struct neighbour *n;

	if (!skb_dst(skb)) {
		printk(KERN_DEBUG "arp_find is called with dst==NULL\n");
		kfree_skb(skb);
		return 1;
	}

	paddr = skb_rtable(skb)->rt_gateway;

	if (arp_set_predefined(inet_addr_type(dev_net(dev), paddr), haddr,
			       paddr, dev))
		return 0;

	n = __neigh_lookup(&arp_tbl, &paddr, dev, 1);

	if (n) {
		n->used = jiffies;
		if (n->nud_state & NUD_VALID || neigh_event_send(n, skb) == 0) {
			neigh_ha_snapshot(haddr, n, dev);
			neigh_release(n);
			return 0;
		}
		neigh_release(n);
	} else
		kfree_skb(skb);
	return 1;
}
EXPORT_SYMBOL(arp_find);

/* END OF OBSOLETE FUNCTIONS */

/*
 * Check if we can use proxy ARP for this path
 */
static inline int arp_fwd_proxy(struct in_device *in_dev,
				struct net_device *dev,	struct rtable *rt)
{
	struct in_device *out_dev;
	int imi, omi = -1;

	if (rt->dst.dev == dev)
		return 0;

	if (!IN_DEV_PROXY_ARP(in_dev))
		return 0;
	imi = IN_DEV_MEDIUM_ID(in_dev);
	if (imi == 0)
		return 1;
	if (imi == -1)
		return 0;

	/* place to check for proxy_arp for routes */

	out_dev = __in_dev_get_rcu(rt->dst.dev);
	if (out_dev)
		omi = IN_DEV_MEDIUM_ID(out_dev);

	return omi != imi && omi != -1;
}

/*
 * Check for RFC3069 proxy arp private VLAN (allow to send back to same dev)
 *
 * RFC3069 supports proxy arp replies back to the same interface.  This
 * is done to support (ethernet) switch features, like RFC 3069, where
 * the individual ports are not allowed to communicate with each
 * other, BUT they are allowed to talk to the upstream router.  As
 * described in RFC 3069, it is possible to allow these hosts to
 * communicate through the upstream router, by proxy_arp'ing.
 *
 * RFC 3069: "VLAN Aggregation for Efficient IP Address Allocation"
 *
 *  This technology is known by different names:
 *    In RFC 3069 it is called VLAN Aggregation.
 *    Cisco and Allied Telesyn call it Private VLAN.
 *    Hewlett-Packard call it Source-Port filtering or port-isolation.
 *    Ericsson call it MAC-Forced Forwarding (RFC Draft).
 *
 */
static inline int arp_fwd_pvlan(struct in_device *in_dev,
				struct net_device *dev,	struct rtable *rt,
				__be32 sip, __be32 tip)
{
	/* Private VLAN is only concerned about the same ethernet segment */
	if (rt->dst.dev != dev)
		return 0;

	/* Don't reply on self probes (often done by windowz boxes)*/
	if (sip == tip)
		return 0;

	if (IN_DEV_PROXY_ARP_PVLAN(in_dev))
		return 1;
	else
		return 0;
}

/*
 *	Interface to link layer: send routine and receive handler.
 */

/*
 *	Create an arp packet. If (dest_hw == NULL), we create a broadcast
 *	message.
 */
struct sk_buff *arp_create(int type, int ptype, __be32 dest_ip,
			   struct net_device *dev, __be32 src_ip,
			   const unsigned char *dest_hw,
			   const unsigned char *src_hw,
			   const unsigned char *target_hw)
{
	struct sk_buff *skb;
	struct arphdr *arp;
	unsigned char *arp_ptr;
	int hlen = LL_RESERVED_SPACE(dev);
	int tlen = dev->needed_tailroom;

	/*
	 *	Allocate a buffer
	 */
	//创建一个arp skb
	skb = alloc_skb(arp_hdr_len(dev) + hlen + tlen, GFP_ATOMIC);
	if (skb == NULL)
		return NULL;

	skb_reserve(skb, hlen);
	skb_reset_network_header(skb);
	arp = (struct arphdr *) skb_put(skb, arp_hdr_len(dev));
	skb->dev = dev;
	skb->protocol = htons(ETH_P_ARP);
	if (src_hw == NULL)
		src_hw = dev->dev_addr;
	if (dest_hw == NULL)
		dest_hw = dev->broadcast;

	/*
	 *	Fill the device header for the ARP frame
	 */
	if (dev_hard_header(skb, dev, ptype, dest_hw, src_hw, skb->len) < 0)
		goto out;

	/*
	 * Fill out the arp protocol part.
	 *
	 * The arp hardware type should match the device type, except for FDDI,
	 * which (according to RFC 1390) should always equal 1 (Ethernet).
	 */
	/*
	 *	Exceptions everywhere. AX.25 uses the AX.25 PID value not the
	 *	DIX code for the protocol. Make these device structure fields.
	 */
	switch (dev->type) {
	default:
		arp->ar_hrd = htons(dev->type);
		arp->ar_pro = htons(ETH_P_IP);
		break;

#if IS_ENABLED(CONFIG_AX25)
	case ARPHRD_AX25:
		arp->ar_hrd = htons(ARPHRD_AX25);
		arp->ar_pro = htons(AX25_P_IP);
		break;

#if IS_ENABLED(CONFIG_NETROM)
	case ARPHRD_NETROM:
		arp->ar_hrd = htons(ARPHRD_NETROM);
		arp->ar_pro = htons(AX25_P_IP);
		break;
#endif
#endif

#if IS_ENABLED(CONFIG_FDDI)
	case ARPHRD_FDDI:
		arp->ar_hrd = htons(ARPHRD_ETHER);
		arp->ar_pro = htons(ETH_P_IP);
		break;
#endif
#if IS_ENABLED(CONFIG_TR)
	case ARPHRD_IEEE802_TR:
		arp->ar_hrd = htons(ARPHRD_IEEE802);
		arp->ar_pro = htons(ETH_P_IP);
		break;
#endif
	}

	arp->ar_hln = dev->addr_len;
	arp->ar_pln = 4;
	arp->ar_op = htons(type);

	arp_ptr = (unsigned char *)(arp + 1);

	memcpy(arp_ptr, src_hw, dev->addr_len);
	arp_ptr += dev->addr_len;
	memcpy(arp_ptr, &src_ip, 4);
	arp_ptr += 4;
	if (target_hw != NULL)
		memcpy(arp_ptr, target_hw, dev->addr_len);
	else
		memset(arp_ptr, 0, dev->addr_len);
	arp_ptr += dev->addr_len;
	memcpy(arp_ptr, &dest_ip, 4);

	return skb;

out:
	kfree_skb(skb);
	return NULL;
}
EXPORT_SYMBOL(arp_create);

/*
 *	Send an arp packet.
 */
void arp_xmit(struct sk_buff *skb)
{
	/* Send it off, maybe filter it using firewalling first.  */
	//先让netfilter的防护墙过一遍，没问题的话在调用dev_queue_xmit发送
	NF_HOOK(NFPROTO_ARP, NF_ARP_OUT, skb, NULL, skb->dev, dev_queue_xmit);
}
EXPORT_SYMBOL(arp_xmit);

/*
 *	Create and send an arp packet.
 */
void arp_send(int type, int ptype, __be32 dest_ip,
	      struct net_device *dev, __be32 src_ip,
	      const unsigned char *dest_hw, const unsigned char *src_hw,
	      const unsigned char *target_hw)
{
	struct sk_buff *skb;

	/*
	 *	No arp on this interface.
	 */
	//如果该设备不允许发送ARP的话直接返回就好了
	if (dev->flags&IFF_NOARP)
		return;

	//创建一个arp报文
	skb = arp_create(type, ptype, dest_ip, dev, src_ip,
			 dest_hw, src_hw, target_hw);
	if (skb == NULL)
		return;

	//调用arp_xmit发送
	arp_xmit(skb);
}
EXPORT_SYMBOL(arp_send);

/*
 *	Process an arp request.
 */

//处理arp报文
static int arp_process(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct in_device *in_dev = __in_dev_get_rcu(dev);
	struct arphdr *arp;
	unsigned char *arp_ptr;
	struct rtable *rt;
	unsigned char *sha;
	__be32 sip, tip;
	u16 dev_type = dev->type;
	int addr_type;
	struct neighbour *n;
	struct net *net = dev_net(dev);

	/* arp_rcv below verifies the ARP header and verifies the device
	 * is ARP'able.
	 */
	//每个网络设备都有IP配置，如果没有的话就不用处理了
	if (in_dev == NULL)
		goto out;

	//arp首部
	arp = arp_hdr(skb);

	//判断设备类型
	switch (dev_type) {
	default:
		if (arp->ar_pro != htons(ETH_P_IP) ||
		    htons(dev_type) != arp->ar_hrd)
			goto out;
		break;
	case ARPHRD_ETHER:
	case ARPHRD_IEEE802_TR:
	case ARPHRD_FDDI:
	case ARPHRD_IEEE802:
		/*
		 * ETHERNET, Token Ring and Fibre Channel (which are IEEE 802
		 * devices, according to RFC 2625) devices will accept ARP
		 * hardware types of either 1 (Ethernet) or 6 (IEEE 802.2).
		 * This is the case also of FDDI, where the RFC 1390 says that
		 * FDDI devices should accept ARP hardware of (1) Ethernet,
		 * however, to be more robust, we'll accept both 1 (Ethernet)
		 * or 6 (IEEE 802.2)
		 */
		if ((arp->ar_hrd != htons(ARPHRD_ETHER) &&
		     arp->ar_hrd != htons(ARPHRD_IEEE802)) ||
		    arp->ar_pro != htons(ETH_P_IP))
			goto out;
		break;
	case ARPHRD_AX25:
		if (arp->ar_pro != htons(AX25_P_IP) ||
		    arp->ar_hrd != htons(ARPHRD_AX25))
			goto out;
		break;
	case ARPHRD_NETROM:
		if (arp->ar_pro != htons(AX25_P_IP) ||
		    arp->ar_hrd != htons(ARPHRD_NETROM))
			goto out;
		break;
	}

	/* Understand only these message types */

	//只处理request和reply类型的ARP
	if (arp->ar_op != htons(ARPOP_REPLY) &&
	    arp->ar_op != htons(ARPOP_REQUEST))
		goto out;

/*
 *	Extract fields
 */
	arp_ptr = (unsigned char *)(arp + 1);
	sha	= arp_ptr;
	arp_ptr += dev->addr_len;
	memcpy(&sip, arp_ptr, 4);
	arp_ptr += 4;
	arp_ptr += dev->addr_len;
	memcpy(&tip, arp_ptr, 4);
/*
 *	Check for bad requests for 127.x.x.x and requests for multicast
 *	addresses.  If this is one such, delete it.
 */
 	//过滤非法报文，换回地址和多播地址是不需要ARP的
	if (ipv4_is_loopback(tip) || ipv4_is_multicast(tip))
		goto out;

/*
 *     Special case: We must set Frame Relay source Q.922 address
 */
	if (dev_type == ARPHRD_DLCI)
		sha = dev->broadcast;

/*
 *  Process entry.  The idea here is we want to send a reply if it is a
 *  request for us or if it is a request for someone else that we hold
 *  a proxy for.  We want to add an entry to our cache if it is a reply
 *  to us or if it is a request for our address.
 *  (The assumption for this last is that if someone is requesting our
 *  address, they are probably intending to talk to us, so it saves time
 *  if we cache their address.  Their address is also probably not in
 *  our cache, since ours is not in their cache.)
 *
 *  Putting this another way, we only care about replies if they are to
 *  us, in which case we add them to the cache.  For requests, we care
 *  about those for us and those for our proxies.  We reply to both,
 *  and in the case of requests for us we add the requester to the arp
 *  cache.
 */

	/* Special case: IPv4 duplicate address detection packet (RFC2131) */
	if (sip == 0) {

        //有可能是DHCP发出的重复地址探测报文，这时候需要给个回复
		if (arp->ar_op == htons(ARPOP_REQUEST) &&
		    inet_addr_type(net, tip) == RTN_LOCAL &&
		    !arp_ignore(in_dev, sip, tip))
			arp_send(ARPOP_REPLY, ETH_P_ARP, sip, dev, tip, sha,
				 dev->dev_addr, sha);
		goto out;
	}

	//对于请求类型的报文，还必须确保系统可以达到，即存在到源地址的路由
	if (arp->ar_op == htons(ARPOP_REQUEST) &&
	    ip_route_input_noref(skb, tip, sip, 0, dev) == 0) {

		rt = skb_rtable(skb);
		addr_type = rt->rt_type;

		//发往本地的报文
		if (addr_type == RTN_LOCAL) {
			int dont_send;

			//ignore属性是指在多IP的主机上收到一个目的IP不属于接收接口的arp时
			//是否做出相应，可以在/proc/sys/net/ipv4/conf/all/arp_ignore配置
			dont_send = arp_ignore(in_dev, sip, tip);

			//arp filter属性控制一个接口是否对入口request做出应答
			if (!dont_send && IN_DEV_ARPFILTER(in_dev))
				dont_send = arp_filter(sip, tip, dev);

			//可以处理的话则走进去	
			if (!dont_send) {
				//查找邻居表项并更新
				n = neigh_event_ns(&arp_tbl, sha, &sip, dev);

				//存在邻居表项则调用arp回复
				if (n) {
					arp_send(ARPOP_REPLY, ETH_P_ARP, sip,
						 dev, tip, sha, dev->dev_addr,
						 sha);
					//释放查找时添加的引用	 
					neigh_release(n);
				}
			}
			goto out;
		} else if (IN_DEV_FORWARD(in_dev)) {
			//既然不是发给自己的，下面就是ARP代理的处理
			if (addr_type == RTN_UNICAST  &&
			    (arp_fwd_proxy(in_dev, dev, rt) ||
			     arp_fwd_pvlan(in_dev, dev, rt, sip, tip) ||
			     (rt->dst.dev != dev &&
			      pneigh_lookup(&arp_tbl, net, &tip, dev, 0)))) {
				n = neigh_event_ns(&arp_tbl, sha, &sip, dev);
				if (n)
					neigh_release(n);

				if (NEIGH_CB(skb)->flags & LOCALLY_ENQUEUED ||
				    skb->pkt_type == PACKET_HOST ||
				    in_dev->arp_parms->proxy_delay == 0) {
					arp_send(ARPOP_REPLY, ETH_P_ARP, sip,
						 dev, tip, sha, dev->dev_addr,
						 sha);
				} else {
					pneigh_enqueue(&arp_tbl,
						       in_dev->arp_parms, skb);
					return 0;
				}
				goto out;
			}
		}
	}

    //查找arp缓存
	n = __neigh_lookup(&arp_tbl, &sip, dev, 0);
	
	//设备是否配置了接收unsolicit的arp请求选项
	if (IN_DEV_ARP_ACCEPT(in_dev)) {
		/* Unsolicited ARP is not accepted by default.
		   It is possible, that this option should be enabled for some
		   devices (strip is candidate)
		 */
		if (n == NULL &&
		    (arp->ar_op == htons(ARPOP_REPLY) ||
		     (arp->ar_op == htons(ARPOP_REQUEST) && tip == sip)) &&
		    inet_addr_type(net, sip) == RTN_UNICAST)
			n = __neigh_lookup(&arp_tbl, &sip, dev, 1);
	}

	//
	if (n) {
		int state = NUD_REACHABLE;
		int override;

		/* If several different ARP replies follows back-to-back,
		   use the FIRST one. It is possible, if several proxy
		   agents are active. Taking the first reply prevents
		   arp trashing and chooses the fastest router.
		 */
		//锁定期是指收到reply的之后一段时间
		//如果在这段时间内又收到reply则不做任何处理
		override = time_after(jiffies, n->updated + n->parms->locktime);

		/* Broadcast replies and request packets
		   do not assert neighbour reachability.
		 */
		//广播报文或者request报文的邻居表状态会更新成stale，
		//即还需要可到达性证明
		if (arp->ar_op != htons(ARPOP_REPLY) ||
		    skb->pkt_type != PACKET_HOST)
			state = NUD_STALE;
		//更新邻居表项	
		neigh_update(n, sha, state,
			     override ? NEIGH_UPDATE_F_OVERRIDE : 0);
		//释放查找时添加的引用     
		neigh_release(n);
	}

out:
	//释放报文缓存
	consume_skb(skb);
	return 0;
}

static void parp_redo(struct sk_buff *skb)
{
	arp_process(skb);
}


/*
 *	Receive an arp request from the device layer.
 */
//ARP报文接受处理函数
static int arp_rcv(struct sk_buff *skb, struct net_device *dev,
		   struct packet_type *pt, struct net_device *orig_dev)
{
	struct arphdr *arp;

	/* ARP header, plus 2 device addresses, plus 2 IP addresses.  */
	if (!pskb_may_pull(skb, arp_hdr_len(dev)))
		goto freeskb;

	arp = arp_hdr(skb);
	//arp报文合理性检查
	if (arp->ar_hln != dev->addr_len ||
	    dev->flags & IFF_NOARP ||
	    skb->pkt_type == PACKET_OTHERHOST ||
	    skb->pkt_type == PACKET_LOOPBACK ||
	    arp->ar_pln != 4)
		goto freeskb;

	skb = skb_share_check(skb, GFP_ATOMIC);
	if (skb == NULL)
		goto out_of_mem;

	memset(NEIGH_CB(skb), 0, sizeof(struct neighbour_cb));

	//先让防火墙过一遍，没问题的话调用arp_process处理
	return NF_HOOK(NFPROTO_ARP, NF_ARP_IN, skb, dev, NULL, arp_process);

freeskb:
	kfree_skb(skb);
out_of_mem:
	return 0;
}

/*
 *	User level interface (ioctl)
 */

/*
 *	Set (create) an ARP cache entry.
 */

static int arp_req_set_proxy(struct net *net, struct net_device *dev, int on)
{
	if (dev == NULL) {
		IPV4_DEVCONF_ALL(net, PROXY_ARP) = on;
		return 0;
	}
	if (__in_dev_get_rtnl(dev)) {
		IN_DEV_CONF_SET(__in_dev_get_rtnl(dev), PROXY_ARP, on);
		return 0;
	}
	return -ENXIO;
}

static int arp_req_set_public(struct net *net, struct arpreq *r,
		struct net_device *dev)
{
	__be32 ip = ((struct sockaddr_in *)&r->arp_pa)->sin_addr.s_addr;
	__be32 mask = ((struct sockaddr_in *)&r->arp_netmask)->sin_addr.s_addr;

	if (mask && mask != htonl(0xFFFFFFFF))
		return -EINVAL;
	if (!dev && (r->arp_flags & ATF_COM)) {
		dev = dev_getbyhwaddr_rcu(net, r->arp_ha.sa_family,
				      r->arp_ha.sa_data);
		if (!dev)
			return -ENODEV;
	}
	if (mask) {
		if (pneigh_lookup(&arp_tbl, net, &ip, dev, 1) == NULL)
			return -ENOBUFS;
		return 0;
	}

	return arp_req_set_proxy(net, dev, 1);
}

static int arp_req_set(struct net *net, struct arpreq *r,
		       struct net_device *dev)
{
	__be32 ip;
	struct neighbour *neigh;
	int err;

	if (r->arp_flags & ATF_PUBL)
		return arp_req_set_public(net, r, dev);

	ip = ((struct sockaddr_in *)&r->arp_pa)->sin_addr.s_addr;
	if (r->arp_flags & ATF_PERM)
		r->arp_flags |= ATF_COM;
	if (dev == NULL) {
		struct rtable *rt = ip_route_output(net, ip, 0, RTO_ONLINK, 0);

		if (IS_ERR(rt))
			return PTR_ERR(rt);
		dev = rt->dst.dev;
		ip_rt_put(rt);
		if (!dev)
			return -EINVAL;
	}
	switch (dev->type) {
#if IS_ENABLED(CONFIG_FDDI)
	case ARPHRD_FDDI:
		/*
		 * According to RFC 1390, FDDI devices should accept ARP
		 * hardware types of 1 (Ethernet).  However, to be more
		 * robust, we'll accept hardware types of either 1 (Ethernet)
		 * or 6 (IEEE 802.2).
		 */
		if (r->arp_ha.sa_family != ARPHRD_FDDI &&
		    r->arp_ha.sa_family != ARPHRD_ETHER &&
		    r->arp_ha.sa_family != ARPHRD_IEEE802)
			return -EINVAL;
		break;
#endif
	default:
		if (r->arp_ha.sa_family != dev->type)
			return -EINVAL;
		break;
	}

	neigh = __neigh_lookup_errno(&arp_tbl, &ip, dev);
	err = PTR_ERR(neigh);
	if (!IS_ERR(neigh)) {
		unsigned state = NUD_STALE;
		if (r->arp_flags & ATF_PERM)
			state = NUD_PERMANENT;
		err = neigh_update(neigh, (r->arp_flags & ATF_COM) ?
				   r->arp_ha.sa_data : NULL, state,
				   NEIGH_UPDATE_F_OVERRIDE |
				   NEIGH_UPDATE_F_ADMIN);
		neigh_release(neigh);
	}
	return err;
}

static unsigned arp_state_to_flags(struct neighbour *neigh)
{
	if (neigh->nud_state&NUD_PERMANENT)
		return ATF_PERM | ATF_COM;
	else if (neigh->nud_state&NUD_VALID)
		return ATF_COM;
	else
		return 0;
}

/*
 *	Get an ARP cache entry.
 */

static int arp_req_get(struct arpreq *r, struct net_device *dev)
{
	__be32 ip = ((struct sockaddr_in *) &r->arp_pa)->sin_addr.s_addr;
	struct neighbour *neigh;
	int err = -ENXIO;

	neigh = neigh_lookup(&arp_tbl, &ip, dev);
	if (neigh) {
		read_lock_bh(&neigh->lock);
		memcpy(r->arp_ha.sa_data, neigh->ha, dev->addr_len);
		r->arp_flags = arp_state_to_flags(neigh);
		read_unlock_bh(&neigh->lock);
		r->arp_ha.sa_family = dev->type;
		strlcpy(r->arp_dev, dev->name, sizeof(r->arp_dev));
		neigh_release(neigh);
		err = 0;
	}
	return err;
}

int arp_invalidate(struct net_device *dev, __be32 ip)
{
	struct neighbour *neigh = neigh_lookup(&arp_tbl, &ip, dev);
	int err = -ENXIO;

	if (neigh) {
		if (neigh->nud_state & ~NUD_NOARP)
			err = neigh_update(neigh, NULL, NUD_FAILED,
					   NEIGH_UPDATE_F_OVERRIDE|
					   NEIGH_UPDATE_F_ADMIN);
		neigh_release(neigh);
	}

	return err;
}
EXPORT_SYMBOL(arp_invalidate);

static int arp_req_delete_public(struct net *net, struct arpreq *r,
		struct net_device *dev)
{
	__be32 ip = ((struct sockaddr_in *) &r->arp_pa)->sin_addr.s_addr;
	__be32 mask = ((struct sockaddr_in *)&r->arp_netmask)->sin_addr.s_addr;

	if (mask == htonl(0xFFFFFFFF))
		return pneigh_delete(&arp_tbl, net, &ip, dev);

	if (mask)
		return -EINVAL;

	return arp_req_set_proxy(net, dev, 0);
}

static int arp_req_delete(struct net *net, struct arpreq *r,
			  struct net_device *dev)
{
	__be32 ip;

	if (r->arp_flags & ATF_PUBL)
		return arp_req_delete_public(net, r, dev);

	ip = ((struct sockaddr_in *)&r->arp_pa)->sin_addr.s_addr;
	if (dev == NULL) {
		struct rtable *rt = ip_route_output(net, ip, 0, RTO_ONLINK, 0);
		if (IS_ERR(rt))
			return PTR_ERR(rt);
		dev = rt->dst.dev;
		ip_rt_put(rt);
		if (!dev)
			return -EINVAL;
	}
	return arp_invalidate(dev, ip);
}

/*
 *	Handle an ARP layer I/O control request.
 */

int arp_ioctl(struct net *net, unsigned int cmd, void __user *arg)
{
	int err;
	struct arpreq r;
	struct net_device *dev = NULL;

	switch (cmd) {
	case SIOCDARP:
	case SIOCSARP:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
	case SIOCGARP:
		err = copy_from_user(&r, arg, sizeof(struct arpreq));
		if (err)
			return -EFAULT;
		break;
	default:
		return -EINVAL;
	}

	if (r.arp_pa.sa_family != AF_INET)
		return -EPFNOSUPPORT;

	if (!(r.arp_flags & ATF_PUBL) &&
	    (r.arp_flags & (ATF_NETMASK | ATF_DONTPUB)))
		return -EINVAL;
	if (!(r.arp_flags & ATF_NETMASK))
		((struct sockaddr_in *)&r.arp_netmask)->sin_addr.s_addr =
							   htonl(0xFFFFFFFFUL);
	rtnl_lock();
	if (r.arp_dev[0]) {
		err = -ENODEV;
		dev = __dev_get_by_name(net, r.arp_dev);
		if (dev == NULL)
			goto out;

		/* Mmmm... It is wrong... ARPHRD_NETROM==0 */
		if (!r.arp_ha.sa_family)
			r.arp_ha.sa_family = dev->type;
		err = -EINVAL;
		if ((r.arp_flags & ATF_COM) && r.arp_ha.sa_family != dev->type)
			goto out;
	} else if (cmd == SIOCGARP) {
		err = -ENODEV;
		goto out;
	}

	switch (cmd) {
	case SIOCDARP:
		err = arp_req_delete(net, &r, dev);
		break;
	case SIOCSARP:
		err = arp_req_set(net, &r, dev);
		break;
	case SIOCGARP:
		err = arp_req_get(&r, dev);
		break;
	}
out:
	rtnl_unlock();
	if (cmd == SIOCGARP && !err && copy_to_user(arg, &r, sizeof(r)))
		err = -EFAULT;
	return err;
}

static int arp_netdev_event(struct notifier_block *this, unsigned long event,
			    void *ptr)
{
	struct net_device *dev = ptr;

	switch (event) {
	case NETDEV_CHANGEADDR:
		neigh_changeaddr(&arp_tbl, dev);
		rt_cache_flush(dev_net(dev), 0);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block arp_netdev_notifier = {
	.notifier_call = arp_netdev_event,
};

/* Note, that it is not on notifier chain.
   It is necessary, that this routine was called after route cache will be
   flushed.
 */
void arp_ifdown(struct net_device *dev)
{
	neigh_ifdown(&arp_tbl, dev);
}


/*
 *	Called once on startup.
 */

static struct packet_type arp_packet_type __read_mostly = {
	.type =	cpu_to_be16(ETH_P_ARP),
	.func =	arp_rcv,
};

static int arp_proc_init(void);

//arp模块初始化
void __init arp_init(void)
{
    int i;

    //注册一个虚函数表和ARP协议使用的其他常用参数
	neigh_table_init(&arp_tbl);

	//注册arp报文处理函数
	dev_add_pack(&arp_packet_type);

	//注册proc文件
	arp_proc_init();
#ifdef CONFIG_SYSCTL

	//注册sys文件
	neigh_sysctl_register(NULL, &arp_tbl.parms, "ipv4", NULL);
#endif
	//向内核注册一个回调函数，用于接受设备状态和配置变化的通知
	register_netdevice_notifier(&arp_netdev_notifier);
}

#ifdef CONFIG_PROC_FS
#if IS_ENABLED(CONFIG_AX25)

/* ------------------------------------------------------------------------ */
/*
 *	ax25 -> ASCII conversion
 */
static char *ax2asc2(ax25_address *a, char *buf)
{
	char c, *s;
	int n;

	for (n = 0, s = buf; n < 6; n++) {
		c = (a->ax25_call[n] >> 1) & 0x7F;

		if (c != ' ')
			*s++ = c;
	}

	*s++ = '-';
	n = (a->ax25_call[6] >> 1) & 0x0F;
	if (n > 9) {
		*s++ = '1';
		n -= 10;
	}

	*s++ = n + '0';
	*s++ = '\0';

	if (*buf == '\0' || *buf == '-')
		return "*";

	return buf;
}
#endif /* CONFIG_AX25 */

#define HBUFFERLEN 30

static void arp_format_neigh_entry(struct seq_file *seq,
				   struct neighbour *n)
{
	char hbuffer[HBUFFERLEN];
	int k, j;
	char tbuf[16];
	struct net_device *dev = n->dev;
	int hatype = dev->type;

	read_lock(&n->lock);
	/* Convert hardware address to XX:XX:XX:XX ... form. */
#if IS_ENABLED(CONFIG_AX25)
	if (hatype == ARPHRD_AX25 || hatype == ARPHRD_NETROM)
		ax2asc2((ax25_address *)n->ha, hbuffer);
	else {
#endif
	for (k = 0, j = 0; k < HBUFFERLEN - 3 && j < dev->addr_len; j++) {
		hbuffer[k++] = hex_asc_hi(n->ha[j]);
		hbuffer[k++] = hex_asc_lo(n->ha[j]);
		hbuffer[k++] = ':';
	}
	if (k != 0)
		--k;
	hbuffer[k] = 0;
#if IS_ENABLED(CONFIG_AX25)
	}
#endif
	sprintf(tbuf, "%pI4", n->primary_key);
	seq_printf(seq, "%-16s 0x%-10x0x%-10x%s     *        %s\n",
		   tbuf, hatype, arp_state_to_flags(n), hbuffer, dev->name);
	read_unlock(&n->lock);
}

static void arp_format_pneigh_entry(struct seq_file *seq,
				    struct pneigh_entry *n)
{
	struct net_device *dev = n->dev;
	int hatype = dev ? dev->type : 0;
	char tbuf[16];

	sprintf(tbuf, "%pI4", n->key);
	seq_printf(seq, "%-16s 0x%-10x0x%-10x%s     *        %s\n",
		   tbuf, hatype, ATF_PUBL | ATF_PERM, "00:00:00:00:00:00",
		   dev ? dev->name : "*");
}

static int arp_seq_show(struct seq_file *seq, void *v)
{
	if (v == SEQ_START_TOKEN) {
		seq_puts(seq, "IP address       HW type     Flags       "
			      "HW address            Mask     Device\n");
	} else {
		struct neigh_seq_state *state = seq->private;

		if (state->flags & NEIGH_SEQ_IS_PNEIGH)
			arp_format_pneigh_entry(seq, v);
		else
			arp_format_neigh_entry(seq, v);
	}

	return 0;
}

static void *arp_seq_start(struct seq_file *seq, loff_t *pos)
{
	/* Don't want to confuse "arp -a" w/ magic entries,
	 * so we tell the generic iterator to skip NUD_NOARP.
	 */
	return neigh_seq_start(seq, pos, &arp_tbl, NEIGH_SEQ_SKIP_NOARP);
}

/* ------------------------------------------------------------------------ */

static const struct seq_operations arp_seq_ops = {
	.start	= arp_seq_start,
	.next	= neigh_seq_next,
	.stop	= neigh_seq_stop,
	.show	= arp_seq_show,
};

static int arp_seq_open(struct inode *inode, struct file *file)
{
	return seq_open_net(inode, file, &arp_seq_ops,
			    sizeof(struct neigh_seq_state));
}

static const struct file_operations arp_seq_fops = {
	.owner		= THIS_MODULE,
	.open           = arp_seq_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release	= seq_release_net,
};


static int arp_win_policy_read_proc(struct seq_file *m, void *v)
{
    
    seq_printf(m, "%d\n", arp_win_policy_flag);
    return 0;
}

static int arp_win_policy_open_proc(struct inode *inode, struct file *file)
{
    return single_open(file, arp_win_policy_read_proc, NULL);
}

static ssize_t arp_win_policy_write_proc(struct file* file, const char __user * buffer,
                             size_t count, loff_t *ppos)
{
    char *ipbuf;

    ipbuf = kmalloc(count + 1, GFP_ATOMIC);
    if (!ipbuf)
    {
        printk(KERN_ERR "arp win policy buffer kmalloc fial.\n");
        return -EFAULT;
    }
    memset(ipbuf, 0, count + 1);

    if (copy_from_user(ipbuf, buffer, count)) {
        kfree(ipbuf);
        return -EFAULT;
    }

    /* buffer to int */
    if (*ipbuf == '0')
    {
        arp_win_policy_flag = 0;
    }
    else if (*ipbuf == '1')
    {
        arp_win_policy_flag = 1;
    }

    kfree(ipbuf);

    return count;
}

static const struct file_operations arp_win_policy = {
        .open = arp_win_policy_open_proc,
        .read = seq_read,
        .write = arp_win_policy_write_proc,
        .llseek = default_llseek,
};

static int __net_init arp_net_init(struct net *net)
{
	if (!proc_net_fops_create(net, "arp", S_IRUGO, &arp_seq_fops))
		return -ENOMEM;

    proc_net_fops_create(net, "arp_win_policy", 0644, &arp_win_policy);
    
	return 0;
}

static void __net_exit arp_net_exit(struct net *net)
{
	proc_net_remove(net, "arp");
    proc_net_remove(net, "arp_win_policy");
}

static struct pernet_operations arp_net_ops = {
	.init = arp_net_init,
	.exit = arp_net_exit,
};

static int __init arp_proc_init(void)
{
	return register_pernet_subsys(&arp_net_ops);
}

#else /* CONFIG_PROC_FS */

static int __init arp_proc_init(void)
{
	return 0;
}

#endif /* CONFIG_PROC_FS */
