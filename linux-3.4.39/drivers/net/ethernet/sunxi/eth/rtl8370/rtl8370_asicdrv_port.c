#include <linux/string.h>
#include "rtl8370_asicdrv_port.h"
#include "rtl8370_asicdrv_phy.h"

/*
@func ret_t | rtl8370_getAsicPortStatus | Get port link status.
@parm uint32 | port | port number.
@parm rtl8370_port_ability_t* | portability | port ability configuration
@rvalue RT_ERR_OK | Success.
@rvalue RT_ERR_SMI | SMI access error.
@rvalue RT_ERR_FAILED | Invalid parameter.
@comm
      This API can get Port/PHY properties. 
 */
ret_t rtl8370_getAsicPortStatus(uint32 port, rtl8370_port_status_t *portstatus)
{
    ret_t retVal;
    uint32 regData;
    uint16 *accessPtr;
    rtl8370_port_status_t status;

    /* Invalid input parameter */
    if(port >= RTL8370_PORTNO)
        return RT_ERR_PORT_ID;
    
    memset(&status, 0x00, sizeof(rtl8370_port_status_t));


    accessPtr =  (uint16*)&status;
 
    retVal = rtl8370_getAsicReg(RTL8370_REG_PORT0_STATUS + port, &regData);
    if(retVal !=  RT_ERR_OK)
        return retVal;
    
    *accessPtr = regData;

    memcpy(portstatus, &status, sizeof(rtl8370_port_status_t));        
    
    return RT_ERR_OK;  
}

/*
@func ret_t | rtl8370_setAsicPortEnableAll | Set ALL ports enable.
@parm uint32 |enable | enable all ports.
@rvalue RT_ERR_OK | Success.
@rvalue RT_ERR_SMI | SMI access error.
@rvalue RT_ERR_FAILED | Invalid parameter.
@rvalue RT_ERR_INPUT | Invalid input parameter.
@comm
  This API can set all ports enable.  
 */
ret_t rtl8370_setAsicPortEnableAll(uint32 enable)
{
    if(enable >= 2)
        return RT_ERR_INPUT;

    return rtl8370_setAsicRegBit(RTL8370_REG_PHY_AD, RTL8370_PHY_AD_DUMMY_1_OFFSET, !enable);
}

/*
@func ret_t | rtl8370_getAsicPortEnableAll | Get ALL ports enable.
@parm uint32 | *enable | enable all ports.
@rvalue RT_ERR_OK | Success.
@rvalue RT_ERR_SMI | SMI access error.
@rvalue RT_ERR_FAILED | Invalid parameter.
@rvalue RT_ERR_INPUT | Invalid input parameter.
@comm
  This API can set all ports enable.  
 */
ret_t rtl8370_getAsicPortEnableAll(uint32 *enable)
{
    ret_t retVal;
    uint32 regData;
    
    retVal = rtl8370_getAsicRegBit(RTL8370_REG_PHY_AD, RTL8370_PHY_AD_DUMMY_1_OFFSET, &regData);
    if(retVal !=  RT_ERR_OK)
        return retVal;

    if (regData==0)
        *enable = 1;
    else
        *enable = 0;

    return RT_ERR_OK;
}

