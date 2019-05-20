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
#define RM_MEDIA_AUDIO          1   /* ����:audio */
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

/* �������б� */
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
    unsigned char crypto[RM_NAME_MAX_LEN];      /* �����㷨���� */
    unsigned char key[RM_LONG_NAME_MAX_LEN];    /* ����keyֵ */
};

struct rm_rtp_info {
    unsigned short int payload;                 /* �����:0,8,18,4,98 */
    unsigned char encode_name[RM_NAME_MAX_LEN]; /* ��������֣�������ʶ��̬���룬����iLBC */
    unsigned int bitrate;                       /* ������ */
    unsigned char param[RM_NAME_MAX_LEN];       /* �ñ�����Ӧ��һЩ����,�������729��annexb=no */
    unsigned char slience_supp;                 /* �������ƿ��أ�true���򿪾������� */
    unsigned int max_ptime;                     /* �����ʱ�� */
    unsigned char dtmf_detect;                  /* DTMF detect���� */   
    unsigned int  dtmf_action;                  /* DTMF detect���� */
};

struct rm_media_conn {
    unsigned char protocol;                     /* Э�� */
    unsigned char ip_type;                      /* ipv4 or ipv6 */
    
    unsigned char local_mac[RM_ETH_ALEN];       /* ����mac��ַ */
    union rm_inet_addr local_ip;                /* ����ip��ַ */
    __be16 local_port;                          /* ���ض˿ں� */

    unsigned char remote_mac[RM_ETH_ALEN];      /* �Զ�mac��ַ */
    union rm_inet_addr remote_ip;               /* �Զ�IP��ַ */
    __be16 remote_port;                         /* �Զ˶˿ں�1 */

    unsigned char dscp;                         /* dscpֵ */
    __be16 vlanid;                              /* vlan id */

    struct rm_crypto crypto;                    /* ������Ϣ */
    
    char media_type;                            /* audio/video/image ������һ�� */
    union {
        struct rm_rtp_info rtp;                 /* media_type Ϊaudio��Ч */
    } media_data;
    
};

struct rm_media_tbl {
    unsigned int conn_id;                       /* ��ǰ�棬����id��Ŀǰ���ڶ˿ں� */
    unsigned char transcode;                    /* �Ƿ�ת�� */ 

    struct rm_media_conn aconn;                 /* original ����������Ϣ */
    struct rm_media_conn bconn;                 /* replay ����������Ϣ */
};

struct rm_stats_conn {
    unsigned int sendpkts;                      /* ���Ͱ��� */
    unsigned int recvpkts;                      /* ���հ��� */
    unsigned int sendbytes;                     /* �����ֽ��� */
    unsigned int recvbytes;                     /* �����ֽ��� */
    unsigned int discardpkts;                   
    unsigned int locallosts;                    /* ���˶����� */
    unsigned int remotelosts;                   /* �Զ˶����� */
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

/* ��Դ����ģ���ʼ�� */
extern int rm_proc_module_init(void);
/* ��Դ����ģ���ע�� */
extern void rm_proc_module_fini(void);

/* ����һ��rtp���ӣ��������Ѵ�����ֱ���޸� */
extern int rm_proc_create_conn(struct rm_media_tbl *tbl);
/* ɾ��һ��rtp������Ϣ */
extern void rm_proc_delete_conn(unsigned int connid);
/* ɾ������rtp������Ϣ */
extern void rm_proc_delete_all_conn(void);
/* ��ѯָ��rtp���ӵ�ͳ����Ϣ */
extern struct rm_stats_tbl *rm_proc_conn_stats(unsigned int connid);


#ifdef __cplusplus
}
#endif

#endif

