/*****************************************************************************
* Function: fpga_drv.c
* Copyright: Copyright @2014 Dinstar, Inc, All right reserved.
* Author: Tom
*
* Desc:  逻辑程序模块驱动程序，主要实现向上提供接口
******************************************************************************/
#include <linux/init.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/string.h>
#include <mach/hardware.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <mach/sun8i/platform-sun8iw6p1.h>
#include <linux/gpio.h>
#include "fpga_drv.h"

#define NAME "FPGA"
#define DEVNAME	"fpga"

#define SUNXI_PIO_BASE              (((u32)SUNXI_IO_VBASE) + 0xC20800)
#define SUNXI_PL_PIO_BASE           (((u32)SUNXI_IO_VBASE) + 0xF02C00)

#define PG_CFG0             (SUNXI_PIO_BASE + 0xD8)
#define PG_CFG1             (SUNXI_PIO_BASE + 0xDC)

#define PG_PULL0            (SUNXI_PIO_BASE + 0xF4)
#define PG_DATA             (SUNXI_PIO_BASE + 0xE8)

#define PE_CFG1             (SUNXI_PIO_BASE + 0x94)
#define PE_CFG2             (SUNXI_PIO_BASE + 0x98)

#define PE_PULL0            (SUNXI_PIO_BASE + 0xAC)
#define PE_PULL1            (SUNXI_PIO_BASE + 0xB0)
#define PE_DATA             (SUNXI_PIO_BASE + 0xA0)

#define PL_CFG0             (SUNXI_PL_PIO_BASE + 0x0)
#define PL_CFG1             (SUNXI_PL_PIO_BASE + 0x4)

#define PL_PULL0            (SUNXI_PL_PIO_BASE + 0x1C)
#define PL_DATA             (SUNXI_PL_PIO_BASE + 0x10)

#define PG_BASE         0
#define PG_6            (PG_BASE + 6)
#define PG_7            (PG_BASE + 7)
#define PG_8            (PG_BASE + 8)
#define PG_10           (PG_BASE + 10)
#define PG_11           (PG_BASE + 11)

#define PE_BASE         20
#define PE_14           (PE_BASE + 14)
#define PE_15           (PE_BASE + 15)
#define PE_16           (PE_BASE + 16)
#define PE_17           (PE_BASE + 17)
#define PE_18           (PE_BASE + 18)
#define PE_19           (PE_BASE + 19)

#define PL_BASE         60
#define PL_2            (PL_BASE + 2)
#define PL_3            (PL_BASE + 3)
#define PL_4            (PL_BASE + 4)
#define PL_5            (PL_BASE + 5)
#define PL_6            (PL_BASE + 6)
#define PL_7            (PL_BASE + 7)
#define PL_8            (PL_BASE + 8)
#define PL_9            (PL_BASE + 9)
#define PL_10           (PL_BASE + 10)
#define PL_11           (PL_BASE + 11)
#define PL_12           (PL_BASE + 12)

#define FPGA_RE_WE      PL_7
#define FPGA_CSN        PL_11

#define FPGA_ADDR0      PL_3
#define FPGA_ADDR1      PL_5
#define FPGA_ADDR2      PL_2
#define FPGA_ADDR3      PL_6

#define FPGA_DATA0      PL_4
#define FPGA_DATA1      PL_10
#define FPGA_DATA2      PL_12
#define FPGA_DATA3      PL_9
#define FPGA_DATA4      PL_8
#if defined(PRODUCT_UC200)
#define FPGA_SPI_CLK    PE_14
#define FPGA_SPI_MOSI   PE_15
#define FPGA_SPI_CRESET PE_16
#define FPGA_SPI_CDONE  PE_18
#define FPGA_CPU_RST	PE_19
#define FPGA_SPI_CS     PL_7
#else
#define FPGA_SPI_CLK    PG_7
#define FPGA_SPI_MOSI   PG_8
#define FPGA_SPI_CS     PG_6
#define FPGA_SPI_CRESET PG_11
#define FPGA_SPI_CDONE  PG_10
#endif

#define OUTPUT          0
#define INPUT           1
#define DEF_BEAT_TIMEOUT   20

static struct cdev fpga_cdev;
static struct class *fpga_class = NULL;
static struct mutex fpga_lock;

static int gpio_heat = 234; //PH10
const char *name_heat = "gpio_heat";
struct timer_list heat_timer;
static int heartbeat_switch = 0;
/* device id */
static u32 fpga_major = 226, fpga_minor = 0;
static dev_t fpga_devno;
/* 接收缓存4k */
unsigned char eth_recv_buf[1024];
unsigned int heat_timeout = 0;
static struct fpga_mpp stFpgaMpp;

static void fpga_gpio_write(u32 pin, u32 value);
static void fpga_gpio_read(u32 pin, u32* value);

/* 寄存器读写 */
#define get_reg_value(reg)          (*(volatile unsigned int *)((reg)|INTER_REGS_BASE))
#define set_reg_value(reg, value)   ((*(volatile unsigned int *)((reg)|INTER_REGS_BASE)) = (value))

/*
MTG3000 不同硬件版本MPP改动：
HW_version=0					HW_version=1
0  NAND
...
5
6  ADDR 2                       ->wdg
7  SPI_CSN  //spi flash
8  DATA 0
9  DATA 1
10 UA0
11 UA0
12 //配置字            			->ADDR 2
13 NRE_WE   //fpga
14 NCS_GPIO //fpga
15 8370 interrupt               ->UART1_TXD
16 ADDR 1                       ->UART1_RXD
17 RUNLED                       ->心跳
18 NAND
19 NAND
20 GE1
...
27
28 HARDVER
29 HARDVER
30 GE1
...
33
34 HARDID
35 HARDID
36 WDI      //enable
37 ADDR 0
38 WDI      //feed
39 SPI_CLK
40 SPI_MOSI
41 SPI_MISO						->8370 reset
42 SPI_CS     
43 SPI CRESET
44 SPI CDONE

45 ADDR 3
46 DATA 4
47 DATA 3
48 DATA 2
49 空闲                         ->ADDR 1
*/

/* 该模块由bala 实现，整理调用 */
void FPGA_Delay(void)
{
    int k = 20000;   //200-->20000 martin
    while (k--) ;
}

void bzero(char *buf, int count)
{
    int i;

    for(i=0; i<count; i++)
    {
        buf[i] = 0;
    }
}

/************************************************************************************
* Function: write_gpio()
* Desc: 对GPIO 口进行写操作
*       保留该接口(仅仅fpga加载时调用)，尽量不改动，以免影响spi时序相关
* Input: gpio_num, gpio 引脚号
* Input: value, 写入值
*
* Return: SUCCESS/FAILED
*************************************************************************************/
static int write_gpio(int gpio_num, int value)
{
    u32 reg_val;
    
	if(gpio_num >= PG_BASE && gpio_num < PE_BASE)
	{
		reg_val = readl(PG_DATA);
		reg_val &= ~(1 << (gpio_num - PG_BASE));
		if(Bit_SET == value)
		{
			reg_val |= 1 << (gpio_num - PG_BASE);
		}
		writel(reg_val, PG_DATA);
	}
	
	else if(gpio_num >= PE_BASE && gpio_num < PL_BASE)
	{
		reg_val = readl(PE_DATA);
		reg_val &= ~(1 << (gpio_num - PE_BASE));
		if(Bit_SET == value)
		{
			reg_val |= 1 << (gpio_num - PE_BASE);
		}
		writel(reg_val, PE_DATA);
	}
	
    else if(gpio_num >= PL_BASE)
    {
        reg_val = readl(PL_DATA);
        reg_val &= ~(1 << (gpio_num - PL_BASE));
        if(Bit_SET == value)
        {
            reg_val |= 1 << (gpio_num - PL_BASE);
        }
        writel(reg_val, PL_DATA);
    }

	return SUCCESS;
}

static void start_heat_gpio(void)
{
     if(heartbeat_switch)
    {
        gpio_set_value(gpio_heat, 0);
        heartbeat_switch = 0;
    }
    else
    {
        gpio_set_value(gpio_heat, 1);
        heartbeat_switch = 1;
    }
}
static void start_heat_timer(unsigned long data)
{
    mod_timer(&heat_timer, jiffies + heat_timeout*HZ/1000);     
    start_heat_gpio();            
}
static void Delay100Ns(int N)
{
        int i = 20, j = 0;

        for (j = 0; j < N; j++) {
            while (i--)
                ;
            i = 20;
        }
}


static void DelayMs(unsigned long N)
{
    int i = 0;

    for (i = 0; i < N; i++) {
        udelay(1000);
    }
}


static void FPGA_GPIO_set_default(void)
{
    /* 地址的gpio默认初始化为0011，数据的那几个gpio默认初始化为00011，片选的gpio默认初始化为1，读写的gpio默认初始化为0 */
    fpga_gpio_write(stFpgaMpp.NRE_WE, 0);

    fpga_gpio_write(stFpgaMpp.ADDR0, 1);
    fpga_gpio_write(stFpgaMpp.ADDR1, 1);
    fpga_gpio_write(stFpgaMpp.ADDR2, 0);
    fpga_gpio_write(stFpgaMpp.ADDR3, 0);

    fpga_gpio_write(stFpgaMpp.DATA0, 1);
    fpga_gpio_write(stFpgaMpp.DATA1, 1);
    fpga_gpio_write(stFpgaMpp.DATA2, 0);
    fpga_gpio_write(stFpgaMpp.DATA3, 0);
    fpga_gpio_write(stFpgaMpp.DATA4, 0);

    fpga_gpio_write(stFpgaMpp.NCS, 1);
    DelayMs(20);
}

//clk-->gpio39
#define FPGA_SetCLK(NewState) \
do {   \
    if(LOW != (NewState)) \
    {   \
        write_gpio(stFpgaMpp.SPI_CLK, Bit_SET);    \
    }   \
    else    \
    {   \
        write_gpio(stFpgaMpp.SPI_CLK, Bit_RESET);  \
    }   \
} while(0)


//mosi-->gpio40
#define FPGA_SetMOSI(NewState)    \
do {   \
    if(LOW != (NewState)) \
    {   \
        write_gpio(stFpgaMpp.SPI_MOSI, Bit_SET);  \
    }   \
    else    \
    {   \
        write_gpio(stFpgaMpp.SPI_MOSI, Bit_RESET);    \
    }   \
} while(0)


//cs-->gpio42
#define FPGA_SetCS(NewState)  \
do {   \
    if(LOW != (NewState)) \
    {   \
          write_gpio(stFpgaMpp.SPI_CS, Bit_SET);      \
    }   \
    else    \
    {   \
         write_gpio(stFpgaMpp.SPI_CS, Bit_RESET);   \
    }   \
} while(0)


//reset-->gpio43
#define FPGA_SetRESET(NewState)   \
    do {   \
        if(LOW != (NewState)) \
        {   \
            write_gpio(stFpgaMpp.SPI_CRESET, Bit_SET);   \
        }   \
        else    \
        {   \
            write_gpio(stFpgaMpp.SPI_CRESET, Bit_RESET);    \
        }   \
    } while(0)


#define FPGA_SendByte(data)   \
do {   \
    unsigned char datatmp = (data);  \
    int i, len;  \
    \
    len = sizeof((datatmp)) * 8;\
    for(i=0; i<len; i++)    \
    {   \
        if(!((datatmp) & 0x80)) \
        {   \
            FPGA_SetMOSI(LOW);  \
        }   \
        else    \
        {   \
            FPGA_SetMOSI(HIGH);   \
        }   \
        FPGA_SetCLK(LOW);       \
        FPGA_Delay();       \
        FPGA_SetCLK(HIGH);    \
        FPGA_Delay();   \
        (datatmp) <<= 1; \
    }   \
} while(0)


#define FPGA_SendWord(pFgpaDev, data)   \
do {    \
    int i, len = sizeof((data)) * 8;  \
    \
    for(i=0; i<len; i++)    \
    {   \
        if((data) & 0x80000000)   \
        {   \
            FPGA_SetMOSI((pFgpaDev), HIGH);   \
        }   \
        else    \
        {   \
            FPGA_SetMOSI((pFgpaDev), LOW);    \
        }   \
        FPGA_Delay();   \
        FPGA_SetCLK((pFgpaDev), LOW); \
        FPGA_Delay();   \
        FPGA_SetCLK((pFgpaDev), HIGH);    \
        FPGA_Delay();   \
        (data) <<= 1; \
    }   \
} while(0)


/************************************************************************************
* Function: FPGA_CdoneStatus()
* Desc: 检测获取Cdone (GPIO 44) 引脚状态
*
* Return: 0/1
*************************************************************************************/
static int FPGA_CdoneStatus(void)
{
    int val;
    fpga_gpio_read(stFpgaMpp.SPI_CDONE, &val);
    
    if(val)
    {
        return HIGH;
    }
    else
    {
        return LOW;
    }
}


/************************************************************************************
* Function: FPGA_SendCLK()
* Desc: 发送时钟信号，发送一次低电平到高电平变动
* Input: count,  发送次数
*
* Return: 0/1
*************************************************************************************/
static  void FPGA_SendCLK(int count)
{
    int i;

    /* 发送时钟信号 */
    for(i=0; i<count; i++)
    {
        FPGA_SetCLK(LOW);
        //FPGA_Delay();
        FPGA_SetCLK(HIGH);
        //FPGA_Delay();
    }
}


/************************************************************************************
* Function: FPGA_SendCmd()
* Desc: 该接口发送一字节数据到SPI 从设备中
* Input: cmd,  指令内容
*
* Return: No
*************************************************************************************/
static  void FPGA_SendCmd(unsigned int cmd)
{
    unsigned char cmdtmp = (cmd >> 24) & 0xff;

    FPGA_SendByte(cmdtmp);
}

/************************************************************************************
* Function: FPGA_PreLoad()
* Desc: 逻辑加载
* Input: addr,  加载地址
*
* Return: SUCCESS/FAILED
*************************************************************************************/
int FPGA_PreLoad(char *addr, unsigned int ulFileLen)
{
    int i, uiLeftLen, uiReadLen;
    unsigned long ulOffSet;

    /* internal pre-load command sequence */
    unsigned int const spipre1[6] = {0x7E000000, 0xAA000000, 0x99000000, 0x7E000000, 0x01000000, 0x0E000000};
    unsigned int const spipre2[5] = {0x83000000, 0x00000000, 0x00000000, 0x26000000, 0x11000000};
    unsigned int const spipre3[5] = {0x83000000, 0x00000000, 0x00000000, 0x27000000, 0x21000000};
    unsigned int const spipre4[1] = {0x81000000};

    unsigned char  *pbuf;
	int flag = 0;

    /* Index 1: reset FPGA Configuration State Machine : > 200ns */

	/* 初始化GPIO 引脚 */
    //FPGA_mppInit();
    //FPGA_GPIOInit();
#if defined(PRODUCT_UC200)
	write_gpio(FPGA_CPU_RST,0);
#endif
	/* 复位 */
    FPGA_SetCS(LOW);
    Delay100Ns( 1000 );
    FPGA_SetRESET(LOW);

    FPGA_SetCLK(LOW);
    FPGA_SetMOSI(LOW);
    Delay100Ns(2);                 //DelayNs(200);

    while(FPGA_CdoneStatus() == HIGH)
    {
        flag++;
		if (flag > 1000*1000)  // 200ms后退出
		{
			printk(KERN_ERR "FPGA Preload failed\r\n");
	        return FAILED;
		}
    	Delay100Ns(2);             //DelayNs(200);
    }

    //printk("fpga1 cdon is low.\n");
    Delay100Ns(2);                 //DelayNs(200);

    //Index 2: force FPGA Configuration Slave Mode : > 300us
    FPGA_SetRESET(HIGH);
    FPGA_SetCLK(HIGH);     //LOW->(HIGH)
    udelay(1000);              //DelayUs(300);

    //Begin pre-load sequence
    //Index 3: Set SPI_SS_B high for 8 clocks
    FPGA_SetCS(HIGH);
    FPGA_Delay();
    FPGA_SendCLK(8);

    //Index 4: Sent first pre-load commands
    FPGA_SetCS(LOW);
    FPGA_Delay();

    for(i=0; i<6; i++)
    {
        FPGA_SendCmd(spipre1[i]);
    }

    //Index 5: Set SPI_SS_B high for 8 clocks
    FPGA_SetCS(HIGH);
    FPGA_Delay();
    FPGA_SendCLK(13000);

    //Index 6: Sent second pre-load commands
    FPGA_SetCS(LOW);
    FPGA_Delay();

    for(i=0; i<5; i++)
    {
        FPGA_SendCmd(spipre2[i]);
    }

    //Index 7: Set SPI_SS_B high for 8 clocks
    FPGA_SetCS(HIGH);
    FPGA_Delay();
    FPGA_SendCLK(8);

    //Index 8: Sent third pre-load commands
    FPGA_SetCS(LOW);
    FPGA_Delay();
    for(i=0; i<5; i++)
    {
        FPGA_SendCmd(spipre3[i]);
    }

    //Index 9: Set SPI_SS_B high for 8 clocks
    FPGA_SetCS(HIGH);
    FPGA_Delay();
    FPGA_SendCLK(8);

    //Index 10: Sent fourth pre-load commands
    FPGA_SetCS(LOW);
    FPGA_Delay();

    for(i=0; i<1; i++)
    {
          FPGA_SendCmd(spipre4[i]);
    }


    //Index 11: Set SPI_SS_B high for 8 clocks
    FPGA_SetCS(HIGH);
    FPGA_Delay();
    FPGA_SendCLK(8);

    uiLeftLen = ulFileLen;
    ulOffSet = 0;
    while(uiLeftLen > 0)
    {
        bzero((char *)eth_recv_buf, sizeof(eth_recv_buf));
        if(uiLeftLen > sizeof(eth_recv_buf))
        {
            uiReadLen = sizeof(eth_recv_buf);
        }
        else
        {
            uiReadLen = uiLeftLen;
        }

        //printk("load...\n");

	    for(i=0;i<uiReadLen;i++)
	    {
	        //eth_recv_buf[i] = *(volatile unsigned char *)(simple_strtoul(addr, NULL, 16)+ulOffSet+i);
	        eth_recv_buf[i] = *(volatile unsigned char *)( addr + ulOffSet + i);
	    }

	    uiReadLen = i;
        pbuf = eth_recv_buf;
        for(i=0; i<uiReadLen; i++)
        {
             FPGA_SendByte(*pbuf++);
        }
        uiLeftLen -= uiReadLen;
        ulOffSet += uiReadLen;
    }

    FPGA_SendCLK(100);
    if(HIGH != FPGA_CdoneStatus())
    {
        printk(KERN_ERR "FPGA Preload failed\r\n");
        return FAILED;
    }    
#if defined(PRODUCT_UC200)
    ssleep(1);
	write_gpio(FPGA_CPU_RST,1);
#endif

    return SUCCESS;
}

/***********************************************************************************
* Function: fpga_gpio_write()
* Desc: GPIO 口写入操作
* Input: pin, GPIO 引脚号
* Input: value, 待写入的值
*
* Return: No
************************************************************************************/
static void fpga_gpio_write(u32 pin, u32 value)
{
    if(value)
    {
        write_gpio(pin, Bit_SET);
    }
    else
    {
        write_gpio(pin, Bit_RESET);
    }
}


/***********************************************************************************
* Function: fpga_gpio_read()
* Desc: GPIO 口写入操作
* Input: pin, GPIO 引脚号
* Output: value, 读出返回值
*
* Return: No
************************************************************************************/
static void fpga_gpio_read(u32 pin, u32* value)
{
    u32 reg_val;

    if(NULL == value)
    {
        return;
    }

    if(pin >= PG_BASE && pin < PE_BASE)
    {
        reg_val = readl(PG_DATA) & (1 << (pin - PG_BASE));
        if(reg_val)
        {
            *value = 1;
        }
        else
        {
            *value = 0;
        }
    }
    
    else if(pin >= PE_BASE && pin < PL_BASE)
    {
        reg_val = readl(PE_DATA) & (1 << (pin - PE_BASE));
        if(reg_val)
        {
            *value = 1;
        }
        else
        {
            *value = 0;
        }
    }
    
    else if(pin >= PL_BASE)
    {
        reg_val = readl(PL_DATA) & (1 << (pin - PL_BASE));
        if(reg_val)
        {
            *value = 1;
        }
        else
        {
            *value = 0;
        }
    }
}

static void fpga_gpio_data_dir(int dir)
{
    uint val;
    
    if(INPUT == dir)
    {
        val = readl(PL_CFG0);
        val &= ~((0x7 << ((FPGA_DATA0-PL_BASE)*4)));
        writel(val, PL_CFG0);
        val = readl(PL_CFG1);
        val &= ~((0x7 << ((FPGA_DATA4-PL_BASE-8)*4)) | 
            (0x7 << ((FPGA_DATA3-PL_BASE-8)*4)) | 
            (0x7 << ((FPGA_DATA1-PL_BASE-8)*4)) | 
            (0x7 << ((FPGA_DATA2-PL_BASE-8)*4)));
        writel(val, PL_CFG1);
    }
    else
    {
        val = readl(PL_CFG0);
        val &= ~(0x7 << ((FPGA_DATA0-PL_BASE)*4));
        val |= (0x1 << ((FPGA_DATA0-PL_BASE)*4));
        writel(val, PL_CFG0);
        val = readl(PL_CFG1);
        val &= ~((0x7 << ((FPGA_DATA4-PL_BASE-8)*4)) | 
            (0x7 << ((FPGA_DATA3-PL_BASE-8)*4)) | 
            (0x7 << ((FPGA_DATA1-PL_BASE-8)*4)) | 
            (0x7 << ((FPGA_DATA2-PL_BASE-8)*4)));
        val |= (0x1 << ((FPGA_DATA4-PL_BASE-8)*4)) | 
            (0x1 << ((FPGA_DATA3-PL_BASE-8)*4)) | 
            (0x1 << ((FPGA_DATA1-PL_BASE-8)*4)) | 
            (0x1 << ((FPGA_DATA2-PL_BASE-8)*4));
        writel(val, PL_CFG1);
    }
}

static void FPGA_GPIO_init(void)
{
    uint val;
    
#if defined(PRODUCT_UC200)
	val = readl(PE_CFG1);
	val &= ~((0x7 << ((FPGA_SPI_CLK-PE_BASE)+10)) |
			 (0x7 << ((FPGA_SPI_MOSI-PE_BASE)+13)));
	val |= ((1 << ((FPGA_SPI_CLK-PE_BASE)+10)) | 
			(0x1 << ((FPGA_SPI_MOSI-PE_BASE)+13)));
	writel(val, PE_CFG1);

	val = readl(PE_CFG2);
	val &= ~((0x7 << (FPGA_SPI_CDONE-PE_BASE-10)) | 
			 (0x7 << (FPGA_SPI_CRESET-PE_BASE-16)) |
			 (0x7 << (FPGA_CPU_RST-PE_BASE-7)));
	val |=	((0 << (FPGA_SPI_CDONE-PE_BASE-10)) | 
			 (0x1 << (FPGA_SPI_CRESET-PE_BASE-16)) |
			 (0x1 << (FPGA_CPU_RST-PE_BASE-7)));
	writel(val, PE_CFG2);

	val = readl(PE_PULL0);
	val &= ~((0x3 << ((FPGA_SPI_CLK-PE_BASE)*2)) | 
			 (0x3 << ((FPGA_SPI_MOSI-PE_BASE)*2)));
	val |= ((0x1 << ((FPGA_SPI_CLK-PE_BASE)*2)) | 
			(0x1 << ((FPGA_SPI_MOSI-PE_BASE)*2)));
	writel(val, PE_PULL0);

	val = readl(PE_PULL1);
	val &= ~((0x3 << (FPGA_SPI_CRESET-PE_BASE-16)) |
			  (0x3 << (FPGA_CPU_RST-PE_BASE-13)));
	val |= ((0x1 << (FPGA_SPI_CRESET-PE_BASE-16)) |
			(0x10 << (FPGA_CPU_RST-PE_BASE-13)));
	writel(val, PE_PULL1);

	// cs
	val = readl(PL_CFG0);
	val &= ~(0x7 << ((FPGA_SPI_CS-PL_BASE)*4));
	val |= (1 << ((FPGA_SPI_CS-PL_BASE)*4));
	writel(val, PL_CFG0);

	val = readl(PL_PULL0);
	val &= ~(0x3 << ((FPGA_SPI_CS-PL_BASE)*2));
	val |= (0x1 << ((FPGA_SPI_CS-PL_BASE)*2));
	writel(val, PL_PULL0);
#else
    val = readl(PG_CFG0);
    val &= ~((0x7 << ((FPGA_SPI_CS-PG_BASE)*4)) | 
        (0x7 << ((FPGA_SPI_CLK-PG_BASE)*4)));
    val |= ((1 << ((FPGA_SPI_CS-PG_BASE)*4)) | 
        (1 << ((FPGA_SPI_CLK-PG_BASE)*4)));
    writel(val, PG_CFG0);
    val = readl(PG_CFG1);
    val &= ~((0x7 << ((FPGA_SPI_MOSI-PG_BASE-8)*4)) | 
        (0x7 << ((FPGA_SPI_CDONE-PG_BASE-8)*4)) | 
        (0x7 << ((FPGA_SPI_CRESET-PG_BASE-8)*4)));
    val |= ((0x1 << ((FPGA_SPI_MOSI-PG_BASE-8)*4)) | 
        (0 << ((FPGA_SPI_CDONE-PG_BASE-8)*4)) | 
        (0x1 << ((FPGA_SPI_CRESET-PG_BASE-8)*4)));
    writel(val, PG_CFG1);

    val = readl(PG_PULL0);
    val &= ~((0x3 << ((FPGA_SPI_CS-PG_BASE)*2)) | 
        (0x3 << ((FPGA_SPI_CLK-PG_BASE)*2)) | 
        (0x3 << ((FPGA_SPI_MOSI-PG_BASE)*2)) | 
        (0x3 << ((FPGA_SPI_CRESET-PG_BASE)*2)));
    val |= ((0x1 << ((FPGA_SPI_CS-PG_BASE)*2)) | 
        (0x1 << ((FPGA_SPI_CLK-PG_BASE)*2)) | 
        (0x1 << ((FPGA_SPI_MOSI-PG_BASE)*2)) | 
        (0x1 << ((FPGA_SPI_CRESET-PG_BASE)*2)));
    writel(val, PG_PULL0);

    val = readl(PL_CFG0);
    val &= ~((0x7 << ((FPGA_ADDR2-PL_BASE)*4)) | 
        (0x7 << ((FPGA_ADDR0-PL_BASE)*4)) | 
        (0x7 << ((FPGA_DATA0-PL_BASE)*4)) | 
        (0x7 << ((FPGA_ADDR1-PL_BASE)*4)) |
        (0x7 << ((FPGA_ADDR3-PL_BASE)*4)) | 
        (0x7 << ((FPGA_RE_WE-PL_BASE)*4)));
    val |= (0x1 << ((FPGA_ADDR2-PL_BASE)*4)) | 
        (0x1 << ((FPGA_ADDR0-PL_BASE)*4)) | 
        (0x1 << ((FPGA_DATA0-PL_BASE)*4)) | 
        (0x1 << ((FPGA_ADDR1-PL_BASE)*4)) |
        (0x1 << ((FPGA_ADDR3-PL_BASE)*4)) | 
        (0x1 << ((FPGA_RE_WE-PL_BASE)*4));
    writel(val, PL_CFG0);
    val = readl(PL_CFG1);
    val &= ~((0x7 << ((FPGA_DATA4-PL_BASE-8)*4)) | 
        (0x7 << ((FPGA_DATA3-PL_BASE-8)*4)) | 
        (0x7 << ((FPGA_DATA1-PL_BASE-8)*4)) | 
        (0x7 << ((FPGA_CSN-PL_BASE-8)*4)) | 
        (0x7 << ((FPGA_DATA2-PL_BASE-8)*4)));
    val |= (0x1 << ((FPGA_DATA4-PL_BASE-8)*4)) | 
        (0x1 << ((FPGA_DATA3-PL_BASE-8)*4)) | 
        (0x1 << ((FPGA_DATA1-PL_BASE-8)*4)) | 
        (0x1 << ((FPGA_CSN-PL_BASE-8)*4)) | 
        (0x1 << ((FPGA_DATA2-PL_BASE-8)*4));
    writel(val, PL_CFG1);

    val = readl(PL_PULL0);
    val &= ~((0x3 << ((FPGA_ADDR2-PL_BASE)*2)) | 
        (0x3 << ((FPGA_ADDR0-PL_BASE)*2)) | 
        (0x3 << ((FPGA_DATA0-PL_BASE)*2)) | 
        (0x3 << ((FPGA_ADDR1-PL_BASE)*2)) | 
        (0x3 << ((FPGA_ADDR3-PL_BASE)*2)) |
        (0x3 << ((FPGA_RE_WE-PL_BASE)*2)) | 
        (0x3 << ((FPGA_DATA4-PL_BASE)*2)) | 
        (0x3 << ((FPGA_DATA3-PL_BASE)*2)) | 
        (0x3 << ((FPGA_DATA1-PL_BASE)*2)) | 
        (0x3 << ((FPGA_CSN-PL_BASE)*2)) |
        (0x3 << ((FPGA_DATA2-PL_BASE)*2)));
    val |= (0x1 << ((FPGA_ADDR2-PL_BASE)*2)) | 
        (0x1 << ((FPGA_ADDR0-PL_BASE)*2)) | 
        (0x1 << ((FPGA_DATA0-PL_BASE)*2)) | 
        (0x1 << ((FPGA_ADDR1-PL_BASE)*2)) | 
        (0x1 << ((FPGA_ADDR3-PL_BASE)*2)) |
        (0x1 << ((FPGA_RE_WE-PL_BASE)*2)) | 
        (0x1 << ((FPGA_DATA4-PL_BASE)*2)) | 
        (0x1 << ((FPGA_DATA3-PL_BASE)*2)) | 
        (0x1 << ((FPGA_DATA1-PL_BASE)*2)) | 
        (0x1 << ((FPGA_CSN-PL_BASE)*2)) |
        (0x1 << ((FPGA_DATA2-PL_BASE)*2));
    writel(val, PL_PULL0);
    
    stFpgaMpp.NRE_WE = FPGA_RE_WE;
    stFpgaMpp.NCS = FPGA_CSN;

    stFpgaMpp.ADDR0 = FPGA_ADDR0;
    stFpgaMpp.ADDR1 = FPGA_ADDR1;
    stFpgaMpp.ADDR2 = FPGA_ADDR2;
    stFpgaMpp.ADDR3 = FPGA_ADDR3;

    stFpgaMpp.DATA0 = FPGA_DATA0;
    stFpgaMpp.DATA1 = FPGA_DATA1;
    stFpgaMpp.DATA2 = FPGA_DATA2;
    stFpgaMpp.DATA3 = FPGA_DATA3;
    stFpgaMpp.DATA4 = FPGA_DATA4;
#endif

    stFpgaMpp.SPI_CLK = FPGA_SPI_CLK;
    stFpgaMpp.SPI_MOSI = FPGA_SPI_MOSI;
    stFpgaMpp.SPI_CS = FPGA_SPI_CS;
    stFpgaMpp.SPI_CRESET = FPGA_SPI_CRESET;
    stFpgaMpp.SPI_CDONE = FPGA_SPI_CDONE;
}

/***********************************************************************************
* Function: fpga_write_data()
* Desc: 往local bus 中写入数据
* Input: addr, 地址值, 最低位对应addr[0], 依次排列
* Input: data, 写入数据, 最低位对应data[0], 依次排列
*
* Return: No
************************************************************************************/
static void fpga_write_data(u32 addr, u32 data)
{
    /* addr,:3bit , data:5bit */
	u32 mask = 1;
	u32 val = 0;
	u32 addr_mask = 0xf;
	u32 data_mask = 0x1f;

    fpga_gpio_data_dir(OUTPUT);

    /* 1) 每次写操作必须保证写的信号是从0到1变化 */
    fpga_gpio_write(stFpgaMpp.NRE_WE, 0);

    /* clean high bits */
	addr &= addr_mask;
	data &= data_mask;

	/* set addr[0:3], GPIO45/GPIO6/GPIO16/GPIO37 */
	val = (addr >> 0) & mask;
	fpga_gpio_write(stFpgaMpp.ADDR0, val);
	val = (addr >> 1) & mask;
	fpga_gpio_write(stFpgaMpp.ADDR1, val);
	val = (addr >> 2) & mask;
	fpga_gpio_write(stFpgaMpp.ADDR2, val);
	val = (addr >> 3) & mask;
	fpga_gpio_write(stFpgaMpp.ADDR3, val);

	/* set data[0:4], GPIO8/GPIO9/GPIO48/GPIO47/GPIO46 */
	val = (data >> 0) & mask;
	fpga_gpio_write(stFpgaMpp.DATA0, val);
	val = (data >> 1) & mask;
	fpga_gpio_write(stFpgaMpp.DATA1, val);
	val = (data >> 2) & mask;
	fpga_gpio_write(stFpgaMpp.DATA2, val);
	val = (data >> 3) & mask;
	fpga_gpio_write(stFpgaMpp.DATA3, val);
	val = (data >> 4) & mask;
	fpga_gpio_write(stFpgaMpp.DATA4, val);

	/* 适当延时后再准备给片选信号set cs, GPIO14 */
    //udelay(1);
	fpga_gpio_write(stFpgaMpp.NCS, 0);

	/* 2) set en, GPIO13 , write , en = 1 */
    //udelay(1);
	fpga_gpio_write(stFpgaMpp.NRE_WE, 1);
    /* 3) 写完后(延时100ms),恢复默认为读 */
    udelay(1);
    fpga_gpio_write(stFpgaMpp.NCS, 1);
    fpga_gpio_write(stFpgaMpp.NRE_WE, 0);
    
    //printk(KERN_ERR "    [kernel func:%s addr:0x%x data:0x%x\n", __func__, addr, data);
}


/***********************************************************************************
* Function: fpga_read_data()
* Desc: 往local bus 中写入数据
* Input: addr, 地址值, 最低位对应addr[0], 依次排列
* Input: data, 写入数据, 最低位对应data[0], 依次排列
*
* Return: No
************************************************************************************/
static void fpga_read_data(u32 addr, u32* data)
{
    /* addr,:3bit , data:5bit */
	u32 mask = 1;
	u32 val = 0;
	u32 addr_mask = 0xf;
	//u32 data_mask = 0x1f;

    fpga_gpio_data_dir(INPUT);

    /* clean high bits */
	addr &= addr_mask;
	*data = 0;

	/* set addr[0:3], GPIO45/GPIO6/GPIO16/GPIO37 */
	val = (addr >> 0) & mask;
	fpga_gpio_write(stFpgaMpp.ADDR0, val);
	val = (addr >> 1) & mask;
	fpga_gpio_write(stFpgaMpp.ADDR1, val);
	val = (addr >> 2) & mask;
	fpga_gpio_write(stFpgaMpp.ADDR2, val);
	val = (addr >> 3) & mask;
	fpga_gpio_write(stFpgaMpp.ADDR3, val);

	/* set cs, GPIO14 */
	fpga_gpio_write(stFpgaMpp.NCS, 0);

	/* set en, GPIO13 , read, en = 0 */
	fpga_gpio_write(stFpgaMpp.NRE_WE, 0);

	udelay(1);

	/* get data[0:4], GPIO8/GPIO9/GPIO48/GPIO47/GPIO46 */
	fpga_gpio_read(stFpgaMpp.DATA0, &val);
	*data |= (val<<0);
	fpga_gpio_read(stFpgaMpp.DATA1, &val);
	*data |= (val<<1);
	fpga_gpio_read(stFpgaMpp.DATA2, &val);
	*data |= (val<<2);
	fpga_gpio_read(stFpgaMpp.DATA3, &val);
	*data |= (val<<3);
	fpga_gpio_read(stFpgaMpp.DATA4, &val);
	*data |= (val<<4);
	fpga_gpio_write(stFpgaMpp.NCS, 1);

    //printk(KERN_ERR "    [kernel func:%s addr:0x%x data:0x%x\n", __func__, addr, *data);
}


/* ioctl */
long fpga_ioctl(struct file *file, unsigned int req, unsigned long arg)
{
    int i;
	int trycount = 3;
    struct load_param_s param;
	struct local_bus_param_s local_bus_param;
	char *buffer = NULL;
    int iRet = SUCCESS;
    unsigned int user_heat_timeout;

	switch(req) {
	case FPGA_PROGRAM_LOAD:
		if (copy_from_user(&param, (struct load_param_s __user *)arg, sizeof(struct load_param_s)))
		{
		    printk(KERN_ERR "%s: copy param from user failed.\n", __func__);
			return -EFAULT;
		}

		if(param.len > CONFIGURATION_SIZE)
		{
		    printk(KERN_ERR "%s: param len err, %d!=%d.\n", __func__,
				            param.len, CONFIGURATION_SIZE);
			return -EFAULT;
		}

		buffer = kmalloc(CONFIGURATION_SIZE, GFP_KERNEL);
		if(buffer == NULL)
		{
		    printk(KERN_ERR "%s: kmalloc failed.\n", __func__);
			return -EFAULT;
		}

		if (copy_from_user(buffer, (u8 __user *)param.addr, param.len))
		{
		    printk(KERN_ERR "%s: copy file from user failed.\n", __func__);
            kfree(buffer);
			return -EFAULT;
		}

        for(i=0; i<trycount; i++)
        {
            mutex_lock(&fpga_lock);
            iRet = FPGA_PreLoad(buffer, param.len);
            mutex_unlock(&fpga_lock);
            if(!iRet)
            {
                printk("[15-12-09]FPGA Program load OK! len=%d\n", param.len);
                break;
            }
        }

        kfree(buffer);

        break;
#if !defined(PRODUCT_UC200)
	case FPGA_LOCAL_BUS_WRITE:
		if (copy_from_user(&local_bus_param, (struct local_bus_param_s __user *)arg,
			                sizeof(struct local_bus_param_s)))
		{
		    printk(KERN_ERR "%s: copy param from user failed.\n", __func__);
			return -EFAULT;
		}
        mutex_lock(&fpga_lock);  
		fpga_write_data(local_bus_param.addr, local_bus_param.data);
        mutex_unlock(&fpga_lock);
		break;

	case FPGA_LOCAL_BUS_READ:
		if (copy_from_user(&local_bus_param, (struct local_bus_param_s __user *)arg,
			                sizeof(struct local_bus_param_s)))
		{
		    printk(KERN_ERR "%s: copy param from user failed.\n", __func__);
			return -EFAULT;
		}
        mutex_lock(&fpga_lock);
		fpga_read_data(local_bus_param.addr, &local_bus_param.data);
        mutex_unlock(&fpga_lock);
	    if (copy_to_user((struct local_bus_param_s __user *)arg, &local_bus_param, sizeof(struct local_bus_param_s )))
	    {
		    printk(KERN_ERR "%s: copy param to user failed.\n", __func__);
			return -EFAULT;
	    }

		break;

	case CPU_GPIO_READ:
		/* copy  data from user */
		if( copy_from_user(&local_bus_param, (struct local_bus_param_s __user *)arg,
			               sizeof(struct local_bus_param_s)) )
		{
		    printk(KERN_ERR "%s: copy param from user failed.\n", __func__);
			return -EFAULT;
		}
            mutex_lock(&fpga_lock);
		fpga_gpio_read(local_bus_param.addr, &local_bus_param.data);
            mutex_unlock(&fpga_lock);
        /*copy data to user */
		if( copy_to_user((struct local_bus_param_s __user *)arg, &local_bus_param,
			              sizeof(struct local_bus_param_s)))
		{
		    printk(KERN_ERR "%s: copy param to user failed.\n", __func__);
			return -EFAULT;
		}

		break;
	case CPU_GPIO_WRITE:
		/* copy  data from user */
		if( copy_from_user(&local_bus_param, (struct local_bus_param_s __user *)arg,
			               sizeof(struct local_bus_param_s)) )
		{
		    printk(KERN_ERR "%s: copy param from user failed.\n", __func__);
			return -EFAULT;
		}

		local_bus_param.data &= 0x01;
		mutex_lock(&fpga_lock);
		fpga_gpio_write(local_bus_param.addr, local_bus_param.data);
        mutex_unlock(&fpga_lock);
		break;
		
    case HEAT_GPIO_START:
		/* copy  data from user */
		mutex_lock(&fpga_lock);
        mod_timer(&heat_timer, jiffies + heat_timeout*HZ/1000);  
        mutex_unlock(&fpga_lock);
		break;
    case HEAT_GPIO_STOP:
		/* copy  data from user */
        mutex_lock(&fpga_lock);
        del_timer(&heat_timer);  
        mutex_unlock(&fpga_lock);
		break;
    case HEAT_GPIO_SET:
		/* copy  data from user */
		if(copy_from_user(&user_heat_timeout, (void *)arg, sizeof(user_heat_timeout)) )
		{
		    printk(KERN_ERR "%s: copy param from user failed.\n", __func__);
			return -EFAULT;
		}
		if(user_heat_timeout < 10)
		{
            printk(KERN_ERR "%s: user_heat_timeout must be than 10ms.\n", __func__);
            return -EFAULT;                      
		}
        mutex_lock(&fpga_lock);   
		heat_timeout = user_heat_timeout;
		mutex_unlock(&fpga_lock);
		break; 
#endif
	default:
        return -ENOIOCTLCMD;
	}

	return iRet;
}

int fpga_open(struct inode *inode, struct file *file)
{
    /* do nothing */
	return 0;
}

int fpga_release(struct inode *inode, struct file *file)
{
    /* do nothing */
	return 0;
}

struct file_operations fpga_fops =
{
	owner:		THIS_MODULE,
	unlocked_ioctl:		fpga_ioctl,
	open:		fpga_open,
	release:	fpga_release,
};

/* module init */
int __init fpga_init(void)
{
	int r = 0, ret;
    struct device *device;

    fpga_devno = MKDEV(fpga_major, fpga_minor);

	/* fpga_major == 0, dynamic major */
	r = register_chrdev_region(fpga_devno, 1, DEVNAME);
	if (r < 0)
	{
		printk(KERN_ERR NAME": unable to register character device\n");
		return r;
	}
    cdev_init(&fpga_cdev, &fpga_fops);
    cdev_add(&fpga_cdev, fpga_devno, 1);

    fpga_class = class_create(THIS_MODULE, DEVNAME);
    if(IS_ERR(fpga_class))
    {
        printk(KERN_ERR "create fpga class device error\n");
        goto fail0;
    }
    device = device_create(fpga_class, NULL, fpga_devno, NULL, DEVNAME);
    if(IS_ERR(device))
    {
        printk(KERN_ERR "create fpga device error\n");
        goto fail1;
    }

	/* 初始化GPIO 引脚 */
    FPGA_GPIO_init();
    mutex_init(&fpga_lock);
    
#if !defined(PRODUCT_UC200)
    FPGA_GPIO_set_default();
    ret = gpio_request(gpio_heat ,name_heat);
    if(ret < 0)
    {
        printk(KERN_ERR "gpio_request error\n");
        goto fail1;
    }
    gpio_direction_output(gpio_heat, 1);
    init_timer(&heat_timer);
    heat_timer.function = start_heat_timer;
    heat_timer.expires = DEF_BEAT_TIMEOUT;
#endif
    printk("FPGA iCE40 driver initialized\n");

	return 0;
fail1:
    class_destroy(fpga_class);
fail0:
	unregister_chrdev_region(fpga_devno, 1);
    cdev_del(&fpga_cdev);
    return -ENODEV;
}

/* module exit */
void __exit fpga_exit(void)
{
    device_destroy(fpga_class, fpga_devno);
    class_destroy(fpga_class);
    cdev_del(&fpga_cdev);
	unregister_chrdev_region(fpga_devno, 1);
	
#if !defined(PRODUCT_UC200)
    gpio_free(gpio_heat);
#endif
	printk("FPGA iCE40 driver exited\n");
}

MODULE_DESCRIPTION("FPGA iCE40 Driver");
MODULE_AUTHOR("Tom");
MODULE_LICENSE("GPL");

module_init(fpga_init);
module_exit(fpga_exit);

