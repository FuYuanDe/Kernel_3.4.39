

#ifndef __DINSTAR_RELAY_H__
#define __DINSTAR_RELAY_H__

#include <net/ip.h>
#include <net/ipv6.h>
#include <net/udp.h>
#include "relay_rmproc.h"


/* 流水线处理返回值 */
#define RELAY_DROP 0
#define RELAY_ACCEPT 1
#define RELAY_STOLEN 2
#define RELAY_QUEUE 3
#define RELAY_REPEAT 4
#define RELAY_STOP 5
#define RELAY_MAX_VERDICT NF_STOP


// realy out
#define RELAY_OUT_TO_REMOTE    1
#define RELAY_OUT_TO_APP       2

#define RELAY_RTP_MUM_MAX  5000


#define PIPELINE_DIR_IN        0 		//wan口收为in，lan口收为out
#define PIPELINE_DIR_OUT       1
//流水线
struct pipeline_info
{
			
	int			(*function)(struct sk_buff *skb,unsigned long data);			
	unsigned long data;
	struct pipeline_info *next;
};


struct relay_inet_info
{
	// 报文 五元组
	__be32	ip_saddr;
	__be32	ip_daddr;
	__be16	port_source;
	// 收发本地upd 端口一致
	__be16	port_dest;
	//qos
};

struct relay_info
{
	int cur_dir;	
	struct pipeline_info *pipeline_in_head;			//wan口收为in，lan口收为out
	struct pipeline_info *pipeline_out_head;
	struct relay_inet_info inet_in_info;
	struct relay_inet_info inet_out_info;
	spinlock_t lock;
	// qos
	int maxin_cnt;
	int curin_cnt;
	int maxout_cnt;
	int curout_cnt;
	int totalin_cnt;
	int totalout_cnt;
	unsigned long pre_jiffies;
};

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef short  S16;


#define RELAYDBG_LEVEL_NONE  0
#define RELAYDBG_LEVEL_ERROR 1
#define RELAYDBG_LEVEL_WARNING 2
#define RELAYDBG_LEVEL_DEBUGINFO 3
#define RELAYDBG_LEVEL_INFOPRINT 4
#define RELAYDBG_LEVEL_MAX	   5

extern int g_relay_debug_level;
#define RELAYDBG(level, msg...)			\
do {						\
    if (g_relay_debug_level >= level)	\
	    printk(KERN_ERR "relay: " msg);	\
} while (0)

		
int  relay_dsp_init(void);
int  relay_in_init(void);
int  relay_ioctl_init(void);
int  relay_netlink_init(void);
int  relay_match_init(void);
int  relay_out_init(void);
int  relay_qos_init(void);
	
void  relay_dsp_exit(void);
void  relay_in_exit(void);
void  relay_ioctl_exit();
void  relay_netlink_exit(void);
void  relay_match_exit(void);
void  relay_out_exit(void);
void  relay_qos_exit(void);
	
struct relay_info * relay_core_add_example(struct relay_inet_info *in,struct relay_inet_info *out);
int relay_core_add_pipline(struct relay_info *cb,struct pipeline_info *info,int dir);
int relay_core_del(__be16	port_dest);

struct relay_info * relay_match_ipv4_hander(struct sk_buff *skb,struct relay_info *cb);
int relay_qos_handler(struct sk_buff *skb,struct relay_info *cb);
int relay_core_recv(struct sk_buff *skb,int protocol);
int relay_rtp_register(struct relay_info *cb,struct rm_media_conn *media_info,int dir);
int relay_out_register(struct relay_info *cb,int type,int dir);
int relay_core_set_baseport( __u16 port);





#endif

