/*
 * inet fragments management
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * 		Authors:	Pavel Emelyanov <xemul@openvz.org>
 *				Started as consolidation of ipv4/ip_fragment.c,
 *				ipv6/reassembly. and ipv6 nf conntrack reassembly
 */

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/random.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/slab.h>

#include <net/sock.h>
#include <net/inet_frag.h>

static void inet_frag_secret_rebuild(unsigned long dummy)
{
	struct inet_frags *f = (struct inet_frags *)dummy;
	unsigned long now = jiffies;
	int i;

	write_lock(&f->lock);
	get_random_bytes(&f->rnd, sizeof(u32));
	for (i = 0; i < INETFRAGS_HASHSZ; i++) {
		struct inet_frag_queue *q;
		struct hlist_node *p, *n;

		hlist_for_each_entry_safe(q, p, n, &f->hash[i], list) {
			unsigned int hval = f->hashfn(q);

			if (hval != i) {
				hlist_del(&q->list);

				/* Relink to new hash chain. */
				hlist_add_head(&q->list, &f->hash[hval]);
			}
		}
	}
	write_unlock(&f->lock);

	mod_timer(&f->secret_timer, now + f->secret_interval);
}

void inet_frags_init(struct inet_frags *f)
{
	int i;

	for (i = 0; i < INETFRAGS_HASHSZ; i++)
		INIT_HLIST_HEAD(&f->hash[i]);

	rwlock_init(&f->lock);

	f->rnd = (u32) ((num_physpages ^ (num_physpages>>7)) ^
				   (jiffies ^ (jiffies >> 6)));

	setup_timer(&f->secret_timer, inet_frag_secret_rebuild,
			(unsigned long)f);
	f->secret_timer.expires = jiffies + f->secret_interval;
	add_timer(&f->secret_timer);
}
EXPORT_SYMBOL(inet_frags_init);

void inet_frags_init_net(struct netns_frags *nf)
{
	nf->nqueues = 0;
	atomic_set(&nf->mem, 0);
	INIT_LIST_HEAD(&nf->lru_list);
}
EXPORT_SYMBOL(inet_frags_init_net);

void inet_frags_fini(struct inet_frags *f)
{
	del_timer(&f->secret_timer);
}
EXPORT_SYMBOL(inet_frags_fini);

void inet_frags_exit_net(struct netns_frags *nf, struct inet_frags *f)
{
	nf->low_thresh = 0;

	local_bh_disable();
	inet_frag_evictor(nf, f);
	local_bh_enable();
}
EXPORT_SYMBOL(inet_frags_exit_net);

static inline void fq_unlink(struct inet_frag_queue *fq, struct inet_frags *f)
{
	write_lock(&f->lock);
	/* 从哈希分片队列中移除 */
	hlist_del(&fq->list);

	/* 从lru链表中移除 */
	list_del(&fq->lru_list);

	/* 减少排队的分片队列个数 */
	fq->net->nqueues--;
	write_unlock(&f->lock);
}

void inet_frag_kill(struct inet_frag_queue *fq, struct inet_frags *f)
{
    /* 停止分片队列定时器，这个定时器用来防止长时间占用内存 */
	if (del_timer(&fq->timer))
		atomic_dec(&fq->refcnt);

    /* frag_complete一般是重组完成的时候或者释放分片队列的时候去设置，
     * 这里判断如果没有设置的话，就设置该标志位同时调用fq_unlink函数
     * 去处理链表移除的事情，包括哈希表和lru链表。
     */
	if (!(fq->last_in & INET_FRAG_COMPLETE)) {
		fq_unlink(fq, f);
		atomic_dec(&fq->refcnt);
		fq->last_in |= INET_FRAG_COMPLETE;
	}
}
EXPORT_SYMBOL(inet_frag_kill);

/* 释放分片队列的skb buffer */
static inline void frag_kfree_skb(struct netns_frags *nf, struct inet_frags *f,
		struct sk_buff *skb, int *work)
{
    /* 一种情况下是分片队列已经重组完成，这时候需要释放，work 指针为空 
     * 还有一种情况是当内核分片队列所占内存空间过大，这时候内核需要主动
     * 释放一些旧的分片队列，这时候work指针就表示需要释放的空间大小
     */
	if (work)
		*work -= skb->truesize;

    /* 从分片所占用的总的内存数量中减去当前释放的skb缓存大小 */
	atomic_sub(skb->truesize, &nf->mem);

	/* 如果存在私有的释放回调函数的话，这时候调用，
	 * ip4_frags 这个指针为空
	 */
	if (f->skb_free)
		f->skb_free(skb);  

	/* 最后调用kfree_skb释放 skb buffer */	
	kfree_skb(skb);
}

/* 释放分片队列所占资源 */
void inet_frag_destroy(struct inet_frag_queue *q, struct inet_frags *f,
					int *work)
{
	struct sk_buff *fp;
	struct netns_frags *nf;

    /* 正常情况下删除分片队列前都会置上该标志位并且分片队列的定时器
     * 应该停止，这里检查下，有异常就告警
     */
	WARN_ON(!(q->last_in & INET_FRAG_COMPLETE));
	WARN_ON(del_timer(&q->timer) != 0);

	/* Release all fragment data. 
	 * 先释放所有的skb分片缓存
	 */
	fp = q->fragments;
	nf = q->net;
	while (fp) {
		struct sk_buff *xp = fp->next;

        /* 实际的释放函数 */
		frag_kfree_skb(nf, f, fp, work);
		fp = xp;
	}

    /* qsize 是分片结构体 struct ipq的大小 */
	if (work)
		*work -= f->qsize;
	atomic_sub(f->qsize, &nf->mem);

    /* 分片队列释放的回调处理函数
     * ipv4 这个函数是 ip4_frag_free，ipfrag_init中初始化。
     */
	if (f->destructor)
		f->destructor(q);
    /* 最后释放分片队列所占内存 */
	kfree(q);
}
EXPORT_SYMBOL(inet_frag_destroy);

int inet_frag_evictor(struct netns_frags *nf, struct inet_frags *f)
{
	struct inet_frag_queue *q;
	int work, evicted = 0;

    /* 首先得到需要释放的内存空间大小，
     * 用当前所占空间总额减去低阈值得到，这个值可以通过proc文件系统配置。
     */
	work = atomic_read(&nf->mem) - nf->low_thresh;
	while (work > 0) {
	    /* 先获取分片哈希表的读锁，如果lru链表为空就跳出 */
		read_lock(&f->lock);
		if (list_empty(&nf->lru_list)) {
			read_unlock(&f->lock);
			break;
		}

        /* 增加分片队列引用计数，释放分片哈希表读锁 */
		q = list_first_entry(&nf->lru_list,
				struct inet_frag_queue, lru_list);
		atomic_inc(&q->refcnt);
		read_unlock(&f->lock);

        /* 占用分片队列锁，如果还没有设置frag_complete标志位的话，
         * 调用inet_frag_kill去设置，该函数主要是将当前分片队列从分片哈希表中
         * 移除并且从lru链表中移除，这样就不会在使用了。
         */
		spin_lock(&q->lock);
		if (!(q->last_in & INET_FRAG_COMPLETE))
			inet_frag_kill(q, f);
		spin_unlock(&q->lock);

        /* 如果分片队列这时无人引用的话，调用inet_frag_destroy 释放分片缓存
         * 所占用空间，下面再分析该函数 。
         */
		if (atomic_dec_and_test(&q->refcnt))
			inet_frag_destroy(q, f, &work);
		evicted++;
	}

	return evicted;
}
EXPORT_SYMBOL(inet_frag_evictor);

/* 分片队列插入函数 */
static struct inet_frag_queue *inet_frag_intern(struct netns_frags *nf,
		struct inet_frag_queue *qp_in, struct inet_frags *f,
		void *arg)
{
	struct inet_frag_queue *qp;
#ifdef CONFIG_SMP
	struct hlist_node *n;
#endif
	unsigned int hash;

    /* 因为是修改分片hash表，这里要求写锁 */
	write_lock(&f->lock);
	/*
	 * While we stayed w/o the lock other CPU could update
	 * the rnd seed, so we need to re-calculate the hash
	 * chain. Fortunatelly the qp_in can be used to get one.
	 */
	 /*
	 * hashfn函数指针在ipfrag_init()里初始化为ip4_hashfn(),
	 * 就是一个hash函数
	 */
	hash = f->hashfn(qp_in);
#ifdef CONFIG_SMP
	/* With SMP race we have to recheck hash table, because
	 * such entry could be created on other cpu, while we
	 * promoted read lock to write lock.
	 * 
	 * 在多核处理情况下有可能其它CPU也收到同一路报文然后创建 了
	 * 分片队列，如果出现这种情况就将我们新创建的分片队列释放掉，
	 * 即设置last_in标志位，然后调用inet_frag_put()做释放处理，
	 * 这时候把先创建的分片队列qp返回就好了。
	 */
	 
	hlist_for_each_entry(qp, n, &f->hash[hash], list) {
		if (qp->net == nf && f->match(qp, arg)) {
			atomic_inc(&qp->refcnt);
			write_unlock(&f->lock);
			qp_in->last_in |= INET_FRAG_COMPLETE;
			inet_frag_put(qp_in, f);
			return qp;
		}
	}
#endif
	qp = qp_in;
	/* 重新初始化分片队列超时时间 */
	if (!mod_timer(&qp->timer, jiffies + nf->timeout))
		atomic_inc(&qp->refcnt);

	atomic_inc(&qp->refcnt);

	/* 插入到分片hash表的头部 */
	hlist_add_head(&qp->list, &f->hash[hash]);

	/* 插入到lru链表的尾部，当分片所占空用过大的时候，
	 * 内核会从lru的首部顺序释放分片队列，因为排在前面的
	 * 都是旧的分片，新的都挂在lru尾部
	 */
	list_add_tail(&qp->lru_list, &nf->lru_list);

	/* 增加分片队列个数 */
	nf->nqueues++;

	/* 插入结束，释放写锁 */
	write_unlock(&f->lock);
	return qp;
}

static struct inet_frag_queue *inet_frag_alloc(struct netns_frags *nf,
		struct inet_frags *f, void *arg)
{
	struct inet_frag_queue *q;

    /* qsize指的是分片队列的固定大小，等于sizeof(struct ipq) */
	q = kzalloc(f->qsize, GFP_ATOMIC);
	if (q == NULL)
		return NULL;

    /* 初始化分片队列，将五元组赋值给分片队列，
     * 初始化回调函数是ip4_frag_init(), 在ipfrag_init()里设置。
     */
	f->constructor(q, arg);

	/* 增加分片所占用的内存大小 */
	atomic_add(f->qsize, &nf->mem);

	/* 初始化该分片队列的定时器，并设置该定时器的回调处理函数 
	 * 回调处理函数是在系统初始化的时候设置的，ip4的分片定时器
	 * 回调处理函数是ip_expire(), 该定时器的主要作用是重组超时后
	 * 释放该分片队列所占资源，防止分片恶意攻击等。
	 */
	setup_timer(&q->timer, f->frag_expire, (unsigned long)q);
	spin_lock_init(&q->lock);

	/* 初始化引用计数为1 */
	atomic_set(&q->refcnt, 1);
	q->net = nf;

	return q;
}

/* 创建分片队列 */
static struct inet_frag_queue *inet_frag_create(struct netns_frags *nf,
		struct inet_frags *f, void *arg)
{
	struct inet_frag_queue *q;

    /* 创建并初始化分片队列 */
	q = inet_frag_alloc(nf, f, arg);
	if (q == NULL)
		return NULL;

    /* 将分片队列插入到分片哈希表中和lru链表尾部 */
	return inet_frag_intern(nf, q, f, arg);
}

/* 分片队列查找函数 */
struct inet_frag_queue *inet_frag_find(struct netns_frags *nf,
		struct inet_frags *f, void *key, unsigned int hash)
	__releases(&f->lock)
{
	struct inet_frag_queue *q;
	struct hlist_node *n;
	int depth = 0;

    /* 遍历hash表，即ip4_frags->hash[hash],然后调用match回调函数
     * 去和报文的五元组进行匹配，找到的话就增加该队列的引用计数并返回其指针，
     * 找不到的话增加hash桶的深度，继续查找下一个。
     * ip4_frags 注册的match 回调函数是ip4_frag_match，在ip_fragment.c文件里
     * 该函数很简单，就是去比较五元组是否完全一样。
     */
	hlist_for_each_entry(q, n, &f->hash[hash], list) {
		if (q->net == nf && f->match(q, key)) {
			atomic_inc(&q->refcnt);
			read_unlock(&f->lock);
			return q;
		}
		depth++;
	}
	read_unlock(&f->lock);

    /* 还是没找到，如果hash桶深不超过限值的话就调用inet_frag_create
     * 创建一个新的分片队列，超出的话直接返回错误就得了。
     * 通常收到第一个分片的时候会走到这里。
     */
	if (depth <= INETFRAGS_MAXDEPTH)
		return inet_frag_create(nf, f, key);
	else
		return ERR_PTR(-ENOBUFS);
}
EXPORT_SYMBOL(inet_frag_find);

void inet_frag_maybe_warn_overflow(struct inet_frag_queue *q,
				   const char *prefix)
{
	static const char msg[] = "inet_frag_find: Fragment hash bucket"
		" list length grew over limit " __stringify(INETFRAGS_MAXDEPTH)
		". Dropping fragment.\n";

	if (PTR_ERR(q) == -ENOBUFS)
		LIMIT_NETDEBUG(KERN_WARNING "%s%s", prefix, msg);
}
EXPORT_SYMBOL(inet_frag_maybe_warn_overflow);
