#ifndef _RTL8370_H_
#define _RTL8370_H_
#include "rtk_api.h"

#define RTK_MAX_SLOT            6

#define RTK_MAX_PROT            8
#define RTK_MIN_PROT            0

#define MIRROR_DIREXT_RX     0
#define MIRROR_DIREXT_TX     1
#define MIRROR_DIREXT_BOTH     2
#define MIRROR_DIREXT_NONE 3
typedef struct rtk_mirror_port
{
    int mirror_src_port;
    int mirror_dst_port; 
    int mirror_direct;
    
}rtk_mirror_port_t;

int rtk8370_mirror_add2cpu(rtk_mirror_port_t *user_mirrorport);
int rtk8370_mirror_del2cpu(rtk_mirror_port_t *user_mirrorport);

/* �Ĵ������� */
#define PHY_ID_REGISTER            0x2022
#define CONTROL_REG_PORT_POWER_BIT	0x800

#define DTU2_MAX_DSP            2
#define DTU8_MAX_DSP            8
#define INTERNAL_VLAN_ID    10 
#define PORT0_MASK          0x001
#define PORT1_MASK          0x002
#define PORT8_MASK          0x100
#define PORT9_MASK          0x200

typedef struct rtk_port_ability_conf_s
{
    uint32 port;
    rtk_port_phy_ability_t phyability;
} rtk_port_ability_conf_t;

typedef struct rtk_port_phy_cfg_s
{
    u32  ulPort;// phy port
    u32  ulPowerUp;// powerup or powerdwon
    rtk_port_phy_ability_t stAbility;

}rtk_port_phy_cfg_t;

typedef struct rtk_ip_acl_conf_s
{
    uint32 remoteip_flag;
    uint32 ipv6_flag;
	uint32 ipaddr[4];
	uint32 port; //ҵ���: 1�����ܿ�: 0
} rtk_ip_acl_conf_t;

typedef struct port_cntr_s
{
    //����6��uint32
    uint32 ifInOctets;
    uint32 dot3StatsFCSErrors;
    uint32 dot3StatsSymbolErrors;
    uint32 dot3InPauseFrames;
    uint32 ifOutOctets;
    uint32 dot3OutPauseFrames;
}port_cntr_t;

typedef struct rtk_port_mib_s
{
    uint32 port;
    uint32 mib_reset;
    port_cntr_t portcntr;
} rtk_port_mib_t;

typedef struct rtk_sip_acl_conf_s
{
    uint16 sip_port;
    uint16 pad;
	uint32 rate;
	uint32 port; //ҵ���: 1�����ܿ�: 0
} rtk_sip_acl_conf_t;

/* ��д8370 �Ĵ��������ṹ */
struct reg_param_s
{
	u32 reg;
	u32 value;
};

typedef struct  
{
    u32   ulEthPort;// 0:���ܿ� 1:ҵ���
    uint8 octet[ETHER_ADDR_LEN];
    
} voice_mac_bind_t;

typedef enum enEthPortApplyMode
{
    DEV_ETHNET_PORT_NORMAL,// ��ͨģʽ��rtp��sipֻ��ҵ��ڣ����ܿ�ֻ�����ع�����web��½��û��portbonding û��rtp��˫����                  
    DEV_ETHNET_PORT_MEDIA_DOUBLEIP,// rtp��sip����ҵ��ں����ܿ�               
    DEV_ETHNET_PORT_TRUNKING,// ҵ��ں����ܿ��໥�����ݣ�ֻ����ҵ��ڵ�ip �����ܿ�ip��Ч     
    DEV_ETHNET_PORT_BUTT                    
}DEV_ETH_PORT_APPLY_MODE_E;
	
/* ioctl �ӿ���صĶ��� */
#define RTK8370M_IOC_MAGIC        'R'
#define RTK_PORT_PHY_STAT_GET     _IOWR(RTK8370M_IOC_MAGIC,  0x10, int)
#define RTK_PORT_ALL_STAT_GET     _IOWR(RTK8370M_IOC_MAGIC,  0x11, int)
#define RTK_PORT_SPEED_STATUS_GET   _IOWR(RTK8370M_IOC_MAGIC,  0x12, int)
#define RTK_PORT_SPEED_ABILITY_SET  _IOWR(RTK8370M_IOC_MAGIC,  0x13, rtk_port_ability_conf_t)
#define RTK_PORT_ACL_SET            _IOWR(RTK8370M_IOC_MAGIC,  0x14, rtk_mac_t)
#define RTK_SOFT_RESET              _IOWR(RTK8370M_IOC_MAGIC,  0x15, int)
#define RTK_IP_ACL_SET              _IOWR(RTK8370M_IOC_MAGIC,  0x16, rtk_ip_acl_conf_t)
#define RTK_SET_WORK_MODE           _IOWR(RTK8370M_IOC_MAGIC,  0x17, rtk_port_phy_ability_t) //�������нӿڰ�Խ�����
#define RTK_PORT_GET_MIB            _IOWR(RTK8370M_IOC_MAGIC,  0x18, rtk_port_mib_t)
#define RTK_SIP_BANDWIDTH_SET       _IOWR(RTK8370M_IOC_MAGIC,  0x19, rtk_sip_acl_conf_t)
#define RTK_VOICE_MAC_BIND          _IOWR(RTK8370M_IOC_MAGIC,  0x1a, voice_mac_bind_t)
#define RTK_SLOT_PHY_CFG            _IOWR(RTK8370M_IOC_MAGIC,  0x1b, rtk_port_phy_cfg_t)
#define RTK_SERVICE_MAC_UPDATE      _IOWR(RTK8370M_IOC_MAGIC,  0x1c, voice_mac_bind_t)

/* ��дPHY �Ĵ��������������� */
#define RTK_READ_REGISTER         _IOWR(RTK8370M_IOC_MAGIC,  0x30, struct reg_param_s)
#define RTK_WRITE_REGISTER        _IOWR(RTK8370M_IOC_MAGIC,  0x31, struct reg_param_s)

/* ����ץ������ */
#define RTK_CAPTURE_SLOT_START      _IOWR(RTK8370M_IOC_MAGIC,  0x41, int)
#define RTK_CAPTURE_SLOT_STOP       _IOWR(RTK8370M_IOC_MAGIC,  0x42, int)

#define RTK_GE0_CAPTURE_SLOT_START  _IOWR(RTK8370M_IOC_MAGIC,  0x43, int)
#define RTK_GE0_CAPTURE_SLOT_STOP   _IOWR(RTK8370M_IOC_MAGIC,  0x44, int)

/* ����rtp��ʼ�˿� */
#define RTK_SET_RTP_PORT            _IOWR(RTK8370M_IOC_MAGIC,  0x51, int)

#endif
