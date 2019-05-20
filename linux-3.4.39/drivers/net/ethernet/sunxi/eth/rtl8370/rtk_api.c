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
#include <linux/ethtool.h>

#include "rtk_types.h"
#include "rtk_error.h"
#include "rtl8370.h"
#include "rtl8370_reg.h"
#include "rtl8370_asicdrv_dot1x.h"
#include "rtl8370_asicdrv_acl.h"
#include "rtl8370_asicdrv_vlan.h"
#include "rtl8370_asicdrv_svlan.h"
#include "rtl8370_asicdrv_cputag.h"
#include "rtl8370_asicdrv_meter.h"
#include "rtl8370_asicdrv_mirror.h"
#include "rtk_api.h"
#include "rtl8370_asicdrv_phy.h"
#include "rtk_acl.h"

#include "rtl8370_asicdrv_lut.h"
#include "rtk_api_ext.h"
#include "rtk_api.h"
#include "rtl8370_asicdrv_trunking.h"
#include "rtl8370_asicdrv_mib.h"
#include "rtl8370_asicdrv_qos.h"
#include "rtl8370_asicdrv_scheduling.h"
///-
MODULE_LICENSE("GPL");
extern acl_cb_t *acl_cb;
typedef enum rtk_filter_data_type_e
{
    RTK_FILTER_DATA_MAC = 0,
    RTK_FILTER_DATA_UINT16,
    RTK_FILTER_DATA_TAG,
    RTK_FILTER_DATA_IPV4,
    RTK_FILTER_DATA_UINT8_HIGH,    
    RTK_FILTER_DATA_UINT8_LOW,
    RTK_FILTER_DATA_IPV4FLAG,
    RTK_FILTER_DATA_UINT13_LOW,
    RTK_FILTER_DATA_TCPFLAG,
    RTK_FILTER_DATA_IPV6,
} rtk_filter_data_type_t;

static CONST_T uint32 filter_templateField[RTK_MAX_NUM_OF_FILTER_TYPE][RTK_MAX_NUM_OF_FILTER_FIELD] = {
    {DMAC2, DMAC1, DMAC0, SMAC2, SMAC1, SMAC0, ETHERTYPE},
    {IP4SIP1, IP4SIP0, IP4DIP1, IP4DIP0, IP4FLAGOFF, IP4TOSPROTO, CTAG},
    {IP6SIP7, IP6SIP6, IP6SIP4, IP6SIP3, IP6SIP2, IP6SIP1, IP6SIP0},
    {IP6DIP7, IP6DIP6, IP6DIP4, IP6DIP3, IP6DIP2, IP6DIP1, IP6DIP0},
    {TCPSPORT, TCPDPORT, TCPFLAG, ICMPCODETYPE, IGMPTYPE, TOSNH, STAG}
};

static CONST_T uint32            fieldSize[FILTER_FIELD_MAX] = {
    3, 3, 1, 1, 1, 
    2, 2, 1, 1, 1, 1, 8, 8, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1};
static CONST_T uint32             fieldStartIdx[FILTER_FIELD_MAX] = {
    DMAC0, SMAC0, ETHERTYPE, CTAG, STAG, 
    IP4SIP0, IP4DIP0, IP4TOSPROTO, IP4TOSPROTO, IP4FLAGOFF, IP4FLAGOFF, IP6SIP0, IP6DIP0, TOSNH, TOSNH,
    TCPSPORT, TCPDPORT, TCPFLAG, TCPSPORT, TCPDPORT, ICMPCODETYPE, ICMPCODETYPE, IGMPTYPE};
static CONST_T uint32             fieldDataType[FILTER_FIELD_MAX] = {
    RTK_FILTER_DATA_MAC, RTK_FILTER_DATA_MAC, RTK_FILTER_DATA_UINT16, RTK_FILTER_DATA_TAG, RTK_FILTER_DATA_TAG,
    RTK_FILTER_DATA_IPV4, RTK_FILTER_DATA_IPV4, RTK_FILTER_DATA_UINT8_HIGH, RTK_FILTER_DATA_UINT8_LOW, RTK_FILTER_DATA_IPV4FLAG,
    RTK_FILTER_DATA_UINT13_LOW, RTK_FILTER_DATA_IPV6, RTK_FILTER_DATA_IPV6, RTK_FILTER_DATA_UINT8_HIGH, RTK_FILTER_DATA_UINT8_LOW,
    RTK_FILTER_DATA_UINT16, RTK_FILTER_DATA_UINT16, RTK_FILTER_DATA_TCPFLAG,
    RTK_FILTER_DATA_UINT16, RTK_FILTER_DATA_UINT16, RTK_FILTER_DATA_UINT8_HIGH, RTK_FILTER_DATA_UINT8_LOW, RTK_FILTER_DATA_UINT16};

static u8 eth0_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x77, 0x20};
static u8 eth1_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x77, 0x21};
rtk_filter_id_t gfilter_id = 0;   /* filter_id从0开始，后面分给每个slot */

extern rtk_api_ret_t rtk_acl_init(void);
extern void rtk_acl_exit(void);

/* Function Name:
 *      rtk_rate_shareMeter_set
 * Description:
 *      Set meter configuration
 * Input:
 *      index       - Shared meter index
 *      rate        - Rate of share meter
 *      ifg_include - Include IFG or not, ENABLE:include DISABLE:exclude
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_FILTER_METER_ID - Invalid meter
 *      RT_ERR_RATE            - Invalid rate
 *      RT_ERR_INPUT           - Invalid input parameters
 * Note:
 *      The API can set shared meter rate and ifg include for each meter. 
 *      The rate unit is 1 kbps and the range is from 8k to 1048568k.
 *      The granularity of rate is 8 kbps. The ifg_include parameter is used 
 *      for rate calculation with/without inter-frame-gap and preamble.
 */
rtk_api_ret_t rtk_rate_shareMeter_set(rtk_meter_id_t index, rtk_rate_t rate, rtk_enable_t ifg_include)
{
    rtk_api_ret_t retVal;
    
    if (index>=RTK_MAX_NUM_OF_METER)
        return RT_ERR_FILTER_METER_ID;

    if (rate>RTK_QOS_RATE_INPUT_MAX || rate<RTK_QOS_RATE_INPUT_MIN)
        return RT_ERR_RATE ;

    if (ifg_include>=RTK_ENABLE_END)
        return RT_ERR_INPUT;    
        
    if ((retVal = rtl8370_setAsicShareMeter(index,rate>>3,ifg_include))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_rate_shareMeter_get
 * Description:
 *      Get meter configuration
 * Input:
 *      index        - Shared meter index
 * Output:
 *      pRate        - Pointer of rate of share meter
 *      pIfg_include - Include IFG or not, ENABLE:include DISABLE:exclude
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_FILTER_METER_ID - Invalid meter
 * Note:
 *      The API can get shared meter rate and ifg include for each meter. 
 *      The rate unit is 1 kbps and the granularity of rate is 8 kbps.
 *      The ifg_include parameter is used for rate calculation with/without inter-frame-gap and preamble 
 */

rtk_api_ret_t rtk_rate_shareMeter_get(rtk_meter_id_t index, rtk_rate_t *pRate ,rtk_data_t *pIfg_include)
{
    rtk_api_ret_t retVal;
    uint32 regData;
    
    if (index>=RTK_MAX_NUM_OF_METER)
        return RT_ERR_FILTER_METER_ID;

    if ((retVal = rtl8370_getAsicShareMeter(index, &regData, pIfg_include))!=RT_ERR_OK)
        return retVal; 

    *pRate = regData<<3;
        
    return RT_ERR_OK;
}

static rtk_api_ret_t _rtk_filter_igrAcl_writeDataField(rtl8370_acl_rule_t *aclRule, uint32 *tempIdx, uint32 *fieldIdx, rtk_filter_field_t *fieldPtr, rtk_filter_data_type_t type)
{
    uint32 i, aclIdx;

    aclIdx = 0xFF;

    for(i = 0; i < RTK_MAX_NUM_OF_FILTER_TYPE; i++)
    {
        if (FALSE == aclRule[i].valid)
        {
            aclIdx=i;
            break;
        }
        else if (TRUE == aclRule[i].valid && tempIdx[0] == aclRule[i].data_bits.type)
        {
            aclIdx=i;
            break;
        }
    }    

    if (0xFF == aclIdx)
        return RT_ERR_FILTER_INACL_RULE_NOT_SUPPORT;

    switch ( type )
    {
    /* use DMAC structure as representative for mac structure */
    case RTK_FILTER_DATA_MAC:
        if(FILTER_FIELD_DATA_MASK != fieldPtr->filter_pattern_union.dmac.dataType )
            return RT_ERR_FILTER_INACL_RULE_NOT_SUPPORT;

        for(i = 0; i < MAC_ADDR_LEN / 2;i++)
        {
            if(RTK_MAX_NUM_OF_FILTER_FIELD != tempIdx[i] || RTK_MAX_NUM_OF_FILTER_TYPE != fieldIdx[i])
            {
                aclRule[aclIdx].data_bits.field[fieldIdx[i]] = fieldPtr->filter_pattern_union.dmac.value.octet[5-i*2] | (fieldPtr->filter_pattern_union.dmac.value.octet[5-(i*2 + 1)] << 8);
                aclRule[aclIdx].care_bits.field[fieldIdx[i]] = fieldPtr->filter_pattern_union.dmac.mask.octet[5-i*2] | (fieldPtr->filter_pattern_union.dmac.mask.octet[5-(i*2 + 1)] << 8);
                aclRule[aclIdx].data_bits.type = tempIdx[i];
                aclRule[aclIdx].valid = TRUE;
            }        
        }
        break;
    /* use ETHERTYPE structure as representative for uint16 structure */
    case RTK_FILTER_DATA_UINT16:
        if(FILTER_FIELD_DATA_MASK != fieldPtr->filter_pattern_union.etherType.dataType)
            return RT_ERR_FILTER_INACL_RULE_NOT_SUPPORT;
        if(fieldPtr->filter_pattern_union.etherType.value > 0xFFFF || fieldPtr->filter_pattern_union.etherType.mask > 0xFFFF)
            return RT_ERR_INPUT;

        aclRule[aclIdx].data_bits.field[fieldIdx[0]] = fieldPtr->filter_pattern_union.etherType.value;
        aclRule[aclIdx].care_bits.field[fieldIdx[0]] = fieldPtr->filter_pattern_union.etherType.mask;
        aclRule[aclIdx].data_bits.type = tempIdx[0];
        aclRule[aclIdx].valid = TRUE;
        break;
    /* use STAG structure as representative for TAG structure */        
    case RTK_FILTER_DATA_TAG:
        if(FILTER_FIELD_DATA_MASK != fieldPtr->filter_pattern_union.stag.vid.dataType ||
             FILTER_FIELD_DATA_MASK != fieldPtr->filter_pattern_union.stag.pri.dataType)
            return RT_ERR_FILTER_INACL_RULE_NOT_SUPPORT;
        if(fieldPtr->filter_pattern_union.stag.cfi.value > TRUE || fieldPtr->filter_pattern_union.stag.cfi.mask > TRUE ||
             fieldPtr->filter_pattern_union.stag.pri.value > RTK_DOT1P_PRIORITY_MAX || fieldPtr->filter_pattern_union.stag.pri.mask > RTK_DOT1P_PRIORITY_MAX ||
             fieldPtr->filter_pattern_union.stag.vid.value > RTK_VLAN_ID_MAX|| fieldPtr->filter_pattern_union.stag.vid.mask > RTK_VLAN_ID_MAX )
            return RT_ERR_INPUT;

        aclRule[aclIdx].data_bits.field[fieldIdx[0]] = (fieldPtr->filter_pattern_union.stag.pri.value << 13) | (fieldPtr->filter_pattern_union.stag.cfi.value << 12) | fieldPtr->filter_pattern_union.stag.vid.value;
        aclRule[aclIdx].care_bits.field[fieldIdx[0]] = (fieldPtr->filter_pattern_union.stag.pri.mask << 13) | (fieldPtr->filter_pattern_union.stag.cfi.mask << 12) | fieldPtr->filter_pattern_union.stag.vid.mask;
        aclRule[aclIdx].data_bits.type = tempIdx[0];
        aclRule[aclIdx].valid = TRUE;
        break;            
    /* use sip structure as representative for IPV4 structure */        
    case RTK_FILTER_DATA_IPV4:
        if(FILTER_FIELD_DATA_MASK != fieldPtr->filter_pattern_union.sip.dataType)
            return RT_ERR_FILTER_INACL_RULE_NOT_SUPPORT;
        for(i = 0; i < IPV4_ADDR_LEN / 2; i++)
        {
            if(RTK_MAX_NUM_OF_FILTER_FIELD != tempIdx[i] || RTK_MAX_NUM_OF_FILTER_TYPE != fieldIdx[i] )
            {
                aclRule[aclIdx].data_bits.field[fieldIdx[i]] = (fieldPtr->filter_pattern_union.sip.value & (0xFFFF << (i << 4))) >> (i << 4);
                aclRule[aclIdx].care_bits.field[fieldIdx[i]] = (fieldPtr->filter_pattern_union.sip.mask & (0xFFFF << (i << 4))) >> (i << 4);
                aclRule[aclIdx].data_bits.type = tempIdx[i];
                aclRule[aclIdx].valid = TRUE;
            }
        }
        break;
    /* use ToS structure as representative for UINT8_HIGH structure */
    case RTK_FILTER_DATA_UINT8_HIGH:
        if(FILTER_FIELD_DATA_MASK != fieldPtr->filter_pattern_union.ipTos.dataType)
            return RT_ERR_FILTER_INACL_RULE_NOT_SUPPORT;
        if(fieldPtr->filter_pattern_union.ipTos.value > 0xFF || fieldPtr->filter_pattern_union.ipTos.mask > 0xFF )
            return RT_ERR_INPUT;

        aclRule[aclIdx].data_bits.field[fieldIdx[0]] &= 0xFF;
        aclRule[aclIdx].data_bits.field[fieldIdx[0]] |= (fieldPtr->filter_pattern_union.ipTos.value << 8);
        aclRule[aclIdx].care_bits.field[fieldIdx[0]] &= 0xFF;
        aclRule[aclIdx].care_bits.field[fieldIdx[0]] |= (fieldPtr->filter_pattern_union.ipTos.mask << 8);
        aclRule[aclIdx].data_bits.type = tempIdx[0]; 
        aclRule[aclIdx].valid = TRUE;                        
        break;
    /* use protocol structure as representative for UINT8_LOW structure */
    case RTK_FILTER_DATA_UINT8_LOW:
        if(FILTER_FIELD_DATA_MASK != fieldPtr->filter_pattern_union.protocol.dataType)
            return RT_ERR_FILTER_INACL_RULE_NOT_SUPPORT;
        if(fieldPtr->filter_pattern_union.protocol.value > 0xFF || fieldPtr->filter_pattern_union.protocol.mask > 0xFF )
            return RT_ERR_INPUT;
            
        aclRule[aclIdx].data_bits.field[fieldIdx[0]] &= 0xFF00;
        aclRule[aclIdx].data_bits.field[fieldIdx[0]] |= fieldPtr->filter_pattern_union.protocol.value;
        aclRule[aclIdx].care_bits.field[fieldIdx[0]] &= 0xFF00;
        aclRule[aclIdx].care_bits.field[fieldIdx[0]] |= fieldPtr->filter_pattern_union.protocol.mask;
        aclRule[aclIdx].data_bits.type = tempIdx[0]; 
        aclRule[aclIdx].valid = TRUE;                        
        break;
    case RTK_FILTER_DATA_IPV4FLAG:
        aclRule[aclIdx].data_bits.field[fieldIdx[0]] &= 0xFFF;
        aclRule[aclIdx].data_bits.field[fieldIdx[0]] |= (fieldPtr->filter_pattern_union.ipFlag.df.value<< 14);
        aclRule[aclIdx].data_bits.field[fieldIdx[0]] |= (fieldPtr->filter_pattern_union.ipFlag.mf.value << 13);
        aclRule[aclIdx].care_bits.field[fieldIdx[0]] &= 0xFFF;
        aclRule[aclIdx].care_bits.field[fieldIdx[0]] |= (fieldPtr->filter_pattern_union.ipFlag.df.mask << 14);
        aclRule[aclIdx].care_bits.field[fieldIdx[0]] |= (fieldPtr->filter_pattern_union.ipFlag.mf.mask << 13);
        aclRule[aclIdx].data_bits.type = tempIdx[0]; 
        aclRule[aclIdx].valid = TRUE;         
        break;
    case RTK_FILTER_DATA_UINT13_LOW:
        if(FILTER_FIELD_DATA_MASK != fieldPtr->filter_pattern_union.ipOffset.dataType)
            return RT_ERR_FILTER_INACL_RULE_NOT_SUPPORT;
        if(fieldPtr->filter_pattern_union.ipOffset.value > 0x1FFF || fieldPtr->filter_pattern_union.ipOffset.mask > 0x1FFF )
            return RT_ERR_INPUT;
            
        aclRule[aclIdx].data_bits.field[fieldIdx[0]] &= 0xE000;
        aclRule[aclIdx].data_bits.field[fieldIdx[0]] |= fieldPtr->filter_pattern_union.ipOffset.value;
        aclRule[aclIdx].care_bits.field[fieldIdx[0]] &= 0xE000;
        aclRule[aclIdx].care_bits.field[fieldIdx[0]] |= fieldPtr->filter_pattern_union.ipOffset.mask;
        aclRule[aclIdx].data_bits.type = tempIdx[0]; 
        aclRule[aclIdx].valid = TRUE;                        
        break;
    case RTK_FILTER_DATA_TCPFLAG:
        aclRule[aclIdx].data_bits.field[fieldIdx[0]] = 
            (fieldPtr->filter_pattern_union.tcpFlag.fin.value) | (fieldPtr->filter_pattern_union.tcpFlag.syn.value << 1) |
            (fieldPtr->filter_pattern_union.tcpFlag.rst.value << 2) | (fieldPtr->filter_pattern_union.tcpFlag.psh.value << 3) |    
            (fieldPtr->filter_pattern_union.tcpFlag.ack.value << 4) | (fieldPtr->filter_pattern_union.tcpFlag.urg.value << 5) |
            (fieldPtr->filter_pattern_union.tcpFlag.ece.value << 6) | (fieldPtr->filter_pattern_union.tcpFlag.cwr.value << 7);
        aclRule[aclIdx].care_bits.field[fieldIdx[0]] = 
            (fieldPtr->filter_pattern_union.tcpFlag.fin.mask) | (fieldPtr->filter_pattern_union.tcpFlag.syn.mask << 1) |
            (fieldPtr->filter_pattern_union.tcpFlag.rst.mask << 2) | (fieldPtr->filter_pattern_union.tcpFlag.psh.mask << 3) |    
            (fieldPtr->filter_pattern_union.tcpFlag.ack.mask << 4) | (fieldPtr->filter_pattern_union.tcpFlag.urg.mask << 5) |
            (fieldPtr->filter_pattern_union.tcpFlag.ece.mask << 6) | (fieldPtr->filter_pattern_union.tcpFlag.cwr.mask << 7);
        aclRule[aclIdx].valid = TRUE; 
        break;        
    case RTK_FILTER_DATA_IPV6:
        if(FILTER_FIELD_DATA_MASK != fieldPtr->filter_pattern_union.dipv6.dataType )
            return RT_ERR_FILTER_INACL_RULE_NOT_SUPPORT;                
        for(i = 0; i < IPV6_ADDR_LEN / 2; i++)
        {
            if(RTK_MAX_NUM_OF_FILTER_FIELD != tempIdx[i] || RTK_MAX_NUM_OF_FILTER_TYPE != fieldIdx[i] )
            {
                if (i!=5)
                {
                    aclRule[aclIdx].data_bits.field[fieldIdx[i]] = (fieldPtr->filter_pattern_union.dipv6.value.addr[i/2] & (0xFFFF << ((i & 1) << 4))) >> ((i & 1) << 4);
                    aclRule[aclIdx].care_bits.field[fieldIdx[i]] = (fieldPtr->filter_pattern_union.dipv6.mask.addr[i/2] & (0xFFFF << ((i & 1) << 4))) >> ((i & 1) << 4);
                }
                aclRule[aclIdx].data_bits.type = tempIdx[i];
                aclRule[aclIdx].valid = TRUE;
            }                
        }
        aclRule[aclIdx].data_bits.type = tempIdx[0]; 
        aclRule[aclIdx].valid = TRUE;                        
        break;     
    default:
        return RT_ERR_INPUT;
    }

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_filter_igrAcl_cfg_del
 * Description:
 *      Delete an ACL configuration from ASIC
 * Input:
 *      filter_id | Start index of ACL configuration.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_FILTER_ENTRYIDX - Invalid filter_id.
 * Note:
 *      This function delete a group of ACL rules starting from filter_id.
 */
rtk_api_ret_t rtk_filter_igrAcl_cfg_del(rtk_filter_id_t filter_id)
{
#define FILTER_ACL_ACTCTRL_INIT 0x1F

    rtl8370_acl_rule_t initRule;
    rtl8370_acl_act_t initAct;
    rtk_api_ret_t ret;

    if(filter_id >= RTK_MAX_NUM_OF_ACL_RULE )
        return RT_ERR_FILTER_ENTRYIDX;

    memset(&initRule, 0, sizeof(rtl8370_acl_rule_t));
    memset(&initAct, 0, sizeof(rtl8370_acl_act_t));

    if((ret = rtl8370_setAsicAclRule(filter_id, &initRule)) != RT_ERR_OK)
        return ret;
    if((ret = rtl8370_setAsicAclActCtrl(filter_id, FILTER_ACL_ACTCTRL_INIT))!= RT_ERR_OK)
        return ret;
     if ((ret = rtl8370_setAsicAclAct(filter_id, initAct))!=RT_ERR_OK)
        return ret;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_filter_igrAcl_cfg_delAll
 * Description:
 *      Delete all ACL entries from ASIC
 * Input:
 *      None
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 * Note:
 *      This function delete all ACL configuration from ASIC.
 */
rtk_api_ret_t rtk_filter_igrAcl_cfg_delAll(void)
{
#define ACL_ACTCTRL_INIT 0x1F

    rtl8370_acl_rule_t initRule;
    rtl8370_acl_act_t initAct;
    uint32 i;
    rtk_api_ret_t ret;

    memset(&initRule, 0, sizeof(rtl8370_acl_rule_t));
    memset(&initAct, 0, sizeof(rtl8370_acl_act_t));

    for(i = 0; i < RTK_MAX_NUM_OF_ACL_RULE;i++)
    {
        if((ret = rtl8370_setAsicAclRule(i, &initRule)) != RT_ERR_OK)
            return ret;
        if((ret = rtl8370_setAsicAclActCtrl(i, ACL_ACTCTRL_INIT))!= RT_ERR_OK)
            return ret;
         if ((ret = rtl8370_setAsicAclAct(i, initAct))!=RT_ERR_OK)
            return ret;
    }

    return RT_ERR_OK;
}

/* Function Name:
 *      mtg_filter_igrAcl_init 
 * Description:
 *      ACL initialization function, only for external port0/por1
 * Input:
 *      None
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_NULL_POINTER    - Pointer pFilter_field or pFilter_cfg point to NULL.
 * Note:
 *      This function enable and intialize ACL function
 */
rtk_api_ret_t rtl8370_filter_igrAcl_init(void)
{
    rtl8370_acl_template_t aclTemp;
    uint32                 i, j;
    rtk_api_ret_t          ret;

    if ((ret = rtk_filter_igrAcl_cfg_delAll()) != RT_ERR_OK)
        return ret;

    /* 模版初始化 */
    for(i = 0; i < RTK_MAX_NUM_OF_FILTER_TYPE; i++)
    {
        for(j = 0; j < RTK_MAX_NUM_OF_FILTER_FIELD;j++)
            aclTemp.field[j] = filter_templateField[i][j];
        
        if ((ret = rtl8370_setAsicAclType(i, aclTemp)) != SUCCESS)
            return ret;
    }

    /* 只激活0~7 口的ACL 功能，两个RGMII口也激活ACL */
    for(i=PORT0; i<=PORT9; i++)
    {
        if ((ret = rtl8370_setAsicAcl(i, TRUE)) != SUCCESS)
            return ret;    
        
        if ((ret = rtl8370_setAsicAclUnmatchedPermit(i, TRUE)) != SUCCESS)
            return ret;    
    }


    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_filter_igrAcl_field_add
 * Description:
 *      Add comparison rule to an ACL configuration
 * Input:
 *      pFilter_cfg | The ACL configuration that this function will add comparison rule
 *      pFilter_field | The comparison rule that will be added.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_NULL_POINTER    - Pointer pFilter_field or pFilter_cfg point to NULL.
 *      RT_ERR_INPUT - Invalid input parameters. 
 * Note:
 *      This function add a comparison rule (*pFilter_field) to an ACL configuration (*pFilter_cfg). 
 *      Pointer pFilter_cfg points to an ACL configuration structure, this structure keeps multiple ACL 
 *      comparison rules by means of linked list. Pointer pFilter_field will be added to linked 
 *      list keeped by structure that pFilter_cfg points to.
 */
rtk_api_ret_t rtk_filter_igrAcl_field_add(rtk_filter_cfg_t* pFilter_cfg, rtk_filter_field_t* pFilter_field)
{
    rtk_filter_field_t *tailPtr;

    if(NULL == pFilter_cfg || NULL == pFilter_field)
        return RT_ERR_NULL_POINTER;

    if (pFilter_field->fieldType >= FILTER_FIELD_MAX)
        return RT_ERR_ENTRY_INDEX;

    if(NULL == pFilter_cfg->fieldHead )
    {
        pFilter_cfg->fieldHead = pFilter_field;
    }
    else
    {
        if (pFilter_cfg->fieldHead->next == NULL)
        {
            pFilter_cfg->fieldHead->next = pFilter_field;
        }
        else
        {
            tailPtr = pFilter_cfg->fieldHead->next;
            while( tailPtr->next != NULL)
            {
                tailPtr = tailPtr->next;
            }
            tailPtr->next = pFilter_field;
        }
    }

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_filter_igrAcl_cfg_add
 * Description:
 *      Add an ACL configuration to ASIC
 * Input:
 *      filter_id - Start index of ACL configuration.
 *      pFilter_cfg - The ACL configuration that this function will add comparison rule
 *      pFilter_action - Action(s) of ACL configuration.
 * Output:
 *      ruleNum - number of rules written in acl table
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_NULL_POINTER    - Pointer pFilter_field or pFilter_cfg point to NULL.
 *      RT_ERR_INPUT - Invalid input parameters. 
 *      RT_ERR_ENTRY_INDEX - Invalid filter_id .
 *      RT_ERR_NULL_POINTER - Pointer pFilter_action or pFilter_cfg point to NULL.
 *      RT_ERR_FILTER_INACL_ACT_NOT_SUPPORT - Action is not supported in this chip.
 *      RT_ERR_FILTER_INACL_RULE_NOT_SUPPORT - Rule is not supported.
 * Note:
 *      This function store pFilter_cfg, pFilter_action into ASIC. The starting
 *      index(es) is filter_id.
 */
rtk_api_ret_t rtk_filter_igrAcl_cfg_add(rtk_filter_id_t filter_id, rtk_filter_cfg_t* pFilter_cfg, rtk_filter_action_t* pFilter_action, rtk_filter_number_t *ruleNum)
{
    rtk_api_ret_t           ret;
    uint32                  careTagData = 0, careTagMask = 0;
    uint32                  i, j, k, fieldCnt = 0;
    uint32                  fieldTypeLog[sizeof(filter_templateField) / sizeof(uint32)];
    uint32                  tempIdx[8], fieldIdx[8];
    uint32                  fwdEnable;
    uint32                  aclActCtrl;
    uint32                  cpuPort;
    uint32                  usedRule;
    rtl8370_acl_template_t  aclTemp[RTK_MAX_NUM_OF_FILTER_TYPE];
    rtk_filter_field_t*     fieldPtr;
    rtl8370_acl_rule_t      aclRule[RTK_MAX_NUM_OF_FILTER_TYPE];
    rtl8370_acl_rule_t      tempRule;
    rtl8370_acl_act_t       aclAct;
    rtl8370_svlan_memconf_t svlanMemConf;
    uint32 matchIdx;

    if(filter_id >= RTK_MAX_NUM_OF_ACL_RULE )
        return RT_ERR_ENTRY_INDEX;

    if(NULL == pFilter_cfg)
        return RT_ERR_NULL_POINTER;

    if(NULL == pFilter_action )
        return RT_ERR_NULL_POINTER;

    fieldPtr = pFilter_cfg->fieldHead;

    /* get template table from ASIC */
    for(i = 0; i < RTK_MAX_NUM_OF_FILTER_TYPE; i++)
    {
        if((ret=rtl8370_getAsicAclType(i, &aclTemp[i])) != SUCCESS )
            return ret;
    }
    /* init RULE */
    for(i = 0; i < RTK_MAX_NUM_OF_FILTER_TYPE; i++)
    {
        memset(&aclRule[i], 0, sizeof(rtl8370_acl_rule_t));
        aclRule[i].care_bits.type= 0x7;
    }
 
    while(NULL != fieldPtr)
    {
        /* check if the same data type has inputed */
        for(i = 0; i < fieldCnt;i++)
        {
            if(fieldTypeLog[i] == fieldPtr->fieldType )
                return RT_ERR_INPUT;
        }
        fieldTypeLog[fieldCnt] = fieldPtr->fieldType;
        fieldCnt++;

        /* check if data type is supported in RTL8370 */
        if(TYPE_MAX <= fieldStartIdx[fieldPtr->fieldType] )
            return RT_ERR_FILTER_INACL_RULE_NOT_SUPPORT;

        /* initialize field and template index array */
        for(i = 0; i < 8;i++)
        {
            fieldIdx[i] = RTK_MAX_NUM_OF_FILTER_FIELD;
            tempIdx[i]  = RTK_MAX_NUM_OF_FILTER_TYPE;
        }


        /* find the position in template */
        for(i = 0; i < fieldSize[fieldPtr->fieldType]; i++)
        {                
            for(j = 0;j < RTK_MAX_NUM_OF_FILTER_TYPE; j++)
            {
                for(k = 0; k < RTK_MAX_NUM_OF_FILTER_FIELD ;k++)
                {
                    if((fieldStartIdx[fieldPtr->fieldType] + i) == aclTemp[j].field[k] )
                    {
                        tempIdx[i] = j;
                        fieldIdx[i] = k;
                    }
                }
            }
        }

        /* if no template match the input field, return err */
        for(i = 0; i < 8; i++)
        {
            if(RTK_MAX_NUM_OF_FILTER_FIELD != tempIdx[i] || RTK_MAX_NUM_OF_FILTER_TYPE != fieldIdx[i])
                break;
        }

        if(8 == i )
            return RT_ERR_FILTER_INACL_RULE_NOT_SUPPORT;

        ret = _rtk_filter_igrAcl_writeDataField(aclRule, tempIdx, fieldIdx, fieldPtr, fieldDataType[fieldPtr->fieldType]);
        if(RT_ERR_OK != ret )
            return ret; 
        
        fieldPtr = fieldPtr->next;
    }

    for(i = 0; i < CARE_TAG_MAX;i++)
    {
        if(0 == pFilter_cfg->careTag.tagType[i].mask )
        {
            careTagMask &=  ~(1 << i);
        }
        else
        {
            careTagMask |= (1 << i);
            if(0 == pFilter_cfg->careTag.tagType[i].value )
                careTagData &= ~(1 << i);
            else
                careTagData |= (1 << i);
        }
    }

    for(i = 0; i < RTK_MAX_NUM_OF_FILTER_TYPE;i++)
    {
        aclRule[i].data_bits.tag_exist = (careTagData) & 0x1FF;
        aclRule[i].care_bits.tag_exist = (careTagMask) & 0x1FF;
    }
    
    if(FILTER_FIELD_DATA_RANGE == pFilter_cfg->activeport.dataType )
    {

        if(pFilter_cfg->activeport.rangeStart >= RTK_MAX_NUM_OF_FILTER_PORT || pFilter_cfg->activeport.rangeEnd >= RTK_MAX_NUM_OF_FILTER_PORT 
          || pFilter_cfg->activeport.rangeStart > pFilter_cfg->activeport.rangeEnd)
            return RT_ERR_INPUT;
    
        for(i = pFilter_cfg->activeport.rangeStart;i <= pFilter_cfg->activeport.rangeEnd;i++)
            aclRule[0].data_bits.active_portmsk |= 1 << i;

        aclRule[0].care_bits.active_portmsk = 0xFFFF;
    }
    else if(FILTER_FIELD_DATA_MASK == pFilter_cfg->activeport.dataType )
    {   
        if(pFilter_cfg->activeport.value >= (1 << RTK_MAX_NUM_OF_FILTER_PORT) || pFilter_cfg->activeport.mask >= (1 << RTK_MAX_NUM_OF_FILTER_PORT))
            return RT_ERR_INPUT;            
        aclRule[0].data_bits.active_portmsk = pFilter_cfg->activeport.value;
        aclRule[0].care_bits.active_portmsk = pFilter_cfg->activeport.mask;
    }
    else
        return RT_ERR_INPUT;
    if(pFilter_cfg->invert >= FILTER_INVERT_END )
        return RT_ERR_INPUT;

    /* check if there are multiple cvlan action */
    if(pFilter_action->actEnable[FILTER_ENACT_INGRESS_CVLAN_INDEX] == TRUE &&
         pFilter_action->actEnable[FILTER_ENACT_INGRESS_CVLAN_VID] == TRUE )
        return RT_ERR_FILTER_INACL_ACT_NOT_SUPPORT;

    /* check if there are multiple forwarding action */
    fwdEnable = FALSE;
    for(i = FILTER_ENACT_TRAP_CPU; i <=  FILTER_ENACT_ADD_DSTPORT; i++)
    {
        if(pFilter_action->actEnable[i] == TRUE )
        {
            if(fwdEnable == FALSE )
                fwdEnable = TRUE;
            else
                return RT_ERR_FILTER_INACL_ACT_NOT_SUPPORT;
        }
    }

    memset(&aclAct, 0, sizeof(rtl8370_acl_act_t));
    aclActCtrl = 0;
    for(i = 0; i < FILTER_ENACT_MAX;i++)
    {    
        if(pFilter_action->actEnable[i] > TRUE )
            return RT_ERR_INPUT;

        if(pFilter_action->actEnable[i] == TRUE )
        {
            switch (i)
            {
            case FILTER_ENACT_INGRESS_CVLAN_INDEX:
                if(pFilter_action->filterIngressCvlanIdx > RTK_VLAN_ID_MAX )
                    return RT_ERR_INPUT;
                aclAct.ct = TRUE;
                aclAct.aclcvid = pFilter_action->filterIngressCvlanIdx;
                aclActCtrl |= ACL_ACT_CVLAN_ENABLE_MASK;
                break;
            case FILTER_ENACT_INGRESS_CVLAN_VID:
                if(pFilter_action->filterIngressCvlanIdx >= RTK_MAX_NUM_OF_VLAN_INDEX )
                    return RT_ERR_INPUT;
                aclAct.ct = FALSE;
                aclAct.aclcvid = pFilter_action->filterIngressCvlanVid;
                aclActCtrl |= ACL_ACT_CVLAN_ENABLE_MASK;
                break;
            case FILTER_ENACT_EGRESS_SVLAN_INDEX:
                if(pFilter_action->filterEgressSvlanIdx >= RTK_MAX_NUM_OF_SVLAN_INDEX )
                    return RT_ERR_INPUT;
                aclAct.aclsvidx = pFilter_action->filterEgressSvlanIdx;
                aclActCtrl |= ACL_ACT_SVLAN_ENABLE_MASK;
                break;
            case FILTER_ENACT_POLICING_0:
                if(pFilter_action->filterPolicingIdx[0] >= RTK_MAX_NUM_OF_METER )
                    return RT_ERR_INPUT;
                aclAct.aclmeteridx = pFilter_action->filterPolicingIdx[0];
                aclActCtrl |= ACL_ACT_POLICING_ENABLE_MASK;
                break;
            case FILTER_ENACT_PRIORITY:
                if(pFilter_action->filterPriority > RTK_DOT1P_PRIORITY_MAX )
                    return RT_ERR_INPUT;
                aclAct.aclpri= pFilter_action->filterPriority;
                aclActCtrl |= ACL_ACT_PRIORITY_ENABLE_MASK;
                break;
            case FILTER_ENACT_DROP:
                aclAct.arpmsk = 0;
                aclAct.mrat = RTK_FILTER_FWD_REDIRECT;
                aclActCtrl |= ACL_ACT_FWD_ENABLE_MASK;
                break;
            case FILTER_ENACT_REDIRECT:
                if(pFilter_action->filterRedirectPortmask >= 1 << 10 )
                    return RT_ERR_INPUT;
                aclAct.arpmsk = pFilter_action->filterRedirectPortmask;
                aclAct.mrat = RTK_FILTER_FWD_REDIRECT;
                aclActCtrl |= ACL_ACT_FWD_ENABLE_MASK;
                break;
            case FILTER_ENACT_ADD_DSTPORT:
                if(pFilter_action->filterAddDstPortmask>= 1 << RTK_MAX_NUM_OF_FILTER_PORT )
                    return RT_ERR_INPUT;
                aclAct.arpmsk = pFilter_action->filterAddDstPortmask;
                aclAct.mrat = RTK_FILTER_FWD_MIRROR;
                aclActCtrl |= ACL_ACT_FWD_ENABLE_MASK;
                break;
            case FILTER_ENACT_MIRROR:
                aclAct.mrat = RTK_FILTER_FWD_MIRRORFUNTION;
                aclActCtrl |= ACL_ACT_FWD_ENABLE_MASK;
                break;
            case FILTER_ENACT_TRAP_CPU:
                aclAct.mrat = RTK_FILTER_FWD_TRAP;
                aclActCtrl |= ACL_ACT_FWD_ENABLE_MASK;
                break;
            case FILTER_ENACT_COPY_CPU:
                aclAct.mrat = RTK_FILTER_FWD_MIRROR;
                if((ret = rtl8370_getAsicCputagTrapPort(&cpuPort)) != SUCCESS)
                    return ret;
                aclAct.arpmsk = 1 << cpuPort;
                aclActCtrl |= ACL_ACT_FWD_ENABLE_MASK;
                break;
            case FILTER_ENACT_EGRESS_SVLAN_VID:
               if (pFilter_action->actEnable[FILTER_ENACT_EGRESS_SVLAN_INDEX] != TRUE)
                {
                    if (pFilter_action->filterEgressSvlanVid > RTK_VLAN_ID_MAX )
                        return RT_ERR_INPUT;
                    matchIdx = 0xFF;
                    for (j = 0; j <= RTK_MAX_NUM_OF_SVLAN_INDEX; j++)
                    {       
                        if ((ret = rtl8370_getAsicSvlanMemberConfiguration(j,&svlanMemConf))!=RT_ERR_OK)
                            return ret;
                        if ((pFilter_action->filterEgressSvlanVid==svlanMemConf.vs_svid) && (svlanMemConf.vs_member!=0))
                        { 
                            matchIdx = j;
                            aclAct.aclsvidx = j;
                            aclActCtrl |= ACL_ACT_SVLAN_ENABLE_MASK;
                            break;
                        }
                    }
                    if (matchIdx == 0xFF)
                           return RT_ERR_SVLAN_ENTRY_NOT_FOUND;
                }
                else
                    return RT_ERR_INPUT;
                break;
            default:
                return RT_ERR_FILTER_INACL_ACT_NOT_SUPPORT;                
            }
        }
    }

    usedRule = 0;
    for(i = 0; i < RTK_MAX_NUM_OF_FILTER_TYPE;i++)
    {
        if(aclRule[i].valid == TRUE )
            usedRule++;
    }

    *ruleNum = usedRule; 

    for(i = filter_id; i < filter_id + usedRule;i++)
    {
        if((ret = rtl8370_getAsicAclRule(i, &tempRule)) != SUCCESS )
            return ret;

        if(tempRule.valid== TRUE )
        {           
            return RT_ERR_TBL_FULL;
        }
    }

    for(i = 0; i < RTK_MAX_NUM_OF_FILTER_TYPE;i++)
    {
        if(aclRule[i].valid == TRUE )
        {         
            /* write ACL action control */
            if((ret = rtl8370_setAsicAclActCtrl(filter_id + i, aclActCtrl)) != SUCCESS )
                return ret;
            /* write ACL action */
            if((ret = rtl8370_setAsicAclAct(filter_id + i, aclAct)) != SUCCESS )
                return ret;
            /* write ACL not */
            if((ret = rtl8370_setAsicAclNot(filter_id + i, pFilter_cfg->invert)) != SUCCESS )
                return ret;  
            /* write ACL rule */
            if((ret = rtl8370_setAsicAclRule(filter_id + i, &aclRule[i])) != SUCCESS )
                return ret;                      
            /* only the first rule will be written with input action control, aclActCtrl of other rules will be zero */
            aclActCtrl = 0;            
            memset(&aclAct, 0, sizeof(rtl8370_acl_act_t));
        }
    }    
 
    return RT_ERR_OK;
}


/* Function Name:
 *      rtk_qos_init
 * Description:
 *      Configure Qos default settings with queue number assigment to each port.
 * Input:
 *      queueNum - Queue number of each port.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_QUEUE_NUM - Invalid queue number.
 *      RT_ERR_INPUT - Invalid input parameters.
 * Note:
 *      This API will initialize related Qos setting with queue number assigment.
 *      The queue number is from 1 to 8.
 */
rtk_api_ret_t rtk_qos_init(rtk_queue_num_t queueNum)
{
    CONST_T uint16 g_prioritytToQid[8][8]= { 
        {0,0,0,0,0,0,0,0}, 
        {0,0,0,0,7,7,7,7}, 
        {0,0,0,0,1,1,7,7}, 
        {0,0,1,1,2,2,7,7},
        {0,0,1,1,2,3,7,7},
        {0,0,1,2,3,4,7,7},
        {0,0,1,2,3,4,5,7},
        {0,1,2,3,4,5,6,7}
    };

    CONST_T uint32 g_priorityDecision[8] = {0x40,0x80,0x04,0x02,0x20,0x01,0x10,0x08};
    CONST_T uint32 g_prioritytRemap[8] = {0,1,2,3,4,5,6,7};

    rtk_api_ret_t retVal;
    uint32 qmapidx;
    uint32 priority;
    uint32 priDec;
    
    uint32 qid;
    uint32 port;
    uint32 dscp;

    if (queueNum <= 0 || queueNum > RTK_MAX_NUM_OF_QUEUE)
        return RT_ERR_QUEUE_NUM;

    /*Set Output Queue Number*/
    if (RTK_MAX_NUM_OF_QUEUE == queueNum)
        qmapidx = 0;
    else
        qmapidx = queueNum;           
    for (port = 0;port<RTK_MAX_NUM_OF_PORT;port++)
    {
        if ((retVal = rtl8370_setAsicOutputQueueMappingIndex(port, qmapidx))!=RT_ERR_OK)
            return retVal;             
    }

    /*Set Priority to Qid*/
    for (priority = 0;priority<RTK_DOT1P_PRIORITY_MAX;priority++)
    {
        if ((retVal = rtl8370_setAsicPriorityToQIDMappingTable(qmapidx, priority, g_prioritytToQid[qmapidx][priority]))!=RT_ERR_OK)
            return retVal;
    }

    /*Set Queue Type to Strict Priority*/
    for (port = 0;port<RTK_MAX_NUM_OF_PORT;port++)
    {
        for (qid = 0;qid<RTK_MAX_NUM_OF_QUEUE;qid++)
        {
            if ((retVal = rtl8370_setAsicQueueType(port,qid,QTYPE_STRICT))!=RT_ERR_OK)
                return retVal;
        }    
    }

    /*Priority Decision Order*/
    for (priDec = 0;priDec<PRIDEC_MAX;priDec++)
    {
        if ((retVal = rtl8370_setAsicPriorityDecision(priDec,g_priorityDecision[priDec]))!=RT_ERR_OK)
            return retVal;
    }

    /*Set Port-based Priority to 0*/
    for (port = 0;port<RTK_MAX_NUM_OF_PORT;port++)
    {
        if ((retVal = rtl8370_setAsicPriorityPortBased(port,0))!=RT_ERR_OK)
            return retVal;    
    }

    /*Disable 1p Remarking*/
    if ((retVal = rtl8370_setAsicRemarkingDot1pAbility(DISABLE))!=RT_ERR_OK)
        return retVal;

    /*Disable DSCP Remarking*/
    if ((retVal = rtl8370_setAsicRemarkingDscpAbility(DISABLE))!=RT_ERR_OK)
        return retVal;

    /*Set 1p & DSCP  Priority Remapping & Remarking*/
    for (priority = 0;priority<RTK_DOT1P_PRIORITY_MAX;priority++)
    {
        if ((retVal = rtl8370_setAsicPriorityDot1qRemapping(priority,g_prioritytRemap[priority]))!=RT_ERR_OK)
            return retVal;

        if ((retVal = rtl8370_setAsicRemarkingDot1pParameter(priority,0))!=RT_ERR_OK)
            return retVal;
        
        if ((retVal = rtl8370_setAsicRemarkingDscpParameter(priority,0))!=RT_ERR_OK)
            return retVal;
    }

    /*Set DSCP Priority*/
    for (dscp = 0;dscp<63;dscp++)
    {
        if ((retVal = rtl8370_setAsicPriorityDscpBased(dscp,0))!=RT_ERR_OK)
            return retVal;
    }


    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_priSel_set
 * Description:
 *      Configure the priority order among different priority mechanism.
 * Input:
 *      pPriDec - Priority assign for port, dscp, 802.1p, cvlan, svlan, acl based priority decision.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_QOS_SEL_PRI_SOURCE - Invalid priority decision source parameter.
 * Note:
 *      ASIC will follow user priority setting of mechanisms to select mapped queue priority for receiving frame. 
 *      If two priority mechanisms are the same, the ASIC will chose the highest priority from mechanisms to 
 *      assign queue priority to receiving frame.  
 *      The priority sources are:
 *      PRIDEC_PORT
 *      PRIDEC_ACL
 *      PRIDEC_DSCP
 *      PRIDEC_1Q
 *      PRIDEC_1AD
 *      PRIDEC_CVLAN
 *      PRIDEC_DA
 *      PRIDEC_SA 
 */

rtk_api_ret_t rtk_qos_priSel_set(rtk_priority_select_t *pPriDec)
{ 
    rtk_api_ret_t retVal;
    uint32 port_pow;
    uint32 dot1q_pow;
    uint32 dscp_pow;
    uint32 acl_pow;
    uint32 svlan_pow;
    uint32 cvlan_pow;
    uint32 smac_pow;
    uint32 dmac_pow;
    
    if (pPriDec->port_pri > 8 || pPriDec->dot1q_pri > 8 || pPriDec->acl_pri > 8 || pPriDec->dscp_pri > 8 ||
       pPriDec->cvlan_pri > 8 || pPriDec->svlan_pri > 8 || pPriDec->dmac_pri > 8 || pPriDec->smac_pri > 8)
        return RT_ERR_QOS_SEL_PRI_SOURCE;

    port_pow = 1;  
    for (; pPriDec->port_pri > 0; pPriDec->port_pri--)
        port_pow = (port_pow)*2;

    dot1q_pow = 1;
    for (; pPriDec->dot1q_pri > 0; pPriDec->dot1q_pri--)
        dot1q_pow = (dot1q_pow)*2;

    acl_pow = 1;
    for (; pPriDec->acl_pri > 0; pPriDec->acl_pri--)
        acl_pow = (acl_pow)*2;

    dscp_pow = 1;
    for (; pPriDec->dscp_pri > 0; pPriDec->dscp_pri--)
        dscp_pow = (dscp_pow)*2;

    svlan_pow = 1;
    for (; pPriDec->svlan_pri > 0; pPriDec->svlan_pri--)
        svlan_pow = (svlan_pow)*2;

    cvlan_pow = 1;
    for (; pPriDec->cvlan_pri > 0; pPriDec->cvlan_pri--)
        cvlan_pow = (cvlan_pow)*2;

    dmac_pow = 1;
    for (; pPriDec->dmac_pri > 0; pPriDec->dmac_pri--)
        dmac_pow = (dmac_pow)*2;

    smac_pow = 1;
    for (; pPriDec->smac_pri > 0; pPriDec->smac_pri--)
        smac_pow = (smac_pow)*2;   

    if ((retVal = rtl8370_setAsicPriorityDecision(PRIDEC_PORT,port_pow))!=RT_ERR_OK)
        return retVal;

    if ((retVal = rtl8370_setAsicPriorityDecision(PRIDEC_ACL,acl_pow))!=RT_ERR_OK)
        return retVal;

    if ((retVal = rtl8370_setAsicPriorityDecision(PRIDEC_DSCP,dscp_pow))!=RT_ERR_OK)
        return retVal;

    if ((retVal = rtl8370_setAsicPriorityDecision(PRIDEC_1Q,dot1q_pow))!=RT_ERR_OK)
        return retVal;
    
    if ((retVal = rtl8370_setAsicPriorityDecision(PRIDEC_1AD,svlan_pow))!=RT_ERR_OK)
        return retVal;

    if ((retVal = rtl8370_setAsicPriorityDecision(PRIDEC_CVLAN,cvlan_pow))!=RT_ERR_OK)
        return retVal;
    
    if ((retVal = rtl8370_setAsicPriorityDecision(PRIDEC_DA,dmac_pow))!=RT_ERR_OK)
        return retVal;
    
    if ((retVal = rtl8370_setAsicPriorityDecision(PRIDEC_SA,smac_pow))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_priSel_get
 * Description:
 *      Get the priority order configuration among different priority mechanism.
 * Input:
 *      None
 * Output:
 *      pPriDec - Priority assign for port, dscp, 802.1p, cvlan, svlan, acl based priority decision .
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 * Note:
 *      ASIC will follow user priority setting of mechanisms to select mapped queue priority for receiving frame. 
 *      If two priority mechanisms are the same, the ASIC will chose the highest priority from mechanisms to 
 *      assign queue priority to receiving frame. 
 *      The priority sources are:
 *      PRIDEC_PORT,
 *      PRIDEC_ACL,
 *      PRIDEC_DSCP,
 *      PRIDEC_1Q,
 *      PRIDEC_1AD,
 *      PRIDEC_CVLAN,
 *      PRIDEC_DA,
 *      PRIDEC_SA,
 */

rtk_api_ret_t rtk_qos_priSel_get(rtk_priority_select_t *pPriDec)
{
    rtk_api_ret_t retVal;
    uint32 i;
    uint32 port_pow;
    uint32 dot1q_pow;
    uint32 dscp_pow;
    uint32 acl_pow;
    uint32 svlan_pow;
    uint32 cvlan_pow;
    uint32 smac_pow;
    uint32 dmac_pow;    

    if ((retVal = rtl8370_getAsicPriorityDecision(PRIDEC_PORT,&port_pow))!=RT_ERR_OK)
        return retVal;

    if ((retVal = rtl8370_getAsicPriorityDecision(PRIDEC_ACL,&acl_pow))!=RT_ERR_OK)
        return retVal;

    if ((retVal = rtl8370_getAsicPriorityDecision(PRIDEC_DSCP,&dscp_pow))!=RT_ERR_OK)
        return retVal;

    if ((retVal = rtl8370_getAsicPriorityDecision(PRIDEC_1Q,&dot1q_pow))!=RT_ERR_OK)
        return retVal;
    
    if ((retVal = rtl8370_getAsicPriorityDecision(PRIDEC_1AD,&svlan_pow))!=RT_ERR_OK)
        return retVal;

    if ((retVal = rtl8370_getAsicPriorityDecision(PRIDEC_CVLAN,&cvlan_pow))!=RT_ERR_OK)
        return retVal;
    
    if ((retVal = rtl8370_getAsicPriorityDecision(PRIDEC_DA,&dmac_pow))!=RT_ERR_OK)
        return retVal;
    
    if ((retVal = rtl8370_getAsicPriorityDecision(PRIDEC_SA,&smac_pow))!=RT_ERR_OK)
        return retVal;

    for (i=31;i>=0;i--)
    {
        if (port_pow&(1<<i))
        {
            pPriDec->port_pri = i;
            break;
        }
    }
    
    for (i=31;i>=0;i--)
    {
        if (dot1q_pow&(1<<i))
        {
            pPriDec->dot1q_pri = i;
            break;
        }
    }

    for (i=31;i>=0;i--)
    {
        if (acl_pow&(1<<i))
        {
            pPriDec->acl_pri = i;
            break;
        }
    }

    for (i=31;i>=0;i--)
    {
        if (dscp_pow&(1<<i))
        {
            pPriDec->dscp_pri = i;
            break;
        }
    }

    for (i=31;i>=0;i--)
    {
        if (svlan_pow&(1<<i))
        {
            pPriDec->svlan_pri = i;
            break;
        }
    }

    for (i=31;i>=0;i--)
    {
        if (cvlan_pow&(1<<i))
        {
            pPriDec->cvlan_pri = i;
            break;
        }
    }

    for (i=31;i>=0;i--)
    {
        if (dmac_pow&(1<<i))
        {
            pPriDec->dmac_pri = i;
            break;
        }
    }

    for (i=31;i>=0;i--)
    {
        if (smac_pow&(1<<i))
        {
            pPriDec->smac_pri = i;
            break;
        }
    }

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_1pPriRemap_set
 * Description:
 *      Configure 1Q priorities mapping to internal absolute priority.
 * Input:
 *      dot1p_pri - 802.1p priority value.
 *      int_pri - internal priority value.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 *      RT_ERR_VLAN_PRIORITY - Invalid 1p priority.
 *      RT_ERR_QOS_INT_PRIORITY - Invalid priority. 
 * Note:
 *      Priority of 802.1Q assignment for internal asic priority, and it is used for queue usage and packet scheduling.
 */
rtk_api_ret_t rtk_qos_1pPriRemap_set(rtk_pri_t dot1p_pri, rtk_pri_t int_pri)
{
    rtk_api_ret_t retVal;

    if (int_pri>RTK_DOT1P_PRIORITY_MAX)
        return  RT_ERR_QOS_INT_PRIORITY;

    if (dot1p_pri>RTK_DOT1P_PRIORITY_MAX||int_pri>RTK_DOT1P_PRIORITY_MAX)
        return  RT_ERR_VLAN_PRIORITY;
    
    if ((retVal = rtl8370_setAsicPriorityDot1qRemapping(dot1p_pri, int_pri))!=RT_ERR_OK)
        return retVal;
    
    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_1pPriRemap_get
 * Description:
 *      Get 1Q priorities mapping to internal absolute priority.  
 * Input:
 *      dot1p_pri - 802.1p priority value .
 * Output:
 *      pInt_pri - internal priority value.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_VLAN_PRIORITY - Invalid priority.
 *      RT_ERR_QOS_INT_PRIORITY - Invalid priority. 
 * Note:
 *      Priority of 802.1Q assigment for internal asic priority, and it is uesed for queue usage and packet scheduling.
 */
rtk_api_ret_t rtk_qos_1pPriRemap_get(rtk_pri_t dot1p_pri, rtk_pri_t *pInt_pri)
{
    rtk_api_ret_t retVal;

    if (dot1p_pri>RTK_DOT1P_PRIORITY_MAX)
        return  RT_ERR_QOS_INT_PRIORITY;
    

    if ((retVal = rtl8370_getAsicPriorityDot1qRemapping(dot1p_pri, pInt_pri))!=RT_ERR_OK)
        return retVal;
    
    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_dscpPriRemap_set
 * Description:
 *      Map dscp value to internal priority.
 * Input:
 *      dscp - Dscp value of receiving frame
 *      int_pri - internal priority value .
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 *      RT_ERR_QOS_DSCP_VALUE - Invalid DSCP value. 
 *      RT_ERR_QOS_INT_PRIORITY - Invalid priority. 
 * Note:
 *      The Differentiated Service Code Point is a selector for router's per-hop behaviors. As a selector, there is no implication that a numerically 
 *      greater DSCP implies a better network service. As can be seen, the DSCP totally overlaps the old precedence field of TOS. So if values of 
 *      DSCP are carefully chosen then backward compatibility can be achieved.    
 */
rtk_api_ret_t rtk_qos_dscpPriRemap_set(rtk_dscp_t dscp, rtk_pri_t int_pri)
{
    rtk_api_ret_t retVal;

    if (int_pri > RTK_DOT1P_PRIORITY_MAX )
        return RT_ERR_QOS_INT_PRIORITY; 

    if (dscp > RTK_VALUE_OF_DSCP_MAX)
        return RT_ERR_QOS_DSCP_VALUE; 

    if ((retVal = rtl8370_setAsicPriorityDscpBased(dscp,int_pri))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;    
}


/* Function Name:
 *      rtk_qos_dscpPriRemap_get
 * Description:
 *      Get dscp value to internal priority.
 * Input:
 *      dscp - Dscp value of receiving frame
 * Output:
 *      pInt_pri - internal priority value. 
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_QOS_DSCP_VALUE - Invalid DSCP value. 
 * Note:
 *      The Differentiated Service Code Point is a selector for router's per-hop behaviors. As a selector, there is no implication that a numerically 
 *      greater DSCP implies a better network service. As can be seen, the DSCP totally overlaps the old precedence field of TOS. So if values of 
 *      DSCP are carefully chosen then backward compatibility can be achieved.    
 */
rtk_api_ret_t rtk_qos_dscpPriRemap_get(rtk_dscp_t dscp, rtk_pri_t *pInt_pri)
{
    rtk_api_ret_t retVal;

    if (dscp > RTK_VALUE_OF_DSCP_MAX)
        return RT_ERR_QOS_DSCP_VALUE; 

    if ((retVal = rtl8370_getAsicPriorityDscpBased(dscp,pInt_pri))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;    
}

/* Function Name:
 *      rtk_qos_portPri_set
 * Description:
 *      Configure priority usage to each port.
 * Input:
 *      port - Port id.
 *      int_pri - Internal priority value. 
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_QOS_SEL_PORT_PRI - Invalid port priority.
 *      RT_ERR_QOS_INT_PRIORITY - Invalid priority. 
 * Note:
 *      The API can set priority of port assignments for queue usage and packet scheduling.
 */
rtk_api_ret_t rtk_qos_portPri_set(rtk_port_t port, rtk_pri_t int_pri)
{
    rtk_api_ret_t retVal;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID; 

    if (int_pri > RTK_DOT1P_PRIORITY_MAX )
        return RT_ERR_QOS_INT_PRIORITY; 

    if ((retVal = rtl8370_setAsicPriorityPortBased(port, int_pri))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_portPri_get
 * Description:
 *      Get priority usage to each port.
 * Input:
 *      port - Port id.
 * Output:
 *      pInt_pri - Internal priority value. 
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number. 
 *      RT_ERR_INPUT - Invalid input parameters.
 * Note:
 *      The API can get priority of port assignments for queue usage and packet scheduling.
 */
rtk_api_ret_t rtk_qos_portPri_get(rtk_port_t port, rtk_pri_t *pInt_pri)
{
    rtk_api_ret_t retVal;
    
    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID; 

    if ((retVal = rtl8370_getAsicPriorityPortBased(port, pInt_pri))!=RT_ERR_OK)
        return retVal;


    return RT_ERR_OK;
}
/* Function Name:
 *      rtk_qos_queueNum_set
 * Description:
 *      Set output queue number for each port.
 * Input:
 *      port - Port id.
 *      queue_num - Queue number
 * Output:
 *      None 
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_QUEUE_NUM - Invalid queue number. 
 * Note:
 *      The API can set the output queue number of the specified port. The queue number is from 1 to 8.
 */
rtk_api_ret_t rtk_qos_queueNum_set(rtk_port_t port, rtk_queue_num_t queue_num)
{
    rtk_api_ret_t retVal;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;

    if (( 0 == queue_num) || (queue_num > RTK_MAX_NUM_OF_QUEUE)) 
        return RT_ERR_FAILED;

    if (RTK_MAX_NUM_OF_QUEUE== queue_num)
        queue_num = 0;

    if ((retVal = rtl8370_setAsicOutputQueueMappingIndex(port, queue_num))!=RT_ERR_OK)
        return retVal;      

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_queueNum_get
 * Description:
 *      Get output queue number.
 * Input:
 *      port - Port id.
 * Output:
 *      pQueue_num - Mapping queue number
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number. 
 * Note:
 *      The API will return the output queue number of the specified port. The queue number is from 1 to 8.
 */
rtk_api_ret_t rtk_qos_queueNum_get(rtk_port_t port, rtk_queue_num_t *pQueue_num)
{
    rtk_api_ret_t retVal;
    uint32 qidx;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;

    if ((retVal = rtl8370_getAsicOutputQueueMappingIndex(port, &qidx))!=RT_ERR_OK)
        return retVal;  

    if (0 == qidx)
        *pQueue_num    = 8;
    else
        *pQueue_num = qidx;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_priMap_set
 * Description:
 *      Set output queue number for each port.
 * Input:
 *      queue_num - Queue number usage.
 *      pPri2qid - Priority mapping to queue ID.
 * Output:
 *      None 
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 *      RT_ERR_QUEUE_NUM - Invalid queue number. 
 *      RT_ERR_QUEUE_ID - Invalid queue id.
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_QOS_INT_PRIORITY - Invalid priority.
 * Note:
 *      ASIC supports priority mapping to queue with different queue number from 1 to 8.
 *      For different queue numbers usage, ASIC supports different internal available queue IDs.
 */
rtk_api_ret_t rtk_qos_priMap_set(rtk_queue_num_t queue_num, rtk_qos_pri2queue_t *pPri2qid)
{    
    rtk_api_ret_t retVal;
    uint32 pri;
    
    if ((0 == queue_num) || (queue_num > RTK_MAX_NUM_OF_QUEUE)) 
        return RT_ERR_QUEUE_NUM;

    for (pri=0;pri<=RTK_DOT1P_PRIORITY_MAX;pri++)
    {
        if (pPri2qid->pri2queue[pri] > RTK_QUEUE_ID_MAX) 
            return RT_ERR_QUEUE_ID;

        if ((retVal =  rtl8370_setAsicPriorityToQIDMappingTable(queue_num-1,pri,pPri2qid->pri2queue[pri]))!=RT_ERR_OK)
            return retVal;        
    }

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_priMap_get
 * Description:
 *      Get priority to queue ID mapping table parameters.
 * Input:
 *      queue_num - Queue number usage. 
 * Output:
 *      pPri2qid - Priority mapping to queue ID.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 *      RT_ERR_QUEUE_NUM - Invalid queue number.
 * Note:
 *      The API can return the mapping queue id of the specified priority and queue number. 
 *      The queue number is from 1 to 8.
 */
rtk_api_ret_t rtk_qos_priMap_get(rtk_queue_num_t queue_num, rtk_qos_pri2queue_t *pPri2qid)
{
    rtk_api_ret_t retVal;
    uint32 pri;
    
    if ((0 == queue_num) || (queue_num > RTK_MAX_NUM_OF_QUEUE)) 
        return RT_ERR_QUEUE_NUM;

    for (pri=0;pri<=RTK_DOT1P_PRIORITY_MAX;pri++)
    {
        if ((retVal =  rtl8370_getAsicPriorityToQIDMappingTable(queue_num-1,pri,&pPri2qid->pri2queue[pri]))!=RT_ERR_OK)
            return retVal;        
    }
    
    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_schedulingQueue_set
 * Description:
 *      Set weight and type of queues in dedicated port.
 * Input:
 *      port - Port id.
 *      pQweights - The array of weights for WRR/WFQ queue (0 for STRICT_PRIORITY queue).
 * Output:
 *      None 
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_QOS_QUEUE_WEIGHT - Invalid queue weight. 
 * Note:
 *      The API can set weight and type, strict priority or weight fair queue (WFQ) for 
 *      dedicated port for using queues. If queue id is not included in queue usage, 
 *      then its type and weight setting in dummy for setting. There are priorities 
 *      as queue id in strict queues. It means strict queue id 5 carrying higher priority 
 *      than strict queue id 4. The WFQ queue weight is from 1 to 128, and weight 0 is 
 *      for strict priority queue type.
 */
rtk_api_ret_t rtk_qos_schedulingQueue_set(rtk_port_t port,rtk_qos_queue_weights_t *pQweights)
{
    rtk_api_ret_t retVal;
    uint32 qid;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_FAILED;


    for (qid = 0;qid<RTK_MAX_NUM_OF_QUEUE;qid ++)
    {

        if (pQweights->weights[qid] > QOS_WEIGHT_MAX)
            return RT_ERR_QOS_QUEUE_WEIGHT;

        if (0 == pQweights->weights[qid])
        {
            if ((retVal = rtl8370_setAsicQueueType(port, qid, QTYPE_STRICT))!=RT_ERR_OK)
                return retVal;
        }
        else
        {
            if ((retVal = rtl8370_setAsicQueueType(port, qid, QTYPE_WFQ))!=RT_ERR_OK)
                return retVal;
        
            if ((retVal = rtl8370_setAsicWFQWeight(port,qid,pQweights->weights[qid]))!=RT_ERR_OK)
                return retVal;
        }
    }

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_schedulingQueue_get
 * Description:
 *      Get weight and type of queues in dedicated port.
 * Input:
 *      port - Port id.
 * Output:
 *      pQweights - The array of weights for WRR/WFQ queue (0 for STRICT_PRIORITY queue).
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 *      RT_ERR_PORT_ID - Invalid port number.
 * Note:
 *      The API can get weight and type, strict priority or weight fair queue (WFQ) for dedicated port for using queues.
 *      The WFQ queue weight is from 1 to 128, and weight 0 is for strict priority queue type.
 */   
rtk_api_ret_t rtk_qos_schedulingQueue_get(rtk_port_t port, rtk_qos_queue_weights_t *pQweights)
{
    rtk_api_ret_t retVal;
    uint32 qid,qtype,qweight;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_FAILED;


    for (qid = 0;qid<RTK_MAX_NUM_OF_QUEUE;qid ++)
    {
        if ((retVal = rtl8370_getAsicQueueType(port, qid, &qtype))!=RT_ERR_OK)
            return retVal;

        if (QTYPE_STRICT == qtype)
           {
          pQweights->weights[qid]=0;
           }
        else if (QTYPE_WFQ == qtype)
        {
            if ((retVal = rtl8370_getAsicWFQWeight(port,qid,&qweight))!=RT_ERR_OK)
                return retVal;
            pQweights->weights[qid]=qweight;
        }
    }
    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_1pRemarkEnable_set
 * Description:
 *      Set weight and type of queues in dedicated port.
 * Input:
 *      port - Port id.
 *      enable - Status of 802.1p remark.
 * Output:
 *      None 
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_ENABLE - Invalid enable parameter.
 * Note:
 *      The API can enable or disable 802.1p remarking ability for whole system. 
 *      The status of 802.1p remark:
 *      DISABLED
 *      ENABLED
 */
rtk_api_ret_t rtk_qos_1pRemarkEnable_set(rtk_port_t port, rtk_enable_t enable)
{
    rtk_api_ret_t retVal;

    /*for whole system function, the port value should be 0xFF*/
    if (port != RTK_WHOLE_SYSTEM)
        return RT_ERR_PORT_ID;
    
    if (enable>=RTK_ENABLE_END)
        return RT_ERR_INPUT;    

    if ((retVal = rtl8370_setAsicRemarkingDot1pAbility(enable))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_1pRemarkEnable_get
 * Description:
 *      Get 802.1p remarking ability. 
 * Input:
 *      port - Port id.
 * Output:
 *      pEnable - Status of 802.1p remark.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 * Note:
 *      The API can get 802.1p remarking ability.
 *      The status of 802.1p remark:
 *      DISABLED
 *      ENABLED
 */
rtk_api_ret_t rtk_qos_1pRemarkEnable_get(rtk_port_t port, rtk_data_t *pEnable)
{
    rtk_api_ret_t retVal;

    /*for whole system function, the port value should be 0xFF*/
    if (port != RTK_WHOLE_SYSTEM)
        return RT_ERR_PORT_ID;

    if ((retVal = rtl8370_getAsicRemarkingDot1pAbility(pEnable))!=RT_ERR_OK)
        return retVal;
    
    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_1pRemark_set
 * Description:
 *      Set 802.1p remarking parameter.
 * Input:
 *      int_pri - Internal priority value.
 *      dot1p_pri - 802.1p priority value.
 * Output:
 *      None 
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_VLAN_PRIORITY - Invalid 1p priority.
 *      RT_ERR_QOS_INT_PRIORITY - Invalid priority.
 * Note:
 *      The API can set 802.1p parameters source priority and new priority.
 */
rtk_api_ret_t rtk_qos_1pRemark_set(rtk_pri_t int_pri, rtk_pri_t dot1p_pri)
{
    rtk_api_ret_t retVal;

    if (int_pri > RTK_DOT1P_PRIORITY_MAX )
        return RT_ERR_QOS_INT_PRIORITY; 

    if (dot1p_pri > RTK_DOT1P_PRIORITY_MAX)
        return RT_ERR_VLAN_PRIORITY; 

    if ((retVal = rtl8370_setAsicRemarkingDot1pParameter(int_pri, dot1p_pri))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_1pRemark_get
 * Description:
 *      Get 802.1p remarking parameter.
 * Input:
 *      int_pri - Internal priority value.
 * Output:
 *      pDot1p_pri - 802.1p priority value.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_QOS_INT_PRIORITY - Invalid priority. 
 * Note:
 *      The API can get 802.1p remarking parameters. It would return new priority of ingress priority. 
 */
rtk_api_ret_t rtk_qos_1pRemark_get(rtk_pri_t int_pri, rtk_pri_t *pDot1p_pri)
{
    rtk_api_ret_t retVal;
    
    if (int_pri > RTK_DOT1P_PRIORITY_MAX )
        return RT_ERR_QOS_INT_PRIORITY; 

    if ((retVal = rtl8370_getAsicRemarkingDot1pParameter(int_pri, pDot1p_pri))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_dscpRemarkEnable_set
 * Description:
 *      Set DSCP remarking ability.
 * Input:
 *      port - Port id.
 *      enable - status of DSCP remark.
 * Output:
 *      None 
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_QOS_INT_PRIORITY - Invalid priority.
 *      RT_ERR_ENABLE - Invalid enable parameter.
 * Note:
 *      The API can enable or disable DSCP remarking ability for whole system.
 *      The status of DSCP remark:
 *      DISABLED
 *      ENABLED 
 */
rtk_api_ret_t rtk_qos_dscpRemarkEnable_set(rtk_port_t port, rtk_enable_t enable)
{
    rtk_api_ret_t retVal;
    
    /*for whole system function, the port value should be 0xFF*/
    if (port != RTK_WHOLE_SYSTEM)
        return RT_ERR_PORT_ID;

    if (enable>=RTK_ENABLE_END)
        return RT_ERR_INPUT;

    if ((retVal = rtl8370_setAsicRemarkingDscpAbility(enable))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_dscpRemarkEnable_get
 * Description:
 *      Get DSCP remarking ability.
 * Input:
 *      port - Port id.
 * Output:
 *      pEnable - status of DSCP remarking.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 * Note:
 *      The API can get DSCP remarking ability.
 *      The status of DSCP remark:
 *      DISABLED
 *      ENABLED
 */
rtk_api_ret_t rtk_qos_dscpRemarkEnable_get(rtk_port_t port, rtk_data_t *pEnable)
{
    rtk_api_ret_t retVal;

    /*for whole system function, the port value should be 0xFF*/
    if (port != RTK_WHOLE_SYSTEM)
        return RT_ERR_PORT_ID;

    if ((retVal = rtl8370_getAsicRemarkingDscpAbility(pEnable))!=RT_ERR_OK)
        return retVal;
    
    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_qos_dscpRemark_set
 * Description:
 *      Set DSCP remarking parameter.
 * Input:
 *      int_pri - Internal priority value.
 *      dscp - DSCP value.
 * Output:
 *      None 
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_QOS_INT_PRIORITY - Invalid priority. 
 *      RT_ERR_QOS_DSCP_VALUE - Invalid DSCP value. 
 * Note:
 *      The API can set DSCP value and mapping priority.
 */
rtk_api_ret_t rtk_qos_dscpRemark_set(rtk_pri_t int_pri, rtk_dscp_t dscp)
{
    rtk_api_ret_t retVal;

    if (int_pri > RTK_DOT1P_PRIORITY_MAX )
        return RT_ERR_QOS_INT_PRIORITY; 

    if (dscp > RTK_VALUE_OF_DSCP_MAX)
        return RT_ERR_QOS_DSCP_VALUE;     

    if ((retVal = rtl8370_setAsicRemarkingDscpParameter(int_pri, dscp))!=RT_ERR_OK)
        return retVal;
    
    return RT_ERR_OK;
}


/* Function Name:
 *      rtk_qos_dscpRemark_get
 * Description:
 *      Get DSCP remarking parameter.
 * Input:
 *      int_pri - Internal priority value.
 * Output:
 *      Dscp |DSCP value.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_QOS_INT_PRIORITY - Invalid priority. 
 * Note:
 *      The API can get DSCP parameters. It would return DSCP value for mapping priority.
 */
rtk_api_ret_t rtk_qos_dscpRemark_get(rtk_pri_t int_pri, rtk_dscp_t *pDscp)
{
    rtk_api_ret_t retVal;

    if (int_pri > RTK_DOT1P_PRIORITY_MAX )
        return RT_ERR_QOS_INT_PRIORITY; 

    if ((retVal = rtl8370_getAsicRemarkingDscpParameter(int_pri, pDscp))!=RT_ERR_OK)
        return retVal;
    
    return RT_ERR_OK;
}

int rtk_qos_cfg(void)
{
    rtk_api_ret_t RetVal;
    rtk_priority_select_t PriDec;
    //分为2个优先级
    // mtg dsp命令报文 用户板通讯报文最高优先级
    // 其他报文优先级最低
    RetVal = rtk_qos_init(8);
    if( RetVal != RT_ERR_OK )
    {
        printk("rtk_qos_init failed.\n");
        return RetVal;
    }
    
    RetVal = rtk_qos_priSel_get(&PriDec);
    if( RetVal != RT_ERR_OK )
    {
        printk("rtk_qos_priSel_get failed.\n");
        return RetVal;
    } 
    printk("RTK8370M qos_priSel_get port_pri:%u dot1q_pri:%u acl_pri:%u dscp_pri:%u cvlan_pri:%u svlan_pri:%u dmac_pri:%u smac_pri:%u\n",
            PriDec.port_pri,PriDec.dot1q_pri,PriDec.acl_pri,PriDec.dscp_pri,
            PriDec.cvlan_pri,PriDec.svlan_pri,PriDec.dmac_pri,PriDec.smac_pri);    
    return RetVal; 
}


/* Function Name:
 *      rtk_vlan_init
 * Description:
 *      Initialize VLAN.
 * Input:
 *      None
 * Output:
 *      None 
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 * Note:
 *      VLAN is disabled by default. User has to call this API to enable VLAN before
 *      using it. And It will set a default VLAN(vid 1) including all ports and set 
 *      all ports PVID to the default VLAN.
 */
rtk_api_ret_t rtk_vlan_init(void)
{
    rtk_api_ret_t retVal;
    uint32 i;
    rtl8370_user_vlan4kentry vlan4K;
    rtl8370_vlanconfiguser vlanMC;
    
    
    /* clean 32 VLAN member configuration */
    for (i = 0; i < RTK_MAX_NUM_OF_VLAN_INDEX; i++)
    {    
        vlanMC.evid = 0;
        vlanMC.lurep = 0;
        vlanMC.mbr = 0;        
        vlanMC.msti = 0;            
        vlanMC.fid = 0;
        vlanMC.meteridx = 0;        
        vlanMC.envlanpol= 0;            
        vlanMC.vbpen= 0;
        vlanMC.vbpri= 0;        
        if ((retVal = rtl8370_setAsicVlanMemberConfig(i, &vlanMC))!=RT_ERR_OK)
            return retVal;
        
    }

    /* Set a default VLAN with vid 1 to 4K table for all ports */
    memset(&vlan4K,0,sizeof(rtl8370_user_vlan4kentry));
    vlan4K.vid = 1;
    vlan4K.mbr = RTK_MAX_PORT_MASK;
    vlan4K.untag = RTK_MAX_PORT_MASK;
    vlan4K.fid = 0;    
    if ((retVal = rtl8370_setAsicVlan4kEntry(&vlan4K))!=RT_ERR_OK)
        return retVal;
    

    /* Also set the default VLAN to 32 member configuration index 0 */
    memset(&vlanMC,0,sizeof(rtl8370_vlanconfiguser));
    vlanMC.evid = 1;
    vlanMC.mbr = RTK_MAX_PORT_MASK;                   
    vlanMC.fid = 0;
    if ((retVal = rtl8370_setAsicVlanMemberConfig(0, &vlanMC))!=RT_ERR_OK)
            return retVal;  

    /* Set all ports PVID to default VLAN and tag-mode to original */    
    for (i = 0; i < RTK_MAX_NUM_OF_PORT; i++)
    {    
        if ((retVal = rtl8370_setAsicVlanPortBasedVID(i, 0, 0))!=RT_ERR_OK)
            return retVal;  
        if ((retVal = rtl8370_setAsicVlanEgressTagMode(i, EG_TAG_MODE_ORI))!=RT_ERR_OK)
            return retVal;        
    }    

    /* enable VLAN */   
    if ((retVal = rtl8370_setAsicVlanFilter(TRUE))!=RT_ERR_OK)
        return retVal;  
       
    return RT_ERR_OK;
}


/* Function Name:
 *      rtk_vlan_set
 * Description:
 *      Set a VLAN entry.
 * Input:
 *      vid - VLAN ID to configure.
 *      mbrmsk - VLAN member set portmask.
 *      untagmsk - VLAN untag set portmask.
 *      fid - filtering database.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 *      RT_ERR_L2_FID - Invalid FID.
 *      RT_ERR_VLAN_PORT_MBR_EXIST - Invalid member port mask.
 *      RT_ERR_VLAN_VID - Invalid VID parameter.
 * Note:
 *      There are 4K VLAN entry supported. User could configure the member set and untag set
 *      for specified vid through this API. The portmask's bit N means port N.
 *      For example, mbrmask 23=0x17=010111 means port 0,1,2,4 in the member set.
 *      FID is for SVL/IVL usage, and the range is 0~4095.
 */
rtk_api_ret_t rtk_vlan_set(rtk_vlan_t vid, rtk_portmask_t mbrmsk, rtk_portmask_t untagmsk, rtk_fid_t fid)
{
    rtk_api_ret_t retVal;
    rtl8370_user_vlan4kentry vlan4K;   
    
    /* vid must be 0~4095 */
    if (vid > RTK_VLAN_ID_MAX)
        return RT_ERR_VLAN_VID;

    if (mbrmsk.bits[0] > RTK_MAX_PORT_MASK)
        return RT_ERR_VLAN_PORT_MBR_EXIST;

    if (untagmsk.bits[0] > RTK_MAX_PORT_MASK)
        return RT_ERR_VLAN_PORT_MBR_EXIST;

    if (fid > RTK_FID_MAX)
        return RT_ERR_L2_FID;

    /* update 4K table */
    memset(&vlan4K,0,sizeof(rtl8370_user_vlan4kentry));    
    vlan4K.vid = vid;            
    vlan4K.mbr = mbrmsk.bits[0];
    vlan4K.untag = untagmsk.bits[0];
    vlan4K.fid = fid;    
    if ((retVal = rtl8370_setAsicVlan4kEntry(&vlan4K))!=RT_ERR_OK)
            return retVal;
    
    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_vlan_get
 * Description:
 *      Get a VLAN entry.
 * Input:
 *      vid - VLAN ID to configure.
 * Output:
 *      pMbrmsk - VLAN member set portmask.
 *      pUntagmsk - VLAN untag set portmask.
 *      pFid - filtering database. 
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 *      RT_ERR_VLAN_VID - Invalid VID parameter.
 * Note:
 *     The API can get the member set, untag set and fid settings for specified vid.
 */
rtk_api_ret_t rtk_vlan_get(rtk_vlan_t vid, rtk_portmask_t *pMbrmsk, rtk_portmask_t *pUntagmsk, rtk_fid_t *pFid)
{
    rtk_api_ret_t retVal;
    rtl8370_user_vlan4kentry vlan4K;
    
    /* vid must be 0~4095 */
    if (vid > RTK_VLAN_ID_MAX)
        return RT_ERR_VLAN_VID;

    vlan4K.vid = vid;

    if ((retVal = rtl8370_getAsicVlan4kEntry(&vlan4K))!=RT_ERR_OK)
        return retVal;    

    pMbrmsk->bits[0] = vlan4K.mbr;
    pUntagmsk->bits[0] = vlan4K.untag;    
    *pFid = vlan4K.fid;
    return RT_ERR_OK;
}

/* Function Name:
 *     rtk_vlan_portPvid_set
 * Description:
 *      Set port to specified VLAN ID(PVID).
 * Input:
 *      port - Port id.
 *      pvid - Specified VLAN ID.
 *      priority - 802.1p priority for the PVID.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_VLAN_PRIORITY - Invalid priority. 
 *      RT_ERR_VLAN_ENTRY_NOT_FOUND - VLAN entry not found.
 *      RT_ERR_VLAN_VID - Invalid VID parameter.
 * Note:
 *       The API is used for Port-based VLAN. The untagged frame received from the
 *       port will be classified to the specified VLAN and assigned to the specified priority.
 */
rtk_api_ret_t rtk_vlan_portPvid_set(rtk_port_t port, rtk_vlan_t pvid, rtk_pri_t priority)
{
    rtk_api_ret_t retVal;
    int32 i;
    uint32 j;
      uint32 k;
    uint32 index,empty_idx;
      uint32 gvidx,proc;
    uint32  bUsed,pri;    
    rtl8370_user_vlan4kentry vlan4K;
    rtl8370_vlanconfiguser vlanMC;    
    rtl8370_protocolvlancfg ppb_vlan_cfg;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;
    
    /* vid must be 0~4095 */
    if (pvid > RTK_VLAN_ID_MAX)
        return RT_ERR_VLAN_VID;

    /* priority must be 0~7 */
    if (priority > RTK_DOT1P_PRIORITY_MAX)
        return RT_ERR_VLAN_PRIORITY;


      empty_idx = 0xFFFF;

    for (i = (RTK_MAX_NUM_OF_VLAN_INDEX-1); i >= 0; i--)
    {               
        if ((retVal = rtl8370_getAsicVlanMemberConfig(i, &vlanMC))!=RT_ERR_OK)
            return retVal;
            
        if (pvid == vlanMC.evid)
        {          
            if ((retVal = rtl8370_setAsicVlanPortBasedVID(port,i,priority))!=RT_ERR_OK)
                return retVal;
            
            return RT_ERR_OK;
        }
        else if (vlanMC.evid == 0 && vlanMC.mbr == 0)
        {
            empty_idx = i;
            }
    }


    /*
        vid doesn't exist in 32 member configuration. Find an empty entry in 
        32 member configuration, then copy entry from 4K. If 32 member configuration
        are all full, then find an entry which not used by Port-based VLAN and 
        then replace it with 4K. Finally, assign the index to the port.
    */ 

    if (empty_idx!=0xFFFF)
    {
        vlan4K.vid = pvid;
        if ((retVal = rtl8370_getAsicVlan4kEntry(&vlan4K))!=RT_ERR_OK)
            return retVal;

        vlanMC.evid = pvid;
        vlanMC.mbr = vlan4K.mbr;                   
        vlanMC.fid = vlan4K.fid;
        vlanMC.msti= vlan4K.msti;
        vlanMC.meteridx= vlan4K.meteridx;
        vlanMC.envlanpol= vlan4K.envlanpol;
        vlanMC.lurep= vlan4K.lurep;    

        if ((retVal = rtl8370_setAsicVlanMemberConfig(empty_idx,&vlanMC))!=RT_ERR_OK)
            return retVal; 

        if ((retVal = rtl8370_setAsicVlanPortBasedVID(port,empty_idx,priority))!=RT_ERR_OK)
            return retVal;  
        
        return RT_ERR_OK;            
     }       

    if ((retVal = rtl8370_getAsic1xGuestVidx(&gvidx))!=RT_ERR_OK)
        return retVal; 

    /* 32 member configuration is full, found a unused entry to replace */
    for (i = 0; i < RTK_MAX_NUM_OF_VLAN_INDEX; i++)
    {    
        bUsed = FALSE;    

        for (j = 0; j < RTK_MAX_NUM_OF_PORT; j++)
        {    
            if ((retVal = rtl8370_getAsicVlanPortBasedVID(j, &index, &pri))!=RT_ERR_OK)
                return retVal;

            if (i == index)/*index i is in use by port j*/
            {
                bUsed = TRUE;
                break;
            } 

            if (i == gvidx)
            {
                if ((retVal = rtl8370_getAsic1xProcConfig(j, &proc))!=RT_ERR_OK)
                    return retVal;
                if (DOT1X_UNAUTH_GVLAN == proc )
                {
                    bUsed = TRUE;
                    break;
                }
            }

            for (k=0;k<=RTK_PROTOVLAN_GROUP_ID_MAX;k++)
            {
                if ((retVal = rtl8370_getAsicVlanPortAndProtocolBased(port, k, &ppb_vlan_cfg)) != RT_ERR_OK)
                    return retVal; 
                if (ppb_vlan_cfg.valid==TRUE && ppb_vlan_cfg.vlan_idx==i)
                {
                    bUsed = TRUE;
                    break;
                }
            }    
        }

        if (FALSE == bUsed)/*found a unused index, replace it*/
        {
            vlan4K.vid = pvid;
            if ((retVal = rtl8370_getAsicVlan4kEntry(&vlan4K))!=RT_ERR_OK)
                return retVal; 
            vlanMC.mbr = vlan4K.mbr;                   
            vlanMC.fid = vlan4K.fid;
            vlanMC.msti= vlan4K.msti;
            vlanMC.meteridx= vlan4K.meteridx;
            vlanMC.envlanpol= vlan4K.envlanpol;
            vlanMC.lurep= vlan4K.lurep;               
            if ((retVal = rtl8370_setAsicVlanMemberConfig(i,&vlanMC))!=RT_ERR_OK)
                return retVal; 

            if ((retVal = rtl8370_setAsicVlanPortBasedVID(port,i,priority))!=RT_ERR_OK)
                return retVal;    

            return RT_ERR_OK;            
        }
    }    
    
    return RT_ERR_FAILED;
}

/* Function Name:
 *      rtk_vlan_portPvid_get
 * Description:
 *      Get VLAN ID(PVID) on specified port.
 * Input:
 *      port - Port id.
 * Output:
 *      pPvid - Specified VLAN ID.
 *      pPriority - 802.1p priority for the PVID.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 *      RT_ERR_PORT_ID - Invalid port number.
 * Note:
 *     The API can get the PVID and 802.1p priority for the PVID of Port-based VLAN.
 */
rtk_api_ret_t rtk_vlan_portPvid_get(rtk_port_t port, rtk_vlan_t *pPvid, rtk_pri_t *pPriority)
{
    rtk_api_ret_t retVal;
    uint32 index,pri;
    rtl8370_vlanconfiguser vlanMC;    
    
    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;

    if ((retVal = rtl8370_getAsicVlanPortBasedVID(port,&index,&pri))!=RT_ERR_OK)
        return retVal; 

    if ((retVal = rtl8370_getAsicVlanMemberConfig(index, &vlanMC))!=RT_ERR_OK)
        return retVal; 

    *pPvid = vlanMC.evid;
    *pPriority = pri;   
    
    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_vlan_portIgrFilterEnable_set
 * Description:
 *      Set VLAN ingress for each port.
 * Input:
 *      port - Port id.
 *      igr_filter - VLAN ingress function enable status.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_ENABLE - Invalid enable input.
 * Note:
 *      The status of vlan ingress filter is as following:
 *      DISABLED
 *      ENABLED
 *      While VLAN function is enabled, ASIC will decide VLAN ID for each received frame and get belonged member
 *      ports from VLAN table. If received port is not belonged to VLAN member ports, ASIC will drop received frame if VLAN ingress function is enabled.
 */
rtk_api_ret_t rtk_vlan_portIgrFilterEnable_set(rtk_port_t port, rtk_enable_t igr_filter)
{
    rtk_api_ret_t retVal;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;

    if (igr_filter>=RTK_ENABLE_END)
        return RT_ERR_ENABLE;      

    if ((retVal = rtl8370_setAsicVlanIngressFilter(port,igr_filter))!=RT_ERR_OK)
        return retVal; 

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_vlan_portIgrFilterEnable_get
 * Description:
 *      Get VLAN Ingress Filter
 * Input:
 *      port - Port id.
 * Output:
 *      pIgr_filter - VLAN ingress function enable status.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 *      RT_ERR_PORT_ID - Invalid port number.
 * Note:
 *     The API can Get the VLAN ingress filter status.
 *     The status of vlan ingress filter is as following:
 *     DISABLED
 *     ENABLED    
 */

rtk_api_ret_t rtk_vlan_portIgrFilterEnable_get(rtk_port_t port, rtk_data_t *pIgr_filter)
{
    rtk_api_ret_t retVal;
    
    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;

    if ((retVal = rtl8370_getAsicVlanIngressFilter(port, pIgr_filter))!=RT_ERR_OK)
        return retVal; 

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_vlan_portAcceptFrameType_set
 * Description:
 *      Set VLAN accept_frame_type
 * Input:
 *      port - Port id.
 *      accept_frame_type - accept frame type
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_VLAN_ACCEPT_FRAME_TYPE - Invalid frame type.
 * Note:
 *      The API is used for checking 802.1Q tagged frames.
 *      The accept frame type as following:
 *      ACCEPT_FRAME_TYPE_ALL
 *      ACCEPT_FRAME_TYPE_TAG_ONLY
 *      ACCEPT_FRAME_TYPE_UNTAG_ONLY
 */
rtk_api_ret_t rtk_vlan_portAcceptFrameType_set(rtk_port_t port, rtk_vlan_acceptFrameType_t accept_frame_type)
{
    rtk_api_ret_t retVal;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;

    if (accept_frame_type >= ACCEPT_FRAME_TYPE_END)
        return RT_ERR_VLAN_ACCEPT_FRAME_TYPE;    

    if ((retVal = rtl8370_setAsicVlanAccpetFrameType(port, accept_frame_type)) != RT_ERR_OK)
        return retVal; 

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_vlan_portAcceptFrameType_get
 * Description:
 *      Get VLAN accept_frame_type
 * Input:
 *      port - Port id.
 * Output:
 *      pAccept_frame_type - accept frame type
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 *      RT_ERR_PORT_ID - Invalid port number.
 * Note:
 *     The API can Get the VLAN ingress filter.
 *     The accept frame type as following:
 *     ACCEPT_FRAME_TYPE_ALL
 *     ACCEPT_FRAME_TYPE_TAG_ONLY
 *     ACCEPT_FRAME_TYPE_UNTAG_ONLY
 */
rtk_api_ret_t rtk_vlan_portAcceptFrameType_get(rtk_port_t port, rtk_data_t *pAccept_frame_type)
{
    rtk_api_ret_t retVal;
    
    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;

    if ((retVal = rtl8370_getAsicVlanAccpetFrameType(port, pAccept_frame_type)) != RT_ERR_OK)
        return retVal; 

    return RT_ERR_OK;
}    

/* Function Name:
 *      rtk_vlan_vlanBasedPriority_set
 * Description:
 *      Set VLAN priority for each CVLAN.
 * Input:
 *      vid - Specified VLAN ID.
 *      priority - 802.1p priority for the PVID.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_VLAN_VID - Invalid VID parameter.
 *      RT_ERR_VLAN_PRIORITY - Invalid priority. 
 * Note:
 *      This API is used to set priority per VLAN.
 */
rtk_api_ret_t rtk_vlan_vlanBasedPriority_set(rtk_vlan_t vid, rtk_pri_t priority)
{
    rtk_api_ret_t retVal;
    rtl8370_user_vlan4kentry vlan4K;   
    
    /* vid must be 0~4095 */
    if (vid > RTK_VLAN_ID_MAX)
        return RT_ERR_VLAN_VID;

    /* priority must be 0~7 */
    if (priority > RTK_DOT1P_PRIORITY_MAX)
        return RT_ERR_VLAN_PRIORITY;

    /* update 4K table */
    vlan4K.vid = vid; 
    if ((retVal = rtl8370_getAsicVlan4kEntry(&vlan4K))!=RT_ERR_OK)
        return retVal;
    
    vlan4K.vbpen= 1;      
    vlan4K.vbpri= priority;    
    if ((retVal = rtl8370_setAsicVlan4kEntry(&vlan4K))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_vlan_vlanBasedPriority_get
 * Description:
 *      Get VLAN priority for each CVLAN.
 * Input:
 *      vid - Specified VLAN ID.
 * Output:
 *      pPriority - 802.1p priority for the PVID.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_VLAN_VID - Invalid VID parameter.
 *      RT_ERR_PORT_ID - Invalid port number.
 * Note:
 *     This API is used to set priority per VLAN.
 */
rtk_api_ret_t rtk_vlan_vlanBasedPriority_get(rtk_vlan_t vid, rtk_pri_t *pPriority)
{
    rtk_api_ret_t retVal;
    rtl8370_user_vlan4kentry vlan4K;   
    
    /* vid must be 0~4095 */
    if (vid > RTK_VLAN_ID_MAX)
        return RT_ERR_VLAN_VID;

    /* update 4K table */
    vlan4K.vid = vid; 
    if ((retVal = rtl8370_getAsicVlan4kEntry(&vlan4K))!=RT_ERR_OK)
        return retVal;
    
    if (vlan4K.vbpen!=1)
        return RT_ERR_FAILED;    
    
    *pPriority = vlan4K.vbpri;    

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_vlan_protoAndPortBasedVlan_add
 * Description:
 *      Add the protocol-and-port-based vlan to the specified port of device. 
 * Input:
 *      port - Port id.
 *      info - Protocol and port based VLAN configuration information.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_VLAN_VID - Invalid VID parameter.
 *      RT_ERR_VLAN_PRIORITY - Invalid priority. 
 *      RT_ERR_TBL_FULL - Table is full.
 *      RT_ERR_OUT_OF_RANGE - input out of range.
 * Note:
 *      The incoming packet which match the protocol-and-port-based vlan will use the configure vid for ingress pipeline
 *      The frame type is shown in the following:
 *      FRAME_TYPE_ETHERNET
 *      FRAME_TYPE_RFC1042
 *      FRAME_TYPE_LLCOTHER
 */
rtk_api_ret_t rtk_vlan_protoAndPortBasedVlan_add(rtk_port_t port, rtk_vlan_protoAndPortInfo_t info)
{
    rtk_api_ret_t retVal,i;
    uint32 j, k, index, pri;
    uint32 gvidx,proc;
    uint32 exist, empty, used, bUsed;
    rtl8370_protocolgdatacfg ppb_data_cfg;
    rtl8370_protocolvlancfg ppb_vlan_cfg;
    rtl8370_user_vlan4kentry vlan4K;
    rtl8370_vlanconfiguser vlanMC; 
    rtl8370_provlan_frametype tmp;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;

    if (info.proto_type > RTK_MAX_NUM_OF_PROTO_TYPE)
        return RT_ERR_OUT_OF_RANGE;

    if (info.frame_type>=FRAME_TYPE_END)
        return RT_ERR_OUT_OF_RANGE;

    if (info.cvid > RTK_VLAN_ID_MAX)
        return RT_ERR_VLAN_VID;

    if (info.cpri>RTK_DOT1P_PRIORITY_MAX)
        return RT_ERR_VLAN_PRIORITY;    

    exist = 0xFF;
    empty = 0xFF;
    for (i = RTK_PROTOVLAN_GROUP_ID_MAX; i >= 0; i--)
    {    
        if ((retVal = rtl8370_getAsicVlanProtocolBasedGroupData(i,&ppb_data_cfg))!=RT_ERR_OK)
            return retVal;
        tmp = info.frame_type;
        if (ppb_data_cfg.ether_type==info.proto_type&&ppb_data_cfg.frame_type==tmp)
        {
            /*Already exist*/
            exist = i;
            break; 
        }
        else if (ppb_data_cfg.ether_type==0&&ppb_data_cfg.frame_type==0)
        {
            /*find empty index*/
            empty = i;    
        }
    }

    used = 0xFF;
    /*No empty and exist index*/
    if (0xFF == exist && 0xFF == empty)
        return RT_ERR_TBL_FULL;    
    else if (exist<RTK_MAX_NUM_OF_PROTOVLAN_GROUP)
    {
       /*exist index*/
       used =exist;    
    }
    else if (empty<RTK_MAX_NUM_OF_PROTOVLAN_GROUP)
    {
        /*No exist index, but have empty index*/
        ppb_data_cfg.frame_type = info.frame_type;
        ppb_data_cfg.ether_type = info.proto_type;        
        if ((retVal = rtl8370_setAsicVlanProtocolBasedGroupData(empty,&ppb_data_cfg))!=RT_ERR_OK)
            return retVal;    
        used = empty;
    }
    else
        return RT_ERR_FAILED;    
    
    /* 
        Search 32 member configuration to see if the entry already existed.
        If existed, update the priority and assign the index to the port.
    */
    for (i = 0; i < RTK_MAX_NUM_OF_VLAN_INDEX; i++)
    {      
        if ((retVal = rtl8370_getAsicVlanMemberConfig(i, &vlanMC))!=RT_ERR_OK)
            return retVal;
        if (info.cvid== vlanMC.evid)
        {
            if ((retVal = rtl8370_getAsicVlanPortAndProtocolBased(port,used,&ppb_vlan_cfg))!=RT_ERR_OK)
                return retVal;    
            if (FALSE == ppb_vlan_cfg.valid)
            {
                ppb_vlan_cfg.vlan_idx = i;
                ppb_vlan_cfg.valid = TRUE;
                ppb_vlan_cfg.priority = info.cpri;
                if ((retVal = rtl8370_setAsicVlanPortAndProtocolBased(port,used,&ppb_vlan_cfg))!=RT_ERR_OK)
                    return retVal;
                return RT_ERR_OK;
            }
            else
                return RT_ERR_VLAN_EXIST;
        }    
    }

    /*
        vid doesn't exist in 32 member configuration. Find an empty entry in 
        32 member configuration, then copy entry from 4K. If 32 member configuration
        are all full, then find an entry which not used by Port-based VLAN and 
        then replace it with 4K. Finally, assign the index to the port.
    */
    for (i = 0; i < RTK_MAX_NUM_OF_VLAN_INDEX; i++)
    {    
        if (rtl8370_getAsicVlanMemberConfig(i, &vlanMC) != RT_ERR_OK)
            return RT_ERR_FAILED;    

        if (vlanMC.evid == 0 && vlanMC.mbr == 0)
        {
            vlan4K.vid = info.cvid;
            if ((retVal = rtl8370_getAsicVlan4kEntry(&vlan4K))!=RT_ERR_OK)
                return retVal;

            vlanMC.evid = info.cvid;
            vlanMC.mbr = vlan4K.mbr;                   
            vlanMC.fid = vlan4K.fid;
            vlanMC.msti= vlan4K.msti;
            vlanMC.meteridx= vlan4K.meteridx;
            vlanMC.envlanpol= vlan4K.envlanpol;
            vlanMC.lurep= vlan4K.lurep;    
            
            if ((retVal = rtl8370_setAsicVlanMemberConfig(i,&vlanMC))!=RT_ERR_OK)
                return retVal; 
            
            if ((retVal = rtl8370_getAsicVlanPortAndProtocolBased(port,used,&ppb_vlan_cfg))!=RT_ERR_OK)
                return retVal;    
            if (FALSE == ppb_vlan_cfg.valid)
            {
                ppb_vlan_cfg.vlan_idx = i;
                ppb_vlan_cfg.valid = TRUE;
                ppb_vlan_cfg.priority = info.cpri;
                if ((retVal = rtl8370_setAsicVlanPortAndProtocolBased(port,used,&ppb_vlan_cfg))!=RT_ERR_OK)
                    return retVal;
                return RT_ERR_OK;
            }
            else
                return RT_ERR_VLAN_EXIST;        
        }    
    }    

    if ((retVal = rtl8370_getAsic1xGuestVidx(&gvidx))!=RT_ERR_OK)
        return retVal; 

    /* 32 member configuration is full, found a unused entry to replace */
    for (i = 0; i < RTK_MAX_NUM_OF_VLAN_INDEX; i++)
    {    
        bUsed = FALSE;    
        for (j = 0; j < RTK_MAX_NUM_OF_PORT; j++)
        {    
            if (rtl8370_getAsicVlanPortBasedVID(j, &index, &pri) != RT_ERR_OK)
                return RT_ERR_FAILED;    

            if (i == index)/*index i is in use by port j*/
            {
                bUsed = TRUE;
                break;
            }  

            if (i == gvidx)
            {
                if ((retVal = rtl8370_getAsic1xProcConfig(j, &proc))!=RT_ERR_OK)
                    return retVal;
                if (DOT1X_UNAUTH_GVLAN == proc)
                {
                    bUsed = TRUE;
                    break;
                }
            }
            
            for (k=0;k<=RTK_PROTOVLAN_GROUP_ID_MAX;k++)
            {
                if ((retVal = rtl8370_getAsicVlanPortAndProtocolBased(port, k, &ppb_vlan_cfg)) != RT_ERR_OK)
                    return retVal; 
                if (TRUE == ppb_vlan_cfg.valid && ppb_vlan_cfg.vlan_idx == i)
                {
                    bUsed = TRUE;
                    break;
                }
            }            
        }

        if (FALSE == bUsed) /*found a unused index, replace it*/
        {
            vlan4K.vid = info.cvid;
            if (rtl8370_getAsicVlan4kEntry(&vlan4K) != RT_ERR_OK)
                return RT_ERR_FAILED;

            vlanMC.mbr = vlan4K.mbr;                   
            vlanMC.fid = vlan4K.fid;
            vlanMC.msti= vlan4K.msti;
            vlanMC.meteridx= vlan4K.meteridx;
            vlanMC.envlanpol= vlan4K.envlanpol;
            vlanMC.lurep= vlan4K.lurep;               
            if ((retVal = rtl8370_setAsicVlanMemberConfig(i,&vlanMC))!=RT_ERR_OK)
                return retVal; 

            if ((retVal = rtl8370_getAsicVlanPortAndProtocolBased(port,used,&ppb_vlan_cfg))!=RT_ERR_OK)
                return retVal;    
            if (FALSE == ppb_vlan_cfg.valid)
            {
                ppb_vlan_cfg.vlan_idx = i;
                ppb_vlan_cfg.valid = TRUE;
                ppb_vlan_cfg.priority = info.cpri;
                if ((retVal = rtl8370_setAsicVlanPortAndProtocolBased(port,used,&ppb_vlan_cfg))!=RT_ERR_OK)
                    return retVal;
                return RT_ERR_OK;
            }
            else
                return RT_ERR_VLAN_EXIST;            
        }
    }        

    return RT_ERR_FAILED;
}

/* Function Name:
 *      rtk_vlan_protoAndPortBasedVlan_get
 * Description:
 *      Get the protocol-and-port-based vlan to the specified port of device. 
 * Input:
 *      port - Port id.
 *      proto_type - protocol-and-port-based vlan protocol type.
 *      frame_type - protocol-and-port-based vlan frame type.
 * Output:
 *      pInfo - Protocol and port based VLAN configuration information.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_OUT_OF_RANGE - input out of range.
 *      RT_ERR_TBL_FULL - Table is full.
 * Note:
 *     The incoming packet which match the protocol-and-port-based vlan will use the configure vid for ingress pipeline
 *     The frame type is shown in the following:
 *     FRAME_TYPE_ETHERNET
 *     FRAME_TYPE_RFC1042
 *     FRAME_TYPE_LLCOTHER
 */
rtk_api_ret_t rtk_vlan_protoAndPortBasedVlan_get(rtk_port_t port, rtk_vlan_proto_type_t proto_type, rtk_vlan_protoVlan_frameType_t frame_type, rtk_vlan_protoAndPortInfo_t *pInfo)
{
    rtk_api_ret_t retVal;
    uint32 i;
    uint32 ppb_idx;
    rtl8370_vlanconfiguser vlanMC; 
    rtl8370_protocolgdatacfg ppb_data_cfg;
    rtl8370_protocolvlancfg ppb_vlan_cfg;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;

    if (proto_type > RTK_MAX_NUM_OF_PROTO_TYPE)
        return RT_ERR_OUT_OF_RANGE;

    if (frame_type>=FRAME_TYPE_END)
        return RT_ERR_OUT_OF_RANGE;    

   ppb_idx = 0;
    
    for (i=0;i<=RTK_PROTOVLAN_GROUP_ID_MAX;i++)
    {
        if ((retVal = rtl8370_getAsicVlanProtocolBasedGroupData(i,&ppb_data_cfg)) != RT_ERR_OK)
            return retVal; 

		//Tom: (rtl8370_provlan_frametype), compile warning	
        if (ppb_data_cfg.frame_type == (rtl8370_provlan_frametype)frame_type && ppb_data_cfg.ether_type==proto_type)
        {
            ppb_idx = i;
            break;
        }
        else if (RTK_PROTOVLAN_GROUP_ID_MAX == i)
            return RT_ERR_TBL_FULL; 
    }            
   
    if ((retVal = rtl8370_getAsicVlanPortAndProtocolBased(port,ppb_idx, &ppb_vlan_cfg)) != RT_ERR_OK)
        return retVal;

    if (FALSE == ppb_vlan_cfg.valid)
        return RT_ERR_FAILED; 
    
    if ((retVal = rtl8370_getAsicVlanMemberConfig(ppb_vlan_cfg.vlan_idx, &vlanMC))!=RT_ERR_OK)
        return retVal;
    
    pInfo->frame_type = frame_type;
    pInfo->proto_type = proto_type;
    pInfo->cvid = vlanMC.evid;
    pInfo->cpri = ppb_vlan_cfg.priority;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_vlan_protoAndPortBasedVlan_del
 * Description:
 *      Delete the protocol-and-port-based vlan from the specified port of device. 
 * Input:
 *      port - Port id.
 *      proto_type - protocol-and-port-based vlan protocol type.
 *      frame_type - protocol-and-port-based vlan frame type.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_OUT_OF_RANGE - input out of range.
 *      RT_ERR_TBL_FULL - Table is full.
 * Note:
 *     The incoming packet which match the protocol-and-port-based vlan will use the configure vid for ingress pipeline
 *     The frame type is shown in the following:
 *     FRAME_TYPE_ETHERNET
 *     FRAME_TYPE_RFC1042
 *     FRAME_TYPE_LLCOTHER
 */
rtk_api_ret_t rtk_vlan_protoAndPortBasedVlan_del(rtk_port_t port, rtk_vlan_proto_type_t proto_type, rtk_vlan_protoVlan_frameType_t frame_type)
{
    rtk_api_ret_t retVal;
    uint32 i, bUsed;
    uint32 ppb_idx;
    rtl8370_protocolgdatacfg ppb_data_cfg;
    rtl8370_protocolvlancfg ppb_vlan_cfg;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;

    if (proto_type > RTK_MAX_NUM_OF_PROTO_TYPE)
        return RT_ERR_OUT_OF_RANGE;

    if (frame_type>=FRAME_TYPE_END)
        return RT_ERR_OUT_OF_RANGE;    

   ppb_idx = 0;

    for (i=0;i<=RTK_PROTOVLAN_GROUP_ID_MAX;i++)
    {
        if ((retVal = rtl8370_getAsicVlanProtocolBasedGroupData(i,&ppb_data_cfg)) != RT_ERR_OK)
            return retVal; 

	    //Tom: (rtl8370_provlan_frametype), compile warning
        if (ppb_data_cfg.frame_type == (rtl8370_provlan_frametype)frame_type && ppb_data_cfg.ether_type == proto_type)
        {
            ppb_idx = i;
            ppb_vlan_cfg.valid = FALSE;
            ppb_vlan_cfg.vlan_idx = 0;
            ppb_vlan_cfg.priority = 0;        
            if ((retVal = rtl8370_setAsicVlanPortAndProtocolBased(port,ppb_idx,&ppb_vlan_cfg)) != RT_ERR_OK)
                return retVal;
        }
    }            

    bUsed = FALSE;
    for (i = 0; i < RTK_MAX_NUM_OF_PORT; i++)
    {    
        if ((retVal = rtl8370_getAsicVlanPortAndProtocolBased(i,ppb_idx, &ppb_vlan_cfg)) != RT_ERR_OK)
            return retVal;
        
        if (TRUE == ppb_vlan_cfg.valid)
        {
            bUsed = TRUE;
                break;
        }        
    }

    if (FALSE == bUsed) /*No Port use this PPB Index, Delete it*/
    {
        ppb_data_cfg.ether_type=0;
        ppb_data_cfg.frame_type=0;
        if ((retVal = rtl8370_setAsicVlanProtocolBasedGroupData(ppb_idx,&ppb_data_cfg))!=RT_ERR_OK)
            return retVal; 
    }

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_vlan_protoAndPortBasedVlan_delAll
 * Description:
 *     Delete all protocol-and-port-based vlans from the specified port of device. 
 * Input:
 *      port - Port id.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_OUT_OF_RANGE - input out of range.
 * Note:
 *     The incoming packet which match the protocol-and-port-based vlan will use the configure vid for ingress pipeline
 *     Delete all flow table protocol-and-port-based vlan entries.
 */
rtk_api_ret_t rtk_vlan_protoAndPortBasedVlan_delAll(rtk_port_t port)
{
    rtk_api_ret_t retVal;
    uint32 i, j, bUsed[4];
    rtl8370_protocolgdatacfg ppb_data_cfg;
    rtl8370_protocolvlancfg ppb_vlan_cfg;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;

    for (i=0;i<=RTK_PROTOVLAN_GROUP_ID_MAX;i++)
    {
        ppb_vlan_cfg.valid = FALSE;
        ppb_vlan_cfg.vlan_idx = 0;
        ppb_vlan_cfg.priority = 0;        
        if ((retVal = rtl8370_setAsicVlanPortAndProtocolBased(port,i,&ppb_vlan_cfg)) != RT_ERR_OK)
            return retVal;
    }            

    bUsed[0] = FALSE;
    bUsed[1] = FALSE;
    bUsed[2] = FALSE;
    bUsed[3] = FALSE;    
    for (i = 0; i < RTK_MAX_NUM_OF_PORT; i++)
    {    
        for (j=0;j<=RTK_PROTOVLAN_GROUP_ID_MAX;j++)
        {
            if ((retVal = rtl8370_getAsicVlanPortAndProtocolBased(i,j,&ppb_vlan_cfg)) != RT_ERR_OK)
                return retVal;
        
            if (TRUE == ppb_vlan_cfg.valid)
            {
                bUsed[j] = TRUE;
            }
        }
    }
    
    for (i=0;i<=RTK_PROTOVLAN_GROUP_ID_MAX;i++)
    {
        if (FALSE == bUsed[i]) /*No Port use this PPB Index, Delete it*/
        {
            ppb_data_cfg.ether_type=0;
            ppb_data_cfg.frame_type=0;
            if ((retVal = rtl8370_setAsicVlanProtocolBasedGroupData(i,&ppb_data_cfg))!=RT_ERR_OK)
                return retVal; 
        }
    }



    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_vlan_tagMode_set
 * Description:
 *      Set CVLAN egress tag mode
 * Input:
 *      port - Port id.
 *      tag_mode - The egress tag mode.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_INPUT - Invalid input parameter.
 *      RT_ERR_ENABLE - Invalid enable input.
 * Note:
 *      The API can set Egress tag mode. There are 4 mode for egress tag:
 *      VLAN_TAG_MODE_ORIGINAL,
 *      VLAN_TAG_MODE_KEEP_FORMAT,
 *      VLAN_TAG_MODE_PRI. 
 *      VLAN_TAG_MODE_REAL_KEEP_FORMAT,
 */
rtk_api_ret_t rtk_vlan_tagMode_set(rtk_port_t port, rtk_vlan_tagMode_t tag_mode)
{
    rtk_api_ret_t retVal;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;    

    if (tag_mode >= VLAN_TAG_MODE_END)
        return RT_ERR_PORT_ID;
    
    if ((retVal = rtl8370_setAsicVlanEgressTagMode(port, tag_mode))!=RT_ERR_OK)
        return retVal;    

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_vlan_tagMode_get
 * Description:
 *      Get CVLAN egress tag mode
 * Input:
 *      port - Port id.
 * Output:
 *      pTag_mode - The egress tag mode.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 *      RT_ERR_PORT_ID - Invalid port number.
 * Note:
 *      The API can get Egress tag mode. There are 4 mode for egress tag:
 *      VLAN_TAG_MODE_ORIGINAL,
 *      VLAN_TAG_MODE_KEEP_FORMAT,
 *      VLAN_TAG_MODE_REAL_KEEP_FORMAT,
 *      VLAN_TAG_MODE_PRI. 
 */
rtk_api_ret_t rtk_vlan_tagMode_get(rtk_port_t port, rtk_data_t *pTag_mode)
{
    rtk_api_ret_t retVal;
    
    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;    
    
    if ((retVal = rtl8370_getAsicVlanEgressTagMode(port, pTag_mode))!=RT_ERR_OK)
        return retVal;    

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_vlan_stg_set
 * Description:
 *      Set spanning tree group instance of the vlan to the specified device
 * Input:
 *      vid - Specified VLAN ID.
 *      stg - spanning tree group instance.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_MSTI - Invalid msti parameter
 *      RT_ERR_INPUT - Invalid input parameter.
 *      RT_ERR_VLAN_VID - Invalid VID parameter.
 * Note:
 *      The API can set spanning tree group instance of the vlan to the specified device.
 */
rtk_api_ret_t rtk_vlan_stg_set(rtk_vlan_t vid, rtk_stg_t stg)
{
    rtk_api_ret_t retVal;
    rtl8370_user_vlan4kentry vlan4K;   
    
    /* vid must be 0~4095 */
    if (vid > RTK_VLAN_ID_MAX)
        return RT_ERR_VLAN_VID;

    /* priority must be 0~15 */
    if (stg >= RTK_MAX_NUM_OF_MSTI)
        return RT_ERR_MSTI;

    /* update 4K table */
    vlan4K.vid = vid; 
    if ((retVal = rtl8370_getAsicVlan4kEntry(&vlan4K))!=RT_ERR_OK)
        return retVal;
    
    vlan4K.msti= stg;        
    if ((retVal = rtl8370_setAsicVlan4kEntry(&vlan4K))!=RT_ERR_OK)
        return retVal;


    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_vlan_stg_get
 * Description:
 *      Get spanning tree group instance of the vlan to the specified device
 * Input:
 *      vid - Specified VLAN ID.
 * Output:
 *      pStg - spanning tree group instance.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 *      RT_ERR_VLAN_VID - Invalid VID parameter.
 * Note:
 *      The API can get spanning tree group instance of the vlan to the specified device.
 */
rtk_api_ret_t rtk_vlan_stg_get(rtk_vlan_t vid, rtk_stg_t *pStg)
{
    rtk_api_ret_t retVal;
    rtl8370_user_vlan4kentry vlan4K;   
    
    /* vid must be 0~4095 */
    if (vid > RTK_VLAN_ID_MAX)
        return RT_ERR_VLAN_VID;

    /* update 4K table */
    vlan4K.vid = vid; 
    if ((retVal = rtl8370_getAsicVlan4kEntry(&vlan4K))!=RT_ERR_OK)
        return retVal;
    
    *pStg = vlan4K.msti;        

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_vlan_portFid_set
 * Description:
 *      Set port-based filtering database
 * Input:
 *      port - Port id.
 *      enable - ebable port-based FID
 *      fid - Specified filtering database.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_L2_FID - Invalid fid.
 *      RT_ERR_INPUT - Invalid input parameter.
 *      RT_ERR_PORT_ID - Invalid port ID.
 * Note:
 *      The API can set port-based filtering database. If the function is enabled, all input
 *      packets will be assigned to the port-based fid regardless vlan tag. 
 */
rtk_api_ret_t rtk_vlan_portFid_set(rtk_port_t port, rtk_enable_t enable, rtk_fid_t fid)
{
    rtk_api_ret_t retVal;  

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;

    if (enable>=RTK_ENABLE_END)
        return RT_ERR_ENABLE;   

    /* fid must be 0~4095 */
    if (fid > RTK_FID_MAX)
        return RT_ERR_L2_FID;
    
    if ((retVal = rtl8370_setAsicPortBasedFidEn(port, enable))!=RT_ERR_OK)
        return retVal;
          
    if ((retVal = rtl8370_setAsicPortBasedFid(port, fid))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_vlan_portFid_get
 * Description:
 *      Get port-based filtering database
 * Input:
 *      port - Port id.
 * Output:
 *      pEnable - ebable port-based FID
 *      pFid - Specified filtering database.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 *      RT_ERR_PORT_ID - Invalid port ID.
 * Note:
 *      The API can get port-based filtering database status. If the function is enabled, all input
 *      packets will be assigned to the port-based fid regardless vlan tag.
 */
rtk_api_ret_t rtk_vlan_portFid_get(rtk_port_t port, rtk_data_t *pEnable, rtk_data_t *pFid)
{
    rtk_api_ret_t retVal; 
    
    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;
    
    if ((retVal = rtl8370_getAsicPortBasedFidEn(port, pEnable))!=RT_ERR_OK)
        return retVal;
          
    if ((retVal = rtl8370_getAsicPortBasedFid(port, pFid))!=RT_ERR_OK)
        return retVal;       

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_trunk_port_set
 * Description:
 *      Set trunking group available port mask
 * Input:
 *      trk_gid - trunk group id
 *      trunk_member_portmask - Logic trunking member port mask
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_LA_TRUNK_ID - Invalid trunking group
 *      RT_ERR_PORT_MASK - Invalid portmask.
 * Note:
 *      The API can set 4 port trunking group enabled port mask. Each port trunking group has max 4 ports.
 *      If enabled port mask has less than 2 ports available setting, then this trunking group function is disabled. 
 *      The group port members for trunk group are as following: 
 *      TRUNK_GROUP0: port 0 to port 3.
 *      TRUNK_GROUP1: port 4 to port 7.
 *      TRUNK_GROUP2: port 8 to port 11.
 *      TRUNK_GROUP3: port 12 to port 15.
 */

/* Function Name:
 *      rtk_l2_addr_get
 * Description:
 *      Get LUT unicast entry.
 * Input:
 *      pMac - 6 bytes unicast(I/G bit is 0) mac address to be written into LUT.
 *      pL2_data - Unicast entry parameter. The fid (filtering database) should be added as input parameter
 * Output:
 *      pL2_data - Unicast entry parameter
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_MAC - Invalid MAC address.
 *      RT_ERR_L2_FID - Invalid FID .
 *      RT_ERR_L2_ENTRY_NOTFOUND - No such LUT entry.
 *      RT_ERR_INPUT - Invalid input parameters. 
 * Note:
 *      If the unicast mac address existed in LUT, it will return the port and fid where
 *      the mac is learned. Otherwise, it will return a RT_ERR_L2_ENTRY_NOTFOUND error.
 */
rtk_api_ret_t rtk_l2_addr_get(rtk_mac_t *pMac, rtk_l2_ucastAddr_t *pL2_data)
{
    rtk_api_ret_t retVal;
    uint32 method;
    rtl8370_luttb l2Table;
        
    /* must be unicast address */
    if ((pMac == NULL) || (pMac->octet[0] & 0x1))
        return RT_ERR_MAC;  

    if (pL2_data->fid > RTK_FID_MAX)
        return RT_ERR_L2_FID;  

    memset(&l2Table,0,sizeof(rtl8370_luttb));

    memcpy(l2Table.mac.octet, pMac->octet, ETHER_ADDR_LEN);
    l2Table.fid = pL2_data->fid;
    method = LUTREADMETHOD_MAC;

    if ((retVal = rtl8370_getAsicL2LookupTb(method,&l2Table))!=RT_ERR_OK)
        return retVal;
    
    memcpy(pL2_data->mac.octet,pMac->octet,ETHER_ADDR_LEN);
    pL2_data->port = l2Table.spa;
    pL2_data->fid  = l2Table.fid;
    pL2_data->is_static = l2Table.static_bit;
    pL2_data->auth = l2Table.auth;
    pL2_data->sa_block = l2Table.block;
    
    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_l2_addr_add
 * Description:
 *      Add LUT unicast entry.
 * Input:
 *      pMac - 6 bytes unicast(I/G bit is 0) mac address to be written into LUT.
 *      pL2_data - Unicast entry parameter
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_MAC - Invalid MAC address.
 *      RT_ERR_L2_FID - Invalid FID .
 *      RT_ERR_L2_INDEXTBL_FULL - hashed index is full of entries.
 *      RT_ERR_INPUT - Invalid input parameters. 
 * Note:
 *      If the unicast mac address already existed in LUT, it will udpate the status of the entry. 
 *      Otherwise, it will find an empty or asic auto learned entry to write. If all the entries 
 *      with the same hash value can't be replaced, ASIC will return a RT_ERR_L2_INDEXTBL_FULL error.
 */
rtk_api_ret_t rtk_l2_addr_add(rtk_mac_t *pMac, rtk_l2_ucastAddr_t *pL2_data)
{
    rtk_api_ret_t retVal;
    uint32 method;
    rtl8370_luttb l2Table;
        
    /* must be unicast address */
    if ((pMac == NULL) || (pMac->octet[0] & 0x1))
        return RT_ERR_MAC;

    if (pL2_data->port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;    

    if (pL2_data->fid > RTK_FID_MAX)
        return RT_ERR_L2_FID;

    if (pL2_data->is_static>= RTK_ENABLE_END)
        return RT_ERR_INPUT; 
    
    if (pL2_data->sa_block>= RTK_ENABLE_END)
        return RT_ERR_INPUT; 

    if (pL2_data->auth>= RTK_ENABLE_END)
        return RT_ERR_INPUT;     

    memset(&l2Table,0,sizeof(rtl8370_luttb));

    /* fill key (MAC,FID) to get L2 entry */
    memcpy(l2Table.mac.octet, pMac->octet, ETHER_ADDR_LEN);
    l2Table.fid = pL2_data->fid;
    method = LUTREADMETHOD_MAC;
    retVal = rtl8370_getAsicL2LookupTb(method,&l2Table);
    if (RT_ERR_OK == retVal )
    {
        memcpy(l2Table.mac.octet, pMac->octet, ETHER_ADDR_LEN);
        l2Table.fid = pL2_data->fid;
        l2Table.spa = pL2_data->port;
        l2Table.static_bit = pL2_data->is_static;
        l2Table.block = pL2_data->sa_block;
        l2Table.ipmul = 0;
        l2Table.age = 6;
        retVal = rtl8370_setAsicL2LookupTb(&l2Table);
        return retVal;        
    }    
    else if (RT_ERR_L2_ENTRY_NOTFOUND == retVal )
    {
        memset(&l2Table,0,sizeof(rtl8370_luttb));    
        memcpy(l2Table.mac.octet, pMac->octet, ETHER_ADDR_LEN);    
        l2Table.fid = pL2_data->fid;
        l2Table.spa = pL2_data->port;
        l2Table.static_bit = pL2_data->is_static;
        l2Table.block = pL2_data->sa_block;
        l2Table.ipmul = 0;
        l2Table.age = 6;
        if ((retVal = rtl8370_setAsicL2LookupTb(&l2Table))!=RT_ERR_OK)
            return retVal;
        
        method = LUTREADMETHOD_MAC;
        retVal = rtl8370_getAsicL2LookupTb(method,&l2Table);
        if (RT_ERR_L2_ENTRY_NOTFOUND == retVal )
            return     RT_ERR_L2_INDEXTBL_FULL;
        else
            return retVal;              
    }
    else
        return retVal;             

}

/* Function Name:
 *      rtk_cpu_enable_set
 * Description:
 *      Set CPU port function enable/disable.
 * Input:
 *      enable - CPU port function enable
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_ENABLE - Invalid enable parameter.
 * Note:
 *      The API can set CPU port function enable/disable. 
 */
rtk_api_ret_t rtk_cpu_enable_set(rtk_enable_t enable)
{
    rtk_api_ret_t retVal;

    if (enable >=RTK_ENABLE_END)
        return RT_ERR_ENABLE;

    if ((retVal = rtl8370_setAsicCputagEnable(enable))!=RT_ERR_OK)
        return retVal;

    if (DISABLED == enable)
    {
        if ((retVal = rtl8370_setAsicCputagPortmask(0))!=RT_ERR_OK)
            return retVal;
    }    

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_cpu_enable_get
 * Description:
 *      Get CPU port enable.
 * Input:
 *      None
 * Output:
 *      pEnable - CPU port function enable
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 * Note:
 *      The API can get CPU port function enable/disable.
 */
rtk_api_ret_t rtk_cpu_enable_get(rtk_data_t *pEnable)
{
    rtk_api_ret_t retVal;

    if ((retVal = rtl8370_getAsicCputagEnable(pEnable))!=RT_ERR_OK)
        return retVal;
   

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_cpu_tagPort_set
 * Description:
 *      Set CPU port and CPU tag insert mode.
 * Input:
 *      port - Port id.
 *      mode - CPU tag insert for packets egress from CPU port.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameter.
 *      RT_ERR_PORT_ID - Invalid port number.
 * Note:
 *      The API can set CPU port and inserting proprietary CPU tag mode (Length/Type 0x8899)
 *      to the frame that transmitting to CPU port.
 *      The inset cpu tag mode is as following:
 *      CPU_INSERT_TO_ALL
 *      CPU_INSERT_TO_TRAPPING
 *      CPU_INSERT_TO_NONE   
 */
rtk_api_ret_t rtk_cpu_tagPort_set(rtk_port_t port, rtk_cpu_insert_t mode)
{
    rtk_api_ret_t retVal;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_INPUT;

    if (mode >= CPU_INSERT_END)
        return RT_ERR_INPUT;
    
    if ((retVal = rtl8370_setAsicCputagPortmask(1<<port))!=RT_ERR_OK)
        return retVal;

    if ((retVal = rtl8370_setAsicCputagTrapPort(port))!=RT_ERR_OK)
        return retVal;

    if ((retVal = rtl8370_setAsicCputagInsertMode(mode))!=RT_ERR_OK)
        return retVal;
        
    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_cpu_tagPort_get
 * Description:
 *      Get CPU port and CPU tag insert mode.
 * Input:
 *      None
 * Output:
 *      pPort - Port id.
 *      pMode - CPU tag insert for packets egress from CPU port, 0:all insert 1:Only for trapped packets 2:no insert.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 *      RT_ERR_L2_NO_CPU_PORT - CPU port is not exist
 * Note:
 *      The API can get configured CPU port and its setting.
 *      The inset cpu tag mode is as following:
 *      CPU_INSERT_TO_ALL
 *      CPU_INSERT_TO_TRAPPING
 *      CPU_INSERT_TO_NONE  
 */
rtk_api_ret_t rtk_cpu_tagPort_get(rtk_port_t *pPort, rtk_data_t *pMode)
{
    rtk_api_ret_t retVal;
    uint32 i, pmsk, port;

    if ((retVal = rtl8370_getAsicCputagPortmask(&pmsk))!=RT_ERR_OK)
        return retVal;

    if ((retVal = rtl8370_getAsicCputagTrapPort(&port))!=RT_ERR_OK)
        return retVal;

    for (i=0;i< RTK_MAX_NUM_OF_PORT;i++)
    {
        if ((pmsk&(1<<i))!=0)
        {
            if (i==port)
                *pPort = port;
            else
                return RT_ERR_FAILED;
        }
    }    

    if ((retVal = rtl8370_getAsicCputagInsertMode(pMode))!=RT_ERR_OK)
        return retVal; 
            
    return RT_ERR_OK;
}

rtk_api_ret_t rtk_trunk_port_set(rtk_trunk_group_t trk_gid, rtk_portmask_t trunk_member_portmask)
{
    rtk_api_ret_t retVal;
    uint32 pmsk;

    if (trk_gid>=TRUNK_GROUP_END)
        return RT_ERR_LA_TRUNK_ID; 

    if (trunk_member_portmask.bits[0] > RTK_MAX_PORT_MASK)
        return RT_ERR_PORT_MASK; 

    if ((trunk_member_portmask.bits[0]|RTK_PORT_TRUNK_GROUP_MASK(trk_gid))!=RTK_PORT_TRUNK_GROUP_MASK(trk_gid))
        return RT_ERR_PORT_MASK;

    pmsk = (trunk_member_portmask.bits[0]&RTK_PORT_TRUNK_GROUP_MASK(trk_gid))>>RTK_PORT_TRUNK_GROUP_OFFSET(trk_gid);

    if ((retVal = rtl8370_setAsicTrunkingGroup(trk_gid,pmsk))!=RT_ERR_OK)
        return retVal;
    
    return RT_ERR_OK;
}


/* Function Name:
 *      rtk_trunk_port_get
 * Description:
 *      Get trunking group available port mask
 * Input:
 *      trk_gid - trunk group id
 * Output:
 *      pTrunk_member_portmask - Logic trunking member port mask
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_LA_TRUNK_ID - Invalid trunking group
 * Note:
 *      The API can get 4 port trunking group enabled port mask. Each port trunking group has max 4 ports.
 *      If enabled port mask has less than 2 ports available setting, then this trunking group function is disabled.
 *      The group port members for trunk group are as following: 
 *      TRUNK_GROUP0: port 0 to port 3.
 *      TRUNK_GROUP1: port 4 to port 7.
 *      TRUNK_GROUP2: port 8 to port 11.
 *      TRUNK_GROUP3: port 12 to port 15.
 */
rtk_api_ret_t rtk_trunk_port_get(rtk_trunk_group_t trk_gid, rtk_portmask_t *pTrunk_member_portmask)
{
    rtk_api_ret_t retVal;

    uint32 pmsk;

    if (trk_gid>=TRUNK_GROUP_END)
        return RT_ERR_LA_TRUNK_ID; 

    if ((retVal = rtl8370_getAsicTrunkingGroup(trk_gid,&pmsk))!=RT_ERR_OK)
        return retVal;

    pTrunk_member_portmask->bits[0] = pmsk<<RTK_PORT_TRUNK_GROUP_OFFSET(trk_gid);
        
    return RT_ERR_OK;
}


/* Function Name:
 *      rtk_trunk_distributionAlgorithm_get
 * Description:
 *      Get port trunking hash select sources
 * Input:
 *      trk_gid - trunk group id
 * Output:
 *      pAlgo_bitmask -  Bitmask of the distribution algorithm
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_LA_TRUNK_ID - Invalid trunking group
 * Note:
 *      The API can get port trunking hash algorithm sources.
 */
rtk_api_ret_t rtk_trunk_distributionAlgorithm_get(rtk_trunk_group_t trk_gid, rtk_trunk_hashVal2Port_t *pAlgo_bitmask)
{
    rtk_api_ret_t retVal;
    
    if (trk_gid != RTK_WHOLE_SYSTEM)
        return RT_ERR_LA_TRUNK_ID;
    
        
    if ((retVal = rtl8370_getAsicTrunkingHashSelect((uint32*)&pAlgo_bitmask->value[0]))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}


#ifdef EMBEDDED_SUPPORT

rtk_api_ret_t rtk_stat_global_get(rtk_stat_global_type_t cntr_idx, rtk_stat_counter_t *pCntrH, rtk_stat_counter_t *pCntrL)
{
    rtk_api_ret_t retVal;

    if (cntr_idx!=DOT1D_TP_LEARNED_ENTRY_DISCARDS_INDEX)
            return RT_ERR_STAT_INVALID_GLOBAL_CNTR;

    if ((retVal = rtl8370_getAsicMIBsCounter(0,cntr_idx,pCntrH, pCntrL))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

rtk_api_ret_t rtk_stat_port_get(rtk_port_t port, rtk_stat_port_type_t cntr_idx, rtk_stat_counter_t *pCntrH, rtk_stat_counter_t *pCntrL)
{
    rtk_api_ret_t retVal;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID; 
    
    if (cntr_idx>=STAT_PORT_CNTR_END)
        return RT_ERR_STAT_INVALID_PORT_CNTR;

    if ((retVal = rtl8370_getAsicMIBsCounter(port,cntr_idx,pCntrH, pCntrL))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

#else

/* Function Name:
 *      rtk_stat_global_get
 * Description:
 *      Get global MIB counter
 * Input:
 *      cntr_idx - global counter index.
 * Output:
 *      pCntr - global counter value.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 * Note:
 *      Get global MIB counter by index definition. 
 */
rtk_api_ret_t rtk_stat_global_get(rtk_stat_global_type_t cntr_idx, rtk_stat_counter_t *pCntr)
{
    rtk_api_ret_t retVal;

    if (cntr_idx!=DOT1D_TP_LEARNED_ENTRY_DISCARDS_INDEX)
        return RT_ERR_STAT_INVALID_GLOBAL_CNTR;

    if ((retVal = rtl8370_getAsicMIBsCounter(0,cntr_idx,pCntr))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_stat_global_getAll
 * Description:
 *      Get all global MIB counter
 * Input:
 *      None
 * Output:
 *      pGlobal_cntrs - global counter structure.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 * Note:
 *      Get all global MIB counter by index definition.  
 */
rtk_api_ret_t rtk_stat_global_getAll(rtk_stat_global_cntr_t *pGlobal_cntrs)
{
    rtk_api_ret_t retVal;

    if ((retVal = rtl8370_getAsicMIBsCounter(0,DOT1D_TP_LEARNED_ENTRY_DISCARDS_INDEX,&pGlobal_cntrs->dot1dTpLearnedEntryDiscards))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_stat_port_get
 * Description:
 *      Get per port MIB counter by index
 * Input:
 *      port - port id.
 *      cntr_idx - port counter index.
 * Output:
 *      pCntr - MIB retrived counter.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 * Note:
 *      Get per port MIB counter by index definition. 
 */
rtk_api_ret_t rtk_stat_port_get(rtk_port_t port, rtk_stat_port_type_t cntr_idx, rtk_stat_counter_t *pCntr)
{
    rtk_api_ret_t retVal;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID; 
    
    if (cntr_idx>=STAT_PORT_CNTR_END)
        return RT_ERR_STAT_INVALID_PORT_CNTR;

    if ((retVal = rtl8370_getAsicMIBsCounter(port,cntr_idx,pCntr))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_stat_port_getAll
 * Description:
 *      Get all counters of one specified port in the specified device.
 * Input:
 *      port - port id.
 * Output:
 *      pPort_cntrs - buffer pointer of counter value.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 * Note:
 *      Get all MIB counters of one port.
 */
rtk_api_ret_t rtk_stat_port_getAll(rtk_port_t port, rtk_stat_port_cntr_t *pPort_cntrs)
{
    rtk_api_ret_t retVal;
    uint32 mibIndex;
    uint64 mibCounter;
    uint32 *accessPtr;
    /* address offset to MIBs counter */
    CONST_T uint16 mibLength[STAT_PORT_CNTR_END]= {
        2,1,1,1,1,1,1,1,1,
        2,1,1,1,1,1,1,1,1,1,1,
        2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID; 

    accessPtr = (uint32*)pPort_cntrs;    
    for (mibIndex=0;mibIndex<STAT_PORT_CNTR_END;mibIndex++)
    {
        if ((retVal = rtl8370_getAsicMIBsCounter(port,mibIndex,&mibCounter))!=RT_ERR_OK)        
            return retVal;

        if (2 == mibLength[mibIndex])
            *(uint64*)accessPtr = mibCounter;
        else if (1 == mibLength[mibIndex])
            *accessPtr = mibCounter;
        else 
            return RT_ERR_FAILED;
        
        accessPtr+=mibLength[mibIndex];
    }

    return RT_ERR_OK;
}

#endif




/* Function Name:
 *      rtk_stat_port_reset
 * Description:
 *      Reset per port MIB counter by port.
 * Input:
 *      port - port id.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 * Note:
 *      Reset MIB counter of ports. API will use global reset while port mask is all-ports.
 */

rtk_api_ret_t rtk_stat_port_reset(rtk_port_t port)
{
    rtk_api_ret_t retVal;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID; 
    
    if ((retVal = rtl8370_setAsicMIBsCounterReset(FALSE,FALSE,1<<port))!=RT_ERR_OK)
        return retVal; 
        
    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_port_phyForceModeAbility_set
 * Description:
 *      Set the port speed/duplex mode/pause/asy_pause in the PHY force mode.
 * Input:
 *      port - port id.
 *      pAbility - Ability structure
 * Output:
 *      None 
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_PHY_REG_ID - Invalid PHY address
 *      RT_ERR_INPUT - Invalid input parameters.
 *      RT_ERR_BUSYWAIT_TIMEOUT - PHY access busy
 * Note:
 *      If Full_1000 bit is set to 1, the AutoNegotiation will be automatic set to 1. While both AutoNegotiation and Full_1000 are set to 0, the PHY speed and duplex selection will
 *      be set as following 100F > 100H > 10F > 10H priority sequence.
 */
rtk_api_ret_t rtk_port_phyForceModeAbility_set(rtk_port_t port, rtk_port_phy_ability_t *pAbility)
{
    rtk_api_ret_t retVal;
    uint32 phyData;
    uint32 phyEnMsk0;
    uint32 phyEnMsk4;
    uint32 phyEnMsk9;
    

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;            

    if (pAbility->Half_10>=RTK_ENABLE_END||pAbility->Full_10>=RTK_ENABLE_END||
       pAbility->Half_100>=RTK_ENABLE_END||pAbility->Full_100>=RTK_ENABLE_END||
       pAbility->Full_1000>=RTK_ENABLE_END||pAbility->AutoNegotiation>=RTK_ENABLE_END||       
       pAbility->AsyFC>=RTK_ENABLE_END||pAbility->FC>=RTK_ENABLE_END)
        return RT_ERR_INPUT; 

    if (1 == pAbility->Full_1000)
        return RT_ERR_INPUT;

    /*for PHY force mode setup*/
    pAbility->AutoNegotiation = 0;
    
    phyEnMsk0 = 0;
    phyEnMsk4 = 0;
    phyEnMsk9 = 0;
    
    if (1 == pAbility->Half_10)
    {
        /*10BASE-TX half duplex capable in reg 4.5*/
        phyEnMsk4 = phyEnMsk4 | (1<<5);

        /*Speed selection [1:0] */
        /* 11=Reserved*/
        /* 10= 1000Mpbs*/
        /* 01= 100Mpbs*/
        /* 00= 10Mpbs*/        
        phyEnMsk0 = phyEnMsk0 & (~(1<<6));
        phyEnMsk0 = phyEnMsk0 & (~(1<<13));
    }

    if (1 == pAbility->Full_10)
    {
        /*10BASE-TX full duplex capable in reg 4.6*/
        phyEnMsk4 = phyEnMsk4 | (1<<6);
        /*Speed selection [1:0] */
        /* 11=Reserved*/
        /* 10= 1000Mpbs*/
        /* 01= 100Mpbs*/
        /* 00= 10Mpbs*/        
        phyEnMsk0 = phyEnMsk0 & (~(1<<6));
        phyEnMsk0 = phyEnMsk0 & (~(1<<13));

        /*Full duplex mode in reg 0.8*/
        phyEnMsk0 = phyEnMsk0 | (1<<8);
        
    }

    if (1 == pAbility->Half_100)
    {
        /*100BASE-TX half duplex capable in reg 4.7*/
        phyEnMsk4 = phyEnMsk4 | (1<<7);
        /*Speed selection [1:0] */
        /* 11=Reserved*/
        /* 10= 1000Mpbs*/
        /* 01= 100Mpbs*/
        /* 00= 10Mpbs*/        
        phyEnMsk0 = phyEnMsk0 & (~(1<<6));
        phyEnMsk0 = phyEnMsk0 | (1<<13);
    }


    if (1 == pAbility->Full_100)
    {
        /*100BASE-TX full duplex capable in reg 4.8*/
        phyEnMsk4 = phyEnMsk4 | (1<<8);
        /*Speed selection [1:0] */
        /* 11=Reserved*/
        /* 10= 1000Mpbs*/
        /* 01= 100Mpbs*/
        /* 00= 10Mpbs*/        
        phyEnMsk0 = phyEnMsk0 & (~(1<<6));
        phyEnMsk0 = phyEnMsk0 | (1<<13);
        /*Full duplex mode in reg 0.8*/
        phyEnMsk0 = phyEnMsk0 | (1<<8);
    }
    
    
    if (1 == pAbility->Full_1000)
    {
        /*1000 BASE-T FULL duplex capable setting in reg 9.9*/
        phyEnMsk9 = phyEnMsk9 | (1<<9);

        /*Speed selection [1:0] */
        /* 11=Reserved*/
        /* 10= 1000Mpbs*/
        /* 01= 100Mpbs*/
        /* 00= 10Mpbs*/        
        phyEnMsk0 = phyEnMsk0 | (1<<6);
        phyEnMsk0 = phyEnMsk0 & (~(1<<13));
    

        /*Auto-Negotiation setting in reg 0.12*/
        phyEnMsk0 = phyEnMsk0 | (1<<12);

    }

    if (1 == pAbility->AsyFC)
    {
        /*Asymetric flow control in reg 4.11*/
        phyEnMsk4 = phyEnMsk4 | (1<<11);
    }
    else
    {
        phyEnMsk4 &= ~(1<<11);
    }
    if (1 == pAbility->FC)
    {
        /*Flow control in reg 4.10*/
        phyEnMsk4 = phyEnMsk4 | (1<<10);
    }
    else
    {
        phyEnMsk4 &= ~(1<<10);
    }

    if ((retVal = rtl8370_setAsicPHYReg(port,PHY_PAGE_ADDRESS,0))!=RT_ERR_OK)
        return retVal;  
    
    /*1000 BASE-T control register setting*/
    if ((retVal = rtl8370_getAsicPHYReg(port,PHY_1000_BASET_CONTROL_REG,&phyData))!=RT_ERR_OK)
        return retVal;

    phyData = (phyData & (~0x0200)) | phyEnMsk9 ;

    if ((retVal = rtl8370_setAsicPHYReg(port,PHY_1000_BASET_CONTROL_REG,phyData))!=RT_ERR_OK)
        return retVal;

    /*Auto-Negotiation control register setting*/
    if ((retVal = rtl8370_getAsicPHYReg(port,PHY_AN_ADVERTISEMENT_REG,&phyData))!=RT_ERR_OK)
        return retVal;

    phyData = (phyData & (~0x0DE0)) | phyEnMsk4;
    if ((retVal = rtl8370_setAsicPHYReg(port,PHY_AN_ADVERTISEMENT_REG,phyData))!=RT_ERR_OK)
        return retVal;

     /*Control register setting and power off/on*/
     phyData = phyEnMsk0 & (~(1 << 12));
     phyData |= (1 << 11);   /* power down PHY, bit 11 should be set to 1 */
     if ((retVal = rtl8370_setAsicPHYReg(port, PHY_CONTROL_REG, phyData)) != RT_ERR_OK)
        return retVal;

     phyData = phyData & (~(1 << 11));   /* power on PHY, bit 11 should be set to 0*/
    if ((retVal = rtl8370_setAsicPHYReg(port,PHY_CONTROL_REG,phyData))!=RT_ERR_OK)
        return retVal;

    if ((retVal = rtl8370_setAsicPHYReg(port,PHY_PAGE_ADDRESS,0))!=RT_ERR_OK)
        return retVal;    

    return RT_ERR_OK;
}


/* Function Name:
 *      rtk_l2_limitLearningCnt_set
 * Description:
 *      Set per-Port auto learning limit number
 * Input:
 *      port - Port id.
 *      mac_cnt - Auto learning entries limit number
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_LIMITED_L2ENTRY_NUM - Invalid auto learning limit number
 * Note:
 *      The API can set per-port ASIC auto learning limit number from 0(disable learning) 
 *      to 8k. 
 */
rtk_api_ret_t rtk_l2_limitLearningCnt_set(rtk_port_t port, rtk_mac_cnt_t mac_cnt)
{
    rtk_api_ret_t retVal;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;

    if (mac_cnt > RTK_MAX_NUM_OF_LEARN_LIMIT)
        return RT_ERR_LIMITED_L2ENTRY_NUM;

    if ((retVal = rtl8370_setAsicLutLearnLimitNo(port,mac_cnt))!=RT_ERR_OK)
        return retVal; 
    
    return RT_ERR_OK;
}    

/* Function Name:
 *      rtk_port_phyAutoNegoAbility_set
 * Description:
 *      Set ethernet PHY auto-negotiation desired ability.
 * Input:
 *      port - port id.
 *      pAbility - Ability structure
 * Output:
 *      None 
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_PHY_REG_ID - Invalid PHY address
 *      RT_ERR_INPUT - Invalid input parameters.
 *      RT_ERR_BUSYWAIT_TIMEOUT - PHY access busy
 * Note:
 *      If Full_1000 bit is set to 1, the AutoNegotiation will be automatic set to 1. While both AutoNegotiation and Full_1000 are set to 0, the PHY speed and duplex selection will
 *      be set as following 100F > 100H > 10F > 10H priority sequence.
 */
rtk_api_ret_t rtk_port_phyAutoNegoAbility_set(rtk_port_t port, rtk_port_phy_ability_t *pAbility)
{
    rtk_api_ret_t retVal;
    uint32 phyData;
    uint32 phyEnMsk0;
    uint32 phyEnMsk4;
    uint32 phyEnMsk9;
    

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;            

    if (pAbility->Half_10>=RTK_ENABLE_END||pAbility->Full_10>=RTK_ENABLE_END||
       pAbility->Half_100>=RTK_ENABLE_END||pAbility->Full_100>=RTK_ENABLE_END||
       pAbility->Full_1000>=RTK_ENABLE_END||pAbility->AutoNegotiation>=RTK_ENABLE_END||       
       pAbility->AsyFC>=RTK_ENABLE_END||pAbility->FC>=RTK_ENABLE_END)
        return RT_ERR_INPUT; 

    /*for PHY auto mode setup*/
    pAbility->AutoNegotiation = 1;    

    phyEnMsk0 = 0;
    phyEnMsk4 = 0;
    phyEnMsk9 = 0;
    
    if (1 == pAbility->Half_10)
    {
        /*10BASE-TX half duplex capable in reg 4.5*/
        phyEnMsk4 = phyEnMsk4 | (1<<5);

        /*Speed selection [1:0] */
        /* 11=Reserved*/
        /* 10= 1000Mpbs*/
        /* 01= 100Mpbs*/
        /* 00= 10Mpbs*/        
        phyEnMsk0 = phyEnMsk0 & (~(1<<6));
        phyEnMsk0 = phyEnMsk0 & (~(1<<13));
    }

    if (1 == pAbility->Full_10)
    {
        /*10BASE-TX full duplex capable in reg 4.6*/
        phyEnMsk4 = phyEnMsk4 | (1<<6);
        /*Speed selection [1:0] */
        /* 11=Reserved*/
        /* 10= 1000Mpbs*/
        /* 01= 100Mpbs*/
        /* 00= 10Mpbs*/        
        phyEnMsk0 = phyEnMsk0 & (~(1<<6));
        phyEnMsk0 = phyEnMsk0 & (~(1<<13));

        /*Full duplex mode in reg 0.8*/
        phyEnMsk0 = phyEnMsk0 | (1<<8);
        
    }

    if (1 == pAbility->Half_100)
    {
        /*100BASE-TX half duplex capable in reg 4.7*/
        phyEnMsk4 = phyEnMsk4 | (1<<7);
        /*Speed selection [1:0] */
        /* 11=Reserved*/
        /* 10= 1000Mpbs*/
        /* 01= 100Mpbs*/
        /* 00= 10Mpbs*/        
        phyEnMsk0 = phyEnMsk0 & (~(1<<6));
        phyEnMsk0 = phyEnMsk0 | (1<<13);
    }


    if (1 == pAbility->Full_100)
    {
        /*100BASE-TX full duplex capable in reg 4.8*/
        phyEnMsk4 = phyEnMsk4 | (1<<8);
        /*Speed selection [1:0] */
        /* 11=Reserved*/
        /* 10= 1000Mpbs*/
        /* 01= 100Mpbs*/
        /* 00= 10Mpbs*/        
        phyEnMsk0 = phyEnMsk0 & (~(1<<6));
        phyEnMsk0 = phyEnMsk0 | (1<<13);
        /*Full duplex mode in reg 0.8*/
        phyEnMsk0 = phyEnMsk0 | (1<<8);
    }
    
    
    if (1 == pAbility->Full_1000)
    {
        /*1000 BASE-T FULL duplex capable setting in reg 9.9*/
        phyEnMsk9 = phyEnMsk9 | (1<<9);

        /*Speed selection [1:0] */
        /* 11=Reserved*/
        /* 10= 1000Mpbs*/
        /* 01= 100Mpbs*/
        /* 00= 10Mpbs*/        
        phyEnMsk0 = phyEnMsk0 | (1<<6);
        phyEnMsk0 = phyEnMsk0 & (~(1<<13));
    

        /*Auto-Negotiation setting in reg 0.12*/
        phyEnMsk0 = phyEnMsk0 | (1<<12);

     }
    
    if (1 == pAbility->AutoNegotiation)
    {
        /*Auto-Negotiation setting in reg 0.12*/
        phyEnMsk0 = phyEnMsk0 | (1<<12);
    }

    if (1 == pAbility->AsyFC)
    {
        /*Asymetric flow control in reg 4.11*/
        phyEnMsk4 = phyEnMsk4 | (1<<11);
    }
    else
    {
        phyEnMsk4 &= ~(1<<11);
    }
    if (1 == pAbility->FC)
    {
        /*Flow control in reg 4.10*/
        phyEnMsk4 = phyEnMsk4 | (1<<10);
    }
    else
    {
        phyEnMsk4 &= ~(1<<10);
    }

    if ((retVal = rtl8370_setAsicPHYReg(port,PHY_PAGE_ADDRESS,0))!=RT_ERR_OK)
        return retVal;  
    
    /*1000 BASE-T control register setting*/
    if ((retVal = rtl8370_getAsicPHYReg(port,PHY_1000_BASET_CONTROL_REG,&phyData))!=RT_ERR_OK)
        return retVal;

    phyData = (phyData & (~0x0200)) | phyEnMsk9 ;

    if ((retVal = rtl8370_setAsicPHYReg(port,PHY_1000_BASET_CONTROL_REG,phyData))!=RT_ERR_OK)
        return retVal;

    /*Auto-Negotiation control register setting*/
    if ((retVal = rtl8370_getAsicPHYReg(port,PHY_AN_ADVERTISEMENT_REG,&phyData))!=RT_ERR_OK)
        return retVal;

    phyData = (phyData & (~0x0DE0)) | phyEnMsk4;
    if ((retVal = rtl8370_setAsicPHYReg(port,PHY_AN_ADVERTISEMENT_REG,phyData))!=RT_ERR_OK)
        return retVal;

    /*Control register setting and restart auto*/
    if ((retVal = rtl8370_getAsicPHYReg(port,PHY_CONTROL_REG,&phyData))!=RT_ERR_OK)
        return retVal;

    phyData = (phyData & (~0x3140)) | phyEnMsk0;
    /*If have auto-negotiation capable, then restart auto negotiation*/
    if (1 == pAbility->AutoNegotiation)
    {
        phyData = phyData | (1 << 9);
    }

    if ((retVal = rtl8370_setAsicPHYReg(port,PHY_CONTROL_REG,phyData))!=RT_ERR_OK)
        return retVal;    
    
    if ((retVal = rtl8370_setAsicPHYReg(port,PHY_PAGE_ADDRESS,0))!=RT_ERR_OK)
        return retVal;   
    
    return RT_ERR_OK;
}

/*
从eth0发出的报文，全部从port0出去
*/
static rtk_api_ret_t rtl8370_acl_from_eth0(rtk_filter_id_t filter_id, uint8 *mac)
{
   int retVal = 0;
   u32 ruleNum = 0;
   rtk_filter_cfg_t cfg;
   rtk_filter_action_t act;
   rtk_filter_field_t *field = NULL;

   field = kmalloc(sizeof(rtk_filter_field_t), GFP_KERNEL);
   if( field == NULL )
   {
       printk("%s: kmalloc field failed.\n", __func__);
       return RT_ERR_FAILED;
   }   

   /* set the memory 0 */
   memset(field, 0, sizeof(rtk_filter_field_t));
   memset(&cfg, 0, sizeof(rtk_filter_cfg_t));
   memset(&act, 0, sizeof(rtk_filter_action_t));

   field->fieldType = FILTER_FIELD_SMAC;
   field->filter_pattern_union.smac.dataType = FILTER_FIELD_DATA_MASK;
   field->filter_pattern_union.smac.value.octet[0] = mac[0];
   field->filter_pattern_union.smac.value.octet[1] = mac[1];
   field->filter_pattern_union.smac.value.octet[2] = mac[2];
   field->filter_pattern_union.smac.value.octet[3] = mac[3];
   field->filter_pattern_union.smac.value.octet[4] = mac[4];
   field->filter_pattern_union.smac.value.octet[5] = mac[5];
   
   field->filter_pattern_union.smac.mask.octet[0] = 0xFF;
   field->filter_pattern_union.smac.mask.octet[1] = 0xFF;
   field->filter_pattern_union.smac.mask.octet[2] = 0xFF;
   field->filter_pattern_union.smac.mask.octet[3] = 0xFF;
   field->filter_pattern_union.smac.mask.octet[4] = 0xFF;
   field->filter_pattern_union.smac.mask.octet[5] = 0xFF;        

   retVal = rtk_filter_igrAcl_field_add(&cfg, field);
   if( retVal != RT_ERR_OK )
   {
       printk(KERN_ERR "%s: rtk_filter_igrAcl_field_add failed! retVal=0x%x\n", __func__, retVal);
       goto out;
   }   

   /* config cfg */
   cfg.activeport.dataType = FILTER_FIELD_DATA_RANGE;
   cfg.activeport.rangeStart = PORT8;
   cfg.activeport.rangeEnd = PORT8;
   cfg.invert = FALSE; 

   /* action */
   act.actEnable[FILTER_ENACT_REDIRECT] = TRUE;
   act.filterRedirectPortmask = PORT0_MASK;

   retVal = rtk_filter_igrAcl_cfg_add(filter_id, &cfg, &act, &ruleNum);
   if( retVal != RT_ERR_OK )
   {
       printk("%s: rtk_filter_igrAcl_cfg_add failed! retVal=0x%x\n", __func__, retVal);
   }
   else
   {
       printk("%s: acl set success, filter_id=%d ruleNum=%d\n", __func__, filter_id, ruleNum);
   }   

out:
   kfree(field);
   return retVal;
}

/*
从eth1发出的报文，全部从port1出去
*/
static rtk_api_ret_t rtl8370_acl_from_eth1(rtk_filter_id_t filter_id, uint8 *mac)
{
   int retVal = 0;
   u32 ruleNum = 0;
   rtk_filter_cfg_t cfg;
   rtk_filter_action_t act;
   rtk_filter_field_t *field = NULL;

   field = kmalloc(sizeof(rtk_filter_field_t), GFP_KERNEL);
   if( field == NULL )
   {
       printk("%s: kmalloc field failed.\n", __func__);
       return RT_ERR_FAILED;
   }   

   /* set the memory 0 */
   memset(field, 0, sizeof(rtk_filter_field_t));
   memset(&cfg, 0, sizeof(rtk_filter_cfg_t));
   memset(&act, 0, sizeof(rtk_filter_action_t));

   field->fieldType = FILTER_FIELD_SMAC;
   field->filter_pattern_union.smac.dataType = FILTER_FIELD_DATA_MASK;
   field->filter_pattern_union.smac.value.octet[0] = mac[0];
   field->filter_pattern_union.smac.value.octet[1] = mac[1];
   field->filter_pattern_union.smac.value.octet[2] = mac[2];
   field->filter_pattern_union.smac.value.octet[3] = mac[3];
   field->filter_pattern_union.smac.value.octet[4] = mac[4];
   field->filter_pattern_union.smac.value.octet[5] = mac[5];
   
   field->filter_pattern_union.smac.mask.octet[0] = 0xFF;
   field->filter_pattern_union.smac.mask.octet[1] = 0xFF;
   field->filter_pattern_union.smac.mask.octet[2] = 0xFF;
   field->filter_pattern_union.smac.mask.octet[3] = 0xFF;
   field->filter_pattern_union.smac.mask.octet[4] = 0xFF;
   field->filter_pattern_union.smac.mask.octet[5] = 0xFF;        

   retVal = rtk_filter_igrAcl_field_add(&cfg, field);
   if( retVal != RT_ERR_OK )
   {
       printk(KERN_ERR "%s: rtk_filter_igrAcl_field_add failed! retVal=0x%x\n", __func__, retVal);
       goto out;
   }   

   /* config cfg */
   cfg.activeport.dataType = FILTER_FIELD_DATA_RANGE;
   cfg.activeport.rangeStart = PORT8;
   cfg.activeport.rangeEnd = PORT8;
   cfg.invert = FALSE; 

   /* action */
   act.actEnable[FILTER_ENACT_REDIRECT] = TRUE;
   act.filterRedirectPortmask = PORT1_MASK;

   retVal = rtk_filter_igrAcl_cfg_add(filter_id, &cfg, &act, &ruleNum);
   if( retVal != RT_ERR_OK )
   {
       printk("%s: rtk_filter_igrAcl_cfg_add failed! retVal=0x%x\n", __func__, retVal);
   }
   else
   {
       printk("%s: acl set success, filter_id=%d ruleNum=%d\n", __func__, filter_id, ruleNum);
   }   

out:
   kfree(field);
   return retVal;
}

/*
从port0进来的报文，全部从上报到port8
*/
static rtk_api_ret_t rtl8370_acl_from_extphy(rtk_filter_id_t filter_id)
{
   int                 retVal;
   uint32              ruleNum = 0;
   rtk_filter_field_t *field;
   rtk_filter_cfg_t    cfg;
   rtk_filter_action_t act;

   field = kmalloc(sizeof(rtk_filter_field_t), GFP_KERNEL);
   if( field == NULL )
   {
       printk("%s: kmalloc failed.\n", __func__);
       return -1;
   }

   memset(field, 0, sizeof(rtk_filter_field_t));
   memset(&cfg, 0, sizeof(rtk_filter_cfg_t));
   memset(&act, 0, sizeof(rtk_filter_action_t));

   /* 过滤任意协议类型 */
   {
       field->fieldType = FILTER_FIELD_ETHERTYPE;
       field->filter_pattern_union.etherType.dataType = FILTER_FIELD_DATA_MASK;
       field->filter_pattern_union.etherType.value = 0x0000; 
       field->filter_pattern_union.etherType.mask = 0x0000;
   }
   retVal = rtk_filter_igrAcl_field_add(&cfg, field);
   if( retVal != RT_ERR_OK )
   {
       printk(KERN_ERR "%s: rtk_filter_igrAcl_field_add failed! retVal=0x%x\n", __func__, retVal);
       goto out;
   }

   /* set udp to be care tag and port1  to active port */
   cfg.activeport.dataType = FILTER_FIELD_DATA_RANGE;
   cfg.activeport.rangeStart = PORT0;                              //[from]
   cfg.activeport.rangeEnd = PORT1;  
   cfg.invert = FALSE;   

   /* set action to redirect to port0[act.filterRedirectPortmask = 0x1] */    
   /* action */
   act.actEnable[FILTER_ENACT_REDIRECT] = TRUE;
   act.filterRedirectPortmask = PORT8_MASK;

   retVal = rtk_filter_igrAcl_cfg_add(filter_id, &cfg, &act, &ruleNum);
   if( retVal != RT_ERR_OK )
   {
       printk("%s: rtk_filter_igrAcl_cfg_add failed! retVal=0x%x\n", __func__, retVal);
   }
   else
   {
       printk("%s: acl set success, filter_id=%d ruleNum=%d\n", __func__, filter_id, ruleNum);
   }   

out:
   kfree(field);
   return retVal;
}

int rtk8370_acl_cfg(void)
{
	int retVal = 0;

    retVal = rtl8370_filter_igrAcl_init();
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


/* Function Name:
 *      rtk_mirror_portBased_set
 * Description:
 *      Set port mirror function.
 * Input:
 *      mirroring_port - Monitor port.
 *      pMirrored_rx_portmask - Rx mirror port mask.
 *      pMirrored_tx_portmask - Tx mirror port mask. 
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_PORT_MASK - Invalid portmask.
 * Note:
 *      The API is to set mirror function of source port and mirror port.
 *      The mirror port can only be set to one port and the TX and RX mirror ports
 *      should be identical.
 */
rtk_api_ret_t rtk_mirror_portBased_set(rtk_port_t mirroring_port, rtk_portmask_t *pMirrored_rx_portmask, rtk_portmask_t *pMirrored_tx_portmask)
{
    rtk_api_ret_t retVal;
    rtk_enable_t mirRx, mirTx;
    uint32 i;
      rtk_port_t source_port;
    
    if (mirroring_port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;     

    if (pMirrored_rx_portmask->bits[0] > RTK_MAX_PORT_MASK)
        return RT_ERR_PORT_MASK; 
    
    if (pMirrored_tx_portmask->bits[0] > RTK_MAX_PORT_MASK)
        return RT_ERR_PORT_MASK;
    
    /*Only one port for tx & rx mirror*/
    if (pMirrored_tx_portmask->bits[0]!=pMirrored_rx_portmask->bits[0]&&pMirrored_tx_portmask->bits[0]!=0&&pMirrored_rx_portmask->bits[0]!=0)
        return RT_ERR_PORT_MASK;
    
     /*mirror port != source port*/
    if ((pMirrored_tx_portmask->bits[0]&(1<<mirroring_port))>0||(pMirrored_rx_portmask->bits[0]&(1<<mirroring_port))>0)
        return RT_ERR_PORT_MASK;    
    
   source_port = 0;

   for (i=0;i< RTK_MAX_NUM_OF_PORT;i++)
   {
        if (pMirrored_tx_portmask->bits[0]&(1<<i))
        {
            source_port = i;
            break;
        }

        if (pMirrored_rx_portmask->bits[0]&(1<<i))
        {
            source_port = i;
            break;
        }
    }
    
    /*Only one port for tx & rx mirror*/
    if (pMirrored_tx_portmask->bits[0]>>(source_port+1))
        return RT_ERR_PORT_MASK;
    
    if (pMirrored_rx_portmask->bits[0]>>(source_port+1))
        return RT_ERR_PORT_MASK;    
    //printk(KERN_ERR "%s source_port=%d mirroring_port=%d mask=%x\n", __func__, source_port, mirroring_port,(pMirrored_tx_portmask->bits[0])>>i);
    if ((retVal = rtl8370_setAsicPortMirror(source_port,mirroring_port))!=RT_ERR_OK)
        return retVal;  

    if (pMirrored_rx_portmask->bits[0])
        mirRx = ENABLED;
    else
        mirRx = DISABLED;
    
    if ((retVal = rtl8370_setAsicPortMirrorRxFunction(mirRx))!=RT_ERR_OK)
        return retVal;        

    if (pMirrored_tx_portmask->bits[0])
        mirTx = ENABLED;
    else
        mirTx = DISABLED;
    
    if ((retVal = rtl8370_setAsicPortMirrorTxFunction(mirTx))!=RT_ERR_OK)
        return retVal;        
    printk(KERN_ERR "%s [%d ] mirRx=%d mirTx=%d\n", __func__,__LINE__,mirRx,mirTx);
    return RT_ERR_OK;

}

/* Function Name:
 *      rtk_mirror_portBased_get
 * Description:
 *      Get port mirror function.
 * Input:
 *      None
 * Output:
 *      pMirroring_port - Monitor port.
 *      pMirrored_rx_portmask - Rx mirror port mask.
 *      pMirrored_tx_portmask - Tx mirror port mask.  
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 * Note:
 *      The API is to get mirror function of source port and mirror port.
 */
rtk_api_ret_t rtk_mirror_portBased_get(rtk_port_t* pMirroring_port, rtk_portmask_t *pMirrored_rx_portmask, rtk_portmask_t *pMirrored_tx_portmask)
{
    rtk_api_ret_t retVal;
    rtk_port_t source_port;
    rtk_enable_t mirRx, mirTx;
    
    if ((retVal = rtl8370_getAsicPortMirror(&source_port,pMirroring_port))!=RT_ERR_OK)
        return retVal;     

    if ((retVal = rtl8370_getAsicPortMirrorRxFunction((uint32*)&mirRx))!=RT_ERR_OK)
        return retVal;        

    if ((retVal = rtl8370_getAsicPortMirrorTxFunction((uint32*)&mirTx))!=RT_ERR_OK)
        return retVal; 

    if (DISABLED == mirRx)
        pMirrored_rx_portmask->bits[0]=0;
    else
        pMirrored_rx_portmask->bits[0]=1<<source_port;

     if (DISABLED == mirTx)
        pMirrored_tx_portmask->bits[0]=0;
    else
        pMirrored_tx_portmask->bits[0]=1<<source_port;    

    return RT_ERR_OK;

}
/*************************************************************************************
* Function: rtk8370m_link_status_get()
* Desc: get port link status
*
* Return: RT_ERR_OK
**************************************************************************************/
int rtk8370m_link_status_get(rtk_port_t port, rtk_port_linkStatus_t *pLinkStatus)
{
    rtk_api_ret_t retVal;
    uint32 phyData;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID; 

    retVal = rtl8370_setAsicPHYReg(port, RTL8370_PHY_PAGE_ADDRESS, 0);
    if (RT_ERR_OK != retVal)
    {
        printk(KERN_ERR "%s: failed! retVal=0x%x\n", __func__, retVal);
        return retVal;
    } 

    /*Get PHY status register*/
    retVal = rtl8370_getAsicPHYReg(port,PHY_STATUS_REG,&phyData);
    if (RT_ERR_OK != retVal)
    {
        printk(KERN_ERR "%s: failed! retVal=0x%x\n", __func__, retVal);
        return RT_ERR_FAILED;
    }

    /*check link status*/
    if (phyData & (1<<2))
    {
        *pLinkStatus = 1;
    }
    else
    {
        *pLinkStatus = 0;
    }
    return RT_ERR_OK;
}


int geth_link_status_get(int port, int *link_status)
{
    rtk_port_linkStatus_t phy_status;
    u32 retVal;
    mutex_lock(&acl_cb->lock);
    retVal = rtk8370m_link_status_get(port,&phy_status); 
    if(retVal !=  RT_ERR_OK)
    {   
        mutex_unlock(&acl_cb->lock);
        return retVal;
    }
    *link_status = phy_status;
    mutex_unlock(&acl_cb->lock);
    return RT_ERR_OK;
}
EXPORT_SYMBOL(geth_link_status_get);

/* Function Name:
 *      rtk_port_phyAutoNegoAbility_get
 * Description:
 *      Get PHY ability through PHY registers.
 * Input:
 *      port - Port id.
 * Output:
 *      pAbility - Ability structure
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - FAILED
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_PHY_REG_ID - Invalid PHY address
 *      RT_ERR_INPUT - Invalid input parameters.
 *      RT_ERR_BUSYWAIT_TIMEOUT - PHY access busy
 * Note:
 *      Get the capablity of specified PHY.
 */
rtk_api_ret_t rtk_port_phyAutoNegoAbility_get(rtk_port_t port, rtk_port_phy_ability_t *pAbility)
{
    rtk_api_ret_t retVal;
    uint32 phyData0;
    uint32 phyData4;
    uint32 phyData9;
    
    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID; 

    if ((retVal = rtl8370_setAsicPHYReg(port,PHY_PAGE_ADDRESS,0))!=RT_ERR_OK)
        return retVal;  

    /*Control register setting and restart auto*/
    if ((retVal = rtl8370_getAsicPHYReg(port,PHY_CONTROL_REG,&phyData0))!=RT_ERR_OK)
        return retVal;

    /*Auto-Negotiation control register setting*/
    if ((retVal = rtl8370_getAsicPHYReg(port,PHY_AN_ADVERTISEMENT_REG,&phyData4))!=RT_ERR_OK)
        return retVal;

    /*1000 BASE-T control register setting*/
    if ((retVal = rtl8370_getAsicPHYReg(port,PHY_1000_BASET_CONTROL_REG,&phyData9))!=RT_ERR_OK)
        return retVal;

    if (phyData9 & (1<<9))
        pAbility->Full_1000 = 1;
    else
        pAbility->Full_1000 = 0;

    if (phyData4 & (1<<11))
        pAbility->AsyFC = 1;
    else
        pAbility->AsyFC = 0;

    if (phyData4 & (1<<10))
        pAbility->FC = 1;
    else
        pAbility->FC = 0;
    
    
    if (phyData4 & (1<<8))
        pAbility->Full_100= 1;
    else
        pAbility->Full_100= 0;
    
    if (phyData4 & (1<<7))
        pAbility->Half_100= 1;
    else
        pAbility->Half_100= 0;

    if (phyData4 & (1<<6))
        pAbility->Full_10= 1;
    else
        pAbility->Full_10= 0;
    
    if (phyData4 & (1<<5))
        pAbility->Half_10= 1;
    else
        pAbility->Half_10= 0;


    if (phyData0 & (1<<12))
        pAbility->AutoNegotiation= 1;
    else
        pAbility->AutoNegotiation= 0;

    if ((retVal = rtl8370_setAsicPHYReg(port,PHY_PAGE_ADDRESS,0))!=RT_ERR_OK)
        return retVal; 

    return RT_ERR_OK;
}

int geth_port_phyAutoNegoAbility_get(int port, u32 *mask)
{
    rtk_port_phy_ability_t pAbility;
    u32 retVal;
    *mask = 0;
    mutex_lock(&acl_cb->lock);
    retVal = rtk_port_phyAutoNegoAbility_get(port, &pAbility);
    if(retVal !=  RT_ERR_OK)
    {
        mutex_unlock(&acl_cb->lock);
        return retVal;
    }		
    *mask =  (pAbility.Half_10) |(pAbility.Full_10<<1) |(pAbility.Half_100<<2) | (pAbility.Full_100<<3) |\
        (pAbility.Full_1000<<5) |(pAbility.AutoNegotiation<<6);
    mutex_unlock(&acl_cb->lock);
    return RT_ERR_OK;
}
EXPORT_SYMBOL(geth_port_phyAutoNegoAbility_get);
/*************************************************************************************
* Function: rtk8370m_phy_status_get()
* Desc: get phy status registers
*
* Return: RT_ERR_OK
**************************************************************************************/
int rtk8370m_phy_status_get(rtk_port_t port, u32 *pphyStatus)
{
    rtk_api_ret_t retVal;
    uint32 phyData;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID; 

    retVal = rtl8370_setAsicPHYReg(port, RTL8370_PHY_PAGE_ADDRESS, 0);
    if (RT_ERR_OK != retVal)
    {
        printk(KERN_ERR "%s: failed! retVal=0x%x\n", __func__, retVal);
        return retVal;
    } 

    /*Get PHY status register*/
    retVal = rtl8370_getAsicPHYReg(port,PHY_STATUS_REG,&phyData);
    if (RT_ERR_OK != retVal)
    {
        printk(KERN_ERR "%s: failed! retVal=0x%x\n", __func__, retVal);
        return RT_ERR_FAILED;
    }
    *pphyStatus = phyData;
    
    return RT_ERR_OK;
}


int geth_phy_status_get(int port, u32 *support_mask)
{
    u32 port_status, retVal;
    *support_mask = 0;
    mutex_lock(&acl_cb->lock);	
    retVal = rtk8370m_phy_status_get(port,&port_status);      
    if(retVal !=  RT_ERR_OK)
    {
         mutex_unlock(&acl_cb->lock);
        return retVal;
    }	
    if(port_status & (1<<11))
    {
        *support_mask |= ADVERTISED_10baseT_Half;
    }
    if(port_status & (1<<12))
    {
        *support_mask |= ADVERTISED_10baseT_Full;
    }
    if(port_status & (1<<13))
    {
        *support_mask |= ADVERTISED_100baseT_Half;
    }
    if(port_status & (1<<14))
    {
        *support_mask |= ADVERTISED_100baseT_Full;
    }
    if(port_status & (1<<3))
    {
        *support_mask |= ADVERTISED_Autoneg;
    }
    *support_mask |= (ADVERTISED_1000baseT_Full |SUPPORTED_MII);  
    mutex_unlock(&acl_cb->lock);
    return RT_ERR_OK;
      
}
EXPORT_SYMBOL(geth_phy_status_get);

int  geth_getPortStatus(int port_phy, u32 *pSpeed, u32 *plink, u32 *pduplex, u32 *pnway)
{
    ret_t retVal;
    uint32 regData;

    mutex_lock(&acl_cb->lock);
    /* Invalid input parameter */
    if(port_phy >= RTL8370_PORTNO)
    {
        mutex_unlock(&acl_cb->lock);
        return RT_ERR_PORT_ID;
     }   
    retVal = rtl8370_getAsicReg(RTL8370_REG_PORT0_STATUS + port_phy, &regData);
    if(retVal !=  RT_ERR_OK)
    {
        mutex_unlock(&acl_cb->lock);
        return retVal;
    }
    *pSpeed = regData & 0x03;
    *plink = (regData>>4) & 0x1;
    *pduplex = (regData>>2) & 0x1;
    *pnway = (regData>>7) & 0x1;
    mutex_unlock(&acl_cb->lock);
    return RT_ERR_OK;
    
}
EXPORT_SYMBOL(geth_getPortStatus);