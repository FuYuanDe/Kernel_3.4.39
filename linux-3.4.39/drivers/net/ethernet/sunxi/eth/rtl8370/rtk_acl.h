#ifndef __RTK_ACL_H__
#define __RTK_ACL_H__

#include "rtl8370_asicdrv_acl.h"

#define DEF_PHY_HIGH_ZONE_ACL_FILTER_NUM    19
#define DEF_PHY_LOW_ZONE_ACL_FILTER_NUM     15
#define DEF_RTP_ZONE_ACL_FILTER_NUM         8
#define DEF_DYNAMIC_ZONE_ACL_FILTER_NUM     (RTK_MAX_NUM_OF_ACL_RULE - DEF_PHY_HIGH_ZONE_ACL_FILTER_NUM - DEF_PHY_LOW_ZONE_ACL_FILTER_NUM - DEF_RTP_ZONE_ACL_FILTER_NUM)

#define PHY_HIGH_ZONE_ACL_FILTER_NUM        (zone_high_acl_num)
#define PHY_LOW_ZONE_ACL_FILTER_NUM         (zone_low_acl_num)
#define RTP_ZONE_ACL_FILTER_NUM             (zone_rtp_acl_num)
#define DYNAMIC_ZONE_ACL_FILTER_NUM (RTK_MAX_NUM_OF_ACL_RULE - PHY_HIGH_ZONE_ACL_FILTER_NUM - PHY_LOW_ZONE_ACL_FILTER_NUM - RTP_ZONE_ACL_FILTER_NUM)

#define PHY_HIGH_ZONE_ACL_FILTER_START   0
#define PHY_HIGH_ZONE_ACL_FILTER_END     (PHY_HIGH_ZONE_ACL_FILTER_START+PHY_HIGH_ZONE_ACL_FILTER_NUM-1)
#define DYNAMIC_ZONE_ACL_FILTER_START (PHY_HIGH_ZONE_ACL_FILTER_END+1)
#define DYNAMIC_ZONE_ACL_FILTER_END (DYNAMIC_ZONE_ACL_FILTER_START+DYNAMIC_ZONE_ACL_FILTER_NUM-1)
#define RTP_ZONE_ACL_FILTER_START   (DYNAMIC_ZONE_ACL_FILTER_END+1)
#define RTP_ZONE_ACL_FILTER_END     (RTP_ZONE_ACL_FILTER_START+RTP_ZONE_ACL_FILTER_NUM-1)
#define PHY_LOW_ZONE_ACL_FILTER_START   (RTP_ZONE_ACL_FILTER_END+1)
#define PHY_LOW_ZONE_ACL_FILTER_END     (PHY_LOW_ZONE_ACL_FILTER_START+PHY_LOW_ZONE_ACL_FILTER_NUM-1)

typedef struct
{
    int used;
    u32 rate;
}meter_table_t;

/*��ʾһ��acl����*/
typedef struct
{
    int index; //acl��������ȼ�
    int filter_id; //acl�����Ӧ������ʵ��λ��
    int filter_cnt; //acl����ƥ��������
    int action_cnt; //acl����Ķ�����
    rtk_filter_cfg_t cfg; //aclƥ������
    rtk_filter_action_t act; //acl����
    struct list_head list; //���е�struct acl�ṹ��֯��һ������
    struct mutex lock;
}acl_t;

typedef enum
{
    ACL_ZONE_PHY_HIGH = 0, //����԰��ͨ������˿�������������acl����
    ACL_ZONE_DYNAMIC, //��̬ACL����
    ACL_ZONE_RTP, //���rtp�˿ڷ�����acl����
    ACL_ZONE_PHY_LOW, //�������������˿�������������acl����
    ACL_ZONE_BUTTON,
}acl_zone_e;

typedef struct
{
   struct list_head acl_head[ACL_ZONE_BUTTON]; //ACL��������
   int max_acl_cnt[ACL_ZONE_BUTTON]; //���acl������
   int acl_cnt[ACL_ZONE_BUTTON]; //acl������
   int filter_used[ACL_ZONE_BUTTON]; //��ʹ�õ�����acl������
   struct mutex lock;
} acl_cb_t;

typedef enum
{
    RTK_ACL_INIT = 11, //��ʼ��acl����
    RTK_GET_IDLE_FILTER, //��ȡ�������ACL����������11��ʼ���������ں�Ĭ�������ͻ
    RTK_ACL_ADD, //���ACL����
    RTK_ACL_DEL, //ɾ��ָ��index��ACL����
    RTK_ACL_MOD, //�޸�ָ��index��ACL����
    RTK_ACL_GETALL, //��ȡ����ACL����
    RTK_ACL_GETBYINDEX, //��ȡָ����ACL
    RTK_ACL_CLEAN, //������е�acl
    RTK_ACL_PHYUP, //ʹ�ܶԽӿڰ��phy
    RTK_MIRROR_ADD, //���Ӿ���port
    RTK_MIRROR_DEL, //ɾ������port
	RTK_MIRROR_STATUS, //��ȡ����port
    RTK_GET_REG,
    RTK_GET_MIB,
    RTK_ACL_DEL_FUZZY, //ģ��ƥ��ɾ��
    RTK_ACL_DEL_EXACT, //��ȷƥ��ɾ��
    RTK_SET_REG,
} RTK_ACL_CMD_E;

/*��ACL�����ں����û��ռ䴫��ṹ*/
typedef struct
{
    acl_zone_e acl_zone; //ָ��������ACL����
    int index; //ACL�����ڵ�acl���
    int filter_cnt; //ƥ��������
    rtk_filter_field_t filter_field[RTK_MAX_NUM_OF_FILTER_TYPE]; //ƥ�����������һ��ACL������5��ƥ������
    rtk_filter_action_t action; //ACL����
    rtk_filter_value_t activeport; //ACL�������õ�����˿�
    rtk_filter_care_tag_t careTag; ///< ACL�������õİ�����
    u32 rate; //��������
} user_acl_trans_t;

/*��acl�����ں����û��ռ䴫��ṹ*/
typedef struct
{
    int cnt; //дACL����ʱ����ʾд��ACL����������ACL����ʱ����ʾ�û��ؼ��������ɴ洢��ACL������
    int acl_region; //Ĭ�ϲ�����ACL�������user_acl_trans_t��ָ����ACL�����򰴹���ָ���������ȡACL����
    user_acl_trans_t *user_acl;
} multi_user_acl_trans_t;

typedef struct
{
    int phy; ///< ָ��phy�ӿ�
    int enble; ///< ������ر�
}en_phy_st;

typedef struct
{
    int zone_high_num;
    int zone_low_num;
    int zone_rtp_num;
    int zone_dynamic_num;
} ACL_INIT_ARG_ST;

typedef enum
{
    ACL_NO_EXISTED = 0,
    ACL_EXISTED,
} ACL_EXACT_CHECK_E;

rtk_api_ret_t rtk_acl_init(void);
void rtk_acl_exit(void);

#endif
