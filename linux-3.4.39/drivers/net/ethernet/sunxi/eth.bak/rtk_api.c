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

#include "rtk_types.h"
#include "rtk_error.h"
#include "rtl8370.h"
#include "rtl8370_reg.h"
#include "rtl8370_asicdrv_acl.h"
#include "rtl8370_asicdrv_svlan.h"
#include "rtl8370_asicdrv_cputag.h"

#include "rtk_api.h"

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
从port0进来的报文，全部从上报到port8，由linux系统决定包发向eth0还是eth1
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
		printk("%s: acl init failed.\n", __func__);
		return retVal;
	}

    gfilter_id = 0;
    retVal = rtl8370_acl_from_eth0(gfilter_id, eth0_mac);
    if(retVal != RT_ERR_OK)
    {
		printk("%s: rtl8370_acl_from_eth0 failed.\n", __func__);
		return retVal;
    }
    

    gfilter_id++;
    retVal = rtl8370_acl_from_eth1(gfilter_id, eth1_mac);
    if(retVal != RT_ERR_OK)
    {
		printk("%s: rtl8370_acl_from_eth1 failed.\n", __func__);
		return retVal;
    }
    
    gfilter_id++;
    retVal = rtl8370_acl_from_extphy(gfilter_id);
    if(retVal != RT_ERR_OK)
    {
		printk("%s: rtl8370_acl_from_phy0 failed.\n", __func__);
		return retVal;
    }

    return retVal;
}

