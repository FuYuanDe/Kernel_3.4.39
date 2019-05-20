#include <linux/string.h>
#include <linux/in.h>
#include "rtl8370_asicdrv_svlan.h"

void _rtl8370_svlanConfStSmi2User( rtl8370_svlan_memconf_t *stUser, rtl8370_svlan_memconf_smi_t *stSmi)
{
    stUser->vs_relaysvid = stSmi->vs_relaysvid;
    stUser->vs_msti = stSmi->vs_msti;
    stUser->vs_fid = stSmi->vs_fid;

    stUser->vs_member = stSmi->vs_member;

    stUser->vs_priority = stSmi->vs_priority;

    stUser->vs_svid = stSmi->vs_svid;

    stUser->vs_efiden = stSmi->vs_efiden;
    stUser->vs_efid = stSmi->vs_efid;
}

/*
@func ret_t | rtl8370_getAsicSvlanMemberConfiguration| Get SVLAN member Configure.
@parm uint32 | index | index of 8 s-tag configuration
@parm rtl8370_svlan_memconf_t* | svlanMemConf | SVLAN member configuration
@rvalue RT_ERR_OK | Success.
@rvalue RT_ERR_SMI | SMI access error.
@rvalue RT_ERR_SVLAN_ENTRY_INDEX | Invalid SVLAN configuration index.
@comm
    The API can get system 64 accepted s-tag frame format. Only 64 SVID S-tag frame will be accpeted
    to receiving from uplink ports. Other SVID S-tag frame or S-untagged frame will be droped.

*/
ret_t rtl8370_getAsicSvlanMemberConfiguration(uint32 index,rtl8370_svlan_memconf_t* svlanMemConf)
{
    ret_t retVal;
    uint32 regData;
    uint16 *accessPtr;
    uint32 i;

    rtl8370_svlan_memconf_smi_t smiSvlanMemConf;

    if(index > RTL8370_SVIDXMAX)
        return RT_ERR_SVLAN_ENTRY_INDEX;

    memset(&smiSvlanMemConf,0x00,sizeof(smiSvlanMemConf));

    accessPtr = (uint16*)&smiSvlanMemConf;

    for(i = 0; i < 4; i++)
    {
        retVal = rtl8370_getAsicReg(RTL8370_SVLAN_MEMBERCFG_BASE_REG+(index<<2)+i,&regData);
        if(retVal !=  RT_ERR_OK)
            return retVal;

        *accessPtr = regData;

        accessPtr ++;
    }


    _rtl8370_svlanConfStSmi2User(svlanMemConf,&smiSvlanMemConf);

    return RT_ERR_OK;
}

