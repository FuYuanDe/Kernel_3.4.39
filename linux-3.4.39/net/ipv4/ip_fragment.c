/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		The IP fragmentation functionality.
 *
 * Authors:	Fred N. van Kempen <waltje@uWalt.NL.Mugnet.ORG>
 *		Alan Cox <alan@lxorguk.ukuu.org.uk>
 *
 * Fixes:
 *		Alan Cox	:	Split from ip.c , see ip_input.c for history.
 *		David S. Miller :	Begin massive cleanup...
 *		Andi Kleen	:	Add sysctls.
 *		xxxx		:	Overlapfrag bug.
 *		Ultima          :       ip_expire() kernel panic.
 *		Bill Hawes	:	Frag accounting and evictor fixes.
 *		John McDonald	:	0 length frag bug.
 *		Alexey Kuznetsov:	SMP races, threading, cleanup.
 *		Patrick McHardy :	LRU queue of frag heads for evictor.
 */

#define pr_fmt(fmt) "IPv4: " fmt

#include <linux/compiler.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/jiffies.h>
#include <linux/skbuff.h>
#include <linux/list.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/netdevice.h>
#include <linux/jhash.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <net/route.h>
#include <net/dst.h>
#include <net/sock.h>
#include <net/ip.h>
#include <net/icmp.h>
#include <net/checksum.h>
#include <net/inetpeer.h>
#include <net/inet_frag.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/inet.h>
#include <linux/netfilter_ipv4.h>
#include <net/inet_ecn.h>

/* NOTE. Logic of IP defragmentation is parallel to corresponding IPv6
 * code now. If you change something here, _PLEASE_ update ipv6/reassembly.c
 * as well. Or notify me, at least. --ANK
 */

static int sysctl_ipfrag_max_dist __read_mostly = 64;

struct ipfrag_skb_cb
{
	struct inet_skb_parm	h;
	int			offset;
};

#define FRAG_CB(skb)	((struct ipfrag_skb_cb *)((skb)->cb))

/* Describe an entry in the "incomplete datagrams" queue. */
struct ipq {
	struct inet_frag_queue q;

	u32		user;
	__be32		saddr;
	__be32		daddr;
	__be16		id;
	u8		protocol;
	u8		ecn; /* RFC3168 support */
	int             iif;
	unsigned int    rid;
	struct inet_peer *peer;
};

/* RFC 3168 support :
 * We want to check ECN values of all fragments, do detect invalid combinations.
 * In ipq->ecn, we store the OR value of each ip4_frag_ecn() fragment value.
 */
#define	IPFRAG_ECN_NOT_ECT	0x01 /* one frag had ECN_NOT_ECT */
#define	IPFRAG_ECN_ECT_1	0x02 /* one frag had ECN_ECT_1 */
#define	IPFRAG_ECN_ECT_0	0x04 /* one frag had ECN_ECT_0 */
#define	IPFRAG_ECN_CE		0x08 /* one frag had ECN_CE */

static inline u8 ip4_frag_ecn(u8 tos)
{
	return 1 << (tos & INET_ECN_MASK);
}

/* Given the OR values of all fragments, apply RFC 3168 5.3 requirements
 * Value : 0xff if frame should be dropped.
 *         0 or INET_ECN_CE value, to be ORed in to final iph->tos field
 */
static const u8 ip4_frag_ecn_table[16] = {
	/* at least one fragment had CE, and others ECT_0 or ECT_1 */
	[IPFRAG_ECN_CE | IPFRAG_ECN_ECT_0]			= INET_ECN_CE,
	[IPFRAG_ECN_CE | IPFRAG_ECN_ECT_1]			= INET_ECN_CE,
	[IPFRAG_ECN_CE | IPFRAG_ECN_ECT_0 | IPFRAG_ECN_ECT_1]	= INET_ECN_CE,

	/* invalid combinations : drop frame */
	[IPFRAG_ECN_NOT_ECT | IPFRAG_ECN_CE] = 0xff,
	[IPFRAG_ECN_NOT_ECT | IPFRAG_ECN_ECT_0] = 0xff,
	[IPFRAG_ECN_NOT_ECT | IPFRAG_ECN_ECT_1] = 0xff,
	[IPFRAG_ECN_NOT_ECT | IPFRAG_ECN_ECT_0 | IPFRAG_ECN_ECT_1] = 0xff,
	[IPFRAG_ECN_NOT_ECT | IPFRAG_ECN_CE | IPFRAG_ECN_ECT_0] = 0xff,
	[IPFRAG_ECN_NOT_ECT | IPFRAG_ECN_CE | IPFRAG_ECN_ECT_1] = 0xff,
	[IPFRAG_ECN_NOT_ECT | IPFRAG_ECN_CE | IPFRAG_ECN_ECT_0 | IPFRAG_ECN_ECT_1] = 0xff,
};

static struct inet_frags ip4_frags;

int ip_frag_nqueues(struct net *net)
{
	return net->ipv4.frags.nqueues;
}

int ip_frag_mem(struct net *net)
{
	return atomic_read(&net->ipv4.frags.mem);
}

static int ip_frag_reasm(struct ipq *qp, struct sk_buff *prev,
			 struct net_device *dev);

struct ip4_create_arg {
	struct iphdr *iph;
	u32 user;
};

static unsigned int ipqhashfn(__be16 id, __be32 saddr, __be32 daddr, u8 prot)
{
	return jhash_3words((__force u32)id << 16 | prot,
			    (__force u32)saddr, (__force u32)daddr,
			    ip4_frags.rnd) & (INETFRAGS_HASHSZ - 1);
}

static unsigned int ip4_hashfn(struct inet_frag_queue *q)
{
	struct ipq *ipq;

	ipq = container_of(q, struct ipq, q);
	return ipqhashfn(ipq->id, ipq->saddr, ipq->daddr, ipq->protocol);
}

static int ip4_frag_match(struct inet_frag_queue *q, void *a)
{
	struct ipq *qp;
	struct ip4_create_arg *arg = a;

	qp = container_of(q, struct ipq, q);
	return	qp->id == arg->iph->id &&
			qp->saddr == arg->iph->saddr &&
			qp->daddr == arg->iph->daddr &&
			qp->protocol == arg->iph->protocol &&
			qp->user == arg->user;
}

/* Memory Tracking Functions. */
static void frag_kfree_skb(struct netns_frags *nf, struct sk_buff *skb)
{
	atomic_sub(skb->truesize, &nf->mem);
	kfree_skb(skb);
}

static void ip4_frag_init(struct inet_frag_queue *q, void *a)
{
	struct ipq *qp = container_of(q, struct ipq, q);
	struct ip4_create_arg *arg = a;

	qp->protocol = arg->iph->protocol;
	qp->id = arg->iph->id;
	qp->ecn = ip4_frag_ecn(arg->iph->tos);
	qp->saddr = arg->iph->saddr;
	qp->daddr = arg->iph->daddr;
	qp->user = arg->user;
	qp->peer = sysctl_ipfrag_max_dist ?
		inet_getpeer_v4(arg->iph->saddr, 1) : NULL;
}

static __inline__ void ip4_frag_free(struct inet_frag_queue *q)
{
	struct ipq *qp;

	qp = container_of(q, struct ipq, q);
	if (qp->peer)
		inet_putpeer(qp->peer);
}


/* Destruction primitives. */

static __inline__ void ipq_put(struct ipq *ipq)
{
	inet_frag_put(&ipq->q, &ip4_frags);
}

/* Kill ipq entry. It is not destroyed immediately,
 * because caller (and someone more) holds reference count.
 */
static void ipq_kill(struct ipq *ipq)
{
	inet_frag_kill(&ipq->q, &ip4_frags);
}

/* Memory limiting on fragments.  Evictor trashes the oldest
 * fragment queue until we are back under the threshold.
 * 分片内存限制处理，将分片所占用空间保持到低阈值一下，
 * 主要调用inet_frag_evicor来处理
 */
static void ip_evictor(struct net *net)
{
	int evicted;

	evicted = inet_frag_evictor(&net->ipv4.frags, &ip4_frags);
	if (evicted)
		IP_ADD_STATS_BH(net, IPSTATS_MIB_REASMFAILS, evicted);
}

/*
 * Oops, a fragment queue timed out.  Kill it and send an ICMP reply.
 */
static void ip_expire(unsigned long arg)
{
	struct ipq *qp;
	struct net *net;

	qp = container_of((struct inet_frag_queue *) arg, struct ipq, q);
	net = container_of(qp->q.net, struct net, ipv4.frags);

	spin_lock(&qp->q.lock);

	if (qp->q.last_in & INET_FRAG_COMPLETE)
		goto out;

	ipq_kill(qp);

	IP_INC_STATS_BH(net, IPSTATS_MIB_REASMTIMEOUT);
	IP_INC_STATS_BH(net, IPSTATS_MIB_REASMFAILS);

	if ((qp->q.last_in & INET_FRAG_FIRST_IN) && qp->q.fragments != NULL) {
		struct sk_buff *head = qp->q.fragments;
		const struct iphdr *iph;
		int err;

		rcu_read_lock();
		head->dev = dev_get_by_index_rcu(net, qp->iif);
		if (!head->dev)
			goto out_rcu_unlock;

		/* skb dst is stale, drop it, and perform route lookup again */
		skb_dst_drop(head);
		iph = ip_hdr(head);
		err = ip_route_input_noref(head, iph->daddr, iph->saddr,
					   iph->tos, head->dev);
		if (err)
			goto out_rcu_unlock;

		/*
		 * Only an end host needs to send an ICMP
		 * "Fragment Reassembly Timeout" message, per RFC792.
		 */
		if (qp->user == IP_DEFRAG_AF_PACKET ||
		    (qp->user == IP_DEFRAG_CONNTRACK_IN &&
		     skb_rtable(head)->rt_type != RTN_LOCAL))
			goto out_rcu_unlock;


		/* Send an ICMP "Fragment Reassembly Timeout" message. */
		icmp_send(head, ICMP_TIME_EXCEEDED, ICMP_EXC_FRAGTIME, 0);
out_rcu_unlock:
		rcu_read_unlock();
	}
out:
	spin_unlock(&qp->q.lock);
	ipq_put(qp);
}

/* Find the correct entry in the "incomplete datagrams" queue for
 * this IP datagram, and create new one, if nothing is found.
 * 从哈希表中找到对应的分片队列，找不到就新建一个
 */
static inline struct ipq *ip_find(struct net *net, struct iphdr *iph, u32 user)
{
	struct inet_frag_queue *q;
	struct ip4_create_arg arg;
	unsigned int hash;

    /* arg包含了分片的五元组，源地址、目的地址、协议 
     * IP ID以及user(表示调用者，可能是协议栈也可能是netfilter )
     */
	arg.iph = iph;
	arg.user = user;

    /* 先持有哈希表的读锁，防止更改 */
	read_lock(&ip4_frags.lock);

	/* 根据上述五元组到一个hash值，经典的hash函数，可以拿来自用 */
	hash = ipqhashfn(iph->id, iph->saddr, iph->daddr, iph->protocol);

    /* 根据hash值查找hash表，这里arg的作用是对分片队列进行匹配，
     * 因为hash值相等的分片队列能有很多，在这个函数里，如果找不到
     * 的话就会去新建一个分片队列。
     */
	q = inet_frag_find(&net->ipv4.frags, &ip4_frags, &arg, hash);
	if (IS_ERR_OR_NULL(q)) {
		inet_frag_maybe_warn_overflow(q, pr_fmt());
		return NULL;
	}
	/* 找到了，返回ipq分片队列指针,注意区分struct ipq 和
	 * struct inet_frag_queue的关系，两者是包含关系，前者包含后者
	 */
	return container_of(q, struct ipq, q);
}

/* Is the fragment too far ahead to be part of ipq? */
static inline int ip_frag_too_far(struct ipq *qp)
{
	struct inet_peer *peer = qp->peer;
	unsigned int max = sysctl_ipfrag_max_dist;
	unsigned int start, end;

	int rc;

	if (!peer || !max)
		return 0;

	start = qp->rid;
	end = atomic_inc_return(&peer->rid);
	qp->rid = end;

	rc = qp->q.fragments && (end - start) > max;

	if (rc) {
		struct net *net;

		net = container_of(qp->q.net, struct net, ipv4.frags);
		IP_INC_STATS_BH(net, IPSTATS_MIB_REASMFAILS);
	}

	return rc;
}

static int ip_frag_reinit(struct ipq *qp)
{
	struct sk_buff *fp;

	if (!mod_timer(&qp->q.timer, jiffies + qp->q.net->timeout)) {
		atomic_inc(&qp->q.refcnt);
		return -ETIMEDOUT;
	}

	fp = qp->q.fragments;
	do {
		struct sk_buff *xp = fp->next;
		frag_kfree_skb(qp->q.net, fp);
		fp = xp;
	} while (fp);

	qp->q.last_in = 0;
	qp->q.len = 0;
	qp->q.meat = 0;
	qp->q.fragments = NULL;
	qp->q.fragments_tail = NULL;
	qp->iif = 0;
	qp->ecn = 0;

	return 0;
}

/* Add new segment to existing queue. */
/* 添加一个新的片段到分片队列里面 */
static int ip_frag_queue(struct ipq *qp, struct sk_buff *skb)
{
	struct sk_buff *prev, *next;
	struct net_device *dev;
	int flags, offset;
	int ihl, end;
	int err = -ENOENT;
	u8 ecn;

    /* last_in标志位已经置位，这时候再收到报文就不用处理了，
     * 一种情况是重组已经完成，这时候又收到了报文，可能是重传
     * 当然，分片队列被垃圾回收定时器回收的时候也会设置这个标志位，
     * 表示已废弃。
     */
	if (qp->q.last_in & INET_FRAG_COMPLETE)
		goto err;

    /* 下面这段描述摘自 http://blog.chinaunix.net/uid-23629988-id-3047513.html
     * 关于ip_frag_too_far这个函数我还没有分析清楚，日后搞明白了补上，:-)
     * 欢迎懂得大神讲一下
     *    1. IPCB(skb)->flags只有在本机发送IPv4分片时被置位，那么这里的检查应该是
     *    预防收到本机自己发出的IP分片。
     *    2. 关于ip_frag_too_far：该函数主要保证了来自同一个peer（相同的源地址）不
     *    会占用过多的IP分片队列。
     *    3. 前面两个条件为真时，调用ip_frag_reinit，重新初始化该队列。出错，那么只
     *       好kill掉这个队列了。
     * 
     */
	if (!(IPCB(skb)->flags & IPSKB_FRAG_COMPLETE) &&
	    unlikely(ip_frag_too_far(qp)) &&
	    unlikely(err = ip_frag_reinit(qp))) {
		ipq_kill(qp);
		goto err;
	}

    /* 获取ip头里面的ecn标志位 */
	ecn = ip4_frag_ecn(ip_hdr(skb)->tos);
	offset = ntohs(ip_hdr(skb)->frag_off);
	
	/* 分片标志位 */
	flags = offset & ~IP_OFFSET;
	offset &= IP_OFFSET;

	/* 得到片偏移位置，相对于原始未分片报文，单位为8字节 */
	offset <<= 3;		/* offset is in 8-byte chunks */
	ihl = ip_hdrlen(skb);

	/* Determine the position of this fragment. */
	/* skb的长度减去IP头就剩下数据部分长度，这个长度加上片偏移的长度
	 * 就得到了这段报文相对于原始报文的尾偏移
	 */
	end = offset + skb->len - ihl;
	err = -EINVAL;

	/* Is this the final fragment? */
	/* 如果是最后的一片 */
	if ((flags & IP_MF) == 0) {
		/* If we already have some bits beyond end
		 * or have different end, the segment is corrupted.
		 */
		 /* 
		  * 既然是最后一片，尾偏移肯定要大于或者等于当前分片队列的长度，不是的话就错了
		  * 
		  * 如果已经收到过最后分片(分片重传)并且长度和当前skb所指向的尾偏移不一致，
		  * 出错了
		  */
		if (end < qp->q.len ||
		    ((qp->q.last_in & INET_FRAG_LAST_IN) && end != qp->q.len))
			goto err;

        /* 一切正常，设置last_in 标志位，同时将分片队列长度设置上 
         * 只有收到了最后一个分片报文才能够得知完整的报文长度
         */
		qp->q.last_in |= INET_FRAG_LAST_IN;
		qp->q.len = end;
	} else {
	    /* 如果不是最后一片，并且长度不是的倍数，就截取数据到的8倍数，
	     * 因为数据被截取了，校验和也失效了，这里重置校验和
	     */
		if (end&7) {
			end &= ~7;
			if (skb->ip_summed != CHECKSUM_UNNECESSARY)
				skb->ip_summed = CHECKSUM_NONE;
		}
		
		if (end > qp->q.len) {
		    /* 数据的尾部超出分片队列总长，如果已经收到了最后的分片，
		     说明出错了，直接报错。不是的话更新队列长度就好。*/
			/* Some bits beyond end -> corruption. */
			if (qp->q.last_in & INET_FRAG_LAST_IN)
				goto err;
			qp->q.len = end;
		}
	}
	/* 说明长度为空，丢弃 */
	if (end == offset)
		goto err;

	err = -ENOMEM;
	
	/* 去掉IP头部  */
	if (pskb_pull(skb, ihl) == NULL)
		goto err;

    /* 只保留数据部分 */
	err = pskb_trim_rcsum(skb, end - offset);
	if (err)
		goto err;

	/* Find out which fragments are in front and at the back of us
	 * in the chain of fragments so far.  We must know where to put
	 * this fragment, right?
	 */
	/* 如果是第一个分片报文则直接插入 
     * 如果上一个报文的偏移值小于当前偏移值则放在该报文后面即可
	 */ 
	prev = qp->q.fragments_tail;
	if (!prev || FRAG_CB(prev)->offset < offset) {
		next = NULL;
		goto found;
	}

	/* 乱序到达的话找到它下面一个报文即可
	 * 这里是遍历分片列表，找到当前报文的后一个
	 */
	prev = NULL;
	for (next = qp->q.fragments; next != NULL; next = next->next) {
		if (FRAG_CB(next)->offset >= offset)
			break;	/* bingo! */
		prev = next;
	}

found:
	/* We found where to put this one.  Check for overlap with
	 * preceding fragment, and, if needed, align things so that
	 * any overlaps are eliminated.
	 * 这时候已经找到在分片队列中的位置，需要和前后报文检查看看是否有
	 * 数据重叠。
	 */
	if (prev) {
	    /* i等于与上个报文重叠部分数据长度，如果完全落在上个报文内部则报错 */
		int i = (FRAG_CB(prev)->offset + prev->len) - offset;

		if (i > 0) {
		    /* 重叠的部分直接丢弃，end <= offset说明完全重叠 */
			offset += i;
			err = -EINVAL;
			if (end <= offset)
				goto err;
			err = -ENOMEM;
            /* 去掉重叠部分 */
			if (!pskb_pull(skb, i))
				goto err;
			
		    /* 数据有变更，重置校验和 */
			if (skb->ip_summed != CHECKSUM_UNNECESSARY)
				skb->ip_summed = CHECKSUM_NONE;
		}
	}

	err = -ENOMEM;

	while (next && FRAG_CB(next)->offset < end) {
	    /* 与后面紧邻的报文重叠部分数据长度 */
		int i = end - FRAG_CB(next)->offset; /* overlap is 'i' bytes */

        /* 如果重叠长度小于后面skb的长度，那么只需要将next skb
         * 的长度减去重叠部分即可，同时更新偏移值和校验和
         */
		if (i < next->len) {
			/* Eat head of the next overlapped fragment
			 * and leave the loop. The next ones cannot overlap.
			 */
			if (!pskb_pull(next, i))
				goto err;
			FRAG_CB(next)->offset += i;
			qp->q.meat -= i;
			if (next->ip_summed != CHECKSUM_UNNECESSARY)
				next->ip_summed = CHECKSUM_NONE;
			break;
		} else {
		    /* 走到这说明重叠长度大于next的长度，这时候next可以直接从队列中
		     * 摘掉了。
		     */
			struct sk_buff *free_it = next;

			/* Old fragment is completely overridden with
			 * new one drop it.
			 */
			next = next->next;

			if (prev)
				prev->next = next;
			else
				qp->q.fragments = next;

			qp->q.meat -= free_it->len;

			/* 从分片队列释放该skb */
			frag_kfree_skb(qp->q.net, free_it);
		}
	}

    /* 设置该skb的控制信息，即偏移值 */
	FRAG_CB(skb)->offset = offset;

	/* Insert this fragment in the chain of fragments. */
	/* 插入报文，如果是最后一片则设置fragments_tail指针指向最后一片 */
	skb->next = next;
	if (!next)
		qp->q.fragments_tail = skb;
	if (prev)
		prev->next = skb;
	else
		qp->q.fragments = skb;

	dev = skb->dev;
	if (dev) {
        /* 记录设备的索引同时清空skb的dev指针 */	
		qp->iif = dev->ifindex;
		skb->dev = NULL;
	}
	/* 更新队列的接收时间戳 
	 * 更新队列当前收到长度和，注意meat和len区别，前者保存当前已接受部分数据长度，
	 * 后者表示目前已知分片最大长度，当收到最后一个分片MF=0,就能够得到原始报文长度
	 */
	qp->q.stamp = skb->tstamp;
	qp->q.meat += skb->len;
	qp->ecn |= ecn;

	/* 增加分片内存所占空间 */
	atomic_add(skb->truesize, &qp->q.net->mem);

	/* 设置标志位 */
	if (offset == 0)
		qp->q.last_in |= INET_FRAG_FIRST_IN;

	if (qp->q.last_in == (INET_FRAG_FIRST_IN | INET_FRAG_LAST_IN) &&
	    qp->q.meat == qp->q.len)

	    /* 如果报文已经收集齐，则调用ip_frag_reasm() 进行重组操作 */    
		return ip_frag_reasm(qp, prev, dev);

	write_lock(&ip4_frags.lock);

    /* 移到lru末尾 */
	list_move_tail(&qp->q.lru_list, &qp->q.net->lru_list);
	write_unlock(&ip4_frags.lock);

	/* 分片还在继续，返回EINPROGRESS */
	return -EINPROGRESS;

err:
	kfree_skb(skb);
	return err;
}


/* Build a new IP datagram from all its fragments. */
/* 分片重组 */
static int ip_frag_reasm(struct ipq *qp, struct sk_buff *prev,
			 struct net_device *dev)
{
	struct net *net = container_of(qp->q.net, struct net, ipv4.frags);
	struct iphdr *iph;
	struct sk_buff *fp, *head = qp->q.fragments;
	int len;
	int ihlen;
	int err;
	u8 ecn;

    /* 重组之前首先将分片队列从分片子系统隔离开 */
	ipq_kill(qp);

    /* 检查ecn标志位，0xff则丢弃该报文，之所以这么做是因为rfc文档建议这么做 */
	ecn = ip4_frag_ecn_table[qp->ecn];
	if (unlikely(ecn == 0xff)) {
		err = -EINVAL;
		goto out_fail;
	}
	/* Make the one we just received the head. */
	/* 这一步是将最后接收到的skb指针指向分片队列的首部接收完最后一片后重组完成
	 * 是要将skb传递给上层处理的。这一段代码貌似复杂，多看几遍就懂了，下面放上一个
	 * 简要的图。
	 */
	if (prev) {
		head = prev->next;
		fp = skb_clone(head, GFP_ATOMIC);
		if (!fp)
			goto out_nomem;

		fp->next = head->next;
		if (!fp->next)
			qp->q.fragments_tail = fp;
		prev->next = fp;

        /* skb_morph 作用基本和skb_clone一致，这里的作用是
         * 将刚收到的指针指向分片队列首部，fragments就是分片
         * 首部。
         */
		skb_morph(head, qp->q.fragments);
		head->next = qp->q.fragments->next;

		kfree_skb(qp->q.fragments);
		qp->q.fragments = head;
	}

	WARN_ON(head == NULL);
	WARN_ON(FRAG_CB(head)->offset != 0);

	/* Allocate a new buffer for the datagram. */
	ihlen = ip_hdrlen(head);
	len = ihlen + qp->q.len;

    /* ip报文最大65535字节，超过这个长度就报错 */
	err = -E2BIG;
	if (len > 65535)
		goto out_oversize;

	/* Head of list must not be cloned. */
	if (skb_cloned(head) && pskb_expand_head(head, 0, 0, GFP_ATOMIC))
		goto out_nomem;

	/* If the first fragment is fragmented itself, we split
	 * it to two chunks: the first with data and paged part
	 * and the second, holding only fragments. */
	 /*
	  *通常SKB数据区会由线性缓存和非线性缓存组成，超过MTU大小就要使用
	  * 另外的skb来存储，这个部分放在skb_shinfo(head)->frag_list里。
	  * 分片队列重组完成后也是把原来的一个个分片放到skb_shinfo(head)->frag_list里，
	  * 所以这里为了避免和head原有的frag_list弄混(如果head存在frag_list)，将head的数据分为
	  * 两个部分，head存储线性和非线性数据区，clone指向head的原有frag_list，同时再将分片队列
	  * 里的skb挂到clone后，这样后续的上层处理就非常简单。
	  */
	if (skb_has_frag_list(head)) {
		struct sk_buff *clone;
		int i, plen = 0;

        /* 创建一个线性数据区长度为0的skb */
		if ((clone = alloc_skb(0, GFP_ATOMIC)) == NULL)
			goto out_nomem;
		clone->next = head->next;
		head->next = clone;
		/* 继承head的frag_list */
		skb_shinfo(clone)->frag_list = skb_shinfo(head)->frag_list;

		/* 将head的frag_list指针 重置 */
		skb_frag_list_init(head);
		for (i = 0; i < skb_shinfo(head)->nr_frags; i++)
			plen += skb_frag_size(&skb_shinfo(head)->frags[i]);
		clone->len = clone->data_len = head->data_len - plen;
		head->data_len -= clone->len;
		head->len -= clone->len;
		clone->csum = 0;
		clone->ip_summed = head->ip_summed;
		atomic_add(clone->truesize, &qp->q.net->mem);
	}

    /* 再将所有的分片挂到frag_list队列上 */
	skb_shinfo(head)->frag_list = head->next;
	/* 指针指向传输层首部 */
	skb_push(head, head->data - skb_network_header(head));

    /* 处理校验和 */
	for (fp=head->next; fp; fp = fp->next) {
		head->data_len += fp->len;
		head->len += fp->len;
		if (head->ip_summed != fp->ip_summed)
			head->ip_summed = CHECKSUM_NONE;
		else if (head->ip_summed == CHECKSUM_COMPLETE)
			head->csum = csum_add(head->csum, fp->csum);
		head->truesize += fp->truesize;
	}

	/* 重组完成，从分片占用的系统内存中减去重组后大小 */
	atomic_sub(head->truesize, &qp->q.net->mem);

	head->next = NULL;
	head->dev = dev;
	head->tstamp = qp->q.stamp;

    /* 重置IP头 */
	iph = ip_hdr(head);
	iph->frag_off = 0;
	iph->tot_len = htons(len);
	iph->tos |= ecn;
	IP_INC_STATS_BH(net, IPSTATS_MIB_REASMOKS);
	qp->q.fragments = NULL;
	qp->q.fragments_tail = NULL;
	return 0;

out_nomem:
	LIMIT_NETDEBUG(KERN_ERR pr_fmt("queue_glue: no memory for gluing queue %p\n"),
		       qp);
	err = -ENOMEM;
	goto out_fail;
out_oversize:
	if (net_ratelimit())
		pr_info("Oversized IP packet from %pI4\n", &qp->saddr);
out_fail:
	IP_INC_STATS_BH(net, IPSTATS_MIB_REASMFAILS);
	return err;
}

/* Process an incoming IP datagram fragment. */
int ip_defrag(struct sk_buff *skb, u32 user)
{
	struct ipq *qp;
	struct net *net;

	net = skb->dev ? dev_net(skb->dev) : dev_net(skb_dst(skb)->dev);

	/* snmp mib 统计数据 */
	IP_INC_STATS_BH(net, IPSTATS_MIB_REASMREQDS);

	/* Start by cleaning up the memory. */
	/* 首先判断当前分片队列所占内存是否超过阈值，如果超过的话
	 * 需要主动去释放一些分片，因为内存有限，分片报文在重组好之前
	 * 是一直放在内存里，不能无限度的存放。
	 */
	if (atomic_read(&net->ipv4.frags.mem) > net->ipv4.frags.high_thresh)
		ip_evictor(net);

	/* Lookup (or create) queue header */
	/* 这里根据分片五元组(源地址、目的地址、IP ID，protocol, user)去查找分片队列
	 * ip_find函数查找成功就返回对应的分片队列，查找失败就新建一个分片队列，
	 * 如果分配失败的话就返回NULL;
	 */
	if ((qp = ip_find(net, ip_hdr(skb), user)) != NULL) {
		int ret;

		spin_lock(&qp->q.lock);

        /* 这里是分片队列排队的地方，报文的排队，重组都在这里执行，下面
         * 再来分析该函数。
         */
		ret = ip_frag_queue(qp, skb);

		spin_unlock(&qp->q.lock);

		/* 这是一个包裹函数，减少分片队列的引用计数，如果没人引用该
         * 队列就调用inet_frag_destroy释放队列所占资源。
		 */
		ipq_put(qp);
		return ret;
	}

	IP_INC_STATS_BH(net, IPSTATS_MIB_REASMFAILS);
	/* 创建分片队列失败，释放掉skb并返回ENOMEM */
	kfree_skb(skb);
	return -ENOMEM;
}
EXPORT_SYMBOL(ip_defrag);

struct sk_buff *ip_check_defrag(struct sk_buff *skb, u32 user)
{
	struct iphdr iph;
	u32 len;

	if (skb->protocol != htons(ETH_P_IP))
		return skb;

	if (!skb_copy_bits(skb, 0, &iph, sizeof(iph)))
		return skb;

	if (iph.ihl < 5 || iph.version != 4)
		return skb;

	len = ntohs(iph.tot_len);
	if (skb->len < len || len < (iph.ihl * 4))
		return skb;

	if (ip_is_fragment(&iph)) {
		skb = skb_share_check(skb, GFP_ATOMIC);
		if (skb) {
			if (!pskb_may_pull(skb, iph.ihl*4))
				return skb;
			if (pskb_trim_rcsum(skb, len))
				return skb;
			memset(IPCB(skb), 0, sizeof(struct inet_skb_parm));
			if (ip_defrag(skb, user))
				return NULL;
			skb->rxhash = 0;
		}
	}
	return skb;
}
EXPORT_SYMBOL(ip_check_defrag);

#ifdef CONFIG_SYSCTL
static int zero;

static struct ctl_table ip4_frags_ns_ctl_table[] = {
	{
		.procname	= "ipfrag_high_thresh",
		.data		= &init_net.ipv4.frags.high_thresh,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{
		.procname	= "ipfrag_low_thresh",
		.data		= &init_net.ipv4.frags.low_thresh,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{
		.procname	= "ipfrag_time",
		.data		= &init_net.ipv4.frags.timeout,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_jiffies,
	},
	{ }
};

static struct ctl_table ip4_frags_ctl_table[] = {
	{
		.procname	= "ipfrag_secret_interval",
		.data		= &ip4_frags.secret_interval,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_jiffies,
	},
	{
		.procname	= "ipfrag_max_dist",
		.data		= &sysctl_ipfrag_max_dist,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &zero
	},
	{ }
};

static int __net_init ip4_frags_ns_ctl_register(struct net *net)
{
	struct ctl_table *table;
	struct ctl_table_header *hdr;

	table = ip4_frags_ns_ctl_table;
	if (!net_eq(net, &init_net)) {
		table = kmemdup(table, sizeof(ip4_frags_ns_ctl_table), GFP_KERNEL);
		if (table == NULL)
			goto err_alloc;

		table[0].data = &net->ipv4.frags.high_thresh;
		table[1].data = &net->ipv4.frags.low_thresh;
		table[2].data = &net->ipv4.frags.timeout;
	}

	hdr = register_net_sysctl_table(net, net_ipv4_ctl_path, table);
	if (hdr == NULL)
		goto err_reg;

	net->ipv4.frags_hdr = hdr;
	return 0;

err_reg:
	if (!net_eq(net, &init_net))
		kfree(table);
err_alloc:
	return -ENOMEM;
}

static void __net_exit ip4_frags_ns_ctl_unregister(struct net *net)
{
	struct ctl_table *table;

	table = net->ipv4.frags_hdr->ctl_table_arg;
	unregister_net_sysctl_table(net->ipv4.frags_hdr);
	kfree(table);
}

static void ip4_frags_ctl_register(void)
{
	register_net_sysctl_rotable(net_ipv4_ctl_path, ip4_frags_ctl_table);
}
#else
static inline int ip4_frags_ns_ctl_register(struct net *net)
{
	return 0;
}

static inline void ip4_frags_ns_ctl_unregister(struct net *net)
{
}

static inline void ip4_frags_ctl_register(void)
{
}
#endif

static int __net_init ipv4_frags_init_net(struct net *net)
{
	/*
	 * Fragment cache limits. We will commit 256K at one time. Should we
	 * cross that limit we will prune down to 192K. This should cope with
	 * even the most extreme cases without allowing an attacker to
	 * measurably harm machine performance.
	 */
	net->ipv4.frags.high_thresh = 256 * 1024;
	net->ipv4.frags.low_thresh = 192 * 1024;
	/*
	 * Important NOTE! Fragment queue must be destroyed before MSL expires.
	 * RFC791 is wrong proposing to prolongate timer each fragment arrival
	 * by TTL.
	 */
	net->ipv4.frags.timeout = IP_FRAG_TIME;

	inet_frags_init_net(&net->ipv4.frags);

	return ip4_frags_ns_ctl_register(net);
}

static void __net_exit ipv4_frags_exit_net(struct net *net)
{
	ip4_frags_ns_ctl_unregister(net);
	inet_frags_exit_net(&net->ipv4.frags, &ip4_frags);
}

static struct pernet_operations ip4_frags_ops = {
	.init = ipv4_frags_init_net,
	.exit = ipv4_frags_exit_net,
};

void __init ipfrag_init(void)
{
	ip4_frags_ctl_register();
	register_pernet_subsys(&ip4_frags_ops);
	ip4_frags.hashfn = ip4_hashfn;
	ip4_frags.constructor = ip4_frag_init;
	ip4_frags.destructor = ip4_frag_free;
	ip4_frags.skb_free = NULL;
	ip4_frags.qsize = sizeof(struct ipq);
	ip4_frags.match = ip4_frag_match;
	ip4_frags.frag_expire = ip_expire;
	ip4_frags.secret_interval = 10 * 60 * HZ;
	inet_frags_init(&ip4_frags);
}
