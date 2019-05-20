#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/err.h>

#include "rtk_types.h"
#include "rtk_error.h"
#include "smi.h"

extern void __iomem *gpiobase;

void _rtl865x_gpio_set_direction(int pin, int dir)
{
    uint32 reg;
    
    if(NULL == gpiobase)
    {
        return;
    }
    
    if(smi_SCK == pin)
    {
        if(OUTPUT == dir)
        {
            reg = readl(gpiobase + 0x74);
            reg &= ~(0x7 << 24);
            reg |= 0x1 << 24;
            writel(reg, gpiobase + 0x74);
        }
        else
        {
            reg = readl(gpiobase + 0x74);
            reg &= ~(0x7 << 24);
            writel(reg, gpiobase + 0x74);
        }
    }
    else if(smi_SDA == pin)
    {
        if(OUTPUT == dir)
        {
            reg = readl(gpiobase + 0x74);
            reg &= ~(0x7 << 28);
            reg |= 0x1 << 28;
            writel(reg, gpiobase + 0x74);
        }
        else
        {
            reg = readl(gpiobase + 0x74);
            reg &= ~(0x7 << 28);
            writel(reg, gpiobase + 0x74);
        }
    }
}

void _rtl865x_setGpioDataBit(int pin, uint32 val)
{
    uint32 reg;
    
    if(NULL == gpiobase)
    {
        return;
    }

    reg = readl(gpiobase + 0x7C);
    reg &= ~(1 << pin);
    reg |= val << pin;
    writel(reg, gpiobase + 0x7C);
}

void _rtl865x_getGpioDataBit(int pin, uint32 *ret)
{
    uint32 reg;

    if(NULL == gpiobase)
    {
        return;
    }

    if(NULL == ret)
    {
        return;
    }

    reg = readl(gpiobase + 0x7C);
    reg &= (1 << pin);
    if(reg)
    {
        *ret = 1;
    }
    else
    {
        *ret = 0;
    }
}

static void _smi_start(void)
{
    /* change GPIO pin to Output only */
    _rtl865x_gpio_set_direction(smi_SCK, OUTPUT);
    _rtl865x_gpio_set_direction(smi_SDA, OUTPUT);

    /* Initial state: SCK: 0, SDA: 1 */
    _rtl865x_setGpioDataBit(smi_SCK, 0);
    _rtl865x_setGpioDataBit(smi_SDA, 1);
    CLK_DURATION(DELAY);

    /* CLK 1: 0 -> 1, 1 -> 0 */
    _rtl865x_setGpioDataBit(smi_SCK, 1);
    CLK_DURATION(DELAY);
    _rtl865x_setGpioDataBit(smi_SCK, 0);
    CLK_DURATION(DELAY);

    /* CLK 2: */
    _rtl865x_setGpioDataBit(smi_SCK, 1);
    CLK_DURATION(DELAY);
    _rtl865x_setGpioDataBit(smi_SDA, 0);
    CLK_DURATION(DELAY);
    _rtl865x_setGpioDataBit(smi_SCK, 0);
    CLK_DURATION(DELAY);
    _rtl865x_setGpioDataBit(smi_SDA, 1);
}

static void _smi_writeBit(uint16 signal, uint32 bitLen)
{
    for(; bitLen > 0; bitLen--)
    {
        CLK_DURATION(DELAY);

        /* prepare data */
        if ( signal & (1<<(bitLen-1)) ) 
            _rtl865x_setGpioDataBit(smi_SDA, 1);    
        else 
            _rtl865x_setGpioDataBit(smi_SDA, 0);    
        CLK_DURATION(DELAY);
        
        /* clocking */
        _rtl865x_setGpioDataBit(smi_SCK, 1);
        CLK_DURATION(DELAY);
        _rtl865x_setGpioDataBit(smi_SCK, 0);
    }
}

void _smi_readBit(uint32 bitLen, uint32 *rData) 
{
    uint32 u;

    /* change GPIO pin to Input only */
    _rtl865x_gpio_set_direction(smi_SDA, INPUT);

    for (*rData = 0; bitLen > 0; bitLen--)
    {
        CLK_DURATION(DELAY);

        /* clocking */
        _rtl865x_setGpioDataBit(smi_SCK, 1);
        CLK_DURATION(DELAY);
        _rtl865x_getGpioDataBit(smi_SDA, &u);
        _rtl865x_setGpioDataBit(smi_SCK, 0);

        *rData |= (u << (bitLen - 1));
    }

    /* change GPIO pin to Output only */
    _rtl865x_gpio_set_direction(smi_SDA, OUTPUT);
}



void _smi_stop(void)
{

    CLK_DURATION(DELAY);
    _rtl865x_setGpioDataBit(smi_SDA, 0);    
    _rtl865x_setGpioDataBit(smi_SCK, 1);    
    CLK_DURATION(DELAY);
    _rtl865x_setGpioDataBit(smi_SDA, 1);    
    CLK_DURATION(DELAY);
    _rtl865x_setGpioDataBit(smi_SCK, 1);
    CLK_DURATION(DELAY);
    _rtl865x_setGpioDataBit(smi_SCK, 0);
    CLK_DURATION(DELAY);
    _rtl865x_setGpioDataBit(smi_SCK, 1);

    /* add a click */
    CLK_DURATION(DELAY);
    _rtl865x_setGpioDataBit(smi_SCK, 0);
    CLK_DURATION(DELAY);
    _rtl865x_setGpioDataBit(smi_SCK, 1);


    /* change GPIO pin to Output only */
    _rtl865x_gpio_set_direction(smi_SDA, INPUT);
    _rtl865x_gpio_set_direction(smi_SCK, INPUT);
}


