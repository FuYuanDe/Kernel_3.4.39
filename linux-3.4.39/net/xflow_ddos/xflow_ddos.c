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

#define XFLOW_HZ            1000

#define FLOW_HASH_SIZE      (0x1fff + 1)
#define FLOW_HASH_MASK      0x1fff

#define FLOW_BYTES_KB       1024
#define FLOW_BYTES_MB       (1024 * 1024)
#define FLOW_BYTES_GB       (1024 * 1024 * 1024)

#define DDOS_ACTION_LOG         0       /* action:log */
#define DDOS_ACTION_DROP        1       /* action:drop */
#define DDOS_ACTION_LIMIT_PKT   2       /* action:rate limit */
#define DDOS_ACTION_LIMIT_BYTE  3       /* action:packet limit */

#ifdef PRODUCT_SBCMAIN
#define FLOW_DDOS_MAIN_BOARD     1
#endif
#if defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC1000MAIN)
#define FLOW_DDOS_MAIN_BOARD     1
#endif

#ifdef FLOW_DDOS_MAIN_BOARD
#define FLOW_NF_ACTION      NF_ACCEPT
#else
#define FLOW_NF_ACTION      NF_DROP
#endif

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

#define NETLINK_XFLOW_MODULE    24    /* netlink通信接口id */

//#define FLOW_HASH_KEY(key)   ((unsigned char *)&key)[3]
#define FLOW_HASH_KEY(key)      (ntohl(key) & 0x1fff)


#define hook_info(fmt, arg...)	printk("<3>%s:%d " fmt, __FUNCTION__ , __LINE__, ##arg)

#define hook_debug(fmt, arg...) if (g_debug_enable) \
    printk("<3>%s:%d " fmt, __FUNCTION__ , __LINE__, ##arg)


typedef enum flow_netlink_type {    
    FLOW_NETLINK_REGISTER = NLMSG_MIN_TYPE + 1,   /* value:17 */
    FLOW_NETLINK_UNREGISTER,
    FLOW_NETLINK_REPORT,
    
    FLOW_NETLINK_NOTIFY = 100,
    FLOW_NETLINK_UNNOTIFY = 101,
    FLOW_NETLINK_WARNING = 102,
    FLOW_NETLINK_ADD_BHOST = 103,     /* 设置黑名单主机 */
    FLOW_NETLINK_DEL_BHOST = 104,     /* 删除黑名单主机 */
    
    FLOW_NETLINK_ERROR,
} FLOW_NETLINK_TYPE;

struct flow_rate {
    __be32        ipaddr;           /* 主机ip地址 */

    /* down ~= TX, 发送流量 */
    unsigned int recv_pkts;        /* TX包数量 */
    unsigned int recv_bytes;       /* TX字节数 */
    unsigned int recv_rate;        /* TX流量速率,单位bytes/sec */
    unsigned int recv_rate_rtime;  /* TX流量速率,单位bytes/sec */
    unsigned int recv_pkts_rate;
    unsigned int recv_pkts_rate_rtime;
    unsigned int recv_rate_max;

    unsigned int start_time;   

    unsigned long last_update_time;
    unsigned long last_rate_time;

    unsigned int  action_limit_time;
    unsigned char action;

    /* 单位内流量 */
    unsigned int  recv_limit_rate;
    int           recv_power_rate;

    int           ifindex;
};

struct flow_rate_ctrl {
    struct list_head list;
    struct flow_rate stats;
};

struct flow_rate_hash {
    struct list_head list;
    spinlock_t lock;
};

struct ddos_report {
    unsigned int ip_type;

    union {
        struct in_addr in;
        struct in6_addr in6;
    } u_ipaddr;
    unsigned int port;
    
    unsigned int msg_type;         /* 通知/告警/错误 */
    
    unsigned int recv_pkts;        /* TX包数量 */
    unsigned int recv_bytes;       /* TX字节数 */
    unsigned int recv_rate; 
    unsigned int start_time;

    int           ifindex;
    unsigned char iface[32];
};

struct ddos_bhost {
    unsigned int ip_type;
    union {
        struct in_addr in;
        struct in6_addr in6;
    } u_ipaddr;
    unsigned int port;
    unsigned int live_time;
    unsigned int action;
    unsigned int limit_rate;
};

#define xflow_write_lock(lock)        spin_lock_bh(lock)
#define xflow_write_unlock(lock)      spin_unlock_bh(lock)
#define xflow_read_lock(lock)         spin_lock_bh(lock)
#define xflow_read_unlock(lock)       spin_unlock_bh(lock)

/* 调试缺省关闭 */
static unsigned int g_debug_enable = 0;
static unsigned int g_xflow_ddos_enable = 1;
static struct flow_rate_hash g_flow_hash[FLOW_HASH_SIZE];

/* 统计ip流量时间间隔，缺省5秒计算一次流量速率 */
static unsigned int g_flow_rate_timeval = 5;
static unsigned int g_flow_rate_limit_timeval = 1;
/* 统计超时时间间隔，缺省配置情况下，当超过60+30秒未更新时，缓存老化释放 */
static unsigned int g_flow_rate_timeval_timeout = 30;
/* 缓存记录最大上限 */
static unsigned int g_flow_cache_max = FLOW_HASH_SIZE * 16;
/* 当前缓存记录条数 */
static atomic_t g_flow_cache_count;

/* 单IP流量速率告警阀值，速率超过1MB/sec时告警 */
static unsigned int g_flow_rate_warning = 2 * 1024 * 1024;

static unsigned int g_flow_fwarning_pkts = 100;

static struct sock *g_xflow_nfnl = NULL;
static unsigned int g_flow_user_pid = 0;
    
static struct kmem_cache *g_flow_cachep = NULL;
static struct ctl_table_header *g_flow_sysctl_header = NULL;   
static struct timer_list g_flow_ticktimer;
static unsigned int g_timer_hash_idx = 0;

/* 解决在版本升级过程中，造成对升级IP攻击的误判问题 */
static __be32	    g_upload_host_saddr = 0;
static unsigned int g_upload_host_timer_en = 0;
static struct timer_list g_upload_host_ticktimer;


/* begin added for port rate limit  */
struct flow_port {
    atomic_t      recv_pkts;
    atomic_t      recv_bytes;
    unsigned int  recv_rate_rtime;
    unsigned int  recv_pkts_rtime;
    
    unsigned long last_pkt_time;
    unsigned long last_rate_time;
    
    unsigned long limit_time;
    unsigned int  recv_limit_rate;
    atomic_t      recv_power_rate;
    
    unsigned char action;
};

static int g_iface_eth0_idx = 0;
static struct flow_port *g_flow_port_tbl = NULL;
static unsigned int g_flow_port_rate_warning = 100 * 1024;

static int flow_port_init(void)
{
    int i;

    g_flow_port_tbl = (struct flow_port *)kmalloc(sizeof(struct flow_port) * 65536, GFP_KERNEL);
    if (!g_flow_port_tbl)
    {
        hook_info("kmalloc port manage table fail.\n");
        return -1;
    }
    memset(g_flow_port_tbl, 0, sizeof(struct flow_port) * 65536);
    for (i = 0; i < 65536; i++)
    {
        atomic_set(&g_flow_port_tbl[i].recv_pkts, 0);
        atomic_set(&g_flow_port_tbl[i].recv_bytes, 0);
        atomic_set(&g_flow_port_tbl[i].recv_power_rate, 0);
    }

    return 0;
}

static void flow_port_fini(void)
{
    if (g_flow_port_tbl)
    {
        kfree(g_flow_port_tbl);
        g_flow_port_tbl = NULL;
    }

    return;
}
/* end   added for port rate limit  */

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


static void flow_add_block_host(struct ddos_bhost *host)
{   
    unsigned int hashkey;
    struct flow_rate_ctrl *entry;
    struct flow_port *flow;

    #ifndef FLOW_DDOS_MAIN_BOARD
        /* 在用户板下，soft acl无效 */
        return;
    #endif
    
    if (host->ip_type == PF_INET)
    {
        hook_debug("set soft_acl add block host %d.%d.%d.%d. \n", NIPQUAD(host->u_ipaddr.in.s_addr));
        
        hashkey = FLOW_HASH_KEY(host->u_ipaddr.in.s_addr);
        spin_lock_bh(&g_flow_hash[hashkey].lock);
        entry = flow_hash_find_entry(host->u_ipaddr.in.s_addr, &g_flow_hash[hashkey].list);
        if (entry)
        {
            entry->stats.action = host->action;
            entry->stats.action_limit_time = (jiffies + (host->live_time * XFLOW_HZ));
            entry->stats.recv_pkts_rate = 0;
            entry->stats.recv_pkts_rate_rtime = 0;
            entry->stats.recv_rate = 0;
            entry->stats.recv_rate_rtime = 0;
            entry->stats.recv_rate_max = 0;

            entry->stats.recv_limit_rate = host->limit_rate;
            entry->stats.recv_power_rate = host->limit_rate * g_flow_rate_limit_timeval;
            entry->stats.last_rate_time = jiffies;

            
        }
        spin_unlock_bh(&g_flow_hash[hashkey].lock);
    }
    else if (host->ip_type == PF_INET6)
    {
        
    }
    else if (host->ip_type == (PF_INET + PF_INET6))
    {   
        host->port = host->port & 0xffff;
        flow = &g_flow_port_tbl[host->port];
        
        flow->action = host->action;
        flow->limit_time = (jiffies + (host->live_time * XFLOW_HZ));
        flow->recv_limit_rate = host->limit_rate;
        atomic_set(&flow->recv_power_rate, host->limit_rate * g_flow_rate_limit_timeval);
        flow->last_rate_time = jiffies;

        if (host->action == DDOS_ACTION_DROP)
        {
            flow->recv_pkts_rtime = 0;
            flow->recv_rate_rtime = 0;
            atomic_set(&flow->recv_bytes, 0);
            atomic_set(&flow->recv_pkts,  0);
        }
    }

    return;
}

static void flow_del_block_host(struct ddos_bhost *host)
{
    unsigned int hashkey;
    struct flow_rate_ctrl *entry;

    #ifndef FLOW_DDOS_MAIN_BOARD
        /* 在用户板下，soft acl无效 */
        return;
    #endif
    
    if (host->ip_type == PF_INET)
    {
        hook_debug("set soft_acl del block host %d.%d.%d.%d. \n", NIPQUAD(host->u_ipaddr.in.s_addr));
        
        hashkey = FLOW_HASH_KEY(host->u_ipaddr.in.s_addr);
        spin_lock_bh(&g_flow_hash[hashkey].lock);
        entry = flow_hash_find_entry(host->u_ipaddr.in.s_addr, &g_flow_hash[hashkey].list);
        if (entry)
        {
            entry->stats.action = 0;
            entry->stats.action_limit_time = 0;
            entry->stats.recv_power_rate = 0;
            entry->stats.recv_limit_rate = 0;
        }
        spin_unlock_bh(&g_flow_hash[hashkey].lock);
    }
    else if (host->ip_type == PF_INET6)
    {
        
    }
    else if (host->ip_type == (PF_INET + PF_INET6))
    {
        host->port = host->port & 0xffff;
        g_flow_port_tbl[host->port].action = 0;
        g_flow_port_tbl[host->port].limit_time = 0;
        g_flow_port_tbl[host->port].recv_limit_rate = 0;
        atomic_set(&g_flow_port_tbl[host->port].recv_power_rate, 0);
    }

    return;
}

static void flow_netlink_send_report_v4(unsigned int type, struct flow_rate *stats)
{
    struct sk_buff *send_skb;
    struct nlmsghdr *nlh;
    struct ddos_report *report;
    int datalen;
    struct net_device *in_dev;

    if (!g_flow_user_pid)
    {
        /* 用户态进程未注册，直接返回 */
        return;
    }

    datalen = NLMSG_SPACE(sizeof(struct ddos_report));

    send_skb = alloc_skb(datalen, GFP_KERNEL);
    if(!send_skb)
    {
        hook_info("alloc skb error.\r\n");  
        return;
    }

    /* init send data */
    nlh = nlmsg_put(send_skb, 0, 0, 0, sizeof(struct ddos_report), 0);
    nlh->nlmsg_pid = 0;
    nlh->nlmsg_type = FLOW_NETLINK_NOTIFY;
    
    report = (struct ddos_report *)NLMSG_DATA(nlh);
    report->msg_type = type;

    report->ip_type = AF_INET;
    report->u_ipaddr.in.s_addr = stats->ipaddr;
    report->port = 0;
    report->recv_bytes = stats->recv_bytes;
    report->recv_pkts = stats->recv_pkts;
    report->recv_rate = stats->recv_rate_rtime;
    report->start_time = stats->start_time;

    //dev_get_by_name(&init_net, "eth0");
    in_dev = dev_get_by_index(&init_net, stats->ifindex);
    if (in_dev)
    {
        strncpy(report->iface, in_dev->name, sizeof(report->iface));
        dev_put(in_dev);
    }
    else
    {
        strncpy(report->iface, "ethx", sizeof(report->iface));
    }
    
    hook_debug("send netlink report host %d.%d.%d.%d:%d pkts:%u,bytes:%u,rate:%u.\r\n", 
                NIPQUAD(stats->ipaddr), report->port, report->recv_pkts, report->recv_bytes, report->recv_rate);
    
    netlink_unicast(g_xflow_nfnl, send_skb, g_flow_user_pid, MSG_DONTWAIT);

    return;
}


/* netlink消息分发及处理 */
static int flow_netlink_rcv_msg(struct sk_buff *skb, struct nlmsghdr *nlh)
{
    unsigned int *value;
    struct ddos_bhost *host;
    
    hook_debug("recv msg type:%d, pid:%d\r\n", nlh->nlmsg_type, nlh->nlmsg_pid);
    
    switch (nlh->nlmsg_type)
    {
        case FLOW_NETLINK_REGISTER:
            value = (unsigned int *)NLMSG_DATA(nlh);
            g_flow_user_pid = *value;
            
            break;

        case FLOW_NETLINK_UNREGISTER:
            g_flow_user_pid = 0;
            
            break;

        case FLOW_NETLINK_ADD_BHOST:
            host = (struct ddos_bhost *)NLMSG_DATA(nlh);
            flow_add_block_host(host);
            break;

        case FLOW_NETLINK_DEL_BHOST:
            host = (struct ddos_bhost *)NLMSG_DATA(nlh);
            flow_del_block_host(host);
            break;

        case FLOW_NETLINK_NOTIFY:
            g_flow_user_pid = nlh->nlmsg_pid;
            
            break;

        case FLOW_NETLINK_UNNOTIFY:
            g_flow_user_pid = 0;
            
            break;
            
            
        default:
            hook_info("invalid netlink message type.\r\n");
            break;
    }

    return 0;
}

/* netlink收报处理函数 */
static void flow_netlink_rcv(struct sk_buff *skb)
{
    int res;
	res = netlink_rcv_skb(skb, &flow_netlink_rcv_msg);

    return;
}

static void flow_netlink_send_port_report_vx(struct flow_port *flow, __u16 port, const struct net_device *in)
{
    struct sk_buff *send_skb;
    struct nlmsghdr *nlh;
    struct ddos_report *report;
    int datalen;

    if (!g_flow_user_pid)
    {
        /* 用户态进程未注册，直接返回 */
        return;
    }

    datalen = NLMSG_SPACE(sizeof(struct ddos_report));

    send_skb = alloc_skb(datalen, GFP_KERNEL);
    if(!send_skb)
    {
        hook_info("alloc skb error.\r\n");  
        return;
    }

    /* init send data */
    nlh = nlmsg_put(send_skb, 0, 0, 0, sizeof(struct ddos_report), 0);
    nlh->nlmsg_pid = 0;
    nlh->nlmsg_type = FLOW_NETLINK_NOTIFY;
    
    report = (struct ddos_report *)NLMSG_DATA(nlh);
    memset(report, 0, sizeof(struct ddos_report));
    report->msg_type = FLOW_NETLINK_NOTIFY;

    report->ip_type = AF_INET + AF_INET6;
    report->port = port;
    report->recv_pkts = flow->recv_pkts.counter;
    report->recv_rate = flow->recv_rate_rtime;
    report->recv_bytes = flow->recv_bytes.counter;

    if (in)
    {
        strncpy(report->iface, in->name, sizeof(report->iface));
    }
    else
    {
        strncpy(report->iface, "ethx", sizeof(report->iface));
    }
    
    
    hook_debug("send netlink port report port:%d, rate:%u.\r\n", port, flow->recv_rate_rtime);
    
    netlink_unicast(g_xflow_nfnl, send_skb, g_flow_user_pid, MSG_DONTWAIT);

    return;
}

static unsigned int flow_port_rate_hook_v4(unsigned int hook,
			struct sk_buff *skb,
			const struct net_device *in,
			const struct net_device *out,
			int (*okfn)(struct sk_buff *))
{

    struct iphdr *iph;
    struct tcphdr *tcph;
    struct udphdr *udph;
    __u16  dport;
    struct flow_port *flow;

    iph = ip_hdr(skb);
    if (iph->protocol == IPPROTO_UDP)
    {
        udph = udp_hdr(skb);
        dport = htons(udph->dest);
    }
    else if (iph->protocol == IPPROTO_TCP)
    {
        tcph = tcp_hdr(skb);
        dport = htons(tcph->dest);
    }
    else
    {
        return NF_ACCEPT;
    }

    flow = &g_flow_port_tbl[dport];
    if (flow->action)
    {
        if (jiffies >= flow->limit_time)
        {
            flow->action = 0;
            flow->limit_time = 0;
            flow->recv_limit_rate = 0;
            atomic_set(&flow->recv_power_rate, 0);
        }
        else if (flow->action == DDOS_ACTION_DROP)
        {
            return NF_DROP;
        }
        else if (flow->action == DDOS_ACTION_LIMIT_PKT)
        {
            flow->last_pkt_time = jiffies;
            if ((jiffies - flow->last_rate_time) >= (g_flow_rate_limit_timeval * XFLOW_HZ))
            {
                atomic_set(&flow->recv_power_rate, flow->recv_limit_rate * g_flow_rate_limit_timeval);
                flow->last_rate_time = jiffies;
                flow->recv_rate_rtime = (flow->recv_bytes.counter / g_flow_rate_limit_timeval);
                flow->recv_pkts_rtime = (flow->recv_pkts.counter  / g_flow_rate_limit_timeval);
                atomic_set(&flow->recv_bytes, 0);
                atomic_set(&flow->recv_pkts,  0);
            }
            atomic_sub(1, &flow->recv_power_rate);
            //if (flow->recv_power_rate.counter <= 0)
            if (atomic_read(&flow->recv_power_rate) < 0)
            {
                return NF_DROP;
            }
            else
            {
                atomic_add(htons(iph->tot_len), &flow->recv_bytes);
                atomic_add(1, &flow->recv_pkts);
                return NF_ACCEPT;
            }
        }
        else if (flow->action == DDOS_ACTION_LIMIT_BYTE)
        {
            flow->last_pkt_time = jiffies;
            if ((jiffies - flow->last_rate_time) >= (g_flow_rate_limit_timeval * XFLOW_HZ))
            {
                atomic_set(&flow->recv_power_rate, flow->recv_limit_rate * g_flow_rate_limit_timeval);
                flow->last_rate_time = jiffies;

                flow->recv_rate_rtime = (flow->recv_bytes.counter / g_flow_rate_limit_timeval);
                flow->recv_pkts_rtime = (flow->recv_pkts.counter  / g_flow_rate_limit_timeval);
                atomic_set(&flow->recv_bytes, 0);
                atomic_set(&flow->recv_pkts,  0);
            }
            atomic_sub(htons(iph->tot_len), &flow->recv_power_rate);
            //if (flow->recv_power_rate.counter <= 0)
            if (atomic_read(&flow->recv_power_rate) < 0)
            {
                return NF_DROP;
            }
            else
            {
                atomic_add(htons(iph->tot_len), &flow->recv_bytes);
                atomic_add(1, &flow->recv_pkts);
                return NF_ACCEPT;
            }
                
        }
    }

    flow->last_pkt_time = jiffies;
    atomic_add(htons(iph->tot_len), &flow->recv_bytes);
    atomic_add(1, &flow->recv_pkts);
    if ((jiffies - flow->last_rate_time) >= (g_flow_rate_timeval * XFLOW_HZ))
    {
        flow->last_rate_time = jiffies;
        flow->recv_rate_rtime = (flow->recv_bytes.counter / g_flow_rate_timeval);
        flow->recv_pkts_rtime = (flow->recv_pkts.counter  / g_flow_rate_timeval);
        if (flow->recv_rate_rtime >= g_flow_port_rate_warning)
        {
            flow_netlink_send_port_report_vx(flow, dport, in);
        }
        atomic_set(&flow->recv_bytes, 0);
        atomic_set(&flow->recv_pkts,  0);
    }
    
    return NF_ACCEPT;
}


/* download flow audit, PRE_ROUTING */
static unsigned int flow_pre_hook_v4(unsigned int hook,
			struct sk_buff *skb,
			const struct net_device *in,
			const struct net_device *out,
			int (*okfn)(struct sk_buff *))
{
    struct iphdr *iph;
    struct flow_rate_ctrl *entry;
    struct timeval tv_now;
    unsigned int hashkey;
    
    iph = ip_hdr(skb);
    /* 过滤127.x.x.x 和 0.0.0.0 的数据报文 */
    if ((NIP1(iph->saddr) == 127) || (iph->saddr == 0))
    {
        //hook_info("xflow ddos host %d.%d.%d.%d.\r\n", NIPQUAD(iph->saddr));
        return NF_ACCEPT;
    }

    /* 10.251.x.x 网段为设备内部通信报文，直接放行不作处理 */
    if ((NIP1(iph->saddr) == 10) && (NIP2(iph->saddr) == 251))
    {
        return NF_ACCEPT;
    }

    #ifdef FLOW_DDOS_MAIN_BOARD
    /* 软件升级白名单 */
    if ((g_upload_host_saddr) && (g_upload_host_saddr == iph->saddr))
    {
        /**/
        if (g_debug_enable)
        {
            if (net_ratelimit())
            {
                hook_info("host %d.%d.%d.%d. upload host.\n", NIPQUAD(iph->saddr));
            }
        }
        return NF_ACCEPT;
    }

    skb_set_transport_header(skb, sizeof(struct iphdr));

    /* 基于端口号的流量统计值 */
    //if ((in) && (in->ifindex == g_iface_eth0_idx))
    {
        if (!flow_port_rate_hook_v4(hook, skb, in, out, okfn))
        {
            return NF_DROP;
        }
    }
    #endif
    
    hashkey = FLOW_HASH_KEY(iph->saddr);
    spin_lock_bh(&g_flow_hash[hashkey].lock);
    entry = flow_hash_find_entry(iph->saddr, &g_flow_hash[hashkey].list);
    if (entry != NULL)
    {
        entry->stats.last_update_time = jiffies;
        #ifdef FLOW_DDOS_MAIN_BOARD
        if (entry->stats.action)
        {
            if (entry->stats.action == DDOS_ACTION_DROP)
            {
                spin_unlock_bh(&g_flow_hash[hashkey].lock);
                if (g_debug_enable)
                {
                    if (net_ratelimit())
                        hook_debug("host %d.%d.%d.%d. soft acl action drop.\n", NIPQUAD(iph->saddr));
                }
                return NF_DROP;
            }
            else if (entry->stats.action == DDOS_ACTION_LIMIT_BYTE)
            {
                if ((jiffies - entry->stats.last_rate_time) >= (g_flow_rate_limit_timeval * XFLOW_HZ))
                {
                    entry->stats.last_rate_time = jiffies;
                    entry->stats.recv_power_rate = entry->stats.recv_limit_rate * g_flow_rate_limit_timeval;
                    
                    entry->stats.recv_rate_rtime = (entry->stats.recv_rate / g_flow_rate_limit_timeval);
                    entry->stats.recv_rate = 0;
                    entry->stats.recv_pkts_rate_rtime = (entry->stats.recv_pkts_rate / g_flow_rate_limit_timeval);
                    entry->stats.recv_pkts_rate = 0;
                }
                entry->stats.recv_power_rate -= htons(iph->tot_len);
                if (entry->stats.recv_power_rate < 0)
                {
                    /* 没有流量空间 */
                    spin_unlock_bh(&g_flow_hash[hashkey].lock);
                    hook_debug("host %d.%d.%d.%d. soft acl action limit. drop packet \n", NIPQUAD(iph->saddr));
                    return NF_DROP;
                }
                else
                {
                    entry->stats.recv_pkts++;
                    entry->stats.recv_bytes += htons(iph->tot_len);
                    entry->stats.recv_rate += htons(iph->tot_len);
                    entry->stats.recv_pkts_rate++;

                    spin_unlock_bh(&g_flow_hash[hashkey].lock);
                    return NF_ACCEPT;
                }
            }
            else if (entry->stats.action == DDOS_ACTION_LIMIT_PKT)
            {
                if ((jiffies - entry->stats.last_rate_time) >= (g_flow_rate_limit_timeval * XFLOW_HZ))
                {
                    entry->stats.last_rate_time = jiffies;
                    entry->stats.recv_power_rate = entry->stats.recv_limit_rate * g_flow_rate_limit_timeval;

                    entry->stats.recv_rate_rtime = (entry->stats.recv_rate / g_flow_rate_limit_timeval);
                    entry->stats.recv_rate = 0;
                    entry->stats.recv_pkts_rate_rtime = (entry->stats.recv_pkts_rate / g_flow_rate_limit_timeval);
                    entry->stats.recv_pkts_rate = 0;
                }
                entry->stats.recv_power_rate--;
                if (entry->stats.recv_power_rate < 0)
                {
                    /* 没有流量空间 */
                    spin_unlock_bh(&g_flow_hash[hashkey].lock);
                    hook_debug("host %d.%d.%d.%d. soft acl action limit.\n", NIPQUAD(iph->saddr));
                    return NF_DROP;
                }
                else 
                {
                    entry->stats.recv_pkts++;
                    entry->stats.recv_bytes += htons(iph->tot_len);
                    entry->stats.recv_rate += htons(iph->tot_len);
                    entry->stats.recv_pkts_rate++;

                    spin_unlock_bh(&g_flow_hash[hashkey].lock);
                    return NF_ACCEPT;
                }
            }
        }
        #endif

        entry->stats.recv_pkts++;
        entry->stats.recv_bytes += htons(iph->tot_len);
        entry->stats.recv_rate += htons(iph->tot_len);
        entry->stats.recv_pkts_rate++;
        
        if ((jiffies - entry->stats.last_rate_time) >= (g_flow_rate_timeval * XFLOW_HZ))
        {
            /* 达到计算速率周期 */
            entry->stats.recv_rate_rtime = (entry->stats.recv_rate / g_flow_rate_timeval);
            if (entry->stats.recv_rate_rtime >= g_flow_rate_warning)
            {
                if (!entry->stats.action)
                {
                    flow_netlink_send_report_v4(FLOW_NETLINK_NOTIFY, &entry->stats);
                }
            }
            entry->stats.recv_rate = 0;
            entry->stats.last_rate_time = jiffies;

            /* 计算每秒通过包数 */
            entry->stats.recv_pkts_rate_rtime = (entry->stats.recv_pkts_rate / g_flow_rate_timeval);
            entry->stats.recv_pkts_rate = 0;

            /* 记录最大速率值 */
            if (entry->stats.recv_rate_max < entry->stats.recv_rate_rtime)
            {
                entry->stats.recv_rate_max = entry->stats.recv_rate_rtime;
            }
        }

        #ifndef FLOW_DDOS_MAIN_BOARD
        /* 缺省情况下，收到满100个报文时，发出检测到攻击日志信息 */
        //if (entry->stats.recv_pkts == g_flow_fwarning_pkts)
        //{
        //    flow_netlink_send_report_v4(FLOW_NETLINK_NOTIFY, &entry->stats);
        //}
        #endif

        spin_unlock_bh(&g_flow_hash[hashkey].lock);
        return FLOW_NF_ACTION;
    }
    spin_unlock_bh(&g_flow_hash[hashkey].lock);

    if (g_flow_cache_count.counter >= g_flow_cache_max)
    {
        hook_debug(" xflow ddos detect cache has reached its limit(%d).\r\n", g_flow_cache_max);
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

    entry->stats.last_rate_time= jiffies;
    entry->stats.last_update_time = jiffies;

    entry->stats.recv_rate_max = 0;

    /* 写入创建时间 */
    do_gettimeofday(&tv_now);
    entry->stats.start_time = tv_now.tv_sec;

    entry->stats.ifindex = in->ifindex;

    spin_lock_bh(&g_flow_hash[hashkey].lock);
    //g_flow_cache_count++;
    atomic_inc(&g_flow_cache_count);
    flow_hash_add_entry(entry, &g_flow_hash[hashkey].list);
    spin_unlock_bh(&g_flow_hash[hashkey].lock);

    //hook_debug("Warning: detect ddos attack from host %d.%d.%d.%d ! \r\n", NIPQUAD(iph->saddr));
    
	return FLOW_NF_ACTION;
}


static struct nf_hook_ops flow_rate_ops[] = {
	{
	    .hook		= flow_pre_hook_v4,
	    .pf		    = PF_INET,
	    .hooknum	= NF_INET_PRE_ROUTING,
#ifdef FLOW_DDOS_MAIN_BOARD
	    .priority	= NF_IP_PRI_FIRST + 21,
#else
        .priority	= NF_IP_PRI_FIRST + 21,
#endif
    },
	{}
};

static int flow_rate_read_proc(struct seq_file *m, void *v)
{
    struct flow_rate_ctrl *pos;
    int i;
    unsigned int flow_cache_cnt = 0;

    seq_printf(m, "index stime Action/Limit/Lives ifindex ipaddr Pkts Bytes PktsRate BytesRate RateMax\n");
    for (i = 0; i < FLOW_HASH_SIZE; i++)
    {
        spin_lock_bh(&g_flow_hash[i].lock);
        list_for_each_entry(pos, &g_flow_hash[i].list, list)
        {
            seq_printf(m, "%04d %u %u/%u/%u %d %d.%d.%d.%d %u %u %u %u %u\n",  
                        i, pos->stats.start_time , 
                        pos->stats.action, pos->stats.recv_limit_rate, pos->stats.action_limit_time, 
                        pos->stats.ifindex, NIPQUAD(pos->stats.ipaddr), 
                        pos->stats.recv_pkts, pos->stats.recv_bytes, 
                        pos->stats.recv_pkts_rate_rtime, pos->stats.recv_rate_rtime, 
                        pos->stats.recv_rate_max);
            flow_cache_cnt++;
        }
        spin_unlock_bh(&g_flow_hash[i].lock);
    }
    seq_printf(m, "xflow rate cache count %d.\n", flow_cache_cnt);

    return 0;
}

static int flow_rate_port_read_proc(struct seq_file *m, void *v)
{
    int i;
    unsigned int flow_cache_cnt = 0;
    struct flow_port *flow;

    #ifdef FLOW_DDOS_MAIN_BOARD
    seq_printf(m, "index Action/limits  PktsRate ByteRate\n");
    for (i = 1; i <= 65535; i++)
    {
        if ((g_flow_port_tbl[i].action) || 
            ((jiffies - g_flow_port_tbl[i].last_pkt_time) <= (g_flow_rate_timeval * XFLOW_HZ)))
        {
            flow_cache_cnt++;
            flow = &g_flow_port_tbl[i];
            seq_printf(m, "%05d %d/%u %u %u\n", i, flow->action, flow->recv_limit_rate, 
                flow->recv_pkts_rtime, flow->recv_rate_rtime);
        }
    }
    seq_printf(m, "xflow port rate cache count %d.\n", flow_cache_cnt);
    #endif

    return 0;
}


static int flow_rate_open_proc(struct inode *inode, struct file *file)
{
    return single_open(file, flow_rate_read_proc, NULL);
}

static int flow_rate_port_open_proc(struct inode *inode, struct file *file)
{
    return single_open(file, flow_rate_port_read_proc, NULL);
}

static const struct file_operations flow_rate_fops = {
        .open = flow_rate_open_proc,
        .read = seq_read,
        .llseek = default_llseek,
};

static const struct file_operations flow_rate_port_fops = {
        .open = flow_rate_port_open_proc,
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
		.data		= &g_flow_cache_count.counter,
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
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "xflow_debug_enable",
		.data		= &g_debug_enable,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "xflow_fwarning_pkts",
		.data		= &g_flow_fwarning_pkts,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "xflow_upload_host_addr",
		.data		= &g_upload_host_saddr,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "xflow_port_rate_warning",
		.data		= &g_flow_port_rate_warning,
		.maxlen		= sizeof(unsigned int),
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
	table[2].data = &g_flow_cache_count.counter;
    table[3].data = &g_flow_rate_timeval_timeout;
    table[4].data = &g_flow_rate_warning;
    table[5].data = &g_debug_enable;
    table[6].data = &g_flow_fwarning_pkts;
    table[7].data = &g_upload_host_saddr;
    table[8].data = &g_flow_port_rate_warning;

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

static void flow_timer_upload_handler(unsigned long data)
{
    g_upload_host_timer_en = 0;
    g_upload_host_saddr = 0;
}

static void flow_timer_tick_handler(unsigned long data)
{
    struct flow_rate_ctrl *pos, *entry;
    int i;
    unsigned int start_hash, end_hash, idx;

    /* check 版本升级主机配置，如果已配置升级主机，则在10分钟内恢复配置 */
    #ifdef FLOW_DDOS_MAIN_BOARD
    if ((g_upload_host_saddr) && (!g_upload_host_timer_en))
    {
        hook_debug("set upload host:%d.%d.%d.%d \n", NIPQUAD(g_upload_host_saddr));
        g_upload_host_timer_en = 1;
        init_timer(&g_upload_host_ticktimer);
        g_upload_host_ticktimer.data = 0;
        g_upload_host_ticktimer.expires = jiffies + (600 * XFLOW_HZ);
        g_upload_host_ticktimer.function = flow_timer_upload_handler;
        add_timer(&g_upload_host_ticktimer);
    }
    #endif

    idx = g_timer_hash_idx & 0x7;
    start_hash =     (idx * 0x3ff + idx) & FLOW_HASH_MASK;
    end_hash = ((idx + 1) * 0x3ff + idx) & FLOW_HASH_MASK;
    g_timer_hash_idx++;
        
    for (i = start_hash; i <= end_hash; i++)
    {
        spin_lock_bh(&g_flow_hash[i].lock);
        list_for_each_entry_safe(entry, pos, &g_flow_hash[i].list, list)
        {
            if (entry->stats.action)
            {
                if (jiffies >= entry->stats.action_limit_time)
                {
                    /* 可能由于用户态异常导致acl规则残留，在此清理 */
                    entry->stats.action = 0;
                    entry->stats.action_limit_time = 0;
                    entry->stats.recv_power_rate = 0;
                    entry->stats.recv_rate = 0;
                }
                else
                {
                    /* 在流量禁止周期内，缓存节点不老化 */
                    continue;
                }
            }
            if ((jiffies - entry->stats.last_update_time) >= (g_flow_rate_timeval * XFLOW_HZ))
            {
                /* 清除超时缓存 */
                list_del(&entry->list);
                hook_debug("kfree hash(%d) cache host:%d.%d.%d.%d\r\n", i, NIPQUAD(entry->stats.ipaddr));
                kmem_cache_free(g_flow_cachep, entry);
                //g_flow_cache_count--;
                atomic_dec(&g_flow_cache_count);
            }
        }
        spin_unlock_bh(&g_flow_hash[i].lock);
    }
    
	g_flow_ticktimer.expires = jiffies + (8 * XFLOW_HZ);
	add_timer(&g_flow_ticktimer);

    return;
}

static void flow_timer_init(void)
{
    init_timer(&g_flow_ticktimer);
    g_flow_ticktimer.data = 0;
    g_flow_ticktimer.expires = jiffies + (8 * XFLOW_HZ);
    g_flow_ticktimer.function = flow_timer_tick_handler;
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
    struct net_device *dev_eth0;

    atomic_set(&g_flow_cache_count, 0);

    dev_eth0 = dev_get_by_name(&init_net, "eth0");
    g_iface_eth0_idx = dev_eth0->ifindex;
    dev_put(dev_eth0);

    hook_debug("xflow module init. cache max:%d, timeval:%ds\r\n", g_flow_cache_max, g_flow_rate_timeval);

    g_flow_cachep = kmem_cache_create("xflow_rate_kmem_cache", 
                            sizeof(struct flow_rate_ctrl), 0, SLAB_DESTROY_BY_RCU, NULL);
    if (!g_flow_cachep)
    {
        return -1;
    }

    g_xflow_nfnl = netlink_kernel_create(&init_net, NETLINK_XFLOW_MODULE, 0, flow_netlink_rcv, NULL, THIS_MODULE);
	if (!g_xflow_nfnl)
	{   
        kmem_cache_destroy(g_flow_cachep);
        g_flow_cachep = NULL;
        
        hook_info("kernel create netlink socket fail.\r\n");
		return -1;
	}

    if (flow_port_init())
    {
        kmem_cache_destroy(g_flow_cachep);
        g_flow_cachep = NULL;

        netlink_kernel_release(g_xflow_nfnl);
        g_xflow_nfnl = NULL;

        hook_info("kernel create port flow table fail.\r\n");
        return -1;
    }

    /* init hash table */
    for (i = 0; i < FLOW_HASH_SIZE; i++)
    {
        INIT_LIST_HEAD(&g_flow_hash[i].list);
        spin_lock_init(&g_flow_hash[i].lock);
    }

    /* create proc file */
    proc_create("xflow_rates", 0644, init_net.proc_net, &flow_rate_fops);
    #ifdef FLOW_DDOS_MAIN_BOARD
    proc_create("xflow_port_rates", 0644, init_net.proc_net, &flow_rate_port_fops);
    #endif

    /* init sysctl table */
    (void)flow_rate_init_sysctl(&init_net);

    /* register nf hook */
    if (nf_register_hooks(flow_rate_ops, ARRAY_SIZE(flow_rate_ops)) < 0)
    {
        hook_info("%s, register xflow statis hook fail.\r\n", __FUNCTION__);
    }

    /* init xflow timer */
    (void)flow_timer_init();

    return 0;
}

void __exit xflow_rate_fini(void)
{
    struct flow_rate_ctrl *pos, *entry;
    int i;

    (void)flow_timer_fini();

    hook_debug("xflow module init. cache cnt:%d, timeval:%ds\r\n", g_flow_cache_count.counter, g_flow_rate_timeval);
    nf_unregister_hooks(flow_rate_ops, ARRAY_SIZE(flow_rate_ops));

    remove_proc_entry("xflow_rates", init_net.proc_net);
    #ifdef FLOW_DDOS_MAIN_BOARD
    remove_proc_entry("xflow_port_rates", init_net.proc_net);
    #endif

    flow_rate_fini_sysctl(&init_net);

    if (g_xflow_nfnl != NULL)
    {
        netlink_kernel_release(g_xflow_nfnl);
        g_xflow_nfnl = NULL;
    }

    for (i = 0; i < FLOW_HASH_SIZE; i++)
    {
        spin_lock_bh(&g_flow_hash[i].lock);
        list_for_each_entry_safe(pos, entry, &g_flow_hash[i].list, list)
        {
            kmem_cache_free(g_flow_cachep, pos);
            //g_flow_cache_count--;
            atomic_dec(&g_flow_cache_count);
        }
        spin_unlock_bh(&g_flow_hash[i].lock);
    }

    if (g_flow_cachep)
    {
        kmem_cache_destroy(g_flow_cachep);
        g_flow_cachep = NULL;
    }

    flow_port_fini();

    return;
}

module_init(xflow_rate_init);
module_exit(xflow_rate_fini);

module_param(g_debug_enable, int, 0444);

MODULE_ALIAS("xflow-ddos");
MODULE_AUTHOR("Dinstar Kyle");
MODULE_DESCRIPTION("xflow ddos hook");
MODULE_LICENSE("GPL");


