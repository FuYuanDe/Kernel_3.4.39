#include <linux/clk.h>
#include <linux/mii.h>
#include <linux/gpio.h>
#include <linux/crc32.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/crypto.h>
#include <linux/err.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>


#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC300USER) || \
	defined(PRODUCT_AG) || defined(PRODUCT_SBC1000USER) || defined(PRODUCT_UC200) || \
	defined(PRODUCT_SBC1000MAIN)
#include "rtl8367/rtk_types.h"
#include "rtl8367/rtl8367c_reg.h"
#include "rtl8367/acl.h"
#include "rtl8367/rate.h"

#include "rtl8367/rtk_switch.h"
#include "rtl8367/rtl8367c_asicdrv_mib.h"

#include "rtl8367/rtk_acl.h"

#include "rtl8367/rtk_error.h"
#include "rtl8367/rtl8367.h"
#include "rtl8367/mirror.h"

#else


#include "rtl8370/rtk_types.h"
#include "rtl8370/rtl8370.h"
#include "rtl8370/rtl8370_reg.h"
#include "rtl8370/rtk_api.h"

#include "rtl8370/rtk_acl.h"

#include "rtl8370/rtk_error.h"
#include "rtl8370/rtl8370_asicdrv_mib.h"
#endif

#define ACL_DEV_NAME    "acl"

extern rtk_api_ret_t rtk_filter_igrAcl_field_add(rtk_filter_cfg_t* pFilter_cfg, rtk_filter_field_t* pFilter_field);
extern rtk_api_ret_t rtk_filter_igrAcl_cfg_del(rtk_filter_id_t filter_id);
extern rtk_api_ret_t rtk_filter_igrAcl_cfg_add(rtk_filter_id_t filter_id, rtk_filter_cfg_t* pFilter_cfg, rtk_filter_action_t* pFilter_action, rtk_filter_number_t *ruleNum);
extern int simple_phy_read(u16 reg, u32 *ret);

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC300USER) || \
	defined(PRODUCT_AG) || defined(PRODUCT_SBC1000USER) || defined(PRODUCT_UC200) || \
	defined(PRODUCT_SBC1000MAIN)
extern ret_t rtl8367c_getAsicMIBsCounter(rtk_uint32 port, RTL8367C_MIBCOUNTER mibIdx, rtk_uint64* pCounter);
extern void rtk8367_set_phy_powerdown(uint32 port, uint32 bEnable);
extern rtk_api_ret_t rtk_rate_shareMeter_set(rtk_meter_id_t index, rtk_meter_type_t type, rtk_rate_t rate, rtk_enable_t ifg_include);
extern int rtk8367c_mirror_add2cpu(rtk_mirror_port_t *user_mirrorport);
extern int rtk8367c_mirror_del2cpu(rtk_mirror_port_t *user_mirrorport);
static int port_map[7] = {UTP_PORT0, UTP_PORT1, UTP_PORT2, UTP_PORT3,UTP_PORT4,EXT_PORT0, EXT_PORT1};
#else
extern rtk_api_ret_t rtk_rate_shareMeter_set(rtk_meter_id_t index, rtk_rate_t rate, rtk_enable_t ifg_include);
extern void rtk8370_set_phy_powerdown(uint32 port, uint32 bEnable);
extern rtk_api_ret_t rtk_mirror_portBased_set(rtk_port_t mirroring_port, rtk_portmask_t *pMirrored_rx_portmask, rtk_portmask_t *pMirrored_tx_portmask);
extern rtk_api_ret_t rtk_mirror_portBased_get(rtk_port_t* pMirroring_port, rtk_portmask_t *pMirrored_rx_portmask, rtk_portmask_t *pMirrored_tx_portmask);
extern ret_t rtl8370_getAsicMIBsCounter(uint32 port,enum RTL8370_MIBCOUNTER mibIdx,uint64_t * counter);
extern void rtk8370_set_phyport_work_mode(int phyport);
#endif
acl_cb_t *acl_cb;
meter_table_t meter_table[RTK_MAX_NUM_OF_METER];
static int zone_high_acl_num = DEF_PHY_HIGH_ZONE_ACL_FILTER_NUM;
static int zone_low_acl_num = DEF_PHY_LOW_ZONE_ACL_FILTER_NUM;
static int zone_rtp_acl_num = DEF_RTP_ZONE_ACL_FILTER_NUM;
static int zone_dynamic_acl_num = DEF_DYNAMIC_ZONE_ACL_FILTER_NUM;

/*acl*/
static u32 acl_dev_major = 218;
static dev_t acl_devno;
static struct cdev acl_cdev;
static struct class *acl_class;

void acl_lock(void)
{
    if(acl_cb)
    {
        mutex_lock(&acl_cb->lock);
    }
}

void acl_unlock(void)
{
    if(acl_cb)
    {
        mutex_unlock(&acl_cb->lock);
    }
}

static void free_acl(acl_t *acl)
{
    rtk_filter_field_t *rtl_filter, *tmp_rtl_filter;

    if(NULL == acl)
    {
        return;
    }
    
    for(rtl_filter=acl->cfg.fieldHead; rtl_filter;)
    {
        DBGPRINT("%s free filter\n", __func__);
        tmp_rtl_filter = rtl_filter->next;
        kfree(rtl_filter);
        rtl_filter = tmp_rtl_filter;
    }
    kfree(acl);
}

static int acl_get(acl_t *acl, user_acl_trans_t *user_acl)
{
    rtk_filter_field_t *rtl_filter;
    int i;

    if(NULL == acl || NULL == user_acl)
    {
        printk(KERN_ERR "%s invalid arg\n", __FUNCTION__);
        return -EINVAL;
    }

    if(user_acl->filter_cnt > RTK_MAX_NUM_OF_FILTER_TYPE)
    {
        printk(KERN_ERR "%s user_acl->filter_cnt %d exceed %d\n", __FUNCTION__,
            user_acl->filter_cnt, RTK_MAX_NUM_OF_FILTER_TYPE);
        return -EINVAL;
    }
    DBGPRINT("%s user_acl->filter_cnt:%d\n", __func__, user_acl->filter_cnt);
    
    /*复制用户空间的acl匹配条件*/
    for(i=0; i<user_acl->filter_cnt; i++)
    {
        rtl_filter = kmalloc(sizeof(rtk_filter_field_t), GFP_KERNEL);
        if(NULL == rtl_filter)
        {
            free_acl(acl);
            printk(KERN_ERR "%s kmalloc error\n", __FUNCTION__);
            return -ENOMEM;
        }
    
        if(copy_from_user(rtl_filter, &user_acl->filter_field[i], sizeof(rtk_filter_field_t)))
        {
            free_acl(acl);
            printk(KERN_ERR "%s copy_from_user error\n", __FUNCTION__);
            return -EFAULT;
        }
        DBGPRINT("%s filter_field type %d\n", __func__, rtl_filter->fieldType);
        
        if(RT_ERR_OK != rtk_filter_igrAcl_field_add(&acl->cfg, rtl_filter))
        {
            free_acl(acl);
            printk(KERN_ERR "%s rtk_filter_igrAcl_field_add error\n", __FUNCTION__);
            return -EINVAL;
        }
    
        acl->filter_cnt++;
    }
    
    /*复制作用端口*/
    if(copy_from_user(&acl->cfg.activeport, &user_acl->activeport, sizeof(acl->cfg.activeport)))
    {
        free_acl(acl);
        printk(KERN_ERR "%s copy_from_user error\n", __FUNCTION__);
        return -EFAULT;
    }
    
    /*复制acl动作*/
    if(copy_from_user(&acl->act, &user_acl->action, sizeof(acl->act)))
    {
        free_acl(acl);
        printk(KERN_ERR "%s copy_from_user error\n", __FUNCTION__);
        return -EFAULT;
    }

    /*复制匹配包类型*/
    if(copy_from_user(&acl->cfg.careTag, &user_acl->careTag, sizeof(acl->cfg.careTag)))
    {
        free_acl(acl);
        printk(KERN_ERR "%s copy_from_user error\n", __FUNCTION__);
        return -EFAULT;
    }
    
    return 0;
}

static int acl_put(user_acl_trans_t *user_acl, acl_t *acl)
{
    rtk_filter_field_t *rtl_filter;
    int i;

    for(rtl_filter=acl->cfg.fieldHead, i=0; 
        (rtl_filter!=NULL) && (i<RTK_MAX_NUM_OF_FILTER_TYPE); 
        rtl_filter=rtl_filter->next, i++)
    {
        DBGPRINT("%s acl %d\n", __func__, i);
        if(copy_to_user(&user_acl->filter_field[i], rtl_filter, sizeof(rtk_filter_field_t)))
        {
            printk(KERN_ERR "%s copy_to_user error\n", __FUNCTION__);
            return -EFAULT;
        }
        user_acl->filter_cnt++;
    }
    DBGPRINT("%s user_acl->filter_cnt %d\n", __func__, user_acl->filter_cnt);

    if(copy_to_user(&user_acl->activeport, &acl->cfg.activeport, sizeof(user_acl->activeport)))
    {
        printk(KERN_ERR "%s copy_to_user error\n", __FUNCTION__);
        return -EFAULT;
    }
    if(copy_to_user(&user_acl->action, &acl->act, sizeof(user_acl->action)))
    {
        printk(KERN_ERR "%s copy_to_user error\n", __FUNCTION__);
        return -EFAULT;
    }
    if(copy_to_user(&user_acl->careTag, &acl->cfg.careTag, sizeof(user_acl->careTag)))
    {
        printk(KERN_ERR "%s copy_to_user error\n", __FUNCTION__);
        return -EFAULT;
    }

    if(TRUE == user_acl->action.actEnable[FILTER_ENACT_POLICING_0])
    {
        user_acl->rate = meter_table[user_acl->action.filterPolicingIdx[0]].rate;
        DBGPRINT("%s user_acl->rate:%d\n", __func__, user_acl->rate);
    }
    return 0;
}

static rtk_api_ret_t acl_set_meter(u32 rate, uint32 *index)
{
    int i;
    int retVal;
    
    if(NULL == index)
    {
        printk(KERN_ERR "%s invalid arg\n", __FUNCTION__);
        return -EINVAL;
    }

///-
/*如果涉及到修改操作，共用一个表项不可以，将全部被修改*/
#if 0
    /*meter表项可重复利用，如果有相同速率的表项，直接返回*/
    for(i=0; i<RTK_MAX_NUM_OF_METER; i++)
    {
        if(meter_table[i].used && meter_table[i].rate == rate)
        {
            meter_table[i].used++;
            return i;
        }
    }
#endif
    for(i=0; i<RTK_MAX_NUM_OF_METER; i++)
    {
        if(!meter_table[i].used)
        {
            break;
        }
    }
    if(i == RTK_MAX_NUM_OF_METER)
    {
        printk(KERN_ERR "no space of meter\n");
        return RT_ERR_FAILED;
    }
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC300USER) || \
	defined(PRODUCT_AG) || defined(PRODUCT_SBC1000USER) || defined(PRODUCT_UC200) || \
	defined(PRODUCT_SBC1000MAIN)
    retVal = rtk_rate_shareMeter_set(i, METER_TYPE_KBPS, rate, 0); //[0x147e=0x20]设置256kbps
#else
	retVal = rtk_rate_shareMeter_set(i, rate, 0); //[0x147e=0x20]设置256kbps
#endif	
    if( retVal != RT_ERR_OK )
	{
		printk(KERN_ERR "%s: rtk_rate_shareMeter_set failed! retVal=0x%x\n", __func__, retVal);
        return RT_ERR_FAILED;
	}
    else
    {
        meter_table[i].rate = rate;
        meter_table[i].used++;
        *index = i;
    }

    return RT_ERR_OK;
}

static rtk_api_ret_t acl_del_meter(uint32 index)
{    
    if(index < 0 || index >= RTK_MAX_NUM_OF_METER)
    {
        printk(KERN_ERR "invalid index %d\n", index);
        return RT_ERR_FAILED;
    }
    if(meter_table[index].used > 0)
    {
        meter_table[index].used--;
    }
    return RT_ERR_OK;
}

static rtk_api_ret_t acl_set_to_phy(acl_t *acl, u32 rate)
{
    int i;
    rtk_api_ret_t retVal;
    rtk_filter_number_t ruleNum;

    if(TRUE == acl->act.actEnable[FILTER_ENACT_POLICING_0])
    {
        retVal = acl_set_meter(rate, &acl->act.filterPolicingIdx[0]);
        if(retVal != RT_ERR_OK)
        {
            printk(KERN_ERR "%s acl_set_meter error\n", __FUNCTION__);
            return RT_ERR_FAILED;
        }
        DBGPRINT("%s acl->act.filterPolicingIdx[0]:%d\n", __func__, acl->act.filterPolicingIdx[0]);
    }
    
    
    /*先清空要写入区域的acl*/
    for(i=acl->filter_id; i<(acl->filter_id+acl->filter_cnt); i++)
    {
        DBGPRINT("%s filter_id: %d\n", __func__, i);
        rtk_filter_igrAcl_cfg_del(i);
    }
    retVal = rtk_filter_igrAcl_cfg_add(acl->filter_id, &acl->cfg, &acl->act, &ruleNum);
    if( retVal != RT_ERR_OK )
    {
        if(TRUE == acl->act.actEnable[FILTER_ENACT_POLICING_0])
        {
            acl_del_meter(acl->act.filterPolicingIdx[0]);
        }
        printk(KERN_ERR "%s: rtk_filter_igrAcl_cfg_add failed! retVal=0x%x\n", __func__, retVal);
        return retVal;
    }
    else
    {
        DBGPRINT("%s: acl set success, filter_id=%d ruleNum=%d\n", __func__, acl->filter_id, ruleNum);
    }
    
    return retVal;
}

static rtk_api_ret_t acl_del_on_phy(acl_t *acl)
{
    int meter_index, i;
    
    for(i=acl->filter_id; i<(acl->filter_id+acl->filter_cnt); i++)
    {
        DBGPRINT("%s filter id: %d\n", __func__, i);
        rtk_filter_igrAcl_cfg_del(i);
    }

    if(TRUE == acl->act.actEnable[FILTER_ENACT_POLICING_0])
    {
        meter_index = acl->act.filterPolicingIdx[0];
        if(RT_ERR_OK != acl_del_meter(meter_index))
        {
            printk(KERN_ERR "%s acl_del_meter failed\n", __FUNCTION__);
            return RT_ERR_FAILED;
        }
        DBGPRINT("%s meter_index: %d\n", __func__, meter_index);
    }
    return RT_ERR_OK;
}

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC300USER) || \
	defined(PRODUCT_AG) || defined(PRODUCT_SBC1000USER) || defined(PRODUCT_UC200) || \
	defined(PRODUCT_SBC1000MAIN)
static uint32_t acl_exact_check(acl_zone_e zone, acl_t *acl)
{
    struct list_head *acl_head;
    struct list_head *tmp_list = NULL;
    acl_t *acl_tmp = NULL;
    rtk_filter_field_t *rtl_filter1, *rtl_filter2;
    int i, j;
    rtk_api_ret_t retVal;
    int not_mach;

    if((NULL == acl) || (zone >= ACL_ZONE_BUTTON))
    {
        printk(KERN_ERR "%s invalid arg\n", __FUNCTION__);
        return ACL_NO_EXISTED;
    }

    mutex_lock(&acl_cb->lock);   
    DBGPRINT("%s del exact at zone %d start\n", __func__, zone);

    acl_head = &acl_cb->acl_head[zone];
    for(tmp_list=acl_head->next; tmp_list!=acl_head;)
    {
        acl_tmp = container_of(tmp_list, acl_t, list);
        
        if(acl->filter_cnt != acl_tmp->filter_cnt)
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }

        for(rtl_filter1=acl->cfg.fieldHead, i=0; 
            (rtl_filter1!=NULL) && (i<RTK_MAX_NUM_OF_FILTER_TYPE); 
            rtl_filter1=rtl_filter1->next, i++)
        {
            for(rtl_filter2=acl_tmp->cfg.fieldHead, j=0; 
                (rtl_filter2!=NULL) && (j<RTK_MAX_NUM_OF_FILTER_TYPE); 
                rtl_filter2=rtl_filter2->next, j++)
            {
                if(rtl_filter1->fieldType != rtl_filter2->fieldType)
                {
                    continue;
                }
                if(memcmp(&rtl_filter1->filter_pattern_union, &rtl_filter2->filter_pattern_union, 
                    sizeof(rtl_filter1->filter_pattern_union)))
                {
                    continue;
                }
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                break;
            }
            /*没匹配上*/
            if((rtl_filter2 == NULL) || (j == RTK_MAX_NUM_OF_FILTER_TYPE))
            {
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                break;
            }
        }
        /*有条件没有匹配上，寻找下一条*/
        if((rtl_filter1 != NULL) && (i<RTK_MAX_NUM_OF_FILTER_TYPE))
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }

        if(memcmp(&acl->cfg.activeport, &acl_tmp->cfg.activeport, 
            sizeof(rtk_filter_activeport_t)))
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }

        for(i=0; i<FILTER_ENACT_END; i++)
        {
            if(acl->act.actEnable[i] != acl_tmp->act.actEnable[i])
            {
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                break;
            }
            
            if(acl->act.actEnable[i])
            {
                if(!acl_tmp->act.actEnable[i])
                {
                    DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                    break;
                }

                not_mach = 0;
                switch(i)
                {
                    case FILTER_ENACT_CVLAN_INGRESS:
                        {
                            if(acl->act.filterCvlanVid != acl_tmp->act.filterCvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_CVLAN_EGRESS:
                        {
                            if(acl->act.filterCvlanVid != acl_tmp->act.filterCvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_CVLAN_SVID:
                        break;
                    case FILTER_ENACT_POLICING_1:
                        {
                            if(meter_table[acl->act.filterPolicingIdx[1]].rate !=
                                meter_table[acl_tmp->act.filterPolicingIdx[1]].rate)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_SVLAN_INGRESS:
                        {
                            if(acl->act.filterSvlanVid != acl_tmp->act.filterSvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_SVLAN_EGRESS:
                        {
                            if(acl->act.filterSvlanVid != acl_tmp->act.filterSvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_SVLAN_CVID:
                        break;
                    case FILTER_ENACT_POLICING_2:
                        {
                            if(meter_table[acl->act.filterPolicingIdx[2]].rate !=
                                meter_table[acl_tmp->act.filterPolicingIdx[2]].rate)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_POLICING_0:
                        {
                            if(meter_table[acl->act.filterPolicingIdx[0]].rate !=
                                meter_table[acl_tmp->act.filterPolicingIdx[0]].rate)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_COPY_CPU:
                        break;
                    case FILTER_ENACT_DROP:
                        break;
                    case FILTER_ENACT_ADD_DSTPORT:
                        {
                            if(memcmp(&acl->act.filterPortmask, &acl_tmp->act.filterPortmask,
                                sizeof(rtk_portmask_t)))
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_REDIRECT:
                        {
                            if(memcmp(&acl->act.filterPortmask, &acl_tmp->act.filterPortmask,
                                sizeof(rtk_portmask_t)))
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_MIRROR:
                        {
                            if(memcmp(&acl->act.filterPortmask, &acl_tmp->act.filterPortmask,
                                sizeof(rtk_portmask_t)))
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_TRAP_CPU:
                        break;
                    case FILTER_ENACT_ISOLATION:
                        {
                            if(memcmp(&acl->act.filterPortmask, &acl_tmp->act.filterPortmask,
                                sizeof(rtk_portmask_t)))
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_PRIORITY:
                        {
                            if(acl->act.filterPriority != acl_tmp->act.filterPriority)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_DSCP_REMARK:
                        {
                            if(acl->act.filterPriority != acl_tmp->act.filterPriority)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_1P_REMARK:
                        {
                            if(acl->act.filterPriority != acl_tmp->act.filterPriority)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_POLICING_3:
                        {
                            if(acl->act.filterPriority != acl_tmp->act.filterPriority)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                            if(meter_table[acl->act.filterPolicingIdx[3]].rate !=
                                meter_table[acl_tmp->act.filterPolicingIdx[3]].rate)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_INTERRUPT:
                        break;
                    case FILTER_ENACT_GPO:
                        {
                            if(acl->act.filterPin != acl_tmp->act.filterPin)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_EGRESSCTAG_UNTAG:
                        break;
                    case FILTER_ENACT_EGRESSCTAG_TAG:
                        break;
                    case FILTER_ENACT_EGRESSCTAG_KEEP:
                        break;
                    case FILTER_ENACT_EGRESSCTAG_KEEPAND1PRMK:
                        break;
                }
                if(not_mach)
                {
                    DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                    break;
                }
            }
        }
        if(i < FILTER_ENACT_END)
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }
                
        mutex_unlock(&acl_cb->lock);
        return ACL_EXISTED;
    }

    DBGPRINT("%s del exact at zone %d end\n", __func__, zone);
    mutex_unlock(&acl_cb->lock);
    return ACL_NO_EXISTED;
}
#else
static uint32_t acl_exact_check(acl_zone_e zone, acl_t *acl)
{
    struct list_head *acl_head;
    struct list_head *tmp_list = NULL;
    acl_t *acl_tmp = NULL;
    rtk_filter_field_t *rtl_filter1, *rtl_filter2;
    int i, j;
    int not_mach;

    if((NULL == acl) || (zone >= ACL_ZONE_BUTTON))
    {
        printk(KERN_ERR "%s invalid arg\n", __FUNCTION__);
        return ACL_NO_EXISTED;
    }

    mutex_lock(&acl_cb->lock);
    DBGPRINT("%s  exact at zone %d start\n", __func__, zone);

    acl_head = &acl_cb->acl_head[zone];
    for(tmp_list=acl_head->next; tmp_list!=acl_head;)
    {
        acl_tmp = container_of(tmp_list, acl_t, list);

        if(acl->filter_cnt != acl_tmp->filter_cnt)
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }

        for(rtl_filter1=acl->cfg.fieldHead, i=0;
            (rtl_filter1!=NULL) && (i<RTK_MAX_NUM_OF_FILTER_TYPE);
            rtl_filter1=rtl_filter1->next, i++)
        {
#ifdef DEBUG
            DBGPRINT("rtl_filter1:\n");
            for(k=0; k<sizeof(rtk_filter_field_t); k+=8)
            {
                unsigned char buf[256];
                unsigned char *p = (unsigned char *)rtl_filter1;
                memset(buf, 0, sizeof(buf));
                snprintf(buf, sizeof(buf), "%02x %02x %02x %02x %02x %02x %02x %02x\n",
                    p[k], p[k+1], p[k+2], p[k+3], p[k+4], p[k+5], p[k+6], p[k+7]);
                DBGPRINT(buf);
            }
            if(sizeof(rtk_filter_field_t)%8)
            {
                unsigned char buf[256];
                unsigned char *p = rtl_filter1;
                memset(buf, 0, sizeof(buf));
                for(k=sizeof(rtk_filter_field_t)-(sizeof(rtk_filter_field_t)%8);
                    k<sizeof(rtk_filter_field_t);)
                {
                    snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%02x ",
                        p[k]);
                }
                DBGPRINT("%s\n", buf);
            }
#endif
            for(rtl_filter2=acl_tmp->cfg.fieldHead, j=0;
                (rtl_filter2!=NULL) && (j<RTK_MAX_NUM_OF_FILTER_TYPE);
                rtl_filter2=rtl_filter2->next, j++)
            {
#ifdef DEBUG
                DBGPRINT("rtl_filter2:\n");
                for(k=0; k<sizeof(rtk_filter_field_t); k+=8)
                {
                    unsigned char buf[256];
                    unsigned char *p = rtl_filter2;
                    memset(buf, 0, sizeof(buf));
                    snprintf(buf, sizeof(buf), "%02x %02x %02x %02x %02x %02x %02x %02x\n",
                        p[k], p[k+1], p[k+2], p[k+3], p[k+4], p[k+5], p[k+6], p[k+7]);
                    DBGPRINT(buf);
                }
                if(sizeof(rtk_filter_field_t)%8)
                {
                    unsigned char buf[256];
                    unsigned char *p = rtl_filter2;
                    memset(buf, 0, sizeof(buf));
                    for(k=sizeof(rtk_filter_field_t)-(sizeof(rtk_filter_field_t)%8);
                        k<sizeof(rtk_filter_field_t);)
                    {
                        snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%x ",
                            p[k]);
                    }
                    DBGPRINT("%s\n", buf);
                }
#endif
                if(rtl_filter1->fieldType != rtl_filter2->fieldType)
                {
                    continue;
                }
                if(memcmp(&rtl_filter1->filter_pattern_union, &rtl_filter2->filter_pattern_union,
                    sizeof(rtl_filter1->filter_pattern_union)))
                {
                    continue;
                }
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                break;
            }
            /*Ã»Æ¥ÅäÉÏ*/
            if((rtl_filter2 == NULL) || (j == RTK_MAX_NUM_OF_FILTER_TYPE))
            {
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                break;
            }
        }
        /*ÓÐÌõ¼þÃ»ÓÐÆ¥ÅäÉÏ£¬Ñ°ÕÒÏÂÒ»Ìõ*/
        if((rtl_filter1 != NULL) && (i<RTK_MAX_NUM_OF_FILTER_TYPE))
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }

        if(memcmp(&acl->cfg.activeport, &acl_tmp->cfg.activeport,
            sizeof(rtk_filter_value_t)))
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }

        for(i=0; i<FILTER_ENACT_MAX; i++)
        {
            if(acl->act.actEnable[i] != acl_tmp->act.actEnable[i])
            {
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                break;
            }

            if(acl->act.actEnable[i])
            {
                if(!acl_tmp->act.actEnable[i])
                {
                    DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                    break;
                }

                not_mach = 0;
                switch(i)
                {
                    case FILTER_ENACT_INGRESS_CVLAN_INDEX:
                        {
                            if(acl->act.filterIngressCvlanIdx != acl_tmp->act.filterIngressCvlanIdx)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_INGRESS_CVLAN_VID:
                        {
                            if(acl->act.filterIngressCvlanVid != acl_tmp->act.filterIngressCvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_EGRESS_SVLAN_INDEX:
                        {
                            if(acl->act.filterEgressSvlanIdx != acl_tmp->act.filterEgressSvlanIdx)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_POLICING_0:
                        {
                            if(meter_table[acl->act.filterPolicingIdx[0]].rate !=
                                meter_table[acl_tmp->act.filterPolicingIdx[0]].rate)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_TRAP_CPU:
                        break;
                    case FILTER_ENACT_COPY_CPU:
                        break;
                    case FILTER_ENACT_REDIRECT:
                        {
                            if(acl->act.filterRedirectPortmask != acl_tmp->act.filterRedirectPortmask)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_DROP:
                        break;
                    case FILTER_ENACT_MIRROR:
                        break;
                    case FILTER_ENACT_ADD_DSTPORT:
                        {
                            if(acl->act.filterAddDstPortmask != acl_tmp->act.filterAddDstPortmask)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_PRIORITY:
                        {
                            if(acl->act.filterPriority != acl_tmp->act.filterPriority)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_EGRESS_SVLAN_VID:
                        {
                            if(acl->act.filterEgressSvlanVid != acl_tmp->act.filterEgressSvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                }
                if(not_mach)
                {
                    DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                    break;
                }
            }
        }
        if(i < FILTER_ENACT_MAX)
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }

        mutex_unlock(&acl_cb->lock);
        return ACL_EXISTED;
    }

    DBGPRINT("%s  exact at zone %d end\n", __func__, zone);
    mutex_unlock(&acl_cb->lock);
    return ACL_NO_EXISTED;
}
#endif

static int acl_add(acl_t *acl, u32 rate, acl_zone_e zone)
{
    struct list_head *acl_head;
    rtk_api_ret_t retVal;

    if(zone >= ACL_ZONE_BUTTON)
    {
        printk(KERN_ERR "%s invalid acl zone %d\n", __FUNCTION__, zone);
        return -EINVAL;
    }

    if(ACL_EXISTED == acl_exact_check(zone, acl))
    {
        printk(KERN_WARNING "%s acl is exits\n", __FUNCTION__);
        return 0;
    }
    
    acl_head = &acl_cb->acl_head[zone];

    if((ACL_ZONE_PHY_HIGH == zone) || (ACL_ZONE_PHY_LOW == zone))
    {
        acl_t *acl_tmp = NULL;

        mutex_lock(&acl_cb->lock);
        DBGPRINT("%s zone phy start\n", __func__);
        /*ACL_ZONE_PHY_HIGH和ACL_ZONE_PHY_LOW域的acl规则，从低地址往高地址增长*/
        if(acl_head->next == acl_head)
        {
            if(ACL_ZONE_PHY_HIGH == zone)
            {
                acl->filter_id = PHY_HIGH_ZONE_ACL_FILTER_START;
            }
            else if(ACL_ZONE_PHY_LOW == zone)
            {
                acl->filter_id = PHY_LOW_ZONE_ACL_FILTER_START;
            }
        }
        else
        {
            acl_tmp = container_of(acl_head->prev, acl_t, list);
            acl->filter_id = acl_tmp->filter_id + acl_tmp->filter_cnt;
        }
        DBGPRINT("%s acl->filter_id:%d\n", __func__, acl->filter_id);
        if(((ACL_ZONE_PHY_HIGH == zone) && (acl->filter_id + acl->filter_cnt - 1 > PHY_HIGH_ZONE_ACL_FILTER_END)) ||
            ((ACL_ZONE_PHY_LOW == zone) && (acl->filter_id + acl->filter_cnt - 1 > PHY_LOW_ZONE_ACL_FILTER_END)))
        {
            DBGPRINT("%s zone phy end\n", __func__);
            mutex_unlock(&acl_cb->lock);
            printk(KERN_ERR "invalid filter_id:%d\n", acl->filter_id);
            return -ENOSPC;
        }
        DBGPRINT("%s acl->filter_cnt:%d\n", __func__, acl->filter_cnt);
        
        retVal = acl_set_to_phy(acl, rate);
        if( retVal != RT_ERR_OK )
        {
            DBGPRINT("%s zone phy end\n", __func__);
            mutex_unlock(&acl_cb->lock);
            printk(KERN_ERR "%s: acl_set_to_phy failed\n", __func__);
            return -EIO;
        }
        else
        {
            list_add_tail(&acl->list, acl_head);
            acl_cb->acl_cnt[zone]++;
            acl_cb->filter_used[zone] += acl->filter_cnt;
            DBGPRINT("%s zone phy acl_cnt:%d filter_used:%d\n", __func__, 
                acl_cb->acl_cnt[zone], acl_cb->filter_used[zone]);
        }
        DBGPRINT("%s zone phy end\n", __func__);
        mutex_unlock(&acl_cb->lock);
    }
    else if(ACL_ZONE_DYNAMIC == zone)
    {
        struct list_head *list_prev = NULL, *list_next = NULL;
        acl_t *tmp_acl, *tmp1_acl;
        
        mutex_lock(&acl_cb->lock);
        DBGPRINT("%s zone dynamic start\n", __func__);
        
        /*ACL_ZONE_DYNAMIC域的acl规则，见缝插针*/
        if(acl_head->next == acl_head)
        {
            acl->filter_id = DYNAMIC_ZONE_ACL_FILTER_START;
            list_prev = acl_head;
            list_next = acl_head;
        }
        else
        {
            tmp_acl = container_of(acl_head->next, acl_t, list);
            /*第一个节点的filter_id不是最小值，标志在前面就存在空洞*/
            if((tmp_acl->filter_id > DYNAMIC_ZONE_ACL_FILTER_START) &&
                ((DYNAMIC_ZONE_ACL_FILTER_START+acl->filter_cnt-1) < tmp_acl->filter_id))
            {
                acl->filter_id = DYNAMIC_ZONE_ACL_FILTER_START;
                list_next = acl_head->next;
                list_prev = acl_head;
            }
            else
            {
                /*如果存在两个节点的filter_id不连续，将新节点插入这两个节点之间*/
                for(list_prev=acl_head->next; 
                    list_prev!=acl_head; 
                    list_prev=list_prev->next)
                {
                    if(list_prev->next == acl_head)
                    {
                        break;
                    }
                    tmp_acl = container_of(list_prev, acl_t, list);
                    tmp1_acl= container_of(list_prev->next, acl_t, list);
                    if((tmp1_acl->filter_id - (tmp_acl->filter_id+tmp_acl->filter_cnt)) >= acl->filter_cnt)
                    {
                        acl->filter_id = tmp_acl->filter_id + tmp_acl->filter_cnt;
                        list_next = list_prev->next;
                        break;
                    }
                }
                /*没有满足要求的两个节点，以最后一个节点的filer_id计算*/
                if(list_prev->next == acl_head)
                {
                    tmp_acl = container_of(list_prev, acl_t, list);
                    acl->filter_id = tmp_acl->filter_id + tmp_acl->filter_cnt;
                    list_next = list_prev->next;
                }
            }
        }
        DBGPRINT("%s acl->filter_id:%d\n", __func__, acl->filter_id);
        if((NULL == list_prev) || (NULL == list_next) || (acl->filter_id + acl->filter_cnt - 1 > DYNAMIC_ZONE_ACL_FILTER_END))
        {
            DBGPRINT("%s zone dynamic end\n", __func__);
            mutex_unlock(&acl_cb->lock);
            printk(KERN_ERR "%s list_prev:%p list_next:%p ACL_ZONE_DYNAMIC filter_id:%d\n", __FUNCTION__,
                list_prev, list_next, acl->filter_id);
            return -EFAULT;
        }

        retVal = acl_set_to_phy(acl, rate);
        if( retVal != RT_ERR_OK )
        {
            DBGPRINT("%s zone dynamic end\n", __func__);
            mutex_unlock(&acl_cb->lock);
            printk(KERN_ERR "%s: acl_set_to_phy failed\n", __func__);
            return -EIO;
        }
        else
        {
            list_add(&acl->list, list_prev);
            acl_cb->acl_cnt[ACL_ZONE_DYNAMIC]++;
            acl_cb->filter_used[ACL_ZONE_DYNAMIC] += acl->filter_cnt;
            DBGPRINT("%s zone dynamic acl_cnt:%d filter_used:%d\n", __func__, 
                acl_cb->acl_cnt[ACL_ZONE_DYNAMIC], acl_cb->filter_used[ACL_ZONE_DYNAMIC]);
        }
        DBGPRINT("%s zone dynamic end\n", __func__);
        mutex_unlock(&acl_cb->lock);
    }
    else if(ACL_ZONE_RTP == zone)
    {
        acl_t *acl_tmp = NULL;

        mutex_lock(&acl_cb->lock);
        DBGPRINT("%s zone rtp start\n", __func__);
        /*ACL_ZONE_PHY域的acl规则，从高地址往低地址增长*/
        if(acl_head->next == acl_head)
        {
            acl->filter_id = RTP_ZONE_ACL_FILTER_END - acl->filter_cnt + 1;
        }
        else
        {
            acl_tmp = container_of(acl_head->next, acl_t, list);
            acl->filter_id = acl_tmp->filter_id - acl->filter_cnt;
        }
        DBGPRINT("%s acl->filter_id:%d\n", __func__, acl->filter_id);

        if(acl->filter_id < RTP_ZONE_ACL_FILTER_START)
        {
            DBGPRINT("%s zone rtp end\n", __func__);
            mutex_unlock(&acl_cb->lock);
            printk(KERN_ERR "ACL_ZONE_RTP invalid filter_id:%d\n", acl->filter_id);
            return -ENOSPC;
        }
    
        retVal = acl_set_to_phy(acl, rate);
        if( retVal != RT_ERR_OK )
        {
            DBGPRINT("%s zone rtp end\n", __func__);
            mutex_unlock(&acl_cb->lock);
            printk(KERN_ERR "%s: acl_set_to_phy failed\n", __func__);
            return -EIO;
        }
        else
        {
            list_add(&acl->list, acl_head);
            acl_cb->acl_cnt[ACL_ZONE_RTP]++;
            acl_cb->filter_used[ACL_ZONE_RTP] += acl->filter_cnt;
            DBGPRINT("%s zone rtp acl_cnt:%d filter_used:%d\n", __func__, 
                acl_cb->acl_cnt[ACL_ZONE_RTP], acl_cb->filter_used[ACL_ZONE_RTP]);
        }
        DBGPRINT("%s zone rtp end\n", __func__);
        mutex_unlock(&acl_cb->lock);
    }
    else
    {
        printk(KERN_ERR "%s error zone %d\n", __FUNCTION__, zone);
        return -EINVAL;
    }
    return 0;
}

static acl_t *get_acl_by_index(acl_zone_e zone, int index)
{
    struct list_head *tmp_list = NULL;
    acl_t *acl_tmp = NULL;
    struct list_head *acl_head;
    int tmp_index = -1;

    if(zone >= ACL_ZONE_BUTTON)
    {
        printk(KERN_ERR "%s invalid acl zone %d\n", __FUNCTION__, zone);
        return NULL;
    }
    acl_head = &acl_cb->acl_head[zone];
    if(index >= acl_cb->acl_cnt[zone])
    {
        printk(KERN_ERR "%s invalid acl index %d\n", __FUNCTION__, index);
        return NULL;
    }

    for(tmp_list=acl_head->next; tmp_list!=acl_head; tmp_list=tmp_list->next)
    {
        tmp_index++;
        if(tmp_index == index)
        {
            acl_tmp = container_of(tmp_list, acl_t, list);
            return acl_tmp;
        }
    }

    printk(KERN_ERR "%s zone %d index %d not found\n", __FUNCTION__, zone, index);
    return NULL;
}

static acl_t *acl_del(acl_zone_e zone, int index)
{
    acl_t *acl_tmp = NULL;
    rtk_api_ret_t retVal;

    mutex_lock(&acl_cb->lock);
    DBGPRINT("%s del zone %d start\n", __func__, zone);
    acl_tmp = get_acl_by_index(zone, index);
    if(NULL == acl_tmp)
    {
        DBGPRINT("%s del zone %d end\n", __func__, zone);
        mutex_unlock(&acl_cb->lock);
        printk(KERN_ERR "%s get_acl_by_index error\n", __func__);
        return NULL;
    }
    retVal = acl_del_on_phy(acl_tmp);
    if(retVal != RT_ERR_OK)
    {
        DBGPRINT("%s del zone %d end\n", __func__, zone);
        mutex_unlock(&acl_cb->lock);
        printk(KERN_ERR "%s: acl_del_on_phy failed\n", __func__);
        return NULL;
    }
    else
    {
        list_del(&acl_tmp->list);
        acl_cb->acl_cnt[zone]--;
        acl_cb->filter_used[zone] -= acl_tmp->filter_cnt;
        DBGPRINT("%s zone %d acl_cnt:%d filter_used:%d\n", __func__, zone,
            acl_cb->acl_cnt[zone], acl_cb->filter_used[zone]);
    }
    DBGPRINT("%s del zone %d end\n", __func__, zone);
    mutex_unlock(&acl_cb->lock);

    return acl_tmp;
}


#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC300USER) || \
	defined(PRODUCT_AG) || defined(PRODUCT_SBC1000USER) || defined(PRODUCT_UC200) || \
	defined(PRODUCT_SBC1000MAIN)
static void acl_del_fuzzy(acl_zone_e zone, acl_t *acl)
{
    struct list_head *acl_head;
    struct list_head *tmp_list = NULL;
    acl_t *acl_tmp = NULL;
    rtk_filter_field_t *rtl_filter1, *rtl_filter2;
    int i, j;
    rtk_api_ret_t retVal;
    int not_mach;

    if((NULL == acl) || (zone >= ACL_ZONE_BUTTON))
    {
        printk(KERN_ERR "%s invalid arg\n", __FUNCTION__);
        return;
    }

    mutex_lock(&acl_cb->lock);   
    DBGPRINT("%s del fuzzy at zone %d start\n", __func__, zone);

    acl_head = &acl_cb->acl_head[zone];
    for(tmp_list=acl_head->next; tmp_list!=acl_head;)
    {
        acl_tmp = container_of(tmp_list, acl_t, list);

        for(rtl_filter1=acl->cfg.fieldHead, i=0; 
            (rtl_filter1!=NULL) && (i<RTK_MAX_NUM_OF_FILTER_TYPE); 
            rtl_filter1=rtl_filter1->next, i++)
        {
            for(rtl_filter2=acl_tmp->cfg.fieldHead, j=0; 
                (rtl_filter2!=NULL) && (j<RTK_MAX_NUM_OF_FILTER_TYPE); 
                rtl_filter2=rtl_filter2->next, j++)
            {
                if(rtl_filter1->fieldType != rtl_filter2->fieldType)
                {
                    continue;
                }
                if(memcmp(&rtl_filter1->filter_pattern_union, &rtl_filter2->filter_pattern_union, 
                    sizeof(rtl_filter1->filter_pattern_union)))
                {
                    continue;
                }
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                break;
            }
            /*没匹配上*/
            if((rtl_filter2 == NULL) || (j == RTK_MAX_NUM_OF_FILTER_TYPE))
            {
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                break;
            }
        }
        /*有条件没有匹配上，寻找下一条*/
        if((rtl_filter1 != NULL) && (i<RTK_MAX_NUM_OF_FILTER_TYPE))
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }

        if(acl->cfg.activeport.mask.bits[0] != 0)
        {
            if(memcmp(&acl->cfg.activeport, &acl_tmp->cfg.activeport, 
                sizeof(rtk_filter_activeport_t)))
            {
                tmp_list = tmp_list->next;
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                continue;
            }
        }

        for(i=0; i<FILTER_ENACT_END; i++)
        {
            if(acl->act.actEnable[i])
            {
                if(!acl_tmp->act.actEnable[i])
                {
                    DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                    break;
                }

                not_mach = 0;
                switch(i)
                {
                    case FILTER_ENACT_CVLAN_INGRESS:
                        {
                            if(acl->act.filterCvlanVid != acl_tmp->act.filterCvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_CVLAN_EGRESS:
                        {
                            if(acl->act.filterCvlanVid != acl_tmp->act.filterCvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_CVLAN_SVID:
                        break;
                    case FILTER_ENACT_POLICING_1:
                        {
                            if(meter_table[acl->act.filterPolicingIdx[1]].rate !=
                                meter_table[acl_tmp->act.filterPolicingIdx[1]].rate)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_SVLAN_INGRESS:
                        {
                            if(acl->act.filterSvlanVid != acl_tmp->act.filterSvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_SVLAN_EGRESS:
                        {
                            if(acl->act.filterSvlanVid != acl_tmp->act.filterSvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_SVLAN_CVID:
                        break;
                    case FILTER_ENACT_POLICING_2:
                        {
                            if(meter_table[acl->act.filterPolicingIdx[2]].rate !=
                                meter_table[acl_tmp->act.filterPolicingIdx[2]].rate)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_POLICING_0:
                        {
                            if(meter_table[acl->act.filterPolicingIdx[0]].rate !=
                                meter_table[acl_tmp->act.filterPolicingIdx[0]].rate)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_COPY_CPU:
                        break;
                    case FILTER_ENACT_DROP:
                        break;
                    case FILTER_ENACT_ADD_DSTPORT:
                        {
                            if(memcmp(&acl->act.filterPortmask, &acl_tmp->act.filterPortmask,
                                sizeof(rtk_portmask_t)))
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_REDIRECT:
                        {
                            if(memcmp(&acl->act.filterPortmask, &acl_tmp->act.filterPortmask,
                                sizeof(rtk_portmask_t)))
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_MIRROR:
                        {
                            if(memcmp(&acl->act.filterPortmask, &acl_tmp->act.filterPortmask,
                                sizeof(rtk_portmask_t)))
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_TRAP_CPU:
                        break;
                    case FILTER_ENACT_ISOLATION:
                        {
                            if(memcmp(&acl->act.filterPortmask, &acl_tmp->act.filterPortmask,
                                sizeof(rtk_portmask_t)))
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_PRIORITY:
                        {
                            if(acl->act.filterPriority != acl_tmp->act.filterPriority)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_DSCP_REMARK:
                        {
                            if(acl->act.filterPriority != acl_tmp->act.filterPriority)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_1P_REMARK:
                        {
                            if(acl->act.filterPriority != acl_tmp->act.filterPriority)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_POLICING_3:
                        {
                            if(acl->act.filterPriority != acl_tmp->act.filterPriority)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                            if(meter_table[acl->act.filterPolicingIdx[3]].rate !=
                                meter_table[acl_tmp->act.filterPolicingIdx[3]].rate)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_INTERRUPT:
                        break;
                    case FILTER_ENACT_GPO:
                        {
                            if(acl->act.filterPin != acl_tmp->act.filterPin)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_EGRESSCTAG_UNTAG:
                        break;
                    case FILTER_ENACT_EGRESSCTAG_TAG:
                        break;
                    case FILTER_ENACT_EGRESSCTAG_KEEP:
                        break;
                    case FILTER_ENACT_EGRESSCTAG_KEEPAND1PRMK:
                        break;
                }
                if(not_mach)
                {
                    DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                    break;
                }
            }
        }
        if(i < FILTER_ENACT_END)
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }
		       
        retVal = acl_del_on_phy(acl_tmp);
        if(retVal != RT_ERR_OK)
        {
            DBGPRINT("%s del zone %d end\n", __func__, zone);
            mutex_unlock(&acl_cb->lock);
            printk(KERN_ERR "%s: acl_del_on_phy failed\n", __func__);
            return;
        }
        else
        {
            tmp_list = tmp_list->next;
            list_del(&acl_tmp->list);
            acl_cb->acl_cnt[zone]--;
            acl_cb->filter_used[zone] -= acl_tmp->filter_cnt;
            DBGPRINT("%s zone %d acl_cnt:%d filter_used:%d\n", __func__, zone,
                acl_cb->acl_cnt[zone], acl_cb->filter_used[zone]);
            free_acl(acl_tmp);
        }
    }

    DBGPRINT("%s del fuzzy at zone %d end\n", __func__, zone);
    mutex_unlock(&acl_cb->lock);
}

static void acl_del_exact(acl_zone_e zone, acl_t *acl)
{
    struct list_head *acl_head;
    struct list_head *tmp_list = NULL;
    acl_t *acl_tmp = NULL;
    rtk_filter_field_t *rtl_filter1, *rtl_filter2;
    int i, j;
    rtk_api_ret_t retVal;
    int not_mach;

    if((NULL == acl) || (zone >= ACL_ZONE_BUTTON))
    {
        printk(KERN_ERR "%s invalid arg\n", __FUNCTION__);
        return;
    }

    mutex_lock(&acl_cb->lock);   
    DBGPRINT("%s del exact at zone %d start\n", __func__, zone);

    acl_head = &acl_cb->acl_head[zone];
    for(tmp_list=acl_head->next; tmp_list!=acl_head;)
    {
        acl_tmp = container_of(tmp_list, acl_t, list);
        
        if(acl->filter_cnt != acl_tmp->filter_cnt)
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }

        for(rtl_filter1=acl->cfg.fieldHead, i=0; 
            (rtl_filter1!=NULL) && (i<RTK_MAX_NUM_OF_FILTER_TYPE); 
            rtl_filter1=rtl_filter1->next, i++)
        {
            for(rtl_filter2=acl_tmp->cfg.fieldHead, j=0; 
                (rtl_filter2!=NULL) && (j<RTK_MAX_NUM_OF_FILTER_TYPE); 
                rtl_filter2=rtl_filter2->next, j++)
            {
                if(rtl_filter1->fieldType != rtl_filter2->fieldType)
                {
                    continue;
                }
                if(memcmp(&rtl_filter1->filter_pattern_union, &rtl_filter2->filter_pattern_union, 
                    sizeof(rtl_filter1->filter_pattern_union)))
                {
                    continue;
                }
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                break;
            }
            /*没匹配上*/
            if((rtl_filter2 == NULL) || (j == RTK_MAX_NUM_OF_FILTER_TYPE))
            {
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                break;
            }
        }
        /*有条件没有匹配上，寻找下一条*/
        if((rtl_filter1 != NULL) && (i<RTK_MAX_NUM_OF_FILTER_TYPE))
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }

        if(memcmp(&acl->cfg.activeport, &acl_tmp->cfg.activeport, 
            sizeof(rtk_filter_activeport_t)))
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }

        for(i=0; i<FILTER_ENACT_END; i++)
        {
            if(acl->act.actEnable[i] != acl_tmp->act.actEnable[i])
            {
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                break;
            }
            
            if(acl->act.actEnable[i])
            {
                if(!acl_tmp->act.actEnable[i])
                {
                    DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                    break;
                }

                not_mach = 0;
                switch(i)
                {
                    case FILTER_ENACT_CVLAN_INGRESS:
                        {
                            if(acl->act.filterCvlanVid != acl_tmp->act.filterCvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_CVLAN_EGRESS:
                        {
                            if(acl->act.filterCvlanVid != acl_tmp->act.filterCvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_CVLAN_SVID:
                        break;
                    case FILTER_ENACT_POLICING_1:
                        {
                            if(meter_table[acl->act.filterPolicingIdx[1]].rate !=
                                meter_table[acl_tmp->act.filterPolicingIdx[1]].rate)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_SVLAN_INGRESS:
                        {
                            if(acl->act.filterSvlanVid != acl_tmp->act.filterSvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_SVLAN_EGRESS:
                        {
                            if(acl->act.filterSvlanVid != acl_tmp->act.filterSvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_SVLAN_CVID:
                        break;
                    case FILTER_ENACT_POLICING_2:
                        {
                            if(meter_table[acl->act.filterPolicingIdx[2]].rate !=
                                meter_table[acl_tmp->act.filterPolicingIdx[2]].rate)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_POLICING_0:
                        {
                            if(meter_table[acl->act.filterPolicingIdx[0]].rate !=
                                meter_table[acl_tmp->act.filterPolicingIdx[0]].rate)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_COPY_CPU:
                        break;
                    case FILTER_ENACT_DROP:
                        break;
                    case FILTER_ENACT_ADD_DSTPORT:
                        {
                            if(memcmp(&acl->act.filterPortmask, &acl_tmp->act.filterPortmask,
                                sizeof(rtk_portmask_t)))
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_REDIRECT:
                        {
                            if(memcmp(&acl->act.filterPortmask, &acl_tmp->act.filterPortmask,
                                sizeof(rtk_portmask_t)))
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_MIRROR:
                        {
                            if(memcmp(&acl->act.filterPortmask, &acl_tmp->act.filterPortmask,
                                sizeof(rtk_portmask_t)))
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_TRAP_CPU:
                        break;
                    case FILTER_ENACT_ISOLATION:
                        {
                            if(memcmp(&acl->act.filterPortmask, &acl_tmp->act.filterPortmask,
                                sizeof(rtk_portmask_t)))
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_PRIORITY:
                        {
                            if(acl->act.filterPriority != acl_tmp->act.filterPriority)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_DSCP_REMARK:
                        {
                            if(acl->act.filterPriority != acl_tmp->act.filterPriority)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_1P_REMARK:
                        {
                            if(acl->act.filterPriority != acl_tmp->act.filterPriority)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_POLICING_3:
                        {
                            if(acl->act.filterPriority != acl_tmp->act.filterPriority)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                            if(meter_table[acl->act.filterPolicingIdx[3]].rate !=
                                meter_table[acl_tmp->act.filterPolicingIdx[3]].rate)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_INTERRUPT:
                        break;
                    case FILTER_ENACT_GPO:
                        {
                            if(acl->act.filterPin != acl_tmp->act.filterPin)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_EGRESSCTAG_UNTAG:
                        break;
                    case FILTER_ENACT_EGRESSCTAG_TAG:
                        break;
                    case FILTER_ENACT_EGRESSCTAG_KEEP:
                        break;
                    case FILTER_ENACT_EGRESSCTAG_KEEPAND1PRMK:
                        break;
                }
                if(not_mach)
                {
                    DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                    break;
                }
            }
        }
        if(i < FILTER_ENACT_END)
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }
        
        retVal = acl_del_on_phy(acl_tmp);
        if(retVal != RT_ERR_OK)
        {
            DBGPRINT("%s del zone %d end\n", __func__, zone);
            mutex_unlock(&acl_cb->lock);
            printk(KERN_ERR "%s: acl_del_on_phy failed\n", __func__);
            return;
        }
        else
        {
            tmp_list = tmp_list->next;
            list_del(&acl_tmp->list);
            acl_cb->acl_cnt[zone]--;
            acl_cb->filter_used[zone] -= acl_tmp->filter_cnt;
            DBGPRINT("%s zone %d acl_cnt:%d filter_used:%d\n", __func__, zone,
                acl_cb->acl_cnt[zone], acl_cb->filter_used[zone]);
            free_acl(acl_tmp);
        }
    }

    DBGPRINT("%s del exact at zone %d end\n", __func__, zone);
    mutex_unlock(&acl_cb->lock);
}


#else
static void acl_del_fuzzy(acl_zone_e zone, acl_t *acl)
{
    struct list_head *acl_head;
    struct list_head *tmp_list = NULL;
    acl_t *acl_tmp = NULL;
    rtk_filter_field_t *rtl_filter1, *rtl_filter2;
    int i, j;
    rtk_api_ret_t retVal;
    int not_mach;
    
    if((NULL == acl) || (zone >= ACL_ZONE_BUTTON))
    {
        printk(KERN_ERR "%s invalid arg\n", __FUNCTION__);
        return;
    }

    mutex_lock(&acl_cb->lock);   
    DBGPRINT("%s del fuzzy at zone %d start\n", __func__, zone);
    
    acl_head = &acl_cb->acl_head[zone];
    for(tmp_list=acl_head->next; tmp_list!=acl_head;)
    {
        acl_tmp = container_of(tmp_list, acl_t, list);

        for(rtl_filter1=acl->cfg.fieldHead, i=0; 
            (rtl_filter1!=NULL) && (i<RTK_MAX_NUM_OF_FILTER_TYPE); 
            rtl_filter1=rtl_filter1->next, i++)
        {
            for(rtl_filter2=acl_tmp->cfg.fieldHead, j=0; 
                (rtl_filter2!=NULL) && (j<RTK_MAX_NUM_OF_FILTER_TYPE); 
                rtl_filter2=rtl_filter2->next, j++)
            {
                if(rtl_filter1->fieldType != rtl_filter2->fieldType)
                {
                    continue;
                }
                if(memcmp(&rtl_filter1->filter_pattern_union, &rtl_filter2->filter_pattern_union, 
                    sizeof(rtl_filter1->filter_pattern_union)))
                {
                    continue;
                }
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                break;
            }
            /*没匹配上*/
            if((rtl_filter2 == NULL) || (j == RTK_MAX_NUM_OF_FILTER_TYPE))
            {
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                break;
            }
        }
        /*有条件没有匹配上，寻找下一条*/
        if((rtl_filter1 != NULL) && (i<RTK_MAX_NUM_OF_FILTER_TYPE))
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }

        if(acl->cfg.activeport.dataType != FILTER_FIELD_DATA_MAX)
        {
            if(memcmp(&acl->cfg.activeport, &acl_tmp->cfg.activeport, 
                sizeof(rtk_filter_value_t)))
            {
                tmp_list = tmp_list->next;
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                continue;
            }
        }

        for(i=0; i<FILTER_ENACT_MAX; i++)
        {
            if(acl->act.actEnable[i])
            {
                if(!acl_tmp->act.actEnable[i])
                {
                    DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                    break;
                }

                not_mach = 0;
                switch(i)
                {
                    case FILTER_ENACT_INGRESS_CVLAN_INDEX:
                        {
                            if(acl->act.filterIngressCvlanIdx != acl_tmp->act.filterIngressCvlanIdx)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_INGRESS_CVLAN_VID:
                        {
                            if(acl->act.filterIngressCvlanVid != acl_tmp->act.filterIngressCvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_EGRESS_SVLAN_INDEX:
                        {
                            if(acl->act.filterEgressSvlanIdx != acl_tmp->act.filterEgressSvlanIdx)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_POLICING_0:
                        {
                            if(meter_table[acl->act.filterPolicingIdx[0]].rate !=
                                meter_table[acl_tmp->act.filterPolicingIdx[0]].rate)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_TRAP_CPU:
                        break;
                    case FILTER_ENACT_COPY_CPU:
                        break;
                    case FILTER_ENACT_REDIRECT:
                        {
                            if(acl->act.filterRedirectPortmask != acl_tmp->act.filterRedirectPortmask)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_DROP:
                        break;
                    case FILTER_ENACT_MIRROR:
                        break;
                    case FILTER_ENACT_ADD_DSTPORT:
                        {
                            if(acl->act.filterAddDstPortmask != acl_tmp->act.filterAddDstPortmask)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_PRIORITY:
                        {
                            if(acl->act.filterPriority != acl_tmp->act.filterPriority)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_EGRESS_SVLAN_VID:
                        {
                            if(acl->act.filterEgressSvlanVid != acl_tmp->act.filterEgressSvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                }
                if(not_mach)
                {
                    DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                    break;
                }
            }
        }
        if(i < FILTER_ENACT_MAX)
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }

        retVal = acl_del_on_phy(acl_tmp);
        if(retVal != RT_ERR_OK)
        {
            DBGPRINT("%s del zone %d end\n", __func__, zone);
            mutex_unlock(&acl_cb->lock);
            printk(KERN_ERR "%s: acl_del_on_phy failed\n", __func__);
            return;
        }
        else
        {
            tmp_list = tmp_list->next;
            list_del(&acl_tmp->list);
            acl_cb->acl_cnt[zone]--;
            acl_cb->filter_used[zone] -= acl_tmp->filter_cnt;
            DBGPRINT("%s zone %d acl_cnt:%d filter_used:%d\n", __func__, zone,
                acl_cb->acl_cnt[zone], acl_cb->filter_used[zone]);
            free_acl(acl_tmp);
        }
    }

    DBGPRINT("%s del fuzzy at zone %d end\n", __func__, zone);
    mutex_unlock(&acl_cb->lock);
}

static void acl_del_exact(acl_zone_e zone, acl_t *acl)
{
    struct list_head *acl_head;
    struct list_head *tmp_list = NULL;
    acl_t *acl_tmp = NULL;
    rtk_filter_field_t *rtl_filter1, *rtl_filter2;
    int i, j, k;
    rtk_api_ret_t retVal;
    int not_mach;
    
    if((NULL == acl) || (zone >= ACL_ZONE_BUTTON))
    {
        printk(KERN_ERR "%s invalid arg\n", __FUNCTION__);
        return;
    }

    mutex_lock(&acl_cb->lock);   
    DBGPRINT("%s del exact at zone %d start\n", __func__, zone);
    
    acl_head = &acl_cb->acl_head[zone];
    for(tmp_list=acl_head->next; tmp_list!=acl_head;)
    {
        acl_tmp = container_of(tmp_list, acl_t, list);

        if(acl->filter_cnt != acl_tmp->filter_cnt)
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }
        
        for(rtl_filter1=acl->cfg.fieldHead, i=0; 
            (rtl_filter1!=NULL) && (i<RTK_MAX_NUM_OF_FILTER_TYPE); 
            rtl_filter1=rtl_filter1->next, i++)
        {
#ifdef DEBUG
            DBGPRINT("rtl_filter1:\n");
            for(k=0; k<sizeof(rtk_filter_field_t); k+=8)
            {
                unsigned char buf[256];
                unsigned char *p = (unsigned char *)rtl_filter1;
                memset(buf, 0, sizeof(buf));
                snprintf(buf, sizeof(buf), "%02x %02x %02x %02x %02x %02x %02x %02x\n",
                    p[k], p[k+1], p[k+2], p[k+3], p[k+4], p[k+5], p[k+6], p[k+7]);
                DBGPRINT(buf);
            }
            if(sizeof(rtk_filter_field_t)%8)
            {
                unsigned char buf[256];
                unsigned char *p = rtl_filter1;
                memset(buf, 0, sizeof(buf));
                for(k=sizeof(rtk_filter_field_t)-(sizeof(rtk_filter_field_t)%8);
                    k<sizeof(rtk_filter_field_t);)
                {
                    snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%02x ",
                        p[k]);
                }
                DBGPRINT("%s\n", buf);
            }
#endif
            for(rtl_filter2=acl_tmp->cfg.fieldHead, j=0; 
                (rtl_filter2!=NULL) && (j<RTK_MAX_NUM_OF_FILTER_TYPE); 
                rtl_filter2=rtl_filter2->next, j++)
            {
#ifdef DEBUG
                DBGPRINT("rtl_filter2:\n");
                for(k=0; k<sizeof(rtk_filter_field_t); k+=8)
                {
                    unsigned char buf[256];
                    unsigned char *p = rtl_filter2;
                    memset(buf, 0, sizeof(buf));
                    snprintf(buf, sizeof(buf), "%02x %02x %02x %02x %02x %02x %02x %02x\n",
                        p[k], p[k+1], p[k+2], p[k+3], p[k+4], p[k+5], p[k+6], p[k+7]);
                    DBGPRINT(buf);
                }
                if(sizeof(rtk_filter_field_t)%8)
                {
                    unsigned char buf[256];
                    unsigned char *p = rtl_filter2;
                    memset(buf, 0, sizeof(buf));
                    for(k=sizeof(rtk_filter_field_t)-(sizeof(rtk_filter_field_t)%8);
                        k<sizeof(rtk_filter_field_t);)
                    {
                        snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%x ",
                            p[k]);
                    }
                    DBGPRINT("%s\n", buf);
                }
#endif
                if(rtl_filter1->fieldType != rtl_filter2->fieldType)
                {
                    continue;
                }
                if(memcmp(&rtl_filter1->filter_pattern_union, &rtl_filter2->filter_pattern_union, 
                    sizeof(rtl_filter1->filter_pattern_union)))
                {
                    continue;
                }
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                break;
            }
            /*没匹配上*/
            if((rtl_filter2 == NULL) || (j == RTK_MAX_NUM_OF_FILTER_TYPE))
            {
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                break;
            }
        }
        /*有条件没有匹配上，寻找下一条*/
        if((rtl_filter1 != NULL) && (i<RTK_MAX_NUM_OF_FILTER_TYPE))
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }

        if(memcmp(&acl->cfg.activeport, &acl_tmp->cfg.activeport, 
            sizeof(rtk_filter_value_t)))
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }

        for(i=0; i<FILTER_ENACT_MAX; i++)
        {
            if(acl->act.actEnable[i] != acl_tmp->act.actEnable[i])
            {
                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                break;
            }
            
            if(acl->act.actEnable[i])
            {
                if(!acl_tmp->act.actEnable[i])
                {
                    DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                    break;
                }

                not_mach = 0;
                switch(i)
                {
                    case FILTER_ENACT_INGRESS_CVLAN_INDEX:
                        {
                            if(acl->act.filterIngressCvlanIdx != acl_tmp->act.filterIngressCvlanIdx)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_INGRESS_CVLAN_VID:
                        {
                            if(acl->act.filterIngressCvlanVid != acl_tmp->act.filterIngressCvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_EGRESS_SVLAN_INDEX:
                        {
                            if(acl->act.filterEgressSvlanIdx != acl_tmp->act.filterEgressSvlanIdx)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_POLICING_0:
                        {
                            if(meter_table[acl->act.filterPolicingIdx[0]].rate !=
                                meter_table[acl_tmp->act.filterPolicingIdx[0]].rate)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_TRAP_CPU:
                        break;
                    case FILTER_ENACT_COPY_CPU:
                        break;
                    case FILTER_ENACT_REDIRECT:
                        {
                            if(acl->act.filterRedirectPortmask != acl_tmp->act.filterRedirectPortmask)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_DROP:
                        break;
                    case FILTER_ENACT_MIRROR:
                        break;
                    case FILTER_ENACT_ADD_DSTPORT:
                        {
                            if(acl->act.filterAddDstPortmask != acl_tmp->act.filterAddDstPortmask)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_PRIORITY:
                        {
                            if(acl->act.filterPriority != acl_tmp->act.filterPriority)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                    case FILTER_ENACT_EGRESS_SVLAN_VID:
                        {
                            if(acl->act.filterEgressSvlanVid != acl_tmp->act.filterEgressSvlanVid)
                            {
                                not_mach = 1;
                                DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                            }
                        }
                        break;
                }
                if(not_mach)
                {
                    DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
                    break;
                }
            }
        }
        if(i < FILTER_ENACT_MAX)
        {
            tmp_list = tmp_list->next;
            DBGPRINT("%s %d\n", __FUNCTION__, __LINE__);
            continue;
        }

        retVal = acl_del_on_phy(acl_tmp);
        if(retVal != RT_ERR_OK)
        {
            DBGPRINT("%s del zone %d end\n", __func__, zone);
            mutex_unlock(&acl_cb->lock);
            printk(KERN_ERR "%s: acl_del_on_phy failed\n", __func__);
            return;
        }
        else
        {
            tmp_list = tmp_list->next;
            list_del(&acl_tmp->list);
            acl_cb->acl_cnt[zone]--;
            acl_cb->filter_used[zone] -= acl_tmp->filter_cnt;
            DBGPRINT("%s zone %d acl_cnt:%d filter_used:%d\n", __func__, zone,
                acl_cb->acl_cnt[zone], acl_cb->filter_used[zone]);
            free_acl(acl_tmp);
        }
    }

    DBGPRINT("%s del exact at zone %d end\n", __func__, zone);
    mutex_unlock(&acl_cb->lock);
}
#endif
static int acl_mod(int index, acl_t *new_acl, u32 rate, acl_zone_e zone)
{
    acl_t *old_acl = NULL, *next_acl = NULL;
    rtk_api_ret_t retVal;
    int ret;
    struct list_head *list_prev = NULL;

    if(zone >= ACL_ZONE_BUTTON)
    {
        printk(KERN_ERR "%s invalid acl zone %d\n", __FUNCTION__, zone);
        return -EINVAL;
    }
    mutex_lock(&acl_cb->lock);
    DBGPRINT("%s mod zone %d start\n", __func__, zone);
    old_acl = get_acl_by_index(zone, index);
    if(NULL == old_acl)
    {
        DBGPRINT("%s mod zone %d end\n", __func__, zone);
        mutex_unlock(&acl_cb->lock);
        return -EINVAL;
    }
    if(old_acl->list.next != &acl_cb->acl_head[zone])
    {
        next_acl = container_of(old_acl->list.next, acl_t, list);
        if(new_acl->filter_cnt > (next_acl->filter_id - old_acl->filter_id))
        {
            DBGPRINT("%s mod zone %d end\n", __func__, zone);
            mutex_unlock(&acl_cb->lock);
            printk(KERN_ERR "new_acl->filter_cnt %d old_acl->filter_cnt %d\n",
                new_acl->filter_cnt, old_acl->filter_cnt);
            return -EINVAL;
        }
    }
    else
    {
        if(new_acl->filter_cnt > (acl_cb->max_acl_cnt[zone] - old_acl->filter_id))
        {
            DBGPRINT("%s mod zone %d end\n", __func__, zone);
            mutex_unlock(&acl_cb->lock);
            printk(KERN_ERR "new_acl->filter_cnt %d old_acl->filter_cnt %d\n",
                new_acl->filter_cnt, old_acl->filter_cnt);
            return -EINVAL;
        }
    }
    DBGPRINT("new_acl->filter_cnt %d old_acl->filter_cnt %d\n",
        new_acl->filter_cnt, old_acl->filter_cnt);

    new_acl->filter_id = old_acl->filter_id;
    
    retVal = acl_del_on_phy(old_acl);
    if(retVal != RT_ERR_OK)
    {
        printk(KERN_ERR "%s acl_del_on_phy error\n", __FUNCTION__);
        ret = -EIO;
        goto fail0;
    }
    
    retVal = acl_set_to_phy(new_acl, rate);
    if(retVal != RT_ERR_OK)
    {
        printk(KERN_ERR "%s acl_set_to_phy error\n", __FUNCTION__);
        ret = -EIO;
        goto fail0;
    }

    list_prev = old_acl->list.prev;
    /*占用的acl条目数，以实际硬件占用数为准*/
    //new_acl->filter_cnt = old_acl->filter_cnt;
    acl_cb->filter_used[zone] += new_acl->filter_cnt - old_acl->filter_cnt;
    list_del(&old_acl->list);
    list_add(&new_acl->list, list_prev);
    mutex_unlock(&acl_cb->lock);

    free_acl(old_acl);
    return 0;
fail0:
    mutex_unlock(&acl_cb->lock);
    return ret;
}

static void acl_del_free(acl_zone_e zone)
{
    acl_t *acl = NULL;
    struct list_head *tmp_list = NULL, *list_head = NULL;
    struct list_head *tmp_list0 = NULL;
    int retVal;
    
    if(zone >= ACL_ZONE_BUTTON)
    {
        printk(KERN_ERR "%s invalid zone %d\n", __FUNCTION__, zone);
    }

    mutex_lock(&acl_cb->lock);
    DBGPRINT("%s del zone %d acl start\n", __func__, zone);
    list_head = &acl_cb->acl_head[zone];
    for(tmp_list=list_head->next; tmp_list!=list_head; )
    {
        tmp_list0 = tmp_list->next;
        acl = container_of(tmp_list, acl_t, list);
        retVal = acl_del_on_phy(acl);
        if(retVal != RT_ERR_OK)
        {
            printk(KERN_ERR "%s acl_del_on_phy error\n", __FUNCTION__);
            goto out;
        }
        list_del(&acl->list);
        free_acl(acl);
        tmp_list = tmp_list0;
    }
    acl_cb->acl_cnt[zone] = 0;
    acl_cb->filter_used[zone] = 0;
    
out:
    DBGPRINT("%s del zone %d acl end\n", __func__, zone);
    mutex_unlock(&acl_cb->lock);
    return;
}

static void acl_del_free_all(void)
{
    acl_t *acl = NULL;
    struct list_head *tmp_list = NULL, *list_head = NULL;
    struct list_head *tmp_list0 = NULL;
    int i, retVal;

    mutex_lock(&acl_cb->lock);
    DBGPRINT("%s del all acl start\n", __func__);
    for(i=0; i<ACL_ZONE_BUTTON; i++)
    {
        DBGPRINT("%s zone %d\n", __func__, i);
        list_head = &acl_cb->acl_head[i];
        for(tmp_list=list_head->next; tmp_list!=list_head; )
        {
            tmp_list0 = tmp_list->next;
            acl = container_of(tmp_list, acl_t, list);
            retVal = acl_del_on_phy(acl);
            if(retVal != RT_ERR_OK)
            {
                printk(KERN_ERR "%s acl_del_on_phy error\n", __FUNCTION__);
                goto out;
            }
            list_del(&acl->list);
            free_acl(acl);
            tmp_list = tmp_list0;
        }
        acl_cb->acl_cnt[i] = 0;
        acl_cb->filter_used[i] = 0;
    }
out:
    DBGPRINT("%s del all acl end\n", __func__);
    mutex_unlock(&acl_cb->lock);
    return;
}

static int acl_put_all(multi_user_acl_trans_t *multi_user_acl)
{
    int max_acl_cnt, zone, i = 0;
    acl_t *acl = NULL;
    struct list_head *tmp_list = NULL, *acl_head = NULL;

    if(NULL == multi_user_acl)
    {
        printk(KERN_ERR "%s invalid arg\n", __FUNCTION__);
        return -EINVAL;
    }

    if(get_user(max_acl_cnt, &multi_user_acl->cnt))
    {
        printk(KERN_ERR "%s get_user multi_user_acl->cnt error\n", __FUNCTION__);
        return -EFAULT;
    }
    DBGPRINT("%s max_acl_cnt:%d\n", __func__, max_acl_cnt);
    if(get_user(zone, &multi_user_acl->acl_region))
    {
        printk(KERN_ERR "%s get_user multi_user_acl->acl_region error\n", __FUNCTION__);
        return -EFAULT;
    }
    DBGPRINT("%s zone:%d\n", __func__, zone);

    if(max_acl_cnt <= 0)
    {
        printk(KERN_ERR "%s invalid max_acl_cnt:%d\n", __FUNCTION__, max_acl_cnt);
        return -EFAULT;
    }
    if(zone < 0 || zone >= ACL_ZONE_BUTTON)
    {
        printk(KERN_ERR "%s invalid zone:%d\n", __FUNCTION__, zone);
        return -EFAULT;
    }

    multi_user_acl->cnt = 0;
    mutex_lock(&acl_cb->lock);
    DBGPRINT("%s acl put start\n", __func__);
    acl_head = &acl_cb->acl_head[zone];
    for(tmp_list=acl_head->next; 
        (tmp_list!=acl_head) && (i<max_acl_cnt); 
        tmp_list=tmp_list->next, i++)
    {
        acl = container_of(tmp_list, acl_t, list);
        if(acl_put(&multi_user_acl->user_acl[i], acl) < 0)
        {
            printk(KERN_ERR "acl_put %d error\n", i);
            break;
        }
    }
    DBGPRINT("%s acl put end\n", __func__);
    mutex_unlock(&acl_cb->lock);
    
    multi_user_acl->cnt = i;
    return 0;
}
static int acl_mirror_getstatus(rtk_mirror_port_t *user_mirrorport)
{
    rtk_port_t          rtkport ;
    rtk_portmask_t      rx_portmask;
    rtk_portmask_t      tx_portmask;
    int                 retVal = RT_ERR_OK;
    int source_port = 0, i;
    rtk_enable_t mirRx, mirTx;
    

    retVal = rtk_mirror_portBased_get(&rtkport, &rx_portmask, &tx_portmask);
    if( retVal != RT_ERR_OK )
    {
    	printk("  %s: rtk_mirror_portBased_set failed! retVal=0x%x\n", __func__, retVal);
        return -1;
    }
    else
    {
        printk("  %s:  rtkport:%u rx_portmask:%u tx_portmask:%u\n", __func__, rtkport, rx_portmask.bits[0], tx_portmask.bits[0]);    
    }
    user_mirrorport->mirror_dst_port = rtkport;

   for (i=0;i< RTK_MAX_NUM_OF_PORT;i++)
   {
        if (tx_portmask.bits[0]&(1<<i))
        {
            source_port = i;
            break;
        }

        if (rx_portmask.bits[0]&(1<<i))
        {
            source_port = i;
            break;
        }
    }
    user_mirrorport->mirror_src_port = source_port;
    if (rx_portmask.bits[0])
        mirRx = ENABLED;
    else
        mirRx = DISABLED;
           
    if (tx_portmask.bits[0])
        mirTx = ENABLED;
    else
        mirTx = DISABLED;
        
     if(mirRx && mirTx)
     {
         user_mirrorport->mirror_direct = MIRROR_DIREXT_BOTH;
     }
     else if(mirRx && (!mirTx))
     {
         user_mirrorport->mirror_direct = MIRROR_DIREXT_RX;
     }
     else if(mirTx && (!mirRx))
     {
         user_mirrorport->mirror_direct = MIRROR_DIREXT_TX;
     }
    else
    {
        user_mirrorport->mirror_direct = MIRROR_DIREXT_NONE;
    }
     //printk(KERN_ERR "direct=%d dst=%d src=%d\n",user_mirrorport->mirror_direct,user_mirrorport->mirror_dst_port, user_mirrorport->mirror_src_port);
    return 0;
}

static int acl_open(struct inode *ino, struct file *filp)
{
    return 0;
}

static int acl_close(struct inode *ino, struct file *filp)
{
    return 0;
}

static long acl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    switch(cmd)
    {
        case RTK_ACL_INIT:
            {
                ACL_INIT_ARG_ST init_arg;
                int i;

                if(copy_from_user(&init_arg, (void *)arg, sizeof(init_arg)))
                {
                    printk(KERN_ERR "RTK_ACL_INIT copy_from_user error\n");
                    return -EFAULT;
                }
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC300USER) || \
	defined(PRODUCT_AG) || defined(PRODUCT_SBC1000USER) || defined(PRODUCT_UC200) || \
	defined(PRODUCT_SBC1000MAIN)
                if((init_arg.zone_high_num + init_arg.zone_low_num + init_arg.zone_rtp_num + init_arg.zone_dynamic_num)
                    > RTL8367C_ACLRULENO)
                {
                    printk(KERN_ERR "acl num > max num %d\n", RTL8367C_ACLRULENO);
                    return -EINVAL;
                }
#else
                if((init_arg.zone_high_num + init_arg.zone_low_num + init_arg.zone_rtp_num + init_arg.zone_dynamic_num) 
                    > RTK_MAX_NUM_OF_ACL_RULE)
                {
                    printk(KERN_ERR "acl num > max num %d\n", RTK_MAX_NUM_OF_ACL_RULE);
                    return -EINVAL;
                }
#endif
                mutex_lock(&acl_cb->lock);
                for(i=ACL_ZONE_PHY_HIGH; i<ACL_ZONE_BUTTON; i++)
                {
                    if(acl_cb->acl_head[i].next != &acl_cb->acl_head[i])
                    {
                        mutex_unlock(&acl_cb->lock);
                        printk(KERN_ERR "zone %d is not empty\n", i);
                        return -EPERM;
                    }
                }

                zone_high_acl_num = init_arg.zone_high_num;
                zone_low_acl_num = init_arg.zone_low_num;
                zone_rtp_acl_num = init_arg.zone_rtp_num;
                zone_dynamic_acl_num = init_arg.zone_dynamic_num;
                mutex_unlock(&acl_cb->lock);
            }
            break;
        case RTK_ACL_ADD:
            {
                acl_t *acl;
                user_acl_trans_t *user_acl = (user_acl_trans_t *)arg;
                
                acl = kmalloc(sizeof(acl_t), GFP_KERNEL);
                if(NULL == acl)
                {
                    printk(KERN_ERR "%s kmalloc error\n", __FUNCTION__);
                    return -ENOMEM;
                }
                memset(acl, 0, sizeof(acl_t));
                INIT_LIST_HEAD(&acl->list);
                mutex_init(&acl->lock);

                if(acl_get(acl, user_acl) < 0)
                {
                    free_acl(acl);
                    printk(KERN_ERR "%s acl_get error\n", __FUNCTION__);
                    return -EFAULT;
                }

                if(acl_add(acl, user_acl->rate, user_acl->acl_zone) < 0)
                {
                    free_acl(acl);
                    printk(KERN_ERR "%s acl_add error\n", __FUNCTION__);
                    return -EFAULT;
                }
            }
            break;
        case RTK_ACL_DEL:
            {
                user_acl_trans_t *user_acl = (user_acl_trans_t *)arg;
                acl_t *acl;

                DBGPRINT("%s zone %d index %d\n", __func__, 
                    user_acl->acl_zone, user_acl->index);
                if(NULL == (acl = acl_del(user_acl->acl_zone, user_acl->index)))
                {
                    printk(KERN_ERR "%s acl_del error\n", __FUNCTION__);
                    return -EFAULT;
                }
                free_acl(acl);
            }
            break;
        case RTK_ACL_DEL_FUZZY:
            {
                user_acl_trans_t *user_acl = (user_acl_trans_t *)arg;
                acl_t *acl;
                int i;
        
                acl = kmalloc(sizeof(acl_t), GFP_KERNEL);
                if(NULL == acl)
                {
                    printk(KERN_ERR "%s kmalloc error\n", __FUNCTION__);
                    return -ENOMEM;
                }
                memset(acl, 0, sizeof(acl_t));
                
                if(acl_get(acl, user_acl) < 0)
                {
                    free_acl(acl);
                    printk(KERN_ERR "%s acl_get error\n", __FUNCTION__);
                    return -EFAULT;
                }
        
                if(user_acl->acl_zone >= ACL_ZONE_BUTTON)
                {
                    for(i=0; i<ACL_ZONE_BUTTON; i++)
                    {
                        acl_del_fuzzy(i, acl);
                    }
                }
                else
                {
                    acl_del_fuzzy(user_acl->acl_zone, acl);
                }
                
                free_acl(acl);
            }
            break;
        case RTK_ACL_DEL_EXACT:
            {
                user_acl_trans_t *user_acl = (user_acl_trans_t *)arg;
                acl_t *acl;
                int i;
        
                acl = kmalloc(sizeof(acl_t), GFP_KERNEL);
                if(NULL == acl)
                {
                    printk(KERN_ERR "%s kmalloc error\n", __FUNCTION__);
                    return -ENOMEM;
                }
                memset(acl, 0, sizeof(acl_t));
                
                if(acl_get(acl, user_acl) < 0)
                {
                    free_acl(acl);
                    printk(KERN_ERR "%s acl_get error\n", __FUNCTION__);
                    return -EFAULT;
                }
        
                if(user_acl->acl_zone >= ACL_ZONE_BUTTON)
                {
                    for(i=0; i<ACL_ZONE_BUTTON; i++)
                    {
                        acl_del_exact(i, acl);
                    }
                }
                else
                {
                    acl_del_exact(user_acl->acl_zone, acl);
                }
                
                free_acl(acl);
            }
            break;
        case RTK_ACL_MOD:
            {
                acl_t *acl;
                user_acl_trans_t *user_acl = (user_acl_trans_t *)arg;
                
                acl = kmalloc(sizeof(acl_t), GFP_KERNEL);
                if(NULL == acl)
                {
                    printk(KERN_ERR "%s kmalloc error\n", __FUNCTION__);
                    return -ENOMEM;
                }
                memset(acl, 0, sizeof(acl_t));
                INIT_LIST_HEAD(&acl->list);
                mutex_init(&acl->lock);
                
                if(acl_get(acl, user_acl) < 0)
                {
                    free_acl(acl);
                    printk(KERN_ERR "%s acl_get error\n", __FUNCTION__);
                    return -EFAULT;
                }

                DBGPRINT("%s user_acl->index:%d user_acl->rate:%d user_acl->acl_zone:%d\n",
                    __func__, user_acl->index, user_acl->rate, user_acl->acl_zone);
                if(acl_mod(user_acl->index, acl, user_acl->rate, user_acl->acl_zone) < 0)
                {
                    free_acl(acl);
                    printk(KERN_ERR "%s acl_mod error\n", __FUNCTION__);
                    return -EFAULT;
                }
            }
            break;
        case RTK_ACL_CLEAN:
            {
                user_acl_trans_t *user_acl = (user_acl_trans_t *)arg;
                if((NULL == user_acl) || (user_acl->acl_zone >= ACL_ZONE_BUTTON))
                {
                    acl_del_free_all();
                }
                else
                {
                    acl_del_free(user_acl->acl_zone);
                }
            }
            break;
        case RTK_ACL_GETALL:
            {
                multi_user_acl_trans_t *multi_user_acl = (multi_user_acl_trans_t *)arg;

                DBGPRINT("%s multi_user_acl->cnt:%d\n", __func__, multi_user_acl->cnt);
                if(acl_put_all(multi_user_acl) < 0)
                {
                    printk(KERN_ERR "%s acl_get_all error\n", __FUNCTION__);
                } 
            }
            break;
        case RTK_ACL_GETBYINDEX:
            {
                user_acl_trans_t *user_acl = (user_acl_trans_t *)arg;
                acl_t *acl;
                
                mutex_lock(&acl_cb->lock);
                DBGPRINT("%s acl cmd getindex start\n", __func__);
                DBGPRINT("%s acl_zone:%d index:%d\n", __func__,
                    user_acl->acl_zone, user_acl->index);
                acl = get_acl_by_index(user_acl->acl_zone, user_acl->index);
                if(NULL == acl)
                {
                    DBGPRINT("%s acl cmd getindex end\n", __func__);
                    mutex_unlock(&acl_cb->lock);
                    printk(KERN_ERR "%s get_acl_by_index error\n", __FUNCTION__);
                    return -EINVAL;
                }
                if(acl_put(user_acl, acl))
                {
                    DBGPRINT("%s acl cmd getindex end\n", __func__);
                    mutex_unlock(&acl_cb->lock);
                    printk(KERN_ERR "%s acl_put error\n", __FUNCTION__);
                    return -EFAULT;
                }
                DBGPRINT("%s acl cmd getindex end\n", __func__);
                mutex_unlock(&acl_cb->lock);
            }
            break;
        case RTK_GET_IDLE_FILTER:
            {
                int zone = *(int *)arg;
                int max_filter_cnt;
                if(zone < 0 || zone >= ACL_ZONE_BUTTON)
                {
                    printk(KERN_ERR "%s invalid zone %d\n", __FUNCTION__, zone);
                    return -EINVAL;
                }
                if(ACL_ZONE_PHY_HIGH == zone)
                {
                    max_filter_cnt =  PHY_HIGH_ZONE_ACL_FILTER_NUM;
                }
                else if(ACL_ZONE_DYNAMIC == zone)
                {
                    max_filter_cnt =  DYNAMIC_ZONE_ACL_FILTER_NUM;
                }
                else if(ACL_ZONE_RTP == zone)
                {
                    max_filter_cnt =  RTP_ZONE_ACL_FILTER_NUM;
                }
                else if(ACL_ZONE_PHY_LOW == zone)
                {
                    max_filter_cnt =  PHY_LOW_ZONE_ACL_FILTER_NUM;
                }
                else
                {
                    printk(KERN_ERR "%s invalid zone %d\n", __FUNCTION__, zone);
                    return -EINVAL;
                }
                *(int *)arg = max_filter_cnt - acl_cb->filter_used[zone];
            }
            break;
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC300USER) || \
	defined(PRODUCT_AG) || defined(PRODUCT_SBC1000USER) || defined(PRODUCT_UC200) || \
	defined(PRODUCT_SBC1000MAIN)
        case RTK_ACL_PHYUP:
            {
                en_phy_st *en_phy_arg = (en_phy_st *)arg;
                mutex_lock(&acl_cb->lock);
                rtk8367_set_phy_powerdown(en_phy_arg->phy, !en_phy_arg->enble);
                mutex_unlock(&acl_cb->lock);
            }
            break;

        case RTK_MIRROR_ADD:
            {
                rtk_mirror_port_t *user_mirrorport = (rtk_mirror_port_t *)arg;
                mutex_lock(&acl_cb->lock);

                printk(KERN_ERR "%s src_port %d dst_port %d %d\n", __FUNCTION__, user_mirrorport->mirror_src_port, user_mirrorport->mirror_dst_port, user_mirrorport->mirror_direct);
                if(user_mirrorport->mirror_src_port < RTK_MIN_PROT ||user_mirrorport->mirror_src_port > RTK_MAX_PROT || \
                    user_mirrorport->mirror_dst_port < RTK_MIN_PROT ||user_mirrorport->mirror_dst_port > RTK_MAX_PROT ||\
                    user_mirrorport->mirror_dst_port ==user_mirrorport->mirror_src_port)
                {
                    printk(KERN_ERR "%s invalid src_port %d dst_port %d\n", __FUNCTION__, user_mirrorport->mirror_src_port, user_mirrorport->mirror_dst_port);
                    mutex_unlock(&acl_cb->lock);
                    return RT_ERR_FAILED;
                }
                user_mirrorport->mirror_src_port = port_map[user_mirrorport->mirror_src_port];
                user_mirrorport->mirror_dst_port = port_map[user_mirrorport->mirror_dst_port];
                if(user_mirrorport->mirror_direct < MIRROR_DIREXT_RX || user_mirrorport->mirror_direct> MIRROR_DIREXT_BOTH)
                {
                    printk(KERN_ERR "%s invalid mirror_flag %d \n", __FUNCTION__, user_mirrorport->mirror_direct);
                    mutex_unlock(&acl_cb->lock);
                    return RT_ERR_FAILED;
                }
                if(RT_ERR_OK != rtk8367c_mirror_add2cpu(user_mirrorport))
                {
                    printk(KERN_ERR "%s rtk8370_mirror_add2cpu error\n",
                        __FUNCTION__);
                }
                mutex_unlock(&acl_cb->lock);
            }
            break;
        case RTK_MIRROR_DEL:
            {
                rtk_mirror_port_t *user_mirrorport = (rtk_mirror_port_t *)arg;
                mutex_lock(&acl_cb->lock);
                if(user_mirrorport->mirror_src_port < RTK_MIN_PROT ||user_mirrorport->mirror_src_port > RTK_MAX_PROT || \
                    user_mirrorport->mirror_dst_port < RTK_MIN_PROT ||user_mirrorport->mirror_dst_port > RTK_MAX_PROT ||\
                    user_mirrorport->mirror_dst_port ==user_mirrorport->mirror_src_port)
                {
                    printk(KERN_ERR "%s invalid src_port %d dst_port %d\n", __FUNCTION__, user_mirrorport->mirror_src_port, user_mirrorport->mirror_dst_port);
                    mutex_unlock(&acl_cb->lock);
                    return RT_ERR_FAILED;
                }
                user_mirrorport->mirror_src_port = port_map[user_mirrorport->mirror_src_port];
                user_mirrorport->mirror_dst_port = port_map[user_mirrorport->mirror_dst_port];
                if(user_mirrorport->mirror_direct< MIRROR_DIREXT_RX || user_mirrorport->mirror_direct> MIRROR_DIREXT_BOTH)
                {
                    printk(KERN_ERR "%s invalid mirror_flag %d \n", __FUNCTION__, user_mirrorport->mirror_direct);
                    mutex_unlock(&acl_cb->lock);
                    return RT_ERR_FAILED;
                }
                if(RT_ERR_OK != rtk8367c_mirror_del2cpu(user_mirrorport))
                {
                    printk(KERN_ERR "%s rtk8370_mirror_del2cpu  error\n",
                        __FUNCTION__);
                }
                mutex_unlock(&acl_cb->lock);
            }
            break;
        case RTK_MIRROR_STATUS:
            {
                rtk_mirror_port_t user_mirrorport;
                mutex_lock(&acl_cb->lock);
                acl_mirror_getstatus(&user_mirrorport);
                user_mirrorport.mirror_src_port = port_map[user_mirrorport.mirror_src_port];
                user_mirrorport.mirror_dst_port = port_map[user_mirrorport.mirror_dst_port];
                mutex_unlock(&acl_cb->lock);
                if(copy_to_user((rtk_mirror_port_t *)arg, &user_mirrorport,sizeof(rtk_mirror_port_t)))
                {
                    printk(KERN_ERR "RTK_ACL_INIT copy_from_user error\n");
                    return -EFAULT;
                }
            }
            break;
        case RTK_GET_REG:
            {
                u32 *data = (u32 *)arg;
                mutex_lock(&acl_cb->lock);
                simple_phy_read(data[0], &data[1]);
                mutex_unlock(&acl_cb->lock);
            }
            break;
        case RTK_SET_REG:
            {
                u32 *data = (u32 *)arg;
                mutex_lock(&acl_cb->lock);
                simple_phy_write(data[0], data[1]);
                mutex_unlock(&acl_cb->lock);
            }
            break;
        case RTK_GET_MIB:
            {
                u64 *data = (u64 *)arg;
                mutex_lock(&acl_cb->lock);
                rtl8367c_getAsicMIBsCounter((rtk_uint32)data[0], (RTL8367C_MIBCOUNTER)data[1], (rtk_uint64 *)&data[2]);
                mutex_unlock(&acl_cb->lock);
            }
            break;
#else
		case RTK_ACL_PHYUP:
            {
                en_phy_st *en_phy_arg = (en_phy_st *)arg;
                
                mutex_lock(&acl_cb->lock);
                rtk8370_set_phy_powerdown(en_phy_arg->phy, !en_phy_arg->enble);
#if defined(PRODUCT_MTG2500MAIN) 
                if(en_phy_arg->enble)
                {
                    rtk8370_set_phyport_work_mode(en_phy_arg->phy);
                }
#endif
                mutex_unlock(&acl_cb->lock);
            }
            break;
        case RTK_MIRROR_ADD:
            {
                rtk_mirror_port_t *user_mirrorport = (rtk_mirror_port_t *)arg;
                //int slot = (int)arg;
                mutex_lock(&acl_cb->lock);
                //printk(KERN_ERR "%s src_port %d dst_port %d %d\n", __FUNCTION__, user_mirrorport->mirror_src_port, user_mirrorport->mirror_dst_port, user_mirrorport->mirror_direct);
                if(user_mirrorport->mirror_src_port < RTK_MIN_PROT ||user_mirrorport->mirror_src_port > RTK_MAX_PROT || \
                    user_mirrorport->mirror_dst_port < RTK_MIN_PROT ||user_mirrorport->mirror_dst_port > RTK_MAX_PROT ||\
                    user_mirrorport->mirror_dst_port ==user_mirrorport->mirror_src_port)       
                {
                    printk(KERN_ERR "%s invalid src_port %d dst_port %d\n", __FUNCTION__, user_mirrorport->mirror_src_port, user_mirrorport->mirror_dst_port);
                    mutex_unlock(&acl_cb->lock);
                    return RT_ERR_FAILED;
                }
                if(user_mirrorport->mirror_direct < MIRROR_DIREXT_RX || user_mirrorport->mirror_direct> MIRROR_DIREXT_BOTH)   
                {
                    printk(KERN_ERR "%s invalid mirror_flag %d \n", __FUNCTION__, user_mirrorport->mirror_direct);
                    mutex_unlock(&acl_cb->lock);
                    return RT_ERR_FAILED;
                }
                if(RT_ERR_OK != rtk8370_mirror_add2cpu(user_mirrorport))
                {
                    printk(KERN_ERR "%s rtk8370_mirror_add2cpu error\n",
                        __FUNCTION__);
                }
                mutex_unlock(&acl_cb->lock);
            }
            break;
        case RTK_MIRROR_DEL:
            {
                rtk_mirror_port_t *user_mirrorport = (rtk_mirror_port_t *)arg;
                mutex_lock(&acl_cb->lock);
                if(user_mirrorport->mirror_src_port < RTK_MIN_PROT ||user_mirrorport->mirror_src_port > RTK_MAX_PROT || \
                    user_mirrorport->mirror_dst_port < RTK_MIN_PROT ||user_mirrorport->mirror_dst_port > RTK_MAX_PROT ||\
                    user_mirrorport->mirror_dst_port ==user_mirrorport->mirror_src_port)       
                {
                    printk(KERN_ERR "%s invalid src_port %d dst_port %d\n", __FUNCTION__, user_mirrorport->mirror_src_port, user_mirrorport->mirror_dst_port);
                    mutex_unlock(&acl_cb->lock);
                    return RT_ERR_FAILED;
                }

                if(user_mirrorport->mirror_direct< MIRROR_DIREXT_RX || user_mirrorport->mirror_direct> MIRROR_DIREXT_BOTH)   
                {
                    printk(KERN_ERR "%s invalid mirror_flag %d \n", __FUNCTION__, user_mirrorport->mirror_direct);
                    mutex_unlock(&acl_cb->lock);
                    return RT_ERR_FAILED;
                }
                if(RT_ERR_OK != rtk8370_mirror_del2cpu(user_mirrorport))
                {
                    printk(KERN_ERR "%s rtk8370_mirror_del2cpu  error\n",
                        __FUNCTION__);
                }
                mutex_unlock(&acl_cb->lock);
             }
             break;
         case RTK_MIRROR_STATUS:
            {
                rtk_mirror_port_t user_mirrorport;
                mutex_lock(&acl_cb->lock);
                acl_mirror_getstatus(&user_mirrorport);
                mutex_unlock(&acl_cb->lock);
                //printk(KERN_ERR "direct=%d dst=%d src=%d\n",user_mirrorport->mirror_direct,user_mirrorport->mirror_dst_port, user_mirrorport->mirror_src_port);
                if(copy_to_user((rtk_mirror_port_t *)arg, &user_mirrorport,sizeof(rtk_mirror_port_t)))
                {
                    printk(KERN_ERR "RTK_ACL_INIT copy_from_user error\n");
                    return -EFAULT;
                }   

            }
            break;
        case RTK_GET_REG:
            {
                u32 *data = (u32 *)arg;
                mutex_lock(&acl_cb->lock);
                simple_phy_read(data[0], &data[1]);
                mutex_unlock(&acl_cb->lock);
            }
            break;
        case RTK_SET_REG:
            {
                u32 *data = (u32 *)arg;
                mutex_lock(&acl_cb->lock);
                simple_phy_write(data[0], data[1]);
                mutex_unlock(&acl_cb->lock);
            }
            break;
        case RTK_GET_MIB:
            {
                u64 *data = (u64 *)arg;
                mutex_lock(&acl_cb->lock);
                rtl8370_getAsicMIBsCounter((uint32)data[0], (enum RTL8370_MIBCOUNTER)data[1], (uint64_t *)&data[2]);
            	mutex_unlock(&acl_cb->lock);
			}
            break;
#endif
    }
    return 0;
}

struct file_operations acl_dev_ops =
{
    .open = acl_open,
    .unlocked_ioctl = acl_ioctl,
    .release = acl_close,
};

rtk_api_ret_t rtk_acl_init(void)
{
    int i;
    struct device *acl_dev;
    
    acl_cb = kmalloc(sizeof(acl_cb_t), GFP_KERNEL);
    if(NULL == acl_cb)
    {
        printk(KERN_ERR "%s kmalloc failed\n", __FUNCTION__);
        return RT_ERR_FAILED;
    }

    memset(acl_cb, 0, sizeof(acl_cb_t));
    for(i=0; i<ACL_ZONE_BUTTON; i++)
    {
        INIT_LIST_HEAD(&acl_cb->acl_head[i]);
    }
    mutex_init(&acl_cb->lock);
    acl_cb->max_acl_cnt[ACL_ZONE_PHY_HIGH] = PHY_HIGH_ZONE_ACL_FILTER_NUM;
    acl_cb->max_acl_cnt[ACL_ZONE_RTP] = RTP_ZONE_ACL_FILTER_NUM;
    acl_cb->max_acl_cnt[ACL_ZONE_DYNAMIC] = RTP_ZONE_ACL_FILTER_NUM;
    acl_cb->max_acl_cnt[ACL_ZONE_PHY_LOW] = DYNAMIC_ZONE_ACL_FILTER_NUM;

    acl_devno = MKDEV(acl_dev_major, 0);
    if(register_chrdev_region(acl_devno, 0, ACL_DEV_NAME) < 0)
    {
        if(alloc_chrdev_region(&acl_devno, 0, 1, ACL_DEV_NAME) < 0)
        {
            printk(KERN_ERR "%s register chrdev region error\n", __FUNCTION__);
            goto fail0;
        }
    }

    cdev_init(&acl_cdev, &acl_dev_ops);
    if(cdev_add(&acl_cdev, acl_devno, 1) < 0)
    {
        printk(KERN_ERR "%s cdev_add failed\n", __FUNCTION__);
        goto fail1;
    }

    acl_class = class_create(THIS_MODULE, ACL_DEV_NAME);
    if(IS_ERR(acl_class))
    {
        printk(KERN_ERR "%s class_create failed\n", __FUNCTION__);
        goto fail2;
    }
    acl_dev = device_create(acl_class, NULL, acl_devno, NULL, ACL_DEV_NAME);
    if(IS_ERR(acl_dev))
    {
        printk(KERN_ERR "%s device_create failed\n", __FUNCTION__);
        goto fail3;
    }

    return RT_ERR_OK;
fail3:
    class_destroy(acl_class);
fail2:
    cdev_del(&acl_cdev);
fail1:
    unregister_chrdev_region(acl_devno, 1);
fail0:
    kfree(acl_cb);
    return RT_ERR_FAILED;
}

void rtk_acl_exit(void)
{
    device_destroy(acl_class, acl_devno);
    class_destroy(acl_class);
    cdev_del(&acl_cdev);
    unregister_chrdev_region(acl_devno, 1);
    acl_del_free_all();
    kfree(acl_cb);
}
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC300USER) || \
	defined(PRODUCT_AG) || defined(PRODUCT_SBC1000USER) || defined(PRODUCT_UC200) || \
	defined(PRODUCT_SBC1000MAIN)
int rtk8367_acl_cfg(void)
{
	int retVal = 0;

    retVal = rtk_filter_igrAcl_init();
	if( retVal != RT_ERR_OK )
	{
		printk("%s: acl clean failed.\n", __func__);
		return retVal;
	}

    retVal = rtk_acl_init();
	if( retVal != RT_ERR_OK )
	{
		printk("%s: acl init failed.\n", __func__);
		return retVal;
	}

    return retVal;
}
#endif
