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

/*表示一个acl规则*/
typedef struct
{
    int index; //acl规则的优先级
    int filter_id; //acl规则对应的物理实际位置
    int filter_cnt; //acl规则匹配条件数
    int action_cnt; //acl规则的动作数
    rtk_filter_cfg_t cfg; //acl匹配条件
    rtk_filter_action_t act; //acl动作
    struct list_head list; //所有的struct acl结构组织成一个链表
    struct mutex lock;
}acl_t;

typedef enum
{
    ACL_ZONE_PHY_HIGH = 0, //放针对板间通信物理端口数据流分流的acl规则
    ACL_ZONE_DYNAMIC, //动态ACL规则
    ACL_ZONE_RTP, //针对rtp端口分流的acl规则
    ACL_ZONE_PHY_LOW, //放针对其他物理端口数据流分流的acl规则
    ACL_ZONE_BUTTON,
}acl_zone_e;

typedef struct
{
   struct list_head acl_head[ACL_ZONE_BUTTON]; //ACL规则链表
   int max_acl_cnt[ACL_ZONE_BUTTON]; //最大acl规则数
   int acl_cnt[ACL_ZONE_BUTTON]; //acl规则数
   int filter_used[ACL_ZONE_BUTTON]; //已使用的物理acl表项数
   struct mutex lock;
} acl_cb_t;

typedef enum
{
    RTK_ACL_INIT = 11, //初始化acl分区
    RTK_GET_IDLE_FILTER, //获取物理空闲ACL表项数，从11开始，避免与内核默认命令冲突
    RTK_ACL_ADD, //添加ACL规则
    RTK_ACL_DEL, //删除指定index的ACL规则
    RTK_ACL_MOD, //修改指定index的ACL规则
    RTK_ACL_GETALL, //获取所有ACL规则
    RTK_ACL_GETBYINDEX, //获取指定的ACL
    RTK_ACL_CLEAN, //清空所有的acl
    RTK_ACL_PHYUP, //使能对接口板的phy
    RTK_MIRROR_ADD, //增加镜像port
    RTK_MIRROR_DEL, //删除镜像port
	RTK_MIRROR_STATUS, //获取镜像port
    RTK_GET_REG,
    RTK_GET_MIB,
    RTK_ACL_DEL_FUZZY, //模糊匹配删除
    RTK_ACL_DEL_EXACT, //精确匹配删除
    RTK_SET_REG,
} RTK_ACL_CMD_E;

/*单ACL规则内核与用户空间传输结构*/
typedef struct
{
    acl_zone_e acl_zone; //指定操作的ACL区域
    int index; //ACL区域内的acl序号
    int filter_cnt; //匹配条件数
    rtk_filter_field_t filter_field[RTK_MAX_NUM_OF_FILTER_TYPE]; //匹配条件，最大一个ACL规则有5个匹配条件
    rtk_filter_action_t action; //ACL动作
    rtk_filter_value_t activeport; //ACL规则作用的物理端口
    rtk_filter_care_tag_t careTag; ///< ACL规则作用的包类型
    u32 rate; //限流速率
} user_acl_trans_t;

/*多acl规则内核与用户空间传输结构*/
typedef struct
{
    int cnt; //写ACL规则时，表示写的ACL规则数；读ACL规则时，表示用户控件缓存最大可存储的ACL规则数
    int acl_region; //默认操作的ACL区域，如果user_acl_trans_t中指定了ACL规则，则按规则指定的区域获取ACL规则
    user_acl_trans_t *user_acl;
} multi_user_acl_trans_t;

typedef struct
{
    int phy; ///< 指定phy接口
    int enble; ///< 开启或关闭
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
