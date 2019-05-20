/*
 *	Linux NET3:	xflow ddos detect decoder.
 *
 *	Authors: Kyle
 *
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
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/net.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/time.h>
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

#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_arp.h>
#include <linux/netfilter_ipv6.h>

#include <asm/byteorder.h>

#include <net/netfilter/nf_conntrack_core.h>


#define FLOW_HASH_SIZE      8192   /* 0x1fff + 1 */

#define FLOW_BYTES_KB       1024
#define FLOW_BYTES_MB       (1024 * 1024)
#define FLOW_BYTES_GB       (1024 * 1024 * 1024)

#define FLOW_NF_ACTION      NF_DROP

#ifndef NIPQUAD
#define NIPQUAD(addr) \
    ((unsigned char *)&addr)[0], \
    ((unsigned char *)&addr)[1], \
    ((unsigned char *)&addr)[2], \
    ((unsigned char *)&addr)[3]
#endif
#ifndef NIP1
#define NIP1(addr)  ((unsigned char *)&addr)[0]
#define NIP2(addr)  ((unsigned char *)&addr)[1]
#define NIP3(addr)  ((unsigned char *)&addr)[2]
#define NIP4(addr)  ((unsigned char *)&addr)[3]
#endif

//#define FLOW_HASH_KEY(key)   ((unsigned char *)&key)[3]
#define FLOW_HASH_KEY(key)      (ntohl(key) & 0x1fff)

//#define FLOW_RATE_SEND_DIR        1

#define hook_info(fmt, arg...)	printk("<3>%s:%d " fmt, __FUNCTION__ , __LINE__, ##arg)

#define hook_debug(fmt, arg...) if (g_debug_enable) \
    printk("<3>%s:%d " fmt, __FUNCTION__ , __LINE__, ##arg)


struct flow_rate {
    __be32        ipaddr;           /* 主机ip地址 */

    unsigned int  wrate_cnt;        /* 速率超过阀值告警次数 */
    unsigned int  wincrease_cnt;    /* 增量超过阀值告警次数 */

    /* down ~= TX, 发送流量 */
    unsigned long recv_pkts;        /* TX包数量 */
    unsigned long recv_bytes;       /* TX字节数 */
    unsigned long recv_rate;        /* TX流量速率,单位bytes/sec */
    unsigned long recv_rate_rtime;  /* TX流量速率,单位bytes/sec */
    unsigned long recv_bytes_increase;  /* 流量增量 */
    
    unsigned long last_update_time;
    unsigned long start_time;

    #ifdef FLOW_RATE_SEND_DIR
    /* upload ~= RX, 接收流量 */
    unsigned long send_pkts;      /* RX包数量 */
    unsigned long send_bytes;     /* RX字节数 */
    unsigned long send_rate;
    unsigned long send_rate_rtime;
    unsigned long send_bytes_increase;
    #endif
   
};

struct flow_rate_ctrl {
    struct list_head list;
    struct flow_rate stats;
};

struct flow_rate_hash {
    struct list_head list;
    spinlock_t lock;
};

#define xflow_write_lock(lock)        spin_lock_bh(lock)
#define xflow_write_unlock(lock)      spin_unlock_bh(lock)
#define xflow_read_lock(lock)         spin_lock_bh(lock)
#define xflow_read_unlock(lock)       spin_unlock_bh(lock)

/* 调试缺省关闭 */
static int g_debug_enable = 0;
static struct flow_rate_hash g_flow_hash[FLOW_HASH_SIZE];

/* 统计ip流量时间间隔，缺省没60秒计算一次流量速率 */
static unsigned int g_flow_rate_timeval = 60;
/* 统计超时时间间隔，缺省配置情况下，当超过60+30秒未更新时，缓存老化释放 */
static unsigned int g_flow_rate_timeval_timeout = 30;
/* 缓存记录最大上限 */
static unsigned int g_flow_cache_max = FLOW_HASH_SIZE * 16;
/* 当前缓存记录条数 */
static unsigned int g_flow_cache_count = 0;

/* 单IP流量速率告警阀值，速率超过2048KB/sec时告警 */
static unsigned long g_flow_rate_warning = 2048;
/* 单IP总流量告警阀值，总流量超过1024M时告警 */
static unsigned long g_flow_total_warning = 1024;
/* 单IP总流量告警阀值，流量没增加256M时告警 */
static unsigned long g_flow_increase_warning = 512;


static struct kmem_cache *g_flow_cachep = NULL;
static struct ctl_table_header *g_flow_sysctl_header = NULL;   
static struct timer_list g_flow_ticktimer;

/* must be protect with lock */
static struct flow_rate_ctrl *flow_hash_find_entry(__be32 ipaddr, struct list_head *head)
{
    struct flow_rate_ctrl *pos;
    
    list_for_each_entry(pos, head, list)
    {
        if (pos->stats.ipaddr == ipaddr)
        {
            return pos;
        }
    }

    return NULL;
}

static void flow_hash_add_entry(struct flow_rate_ctrl *ctrl, struct list_head *head)
{
    list_add(&ctrl->list, head);
}

unsigned int xflow_rate_statis_detect(struct sk_buff *skb)
{
    struct iphdr *iph;
    struct flow_rate_ctrl *entry;
    struct timeval tv_now;
    unsigned int hashkey;

    if (skb == NULL)
    {
        return NF_ACCEPT;
    }
    
    iph = ip_hdr(skb);
    if (unlikely(!iph))
    {
        return NF_ACCEPT;
    }

    /* 过滤127.x.x.x 和 0.0.0.0 的数据报文 */
    if ((NIP1(iph->saddr) == 127) || (iph->saddr == 0))
    {
        return NF_ACCEPT;
    }

    /* 10.251.x.x 网段为设备内部通信报文，直接放行不作处理 */
    if ((NIP1(iph->saddr) == 10) && (NIP2(iph->saddr) == 251))
    {
        return NF_ACCEPT;
    }

    #if 0
    if (ipv4_is_zeronet(iph->daddr) || ipv4_is_lbcast(iph->daddr) || ipv4_is_multicast(iph->daddr))
    {
        return NF_ACCEPT;
    }
    #endif
    

    hashkey = FLOW_HASH_KEY(iph->saddr);
    spin_lock_bh(&g_flow_hash[hashkey].lock);
    entry = flow_hash_find_entry(iph->saddr, &g_flow_hash[hashkey].list);
    if (entry != NULL)
    {
        entry->stats.recv_pkts++;
        entry->stats.recv_bytes += htons(iph->tot_len);
        entry->stats.recv_rate += htons(iph->tot_len);
        entry->stats.recv_bytes_increase += htons(iph->tot_len);
        entry->stats.last_update_time = jiffies;

        hook_debug("drop from host(%d.%d.%d.%d) packet!\r\n", NIPQUAD(iph->saddr));
        spin_unlock_bh(&g_flow_hash[hashkey].lock);
        return FLOW_NF_ACTION;
    }
    spin_unlock_bh(&g_flow_hash[hashkey].lock);

    if (g_flow_cache_count >= g_flow_cache_max)
    {
        hook_info(" xflow ddos detect cache has reached its limit(%d).\r\n", g_flow_cache_max);
        /* 超过系统最大上限 */
        return FLOW_NF_ACTION;
    }

    //entry = (struct flow_rate_ctrl *)kmalloc(sizeof(struct flow_rate_ctrl), GFP_KERNEL);
    entry = (struct flow_rate_ctrl *)kmem_cache_alloc(g_flow_cachep, GFP_KERNEL);
    if (entry == NULL)
    {
        hook_info("alloc flow rate cache fail.\r\n");
        return FLOW_NF_ACTION;
    }
    memset(entry, 0, sizeof(struct flow_rate_ctrl));
    entry->stats.ipaddr = iph->saddr;
    entry->stats.recv_pkts = 1;
    entry->stats.recv_bytes = htons(iph->tot_len);
    entry->stats.recv_rate = entry->stats.recv_bytes;
    entry->stats.recv_bytes_increase = entry->stats.recv_bytes;
    entry->stats.last_update_time = jiffies;

    /* 写入创建时间 */
    do_gettimeofday(&tv_now);
    entry->stats.start_time = tv_now.tv_sec;

    spin_lock_bh(&g_flow_hash[hashkey].lock);
    g_flow_cache_count++;
    flow_hash_add_entry(entry, &g_flow_hash[hashkey].list);
    spin_unlock_bh(&g_flow_hash[hashkey].lock);

    hook_debug("Warning: detect ddos attack from host %d.%d.%d.%d ! \r\n", NIPQUAD(iph->saddr));
    
	return FLOW_NF_ACTION;
}
EXPORT_SYMBOL_GPL(xflow_rate_statis_detect);

/* download flow audit, PRE_ROUTING */
static unsigned int flow_pre_hook_v4(unsigned int hook,
			struct sk_buff *skb,
			const struct net_device *in,
			const struct net_device *out,
			int (*okfn)(struct sk_buff *))
{
    return xflow_rate_statis_detect(skb);
}

#ifdef FLOW_RATE_SEND_DIR
static unsigned int flow_post_hook_v4(unsigned int hook,
			struct sk_buff *skb,
			const struct net_device *in,
			const struct net_device *out,
			int (*okfn)(struct sk_buff *))
{
    struct iphdr *iph;
    struct flow_rate_ctrl *entry;
    unsigned int hashkey;

    if (skb == NULL)
    {
        return NF_DROP;
    }
    
    iph = ip_hdr(skb);
    if (!iph)
    {
        return NF_ACCEPT;
    }

    if (((unsigned char *)&iph->daddr)[0] == 127)
    {
        return NF_ACCEPT;
    }

    hashkey = FLOW_HASH_KEY(iph->daddr);
    spin_lock_bh(&g_flow_hash[hashkey].lock);
    entry = flow_hash_find_entry(iph->daddr, &g_flow_hash[hashkey].list);
    if (entry != NULL)
    {
        entry->stats.send_pkts++;
        entry->stats.send_bytes += htons(iph->tot_len);
        entry->stats.send_rate += htons(iph->tot_len);
        entry->stats.send_bytes_increase += htons(iph->tot_len);
        entry->stats.last_update_time = jiffies;
    }
    spin_unlock_bh(&g_flow_hash[hashkey].lock);
    
	return FLOW_NF_ACTION;
}
#endif

static struct nf_hook_ops flow_rate_ops[] = {
	{
	    .hook		= flow_pre_hook_v4,
	    .pf		    = PF_INET,
	    .hooknum	= NF_INET_PRE_ROUTING,
	    .priority	= NF_IP_PRI_FIRST + 21,
    },
    #ifdef FLOW_RATE_SEND_DIR
    {   .hook		= flow_post_hook_v4,
        .pf         = PF_INET,
        .hooknum    = NF_INET_POST_ROUTING,
        .priority   = NF_IP_PRI_FIRST + 21,
    },
    #endif
	{}
};

static int flow_rate_read_proc(struct seq_file *m, void *v)
{
    struct flow_rate_ctrl *pos;
    int i;
    unsigned int flow_cache_cnt = 0;

    #ifndef FLOW_RATE_SEND_DIR
    seq_printf(m, "index stime ipaddr TxPkts TxBytes TxRate\n");
    for (i = 0; i < FLOW_HASH_SIZE; i++)
    {
        spin_lock_bh(&g_flow_hash[i].lock);
        list_for_each_entry(pos, &g_flow_hash[i].list, list)
        {
            seq_printf(m, "%04d %lu %d.%d.%d.%d %lu %lu %lu\n",  
                        i, pos->stats.start_time, NIPQUAD(pos->stats.ipaddr),
                        pos->stats.recv_pkts, pos->stats.recv_bytes, pos->stats.recv_rate_rtime);
            flow_cache_cnt++;
        }
        spin_unlock_bh(&g_flow_hash[i].lock);
    }
    #else
    seq_printf(m, "index stime ipaddr RxPkts RxBytes RxRate TxPkts TxBytes TxRate\n");
    for (i = 0; i < FLOW_HASH_SIZE; i++)
    {
        spin_lock_bh(&g_flow_hash[i].lock);
        list_for_each_entry(pos, &g_flow_hash[i].list, list)
        {
            seq_printf(m, "%09lu %04d %lu %d.%d.%d.%d %lu %lu %lu %lu %lu %lu\n", pos->stats.recv_rate_rtime,
                        i, pos->stats.start_time, NIPQUAD(pos->stats.ipaddr),
                        pos->stats.send_pkts, pos->stats.send_bytes, pos->stats.send_rate_rtime, 
                        pos->stats.recv_pkts, pos->stats.recv_bytes, pos->stats.recv_rate_rtime);
            flow_cache_cnt++;
        }
        spin_unlock_bh(&g_flow_hash[i].lock);
    }
    #endif
    seq_printf(m, "xflow rate cache count %d.\n", flow_cache_cnt);

    return 0;
}

static int flow_rate_open_proc(struct inode *inode, struct file *file)
{
    return single_open(file, flow_rate_read_proc, NULL);
}

static const struct file_operations flow_rate_fops = {
        .open = flow_rate_open_proc,
        .read = seq_read,
        .llseek = default_llseek,
};

static ctl_table flow_rate_sysctl_table[] = {
	{
		.procname	= "xflow_rate_timeval",
		.data		= &g_flow_rate_timeval,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "xflow_cache_max",
		.data		= &g_flow_cache_max,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "xflow_cache_count",
		.data		= &g_flow_cache_count,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "xflow_rate_timeval_timeout",
		.data		= &g_flow_rate_timeval_timeout,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "xflow_rate_warning",
		.data		= &g_flow_rate_warning,
		.maxlen		= sizeof(unsigned long),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "xflow_total_warning",
		.data		= &g_flow_total_warning,
		.maxlen		= sizeof(unsigned long),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "xflow_increase_warning",
		.data		= &g_flow_increase_warning,
		.maxlen		= sizeof(unsigned long),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "xflow_debug_enable",
		.data		= &g_debug_enable,
		.maxlen		= sizeof(unsigned long),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{ }
};

struct ctl_path xflow_rate_sysctl_path[] = {
	{ .procname = "net", },
	{ .procname = "xflow", },
	{ }
};

static int flow_rate_init_sysctl(struct net *net)
{
	struct ctl_table *table;

	table = kmemdup(flow_rate_sysctl_table, sizeof(flow_rate_sysctl_table),
			GFP_KERNEL);
	if (!table)
		goto out_kmemdup;

	table[0].data = &g_flow_rate_timeval;
	table[1].data = &g_flow_cache_max;
	table[2].data = &g_flow_cache_count;
    table[3].data = &g_flow_rate_timeval_timeout;
    table[4].data = &g_flow_rate_warning;
    table[5].data = &g_flow_total_warning;
    table[6].data = &g_flow_increase_warning;
    table[7].data = &g_debug_enable;

	g_flow_sysctl_header = register_net_sysctl_table(net, xflow_rate_sysctl_path, table);
	if (!g_flow_sysctl_header)
	{
		goto out_unregister_xflow;
	}

	return 0;

out_unregister_xflow:
    
	kfree(table);
out_kmemdup:
    
	return -ENOMEM;
}

static void flow_rate_fini_sysctl(struct net *net)
{
	struct ctl_table *table;

    if (g_flow_sysctl_header)
    {
    	table = g_flow_sysctl_header->ctl_table_arg;
    	unregister_net_sysctl_table(g_flow_sysctl_header);
    	kfree(table);
        g_flow_sysctl_header = NULL;
    }

    return;
}

static void flow_rate_cache_report(struct flow_rate_ctrl *entry)
{
    
    hook_info("===========xflow warning host:%d.%d.%d.%d, rate:%lu, No.%d===========\n", 
                        NIPQUAD(entry->stats.ipaddr), 
                        entry->stats.recv_rate_rtime, entry->stats.wrate_cnt);
    
    return;
}

static void flow_increase_cache_report(struct flow_rate_ctrl *entry)
{
    
    hook_info("***********xflow warning host:%d.%d.%d.%d, rate:%lu, NO.%d***********\n", 
                        NIPQUAD(entry->stats.ipaddr), 
                        entry->stats.recv_bytes_increase, entry->stats.wincrease_cnt);
    
    return;
} 

static void flow_tick_timer_handler(unsigned long data)
{
    struct flow_rate_ctrl *pos, *entry;
    //unsigned long time_now = jiffies;
    unsigned long time_diff;
    int i;

    //hook_debug("xflow timer handle. %lu\r\n", jiffies);

    for (i = 0; i < FLOW_HASH_SIZE; i++)
    {
        spin_lock_bh(&g_flow_hash[i].lock);
        list_for_each_entry_safe(entry, pos, &g_flow_hash[i].list, list)
        {
            time_diff = jiffies - entry->stats.last_update_time;
            if (time_diff > ((g_flow_rate_timeval + g_flow_rate_timeval_timeout)*HZ))
            {
                /* 清除超时缓存 */
                list_del(&entry->list);
                hook_debug("kfree cache host:%d.%d.%d.%d, timediff:%lu, last:%lu\r\n", 
                            NIPQUAD(entry->stats.ipaddr), time_diff, entry->stats.last_update_time);

                /* 总流量超过阀值时告警 */
                /*
                if (entry->stats.recv_bytes >= (g_flow_total_warning * FLOW_BYTES_MB))
                {
                    flow_total_cache_report(entry);
                }*/

                kmem_cache_free(g_flow_cachep, entry);
                g_flow_cache_count--;
                continue;
            }

            /* 流量速率超过阀值时告警 */
            entry->stats.recv_rate_rtime = (entry->stats.recv_rate / g_flow_rate_timeval);
            if (entry->stats.recv_rate_rtime >= (g_flow_rate_warning * 1024))
            {
                entry->stats.wrate_cnt++;
                flow_rate_cache_report(entry);
            }
            entry->stats.recv_rate = 0;

            /* 流量每增加一定的流量告警 */
            if (entry->stats.recv_bytes_increase >= (g_flow_increase_warning * FLOW_BYTES_MB))
            {
                entry->stats.wincrease_cnt++;
                flow_increase_cache_report(entry);
                entry->stats.recv_bytes_increase = 0;
            }

            #ifdef FLOW_RATE_SEND_DIR
            /* 流量速率超过阀值时告警 */
            entry->stats.send_rate_rtime = (entry->stats.send_rate / g_flow_rate_timeval);
            if (entry->stats.send_rate_rtime >= (g_flow_rate_warning * 1024))
            {
                flow_rate_cache_report(entry);
            }
            entry->stats.send_rate = 0;
            #endif
        }
        spin_unlock_bh(&g_flow_hash[i].lock);
    }
    
	g_flow_ticktimer.expires = jiffies + (g_flow_rate_timeval * HZ);
	add_timer(&g_flow_ticktimer);

    return;
}

static void flow_timer_init(void)
{
    init_timer(&g_flow_ticktimer);
    g_flow_ticktimer.data = 0;
    g_flow_ticktimer.expires = jiffies + (g_flow_rate_timeval * HZ);
    g_flow_ticktimer.function = flow_tick_timer_handler;
    add_timer(&g_flow_ticktimer);

    return;
}

static void flow_timer_fini(void)
{
    del_timer(&g_flow_ticktimer);
    
    return;
}

int __init xflow_rate_init(void)
{
    int i;

    hook_info("xflow module init. cache max:%d, timeval:%ds\r\n", g_flow_cache_max, g_flow_rate_timeval);
    
    //创建读写锁	
    //rwlock_init(&flow_lock);

    hook_info("begin init xflow rate kmem cache.\r\n");
    g_flow_cachep = kmem_cache_create("xflow_rate_kmem_cache", 
                            sizeof(struct flow_rate_ctrl), 0, SLAB_DESTROY_BY_RCU, NULL);
    if (!g_flow_cachep)
    {
        return -1;
    }

    /* init hash table */
    hook_info("begin init flow hash table.\r\n");
    for (i = 0; i < FLOW_HASH_SIZE; i++)
    {
        INIT_LIST_HEAD(&g_flow_hash[i].list);
        spin_lock_init(&g_flow_hash[i].lock);
    }

    hook_info("begin create proc file.\r\n");
    proc_create("xflow_rates", 0644, init_net.proc_net, &flow_rate_fops);

    hook_info("begin init sysctl table.\r\n");
    (void)flow_rate_init_sysctl(&init_net);

    hook_info("begin register nf hook.\r\n");
    if (nf_register_hooks(flow_rate_ops, ARRAY_SIZE(flow_rate_ops)) < 0)
    {
        hook_info("%s, register xflow statis hook fail.\r\n", __FUNCTION__);
    }

    hook_info("begin init xflow timer.\r\n");
    (void)flow_timer_init();

    return 0;
}

void __exit xflow_rate_fini(void)
{
    struct flow_rate_ctrl *pos, *entry;
    int i;

    (void)flow_timer_fini();

    hook_debug("xflow module init. cache cnt:%d, timeval:%ds\r\n", g_flow_cache_count, g_flow_rate_timeval);
    nf_unregister_hooks(flow_rate_ops, ARRAY_SIZE(flow_rate_ops));

    remove_proc_entry("xflow_rates", init_net.proc_net);

    flow_rate_fini_sysctl(&init_net);

    for (i = 0; i < FLOW_HASH_SIZE; i++)
    {
        spin_lock_bh(&g_flow_hash[i].lock);
        list_for_each_entry_safe(pos, entry, &g_flow_hash[i].list, list)
        {
            kfree(pos);
            g_flow_cache_count--;
        }
        spin_unlock_bh(&g_flow_hash[i].lock);
    }

    if (g_flow_cachep)
    {
        kmem_cache_destroy(g_flow_cachep);
        g_flow_cachep = NULL;
    }

    return;
}

module_init(xflow_rate_init);
module_exit(xflow_rate_fini);

module_param(g_debug_enable, int, 0444);

MODULE_ALIAS("xflow-rate");
MODULE_AUTHOR("Dinstar Kyle");
MODULE_DESCRIPTION("xflow rate hook");
MODULE_LICENSE("GPL");


