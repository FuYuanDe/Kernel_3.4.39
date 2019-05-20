#ifndef __XRTP_PKT_CAPTURE_H__
#define __XRTP_PKT_CAPTURE_H__

#ifndef u8
#define u8 (unsigned char)
#endif

#define MAX_TBL_SIZE 65536


#define PKT_CAPTURE_PROTOCOL_ALL (u8)(1<<0)    /* 1 */
#define PKT_CAPTURE_PROTOCOL_TCP (u8)(1<<1)    /* 2 */
#define PKT_CAPTURE_PROTOCOL_UDP (u8)(1<<2)    /* 4 */

/* 抓包地址信息 */
struct pkt_capture_addr {

    /* 本地信息用于填充报文首部字段 */
    __be16              local_port;                     /* local_port */
    union  rtp_inet_addr local_ip;                      /* local_ip */

    /* 远端抓包服务器信息 */
    __be16              remote_port;                /* remote_port */
    union rtp_inet_addr remote_ip;                  /* remote_ip_addr */
};

/* 抓包表 */
struct pkt_capture_tbl {
    unsigned char g_pkt_capture_port[MAX_TBL_SIZE];   /* 端口标志位 65536      */
    struct pkt_capture_addr server_addr;              /* 抓包服务器地址信息 */
};

struct pkt_capture_ctl {
    struct pkt_capture_tbl *pkt_tbl;                /* 抓包表 */
    unsigned char pkt_capture_enable;               /* 抓包标志 */
    unsigned char pkt_capture_tcp;                  /* tcp抓包标志 */
    unsigned char pkt_capture_udp;                  /* udp抓包标志 */
    
//  unsigned char uid;                              /* 服务器标识 */    
};

struct pkt_capture_ctl *g_pkt_cap_ctl_tbl;
static unsigned int g_pkt_capture_tbl_count = 1;    /* 抓包表个数，用于多个服务器*/

/* netlink 消息数据结构，端口范围，协议以及抓包服务器地址信息 */
struct pkt_capture_nl_port_range {
    unsigned int port_min;
	unsigned int port_max;
};

/* 比特位，1 all,2 tcp,4 udp */
struct pkt_capture_nl_protocol {
    unsigned char protocol;
};

/* 地址信息，包含本地以及远端 */
struct pkt_capture_nl_addr {
    struct pkt_capture_addr addr;
};


#endif
