#include "rtl8370_asicdrv.h"
#include "smi.h"

extern void _rtl865x_gpio_set_direction(int pin, int dir);
extern void _rtl865x_setGpioDataBit(int pin, uint32 val);
extern void _rtl865x_getGpioDataBit(int pin, uint32 *ret);

static int mdio_8370write(U16 MDC_GPIO_Pin, U16 MDIO_GPIO_Pin, U32 cmd)
{
    int i = 0;
    U32 mask = 1;
    U32 val = 0;

    /* 初始MDC/MDIO接口 */
    _rtl865x_gpio_set_direction(MDC_GPIO_Pin, OUTPUT);
    _rtl865x_gpio_set_direction(MDIO_GPIO_Pin, OUTPUT);
    _rtl865x_setGpioDataBit(MDIO_GPIO_Pin, 1);
    _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 0);

    CLK_DURATION(DELAY);

    /* 在发送实际数据之前，前导阶段，保持时钟正常，MDIO保持高位(保持10次左右)，
       当MDIO下拉为低表示采集数据开始。
     */
    for( i=0; i<32; i++ )
    {
        _rtl865x_setGpioDataBit( MDIO_GPIO_Pin, 1);
        _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 0);
        CLK_DURATION(DELAY);
        _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 1);
        CLK_DURATION(DELAY);
    }

    /* 发送写时序32bit数据 */
    for( i=DATA_LEN-1; i>=0 ; i--)
    {
        val = (cmd >> i) & mask;
        _rtl865x_setGpioDataBit(MDIO_GPIO_Pin, val);
        _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 0);
        /* setup time, min 2ns */
        CLK_DURATION(DELAY);
        _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 1);

        /* hold time, min 16ns */
        CLK_DURATION(DELAY);
    }

    /* 写完毕恢复状态 */
    _rtl865x_gpio_set_direction(MDIO_GPIO_Pin, OUTPUT);
    _rtl865x_setGpioDataBit(MDIO_GPIO_Pin, 1);
    _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 0);
    _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 1);

    return 0;

}

/*************************************************************
 * Function: mdio_8370_read()
 * Desc: MDIO统一的访问接口，按照一定 的时序进行
 *          读请求。
 * Input: cmd, 写入的32bit的指令,参考时序图
 * Output: val, 读出的16bit数据返回值。
 *
 * Return: 0, OK  / -1,Invalid input range.
 */
static int mdio_8370read(U16 MDC_GPIO_Pin, U16 MDIO_GPIO_Pin, U32 cmd, U32 *value)
{
    int i = 0;
    U32 val = 0;
    U32 mask = 1;

    /* 初始MDC/MDIO接口， 读准备 */
    _rtl865x_gpio_set_direction(MDC_GPIO_Pin, OUTPUT);
    _rtl865x_gpio_set_direction(MDIO_GPIO_Pin, OUTPUT);
    _rtl865x_setGpioDataBit(MDIO_GPIO_Pin, 1);
    _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 0);

    CLK_DURATION(DELAY);

    /* 在发送实际数据之前，前导阶段，保持时钟正常，MDIO保持高位(保持10次左右)，
       当MDIO下拉为低表示采集数据开始。
     */
    for( i=0; i<32; i++ )
    {
        _rtl865x_setGpioDataBit(MDIO_GPIO_Pin, 1);
        _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 0);
        CLK_DURATION(DELAY);
        _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 1);
        CLK_DURATION(DELAY);
    }

    /* 发送读时序前十六位 */
    //init_gpio_pin(MDIO_GPIOx, MDIO_GPIO_Pin, GPIO_OUT);
    for( i=DATA_LEN-1; i>=16 ; i--)
    {
        val = (cmd >> i) & mask;
        _rtl865x_setGpioDataBit(MDIO_GPIO_Pin, val);
        _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 0);
        /* setup time, min 2ns */
        CLK_DURATION(DELAY);
        _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 1);

        /* hold time, min 16ns */
        CLK_DURATION(DELAY);
    }

    /* 获取时序后十六位，即数据 */
    _rtl865x_gpio_set_direction(MDIO_GPIO_Pin, INPUT);
    for( i=DATA_LEN/2-1; i>=0; i-- )
    {
        _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 0);
        CLK_DURATION(DELAY);
        _rtl865x_getGpioDataBit(MDIO_GPIO_Pin, &val);
        _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 1);

        CLK_DURATION(DELAY);
        *value |= (val << i);
    }

    *value &= 0xFFFF;

    /* 读完毕恢复状态 */
    _rtl865x_gpio_set_direction(MDIO_GPIO_Pin, OUTPUT);
    _rtl865x_setGpioDataBit(MDIO_GPIO_Pin, 1);
    _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 0);
    _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 1);

    return 0;
}

static int phy_8370write(U8 mdio_group, U8 phy_addr, U8 reg_addr, U32 data)
{
    U32 write_data = 0;

    if (mdio_group != MDIO_8370_GROUP0)
    {
        //USART_DispFun("phy_8370_write:mdio_group invalid\r\n");
        return -1;
    }

    if ((reg_addr > 0x1F) || (phy_addr>0x1F) || (data > 0xFFFF))
    {
        //USART_DispFun("phy_8370_write:register addr or data invalid\r\n");
        return -1;
    }
    else
    {
        write_data = 0x50020000;
        write_data |= ((phy_addr << 23) | (reg_addr << 18) | data);

        mdio_8370write(smi_SCK, smi_SDA, write_data);
    }

    return 0;
}

static int phy_8370read(U8 mdio_group, U8 phy_addr, U8 reg_addr, U32 *value)
{
    U32 write_data = 0;

    if (mdio_group != MDIO_8370_GROUP0)
    {
        //USART_DispFun("phy_8370_read:mdio_group invalid\r\n");
        return -1;
    }

    /* send read cmd */
    if ( (reg_addr>0x1F)|| (phy_addr>0x1F))
    {
        //USART_DispFun("phy_8370_read: register addr invalid\r\n");
        return -1;
    }
    else
    {
        /* 标准的读时序，请参考时序图 */
        write_data = 0x60020000;
        write_data |= ((phy_addr << 23) | (reg_addr << 18));

        *value = 0;
        mdio_8370read(smi_SCK, smi_SDA, write_data, value);
    }

    return 0;
}

int simple_phy_read(u16 reg, u32 *ret)
{
    unsigned int rData = -1;

    if(NULL == ret)
    {
        return RT_ERR_FAILED;
    }
    /* Write address control code to register 31 */
    phy_8370write(MDIO_8370_GROUP0, MDC_MDIO_DUMMY_ID, MDC_MDIO_CTRL0_REG, MDC_MDIO_ADDR_OP);

    /* Write address to register 23 */
    phy_8370write(MDIO_8370_GROUP0, MDC_MDIO_DUMMY_ID, MDC_MDIO_ADDRESS_REG, reg);

    /* Write read control code to register 21 */
    phy_8370write(MDIO_8370_GROUP0, MDC_MDIO_DUMMY_ID, MDC_MDIO_CTRL1_REG, MDC_MDIO_READ_OP);

    /* Read data from register 25 */
    phy_8370read(MDIO_8370_GROUP0, MDC_MDIO_DUMMY_ID, MDC_MDIO_DATA_READ_REG, &rData);

    *ret = rData;
    
    return RT_ERR_OK;;
}

int simple_phy_write(u16 reg, u16 data)
{
    /* Write address control code to register 31 */
    phy_8370write(MDIO_8370_GROUP0, MDC_MDIO_DUMMY_ID, MDC_MDIO_CTRL0_REG, MDC_MDIO_ADDR_OP);

    /* Write address to register 23 */
    phy_8370write(MDIO_8370_GROUP0, MDC_MDIO_DUMMY_ID, MDC_MDIO_ADDRESS_REG, reg);

    /* Write data to register 24 */
    phy_8370write(MDIO_8370_GROUP0, MDC_MDIO_DUMMY_ID, MDC_MDIO_DATA_WRITE_REG, data);

    /* Write data control code to register 21 */
    phy_8370write(MDIO_8370_GROUP0, MDC_MDIO_DUMMY_ID, MDC_MDIO_CTRL1_REG, MDC_MDIO_WRITE_OP);

    return RT_ERR_OK;
}

u16 geth_phy_read(int phy_adr, u16 reg)
{
    u32 ret = 0;
    simple_phy_read(reg, &ret);
    return ret;
}

void geth_phy_write(u8 phy_adr, u8 reg, u16 data)
{
    simple_phy_write(reg, data);
}

/*=======================================================================
 * 1. Asic read/write driver through SMI
 *========================================================================*/
/*
@func ret_t | rtl8370_setAsicRegBit | Set a bit value of a specified register.
@parm uint32 | reg | Register's address.
@parm uint32 | bit | Bit location. For 16-bits register only. Maximun value is 15 for MSB location.
@parm uint32 | value | Value to set. It can be value 0 or 1.
@rvalue RT_ERR_OK | Success.
@rvalue RT_ERR_SMI | SMI access error.
@rvalue RT_ERR_INPUT | Invalid input parameter. 
@comm
    Set a bit of a specified register to 1 or 0. It is 16-bits system of RTL8366s chip.
    
*/
ret_t rtl8370_setAsicRegBit(uint32 reg, uint32 bit, uint32 value)
{
      uint32 regData;
      ret_t retVal;
      
      if(bit>=RTL8370_REGBITLENGTH)
          return RT_ERR_INPUT;
    
      retVal = simple_phy_read(reg, &regData);
      if(retVal != RT_ERR_OK) 
          return RT_ERR_SMI;
#ifdef CONFIG_RTL865X_CLE
      if(0x8370 == cleDebuggingDisplay)
          printf("R[0x%4.4x]=0x%4.4x\n",reg,regData);
#endif
      if (value) 
          regData = regData | (1<<bit);
      else
          regData = regData & (~(1<<bit));
      
      retVal = simple_phy_write(reg, regData);
      if (retVal != RT_ERR_OK) 
          return RT_ERR_SMI;
    
#ifdef CONFIG_RTL865X_CLE
      if(0x8370 == cleDebuggingDisplay)
          printf("W[0x%4.4x]=0x%4.4x\n",reg,regData);
#endif
    return RT_ERR_OK;
}

ret_t rtl8370_getAsicRegBit(uint32 reg, uint32 bit, uint32 *value)
{
    uint32 regData;
    ret_t retVal;

    retVal = simple_phy_read(reg, &regData);
    if (retVal != RT_ERR_OK) 
      return RT_ERR_SMI;
#ifdef CONFIG_RTL865X_CLE
    if(0x8370 == cleDebuggingDisplay)
      printf("R[0x%4.4x]=0x%4.4x\n",reg,regData);
#endif

    *value = (regData & (0x1 << bit)) >> bit;    
    return RT_ERR_OK;
}

ret_t rtl8370_setAsicReg(uint32 reg, uint32 value)
{
    ret_t retVal;

    retVal = simple_phy_write(reg, value);
    if (retVal != RT_ERR_OK) 
    return RT_ERR_SMI;
#ifdef CONFIG_RTL865X_CLE
    if(0x8370 == cleDebuggingDisplay)
        printf("W[0x%4.4x]=0x%4.4x\n",reg,value);
#endif
    return RT_ERR_OK;
}

ret_t rtl8370_getAsicReg(uint32 reg, uint32 *value)
{
    uint32 regData;
    ret_t retVal;

    retVal = simple_phy_read(reg, &regData);
    if (retVal != RT_ERR_OK) 
      return RT_ERR_SMI;

    *value = regData;
#ifdef CONFIG_RTL865X_CLE
    if(0x8370 == cleDebuggingDisplay)
        printf("R[0x%4.4x]=0x%4.4x\n",reg,regData);
#endif
    return RT_ERR_OK;
}

ret_t rtl8370_getAsicRegBits(uint32 reg, uint32 bits, uint32 *value)
{
    uint32 regData;    
    ret_t retVal;    
    uint32 bitsShift;    

    if(bits>= (1<<RTL8370_REGBITLENGTH) )
      return RT_ERR_INPUT;    

    bitsShift = 0;
    while(!(bits & (1 << bitsShift)))
    {
      bitsShift++;
      if(bitsShift >= RTL8370_REGBITLENGTH)
          return RT_ERR_INPUT;
    }

    retVal = simple_phy_read(reg, &regData);
    if (retVal != RT_ERR_OK) 
      return RT_ERR_SMI;

    *value = (regData & bits) >> bitsShift;
#ifdef CONFIG_RTL865X_CLE
    if(0x8370 == cleDebuggingDisplay)
        printf("R[0x%4.4x]=0x%4.4x\n",reg,regData);
#endif
    return RT_ERR_OK;
}

/*
@func ret_t | rtl8370_setAsicRegBits | Set bits value of a specified register.
@parm uint32 | reg | Register's address.
@parm uint32 | bits | Bits mask for setting. 
@parm uint32 | value | Bits value for setting. Value of bits will be set with mapping mask bit is 1.   
@rvalue RT_ERR_OK | Success.
@rvalue RT_ERR_SMI | SMI access error.
@rvalue RT_ERR_INPUT | Invalid input parameter. 
@comm
    Set bits of a specified register to value. Both bits and value are be treated as bit-mask.
    
*/
ret_t rtl8370_setAsicRegBits(uint32 reg, uint32 bits, uint32 value)
{
    uint32 regData;    
    ret_t retVal;    
    uint32 bitsShift;    
    uint32 valueShifted;        

    if(bits>= (1<<RTL8370_REGBITLENGTH) )
        return RT_ERR_INPUT;

    bitsShift = 0;
    while(!(bits & (1 << bitsShift)))
    {
        bitsShift++;
        if(bitsShift >= RTL8370_REGBITLENGTH)
            return RT_ERR_INPUT;
    }
    valueShifted = value << bitsShift;

    if(valueShifted > RTL8370_REGDATAMAX)
        return RT_ERR_INPUT;

    retVal = simple_phy_read(reg, &regData);
    if (retVal != RT_ERR_OK) 
		return RT_ERR_SMI;
  #ifdef CONFIG_RTL865X_CLE
    if(0x8370 == cleDebuggingDisplay)
        printf("R[0x%4.4x]=0x%4.4x\n",reg,regData);
  #endif

    regData = regData & (~bits);
    regData = regData | (valueShifted & bits);

    retVal = simple_phy_write(reg, regData);
    if (retVal != RT_ERR_OK) 
		return RT_ERR_SMI;
  #ifdef CONFIG_RTL865X_CLE
    if(0x8370 == cleDebuggingDisplay)
        printf("W[0x%4.4x]=0x%4.4x\n",reg,regData);
  #endif
    return RT_ERR_OK;
}

