/**
 * Dinstar Provison Application
 * Copyright (C) 2013-2016
 * All rights reserved
 *
 * @file    rmproc.h
 * @brief   
 *
 *
 * @author  kyle
 * @version 1.0
 * @date    2016-11-11
*/

#ifndef __PUB_RMPROC_H__
#define __PUB_RMPROC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/types.h>

#ifndef NETLINK_RM_MODULE
#define NETLINK_RM_MODULE      23   /* netlink for rm module */
#endif

#define RM_IPB1(addr) (((unsigned char*)&addr)[0])
#define RM_IPB2(addr) (((unsigned char*)&addr)[1])
#define RM_IPB3(addr) (((unsigned char*)&addr)[2])
#define RM_IPB4(addr) (((unsigned char*)&addr)[3])

#define RM_ETH_ALEN             6
#define RM_MEDIA_AUDIO          1   /* 语音:audio */
#define RM_MEDIA_VIDEO          2   /* media type:video */
#define RM_MEDIA_IMAGE          3   /* media type:image */

//#define RM_MEDIA_TBL_CREATE     (NLMSG_MIN_TYPE + 1)
//#define RM_MEDIA_TBL_DELETE     (NLMSG_MIN_TYPE + 2)
typedef enum rm_netlink_type {    
    RM_MEDIA_TBL_CREATE = NLMSG_MIN_TYPE + 1,   /* value:17 */
    RM_MEDIA_TBL_DELETE,
    RM_MEDIA_TBL_DELETEALL,
    RM_MEDIA_TBL_STATS,
    RM_MEDIA_TBL_STATSALL,
    RM_MEDIA_TBL_DSP,
    RM_MEDIA_TBL_DSPALL,
    RM_MEDIA_TBL_DEBUG,
    RM_MEDIA_TBL_SELF_CHECK,
    RM_MEDIA_TBL_END,
} RM_NETLINK_TYPE;

/* 错误码列表 */
#define RM_ERRNO_MEMORY_OUT     1
#define RM_ERRNO_DSP            2
#define RM_ERRNO_IPC            3
#define RM_ERRNO_KERNEL         4
#define RM_ERRNO_SOCKET         5
#define RM_ERRNO_IOCTL          6
#define RM_ERRNO_PARAM          7


#define RM_NAME_MAX_LEN         32
#define RM_LONG_NAME_MAX_LEN    64
#define RM_MEDIA_MAX_PORT       65535

#define RM_SIOCGSTAT            0x6b01

/* Internet address. */
struct rm_in_addr {
	__be32	s_addr;
};

struct rm_in6_addr {
	union {
		__u8		u6_addr8[16];
		__be16		u6_addr16[8];
		__be32		u6_addr32[4];
	} in6_u;
#define s6_addr			in6_u.u6_addr8
#define s6_addr16		in6_u.u6_addr16
#define s6_addr32		in6_u.u6_addr32
};

union rm_inet_addr {
	__u32		all[4];
	__be32		ip;
	__be32		ip6[4];
	struct rm_in_addr	in;
	struct rm_in6_addr	in6;
};

struct rm_crypto {
    unsigned char crypto[RM_NAME_MAX_LEN];      /* 加密算法名称 */
    unsigned char key[RM_LONG_NAME_MAX_LEN];    /* 加密key值 */
};

struct rm_rtp_info {
    unsigned short int payload;                 /* 编解码:0,8,18,4,98 */
    unsigned char encode_name[RM_NAME_MAX_LEN]; /* 编解码名字，可用于识别动态编码，比如iLBC */
    unsigned int bitrate;                       /* 比特率 */
    unsigned char param[RM_NAME_MAX_LEN];       /* 该编解码对应的一些参数,比如对于729，annexb=no */
    unsigned char slience_supp;                 /* 静音抑制开关，true，打开静音抑制 */
    unsigned int max_ptime;                     /* 最大打包时长 */
    unsigned char dtmf_detect;                  /* DTMF detect开关 */   
    unsigned int  dtmf_action;                  /* DTMF detect动作 */
};

struct rm_media_conn {
    unsigned char protocol;                     /* 协议 */
    unsigned char ip_type;                      /* ipv4 or ipv6 */
    
    unsigned char local_mac[RM_ETH_ALEN];       /* 本端mac地址 */
    union rm_inet_addr local_ip;                /* 本端ip地址 */
    __be16 local_port;                          /* 本地端口号 */

    unsigned char remote_mac[RM_ETH_ALEN];      /* 对端mac地址 */
    union rm_inet_addr remote_ip;               /* 对端IP地址 */
    __be16 remote_port;                         /* 对端端口号1 */

    unsigned char dscp;                         /* dscp值 */
    __be16 vlanid;                              /* vlan id */

    struct rm_crypto crypto;                    /* 加密信息 */
    
    char media_type;                            /* audio/video/image 中其中一种 */
    union {
        struct rm_rtp_info rtp;                 /* media_type 为audio有效 */
    } media_data;
    
};

struct rm_media_tbl {
    unsigned int conn_id;                       /* 最前面，连接id，目前等于端口号 */
    unsigned char transcode;                    /* 是否转码 */ 

    struct rm_media_conn aconn;                 /* original 方向连接信息 */
    struct rm_media_conn bconn;                 /* replay 方向连接信息 */
};

struct rm_stats_conn {
    unsigned int sendpkts;                      /* 发送包数 */
    unsigned int recvpkts;                      /* 接收包数 */
    unsigned int sendbytes;                     /* 发送字节数 */
    unsigned int recvbytes;                     /* 接收字节数 */
    unsigned int discardpkts;                   
    unsigned int locallosts;                    /* 本端丢包率 */
    unsigned int remotelosts;                   /* 对端丢包率 */
};

struct rm_stats_tbl {
    unsigned int connid;
    struct rm_stats_conn astat;
    struct rm_stats_conn bstat;
};

struct rm_dsp_info {
    unsigned int connid;
    struct list_head list;
};

struct rm_media_ctrl_tbl {
    struct list_head list;
    struct rm_media_tbl tbl;
    struct rm_stats_tbl stats;
    struct rm_dsp_info  dsp;
};

struct rm_ctrl_request {
    int msg_type;
    struct rm_media_tbl tbl;
    struct rm_stats_tbl stats;
    struct rm_dsp_info  dsp;
};

/* 资源管理模块初始化 */
extern int rm_proc_module_init(void);
/* 资源管理模块的注销 */
extern void rm_proc_module_fini(void);

/* 创建一条rtp连接，若连接已存在则直接修改 */
extern int rm_proc_create_conn(struct rm_media_tbl *tbl);
/* 删除一条rtp连接信息 */
extern void rm_proc_delete_conn(unsigned int connid);
/* 删除所有rtp连接信息 */
extern void rm_proc_delete_all_conn(void);
/* 查询指定rtp连接的统计信息 */
extern struct rm_stats_tbl *rm_proc_conn_stats(unsigned int connid);


#ifdef __cplusplus
}
#endif

#endif

