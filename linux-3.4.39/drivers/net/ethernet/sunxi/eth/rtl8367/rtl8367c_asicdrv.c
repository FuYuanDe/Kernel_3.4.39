/*
 * Copyright (C) 2013 Realtek Semiconductor Corp.
 * All Rights Reserved.
 *
 * This program is the proprietary software of Realtek Semiconductor
 * Corporation and/or its licensors, and only be used, duplicated,
 * modified or distributed under the authorized license from Realtek.
 *
 * ANY USE OF THE SOFTWARE OTHER THAN AS AUTHORIZED UNDER
 * THIS LICENSE OR COPYRIGHT LAW IS PROHIBITED.
 *
 * $Revision: 50338 $
 * $Date: 2014-08-19 14:00:41 +0800 (週二, 19 八月 2014) $
 *
 * Purpose : RTL8367C switch high-level API for RTL8367C
 * Feature :
 *
 */

#include "rtl8367c_asicdrv.h"
#include "smi.h"

#define MDIO_8370_GROUP0            0

extern void _rtl865x_gpio_set_direction(int pin, int dir);
extern void _rtl865x_setGpioDataBit(int pin, uint32 val);
extern void _rtl865x_getGpioDataBit(int pin, uint32 *ret);

static int mdio_8370write(U16 MDC_GPIO_Pin, U16 MDIO_GPIO_Pin, U32 cmd)
{
    int i = 0;
    U32 mask = 1;
    U32 val = 0;

    /* ��ʼMDC/MDIO�ӿ� */
    _rtl865x_gpio_set_direction(MDC_GPIO_Pin, OUTPUT);
    _rtl865x_gpio_set_direction(MDIO_GPIO_Pin, OUTPUT);
    _rtl865x_setGpioDataBit(MDIO_GPIO_Pin, 1);
    _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 0);

    CLK_DURATION(DELAY);

    /* �ڷ���ʵ������֮ǰ��ǰ���׶Σ�����ʱ��������MDIO���ָ�λ(����10������)��
       ��MDIO����Ϊ�ͱ�ʾ�ɼ����ݿ�ʼ��
     */
    for( i=0; i<32; i++ )
    {
        _rtl865x_setGpioDataBit( MDIO_GPIO_Pin, 1);
        _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 0);
        CLK_DURATION(DELAY);
        _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 1);
        CLK_DURATION(DELAY);
    }

    /* ����дʱ��32bit���� */
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

    /* д��ϻָ�״̬ */
    _rtl865x_gpio_set_direction(MDIO_GPIO_Pin, OUTPUT);
    _rtl865x_setGpioDataBit(MDIO_GPIO_Pin, 1);
    _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 0);
    _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 1);

    return 0;

}

/*************************************************************
 * Function: mdio_8370_read()
 * Desc: MDIOͳһ�ķ��ʽӿڣ�����һ�� ��ʱ�����
 *          ������
 * Input: cmd, д���32bit��ָ��,�ο�ʱ��ͼ
 * Output: val, ������16bit���ݷ���ֵ��
 *
 * Return: 0, OK  / -1,Invalid input range.
 */
static int mdio_8370read(U16 MDC_GPIO_Pin, U16 MDIO_GPIO_Pin, U32 cmd, U32 *value)
{
    int i = 0;
    U32 val = 0;
    U32 mask = 1;

    /* ��ʼMDC/MDIO�ӿڣ� ��׼�� */
    _rtl865x_gpio_set_direction(MDC_GPIO_Pin, OUTPUT);
    _rtl865x_gpio_set_direction(MDIO_GPIO_Pin, OUTPUT);
    _rtl865x_setGpioDataBit(MDIO_GPIO_Pin, 1);
    _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 0);

    CLK_DURATION(DELAY);

    /* �ڷ���ʵ������֮ǰ��ǰ���׶Σ�����ʱ��������MDIO���ָ�λ(����10������)��
       ��MDIO����Ϊ�ͱ�ʾ�ɼ����ݿ�ʼ��
     */
    for( i=0; i<32; i++ )
    {
        _rtl865x_setGpioDataBit(MDIO_GPIO_Pin, 1);
        _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 0);
        CLK_DURATION(DELAY);
        _rtl865x_setGpioDataBit(MDC_GPIO_Pin, 1);
        CLK_DURATION(DELAY);
    }

    /* ���Ͷ�ʱ��ǰʮ��λ */
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

    /* ��ȡʱ���ʮ��λ�������� */
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

    /* ����ϻָ�״̬ */
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
        /* ��׼�Ķ�ʱ����ο�ʱ��ͼ */
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
/* Function Name:
 *      rtl8367c_setAsicRegBit
 * Description:
 *      Set a bit value of a specified register
 * Input:
 *      reg 	- register's address
 *      bit 	- bit location
 *      value 	- value to set. It can be value 0 or 1.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 		- Success
 *      RT_ERR_SMI  	- SMI access error
 *      RT_ERR_INPUT  	- Invalid input parameter
 * Note:
 *      Set a bit of a specified register to 1 or 0.
 */
ret_t rtl8367c_setAsicRegBit(rtk_uint32 reg, rtk_uint32 bit, rtk_uint32 value)
{

	rtk_uint32 regData;
	ret_t retVal;

	if(bit >= RTL8367C_REGBITLENGTH)
		return RT_ERR_INPUT;

	retVal = simple_phy_read(reg, &regData);
	if(retVal != RT_ERR_OK)
		return RT_ERR_SMI;

  #ifdef CONFIG_RTL865X_CLE
	if(0x8367B == cleDebuggingDisplay)
		PRINT("R[0x%4.4x]=0x%4.4x\n", reg, regData);
  #endif
	if(value)
		regData = regData | (1 << bit);
	else
		regData = regData & (~(1 << bit));

	retVal = simple_phy_write(reg, regData);
	if(retVal != RT_ERR_OK)
		return RT_ERR_SMI;

  #ifdef CONFIG_RTL865X_CLE
	if(0x8367B == cleDebuggingDisplay)
		PRINT("W[0x%4.4x]=0x%4.4x\n", reg, regData);
  #endif

	return RT_ERR_OK;
}
/* Function Name:
 *      rtl8367c_getAsicRegBit
 * Description:
 *      Get a bit value of a specified register
 * Input:
 *      reg 	- register's address
 *      bit 	- bit location
 *      value 	- value to get.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 		- Success
 *      RT_ERR_SMI  	- SMI access error
 *      RT_ERR_INPUT  	- Invalid input parameter
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicRegBit(rtk_uint32 reg, rtk_uint32 bit, rtk_uint32 *pValue)
{

	rtk_uint32 regData;
	ret_t retVal;

	retVal = simple_phy_read(reg, &regData);
	if(retVal != RT_ERR_OK)
		return RT_ERR_SMI;

  #ifdef CONFIG_RTL865X_CLE
	if(0x8367B == cleDebuggingDisplay)
		PRINT("R[0x%4.4x]=0x%4.4x\n", reg, regData);
  #endif

	*pValue = (regData & (0x1 << bit)) >> bit;

	return RT_ERR_OK;
}
/* Function Name:
 *      rtl8367c_setAsicRegBits
 * Description:
 *      Set bits value of a specified register
 * Input:
 *      reg 	- register's address
 *      bits 	- bits mask for setting
 *      value 	- bits value for setting
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 		- Success
 *      RT_ERR_SMI  	- SMI access error
 *      RT_ERR_INPUT  	- Invalid input parameter
 * Note:
 *      Set bits of a specified register to value. Both bits and value are be treated as bit-mask
 */
ret_t rtl8367c_setAsicRegBits(rtk_uint32 reg, rtk_uint32 bits, rtk_uint32 value)
{

	rtk_uint32 regData;
	ret_t retVal;
	rtk_uint32 bitsShift;
	rtk_uint32 valueShifted;

	if(bits >= (1 << RTL8367C_REGBITLENGTH) )
		return RT_ERR_INPUT;

	bitsShift = 0;
	while(!(bits & (1 << bitsShift)))
	{
		bitsShift++;
		if(bitsShift >= RTL8367C_REGBITLENGTH)
			return RT_ERR_INPUT;
	}
	valueShifted = value << bitsShift;

	if(valueShifted > RTL8367C_REGDATAMAX)
		return RT_ERR_INPUT;

	retVal = simple_phy_read(reg, &regData);
	if(retVal != RT_ERR_OK)
		return RT_ERR_SMI;
  #ifdef CONFIG_RTL865X_CLE
	if(0x8367B == cleDebuggingDisplay)
		PRINT("R[0x%4.4x]=0x%4.4x\n", reg, regData);
  #endif

	regData = regData & (~bits);
	regData = regData | (valueShifted & bits);

	retVal = simple_phy_write(reg, regData);
	if(retVal != RT_ERR_OK)
		return RT_ERR_SMI;
  #ifdef CONFIG_RTL865X_CLE
	if(0x8367B == cleDebuggingDisplay)
		PRINT("W[0x%4.4x]=0x%4.4x\n", reg, regData);
  #endif
	return RT_ERR_OK;
}
/* Function Name:
 *      rtl8367c_getAsicRegBits
 * Description:
 *      Get bits value of a specified register
 * Input:
 *      reg 	- register's address
 *      bits 	- bits mask for setting
 *      value 	- bits value for setting
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 		- Success
 *      RT_ERR_SMI  	- SMI access error
 *      RT_ERR_INPUT  	- Invalid input parameter
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicRegBits(rtk_uint32 reg, rtk_uint32 bits, rtk_uint32 *pValue)
{
	rtk_uint32 regData;
	ret_t retVal;
	rtk_uint32 bitsShift;

	if(bits>= (1<<RTL8367C_REGBITLENGTH) )
		return RT_ERR_INPUT;

	bitsShift = 0;
	while(!(bits & (1 << bitsShift)))
	{
		bitsShift++;
		if(bitsShift >= RTL8367C_REGBITLENGTH)
			return RT_ERR_INPUT;
	}

	retVal = simple_phy_read(reg, &regData);
	if(retVal != RT_ERR_OK) return RT_ERR_SMI;

	*pValue = (regData & bits) >> bitsShift;
  #ifdef CONFIG_RTL865X_CLE
	if(0x8367B == cleDebuggingDisplay)
		PRINT("R[0x%4.4x]=0x%4.4x\n",reg, regData);
  #endif

	return RT_ERR_OK;
}
/* Function Name:
 *      rtl8367c_setAsicReg
 * Description:
 *      Set content of asic register
 * Input:
 *      reg 	- register's address
 *      value 	- Value setting to register
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 		- Success
 *      RT_ERR_SMI  	- SMI access error
 * Note:
 *      The value will be set to ASIC mapping address only and it is always return RT_ERR_OK while setting un-mapping address registers
 */
ret_t rtl8367c_setAsicReg(rtk_uint32 reg, rtk_uint32 value)
{
	ret_t retVal;

	retVal = simple_phy_write(reg, value);
	if(retVal != RT_ERR_OK)
		return RT_ERR_SMI;
  #ifdef CONFIG_RTL865X_CLE
	if(0x8367B == cleDebuggingDisplay)
		PRINT("W[0x%4.4x]=0x%4.4x\n",reg,value);
  #endif
	return RT_ERR_OK;
}
/* Function Name:
 *      rtl8367c_getAsicReg
 * Description:
 *      Get content of asic register
 * Input:
 *      reg 	- register's address
 *      value 	- Value setting to register
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 		- Success
 *      RT_ERR_SMI  	- SMI access error
 * Note:
 *      Value 0x0000 will be returned for ASIC un-mapping address
 */
ret_t rtl8367c_getAsicReg(rtk_uint32 reg, rtk_uint32 *pValue)
{
	rtk_uint32 regData;
	ret_t retVal;

	retVal = simple_phy_read(reg, &regData);
	if(retVal != RT_ERR_OK)
		return RT_ERR_SMI;

	*pValue = regData;
  #ifdef CONFIG_RTL865X_CLE
	if(0x8367B == cleDebuggingDisplay)
		PRINT("R[0x%4.4x]=0x%4.4x\n", reg, regData);
  #endif
	return RT_ERR_OK;
}
