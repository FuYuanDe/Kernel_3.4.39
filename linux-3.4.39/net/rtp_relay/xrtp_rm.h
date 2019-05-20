/*
 *	Linux NET3:	xrtp resource managent header
 *  资源管理头部
 *
 *	Authors: Kyle <zx_feng807@foxmail.com>
 *
 */

#ifndef __XRTP_RM_H__
#define __XRTP_RM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/types.h>

#define RTP_RELAY_VERSION_2     1
#define RM_ETH_ALEN             6
#define RM_MEDIA_AUDIO          1   /* media type:audio */
#define RM_MEDIA_VIDEO          2   /* media type:video */
#define RM_MEDIA_IMAGE          3   /* media type:image */

#define RM_NAME_MAX_LEN         32
#define RM_LONG_NAME_MAX_LEN    64
#define RM_MEDIA_MAX_PORT       65535
///
///
///
#define RM_SIOCGSTAT            0x6b01

/* Internet address. */
struct rm_in_addr {
	__be32	s_addr;
};

/// ipv6定义，
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

///
/// 加密
/// 算法&密钥
///
struct rm_crypto {
    unsigned char crypto[RM_NAME_MAX_LEN];      /*  */
    unsigned char key[RM_LONG_NAME_MAX_LEN];    /* keyֵ */
};

struct rm_init_cfg {
    unsigned int port_min;
	unsigned int port_max;
};

struct rm_rtp_info {
    unsigned char encode_name[RM_NAME_MAX_LEN]; /*  */
    unsigned char param[RM_NAME_MAX_LEN];       /*  */
    unsigned short int payload;                 /* :0,8,18,4,98 */
    unsigned char slience_supp;                 /*  */
    unsigned char dtmf_detect;                  /* DTMF detect */   
    unsigned int  dtmf_action;                  /* DTMF detect */
    unsigned int bitrate;                       /*  */
    unsigned int max_psize;                     /*  */
    unsigned int rfc2833;
    unsigned int max_ptime;                     /*  */
    unsigned int rfc2833_local;                 /* rfc2833ֵ */
    //unsigned int srtp;                          /* SRTP */
};

///
///
///
#define USER_ONLY_USE_ORIG_FOR_SEND          (1 << 7)

///
/// ip类型位掩码,0x0000 0111
///
#define IP_TYPE_BIT_MASK            (0x7)

/// 媒体链接结构
struct rm_media_conn {
    __u16 local_port;                           
    unsigned char local_mac[RM_ETH_ALEN];       
    union rm_inet_addr local_ip;                
    
    __u16 remote_port; 
    unsigned char remote_mac[RM_ETH_ALEN];      
    union rm_inet_addr remote_ip;               
    
    struct rm_crypto crypto;                    

    __u16 vlanid;                               /* vlan id */
    __u16 dscp;                                 /* dscpֵ */
	unsigned char protocol;
	unsigned char remotelock;
    unsigned char ip_type;                      /* ipv4 or ipv6 */

    char media_type;                            /* audio:1 /video:2 /image:3  */

///
/// 媒体数据    ,主要包含一些编解码等数据。
///
    union {
        struct rm_rtp_info rtp;                 /* media_type audio */
    } media_data;
    
    int chan_id;                                /* chan ID */
    int media_lock_param;                       /* */
};

/// 媒体转发表
struct rm_media_tbl {
    unsigned int conn_id;                       

    struct rm_media_conn aconn;                 /* original */
    struct rm_media_conn bconn;                 /* replay */
};

/// rtcp数据
struct rm_rtcp_statis {
    unsigned int sender_pkts;           /* rtp */
    unsigned int fraction_lost;         /*  */
    unsigned int lost_pkts;             /*  */
    unsigned int jitter;                /*  */
};

/// 连接数据
struct rm_conn_statis {
    unsigned int       recvpkts;                   /*  */
    unsigned int       recvbytes;                  /*  */
    unsigned int       recvrate;
    unsigned int       recvpkts_err;
    unsigned int       recvbytes_err;
    
    unsigned int       sendpkts;
    unsigned int       sendbytes;
    unsigned int       sendrate;
    
    struct rm_rtcp_statis rtcp;
};

/// 媒体数据
struct rm_media_statis {
    unsigned int conn_id;

    struct rm_conn_statis astat;
    struct rm_conn_statis bstat;
};


#ifdef __cplusplus
}
#endif

#endif

