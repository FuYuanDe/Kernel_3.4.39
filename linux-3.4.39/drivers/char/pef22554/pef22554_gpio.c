/******************************************************************************
        (c) COPYRIGHT 2002-2003 by Shenzhen Allywll Information Co.,Ltd
                          All rights reserved.
File: pef22554.c
Desc:the source file of user config
Modification history(no, author, date, desc)
1.Holy 2003-04-02 create file
2.luke 2006-04-20 for AOS
******************************************************************************/

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <asm/io.h>
#include <mach/platform.h>

#include "pef22554_drv.h"

typedef struct 
{
    u32 INT;
    u32 RST;
    u32 ALE;
    u32 PEF_CS_N;
    u32 CPLD_CS_N;
    u32 RE_N;
    u32 WE_N;
} quadFALC_bus_pins_st;

static quadFALC_bus_pins_st quadFALC_bus_pins;

static struct gpio quadFALC_bus_gpios[] = {
    { GPIOG(0),  GPIOF_DIR_OUT, "PEF_A0" },
	{ GPIOG(1),  GPIOF_DIR_OUT, "PEF_A1" },
	{ GPIOG(2),  GPIOF_DIR_OUT, "PEF_A2" },
	{ GPIOG(3),  GPIOF_DIR_OUT, "PEF_A3" },
	{ GPIOG(4),  GPIOF_DIR_OUT, "PEF_A4" },
	{ GPIOG(5),  GPIOF_DIR_OUT, "PEF_A5" },
	{ GPIOG(6),  GPIOF_DIR_OUT, "PEF_A6" },
	{ GPIOG(7),  GPIOF_DIR_OUT, "PEF_A7" },
	{ GPIOG(8),  GPIOF_DIR_OUT, "PEF_A8" },
	{ GPIOG(9),  GPIOF_DIR_OUT, "PEF_A9" },
	
	{ GPIOG(10), GPIOF_DIR_OUT, "PEF_ALE" },
	{ GPIOG(11), GPIOF_DIR_OUT, "PEF_INT" },
	{ GPIOG(12), GPIOF_DIR_OUT, "PEF_RST" },
	{ GPIOG(13), GPIOF_DIR_OUT, "PEF_WR" },

    { GPIOL(2),  GPIOF_DIR_OUT, "PEF_D0" },
    { GPIOL(3),  GPIOF_DIR_OUT, "PEF_D1" },
    { GPIOL(4),  GPIOF_DIR_OUT, "PEF_D2" },
    { GPIOL(5),  GPIOF_DIR_OUT, "PEF_D3" },
    { GPIOL(6),  GPIOF_DIR_OUT, "PEF_D4" },
    { GPIOL(7),  GPIOF_DIR_OUT, "PEF_D5" },
    { GPIOL(8),  GPIOF_DIR_OUT, "PEF_D6" },
    { GPIOL(9),  GPIOF_DIR_OUT, "PEF_D7" },
    { GPIOL(10), GPIOF_DIR_OUT, "PEF_CS" },
    { GPIOL(11), GPIOF_DIR_OUT, "PEF_RD" },
    { GPIOL(12), GPIOF_DIR_OUT, "CPLD_CS" },
};

static struct gpio quadFALC_led_gpios[] = {
    { GPIOE(0),  GPIOF_DIR_OUT, "E1_LINK0" },
    { GPIOE(1),  GPIOF_DIR_OUT, "E1_LINK1" },
    { GPIOE(4),  GPIOF_DIR_OUT, "E1_LINK2" },
    { GPIOE(5),  GPIOF_DIR_OUT, "E1_LINK3" },
};

static struct gpio quadFALC_ver_gpios[] = {
    { GPIOH(4),  GPIOF_DIR_IN,  "VER2" },
    { GPIOH(7),  GPIOF_DIR_IN,  "VER0" },
    { GPIOH(9),  GPIOF_DIR_IN,  "VER1" },//区别硬件版本

    { GPIOE(12), GPIOF_DIR_IN,  "TYPE0" },
    { GPIOE(13), GPIOF_DIR_IN,  "TYPE1" },//区别产品类型
    
#if defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC300USER) || \
	defined(PRODUCT_SBC1000USER) || defined(PRODUCT_SBC1000MAIN)
    { GPIOH(2),  GPIOF_DIR_IN,  "MARK0_0" },//区分cpu0和cpu1
#else defined(PRODUCT_SBCUSER)
    { GPIOH(0),  GPIOF_DIR_IN,  "HARD_ID0" },
    { GPIOH(1),  GPIOF_DIR_IN,  "HARD_ID2" },
    { GPIOH(2),  GPIOF_DIR_IN,  "HARD_ID1" },//区别tg用户板和sbc用户板

    { GPIOC(7),  GPIOF_DIR_IN,  "MARK0_0" },
    { GPIOC(8),  GPIOF_DIR_IN,  "MARK0_1" },
    { GPIOC(9),  GPIOF_DIR_IN,  "MARK0_2" },//区分cpu0和cpu1
#endif

};

static void quadFALC_gpio_set_data_dir(int dir)
{
    u32 reg_val;

    
    if(GPIOF_DIR_IN == dir)
    {
        /* GPIOL(2) GPIOL(3) GPIOL(4) GPIOL(5) GPIOL(6) GPIOL(7) */
        reg_val = readl(SUNXI_R_PIO_VBASE + 0x00);
        reg_val &= ~((0x7 << 8) | (0x7 << 12) | (0x7 << 16) | (0x7 << 20) | (0x7 << 24) | (0x7 << 28));
        writel(reg_val, SUNXI_R_PIO_VBASE + 0x00);

        /* GPIOL(8) GPIOL(9) */
        reg_val = readl(SUNXI_R_PIO_VBASE + 0x04);
        reg_val &= ~((0x7 <<  0) | (0x7 << 4));
        writel(reg_val, SUNXI_R_PIO_VBASE + 0x04);
    }
    else
    {
        /* GPIOL(2) GPIOL(3) GPIOL(4) GPIOL(5) GPIOL(6) GPIOL(7) */
        reg_val = readl(SUNXI_R_PIO_VBASE + 0x00);
        reg_val &= ~((0x7 << 8) | (0x7 << 12) | (0x7 << 16) | (0x7 << 20) | (0x7 << 24) | (0x7 << 28));
        reg_val |=  ((0x1 << 8) | (0x1 << 12) | (0x1 << 16) | (0x1 << 20) | (0x1 << 24) | (0x1 << 28));
        writel(reg_val, SUNXI_R_PIO_VBASE + 0x00);

        /* GPIOL(8) GPIOL(9) */
        reg_val = readl(SUNXI_R_PIO_VBASE + 0x04);
        reg_val &= ~((0x7 <<  0) | (0x7 << 4));
        reg_val |=  ((0x1 <<  0) | (0x1 << 4));
        writel(reg_val, SUNXI_R_PIO_VBASE + 0x04);
    }
}

static u8 quadFALC_gpio_get_data_val(void)
{
    u32 reg_val;
    u8  val = 0;

    reg_val = readl(SUNXI_R_PIO_VBASE + 0x10);
    val |= (reg_val >> 2) & 0xFF; // PEF_D0 - PEF_D7

    return val;
}

static void quadFALC_gpio_set_data_val(u8 val)
{
    u32 reg_val;

    reg_val = readl(SUNXI_R_PIO_VBASE + 0x10);

    reg_val &= ~(0xFF << 2);
    reg_val |=  (val << 2);

    writel(reg_val, SUNXI_R_PIO_VBASE + 0x10);
}

static void quadFALC_gpio_set_addr_val(u32 addr)
{
    u32 reg_val;

    
    /* PG_DATA_REG */
    reg_val = readl(SUNXI_PIO_VBASE + 0xE8);
    
    /* GPIOG(0) GPIOG(1) GPIOG(2) GPIOG(3) GPIOG(4) GPIOG(5) GPIOG(6) GPIOG(7) GPIOG(8) GPIOG(9) */
    reg_val &= ~(0x3FF);
    reg_val |= addr; // PEF_A0 - PEF_A9
    
    writel(reg_val, SUNXI_PIO_VBASE + 0xE8);    
}

void quadFALC_gpio_read_reg(u32 addr, u8* data, u32 cs_pin)
{
	/* Step 1: set data GPIO dir */
    quadFALC_gpio_set_data_dir(GPIOF_DIR_IN);
    
	/* Step 2: set addr */
    quadFALC_gpio_set_addr_val(addr);
    
    /* Step 3: enable CS and RE */
    gpio_direction_output(quadFALC_bus_pins.ALE,  0);
	gpio_direction_output(cs_pin, 0);
	gpio_direction_output(quadFALC_bus_pins.RE_N, 0);
    
    /* Step 4: delay */
    udelay(1);

	/* Step 5: get data */
    *data = quadFALC_gpio_get_data_val();

    /* Step 6: disable CS and RE */
    gpio_direction_output(quadFALC_bus_pins.ALE,  1);
	gpio_direction_output(quadFALC_bus_pins.RE_N, 1);
    gpio_direction_output(cs_pin, 1);
}

void quadFALC_gpio_write_reg(u32 addr, u8 data, u32 cs_pin)
{
	/* Step 1: set data GPIO dir */
    quadFALC_gpio_set_data_dir(GPIOF_DIR_OUT);

	/* Step 2: set addr */
	quadFALC_gpio_set_addr_val(addr);

	/* Step 3: set data */
    quadFALC_gpio_set_data_val(data);

    /* Step 4: enable CS and WE */
    gpio_direction_output(quadFALC_bus_pins.ALE,  0);
	gpio_direction_output(cs_pin, 0);
	gpio_direction_output(quadFALC_bus_pins.WE_N, 0);
    
    /* Step 5: delay */
	udelay(1);

    /* Step 6: disable CS and RE */
    gpio_direction_output(quadFALC_bus_pins.ALE,  1);
	gpio_direction_output(quadFALC_bus_pins.WE_N, 1);
    gpio_direction_output(cs_pin, 1);
}

void quadFALC_pef_read_reg(u32 addr, u8* data)
{
	quadFALC_gpio_read_reg(addr, data, quadFALC_bus_pins.PEF_CS_N);
}

void quadFALC_pef_write_reg(u32 addr, u8 data)
{
	quadFALC_gpio_write_reg(addr, data, quadFALC_bus_pins.PEF_CS_N);
}

void quadFALC_cpld_read_reg(u32 addr, u8* data)
{
	quadFALC_gpio_read_reg(addr, data, quadFALC_bus_pins.CPLD_CS_N);

    *data &= 0xF; 
}

void quadFALC_cpld_write_reg(u32 addr, u8 data)
{
    data &= 0xF;
    
	quadFALC_gpio_write_reg(addr, data, quadFALC_bus_pins.CPLD_CS_N);
}

int quadFALC_gpio_led_cfg(u8 led_idx, u8 led_cfg)
{
    if(led_idx >= ARRAY_SIZE(quadFALC_led_gpios))
    {
        printk(KERN_ERR "%s: led idx:%d out of range!\n", __FUNCTION__, led_idx);
        return -EINVAL;
    }

    if(led_cfg) 
    {
        gpio_direction_output(quadFALC_led_gpios[led_idx].gpio, 0);
    }
    else
    {
        gpio_direction_output(quadFALC_led_gpios[led_idx].gpio, 1);
    }
    
    return 0;
}

u8 quadFALC_gpio_get_board_mark(void)
{
    u8 value = 0;
#if defined(PRODUCT_SBCUSER)
    value |= (gpio_get_value(GPIOC(7)) & 1) << 0;
    value |= (gpio_get_value(GPIOC(8)) & 1) << 1;
    value |= (gpio_get_value(GPIOC(9)) & 1) << 2;
#elif defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC300USER) || \
	defined(PRODUCT_SBC1000USER) || defined(PRODUCT_SBC1000MAIN)
    value |= (gpio_get_value(GPIOH(2)) & 1) << 2;
#endif
    return value;
}

u8 quadFALC_gpio_get_board_type(void)
{
    u8 value = 0;

    value |= (gpio_get_value(GPIOE(12)) & 1) << 0;
    value |= (gpio_get_value(GPIOE(13)) & 1) << 1;

    printk(KERN_ERR "%s : %d\n", __FUNCTION__, value);
    
    return value;
}

u8 quadFALC_gpio_get_hard_id(void)
{
    u8 value = 0;
#if defined(PRODUCT_SBCUSER)
    value |= (gpio_get_value(GPIOH(0)) & 1) << 0;
    value |= (gpio_get_value(GPIOH(2)) & 1) << 1;
    value |= (gpio_get_value(GPIOH(1)) & 1) << 2;
#endif
    return value;
}

u8 quadFALC_gpio_get_board_ver(void)
{
    u8 value = 0;

    value |= (gpio_get_value(GPIOH(4)) & 1) << 0;
    value |= (gpio_get_value(GPIOH(7)) & 1) << 1;
    value |= (gpio_get_value(GPIOH(9)) & 1) << 2;

    printk(KERN_ERR "%s : %d\n", __FUNCTION__, value);
    
    return value;
}

u8 quadFALC_gpio_get_cpld_ver(void)
{
    u8 value = 0;


    quadFALC_cpld_read_reg(0xF, &value);
    printk(KERN_ERR "%s : %d\n", __FUNCTION__, value);
    
    return value;
}


void quadFALC_gpio_reset_chip(int chipno)
{
    //printk(KERN_ERR "%s\n", __FUNCTION__);
    
    gpio_direction_output(quadFALC_bus_pins.RST, 0);
    mdelay(200);
    gpio_direction_output(quadFALC_bus_pins.RST, 1);
}

int quadFALC_gpio_init(void)
{
    int i, ret;

    //printk(KERN_ERR "%s\n", __FUNCTION__);
    
    ret = gpio_request_array(quadFALC_bus_gpios, ARRAY_SIZE(quadFALC_bus_gpios));
    if (IS_ERR_VALUE(ret))
    {
        printk(KERN_ERR "%s: gpio_request_array bus, ret:%d\n", __FUNCTION__, ret);
        return ret;
    }

    ret = gpio_request_array(quadFALC_led_gpios, ARRAY_SIZE(quadFALC_led_gpios));
    if (IS_ERR_VALUE(ret))
    {
        printk(KERN_ERR "%s: gpio_request_array led, ret:%d\n", __FUNCTION__, ret);
        
        gpio_free_array(quadFALC_bus_gpios, ARRAY_SIZE(quadFALC_bus_gpios));
        return ret;
    }

    ret = gpio_request_array(quadFALC_ver_gpios, ARRAY_SIZE(quadFALC_ver_gpios));
    if (IS_ERR_VALUE(ret))
    {
        printk(KERN_ERR "%s: gpio_request_array ver, ret:%d\n", __FUNCTION__, ret);
        
        gpio_free_array(quadFALC_bus_gpios, ARRAY_SIZE(quadFALC_bus_gpios));
        gpio_free_array(quadFALC_led_gpios, ARRAY_SIZE(quadFALC_led_gpios));
        return ret;
    }
    
    quadFALC_bus_pins.ALE       = GPIOG(10);
    quadFALC_bus_pins.INT       = GPIOG(11);
    quadFALC_bus_pins.RST       = GPIOG(12);
    quadFALC_bus_pins.WE_N      = GPIOG(13);
    quadFALC_bus_pins.PEF_CS_N  = GPIOL(10);
    quadFALC_bus_pins.RE_N      = GPIOL(11);
    quadFALC_bus_pins.CPLD_CS_N = GPIOL(12);


    for(i=0; i<ARRAY_SIZE(quadFALC_bus_gpios); i++)
    {
        gpio_direction_output(quadFALC_bus_gpios[i].gpio, 1);
    }

    for(i=0; i<ARRAY_SIZE(quadFALC_led_gpios); i++)
    {
        gpio_direction_output(quadFALC_led_gpios[i].gpio, 1);
    }

    for(i=0; i<ARRAY_SIZE(quadFALC_ver_gpios); i++)
    {
        gpio_direction_input(quadFALC_ver_gpios[i].gpio);
    }

    
    /* 申请 GPIO 中断 */
    gpio_direction_input(quadFALC_bus_pins.INT);
    
	ret = request_irq(gpio_to_irq(quadFALC_bus_pins.INT), quadFALC_irq_handler, IRQF_TRIGGER_FALLING, "quadFALC_irq", NULL);
	if(ret)
	{
		printk(KERN_ERR "%s: request_irq ret:%d\n", __FUNCTION__, ret);
        
        gpio_free_array(quadFALC_bus_gpios, ARRAY_SIZE(quadFALC_bus_gpios));
        gpio_free_array(quadFALC_led_gpios, ARRAY_SIZE(quadFALC_led_gpios));
        gpio_free_array(quadFALC_ver_gpios, ARRAY_SIZE(quadFALC_ver_gpios));

        return ret;
	}
    
    return 0;
}


void quadFALC_gpio_exit(void)
{
    //printk(KERN_ERR "%s\n", __FUNCTION__);
    
    free_irq(gpio_to_irq(quadFALC_bus_pins.INT), NULL);
    
    gpio_free_array(quadFALC_bus_gpios, ARRAY_SIZE(quadFALC_bus_gpios));
    gpio_free_array(quadFALC_led_gpios, ARRAY_SIZE(quadFALC_led_gpios));
    gpio_free_array(quadFALC_ver_gpios, ARRAY_SIZE(quadFALC_ver_gpios));
}

