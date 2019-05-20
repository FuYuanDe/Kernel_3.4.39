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
#include <mach/hardware.h>
#include <mach/sun8i/platform-sun8iw6p1.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/gpio.h>

#include "rtk_types.h"
#include "rtk_error.h"
#include "rtl8367c_reg.h"
#include "rtl8367c_asicdrv.h"
#include "rtl8367c_asicdrv_phy.h"
#include "rtl8367c_asicdrv_port.h"
#include "rtl8367c_asicdrv_led.h"
#include "port.h"
#include "rtk_switch.h"
#include "cpu.h"
#include "led.h"
#include "vlan.h"

#include "smi.h"
#include "stat.h"
#include "acl.h"
#include "rtk_acl.h"
#include "mirror.h"
#include "rtl8367.h"
#include "ra_sw.h"

#define SUNXI_PIO_BASE              (((u32)SUNXI_IO_VBASE) + 0xC20800)
#define PHY_PAGE_ADDRESS                            31
#define NAME			"rtk8367m"
#define RTL8367M_DEVNAME	"rtk8367m"

/* ioctl 接口相关的定义 */
#define RTK8367M_IOC_MAGIC        'R'
#define RTK_PORT_GET_MIB            _IOWR(RTK8367M_IOC_MAGIC,  0x18, rtk_port_mib_t)
#define RTK_PORT_ALL_STAT_GET     _IOWR(RTK8367M_IOC_MAGIC,  0x11, int)
/* 读写PHY 寄存器操作，调试用 */
#define RTK_READ_REGISTER         _IOWR(RTK8367M_IOC_MAGIC,  0x30, struct reg_param_s)
#define RTK_WRITE_REGISTER        _IOWR(RTK8367M_IOC_MAGIC,  0x31, struct reg_param_s)


int rtk8367m_major = 206;
static struct class *rtk_dev_class = NULL;
static struct device *rtk_device = NULL;
extern acl_cb_t *acl_cb;
void __iomem *gpiobase;
static int gpio_port0_led0 = 128; //PE0
static int gpio_port1_led0 = 132; //PE4
int back_board_type;
extern int board_type_val;

typedef struct port_cntr_s
{
    //传参6个uint32
    uint32 ifInOctets;
    uint32 dot3StatsFCSErrors;
    uint32 dot3StatsSymbolErrors;
    uint32 dot3InPauseFrames;
    uint32 ifOutOctets;
    uint32 dot3OutPauseFrames;
}port_cntr_t;

typedef struct rtk_port_mib_s
{
    uint32 port;
    uint32 mib_reset;
    port_cntr_t portcntr;
} rtk_port_mib_t;

/* 读写8367 寄存器参数结构 */
struct reg_param_s
{
	u32 reg;
	u32 value;
};

static struct task_struct *led_run_tsk;

extern int rtk8367_acl_cfg(void);
extern void rtk_acl_exit(void);

/******************************************************************
  * Function: rtk8370_set_phy_powerdown
  * Desc: 1) uboot下8370未划分VLAN，增加对网管口、CPU1、业务口、CPU0隔离，可以避免业务口与网管口同时接入后网络故障，
  *          但uboot下业务口与网管口同时接入，影响通过串口升级(不能ping通server)，在此powerdown网管口，在8370ko中恢复
  *          (仅业务口接入可以ping通server，无需powerdown网管口)
  * Note: 
  * Input:        
  * Output: 
  * Return: 
  */
void rtk8367_set_phy_powerdown(uint32 port, uint32 bEnable)
{
    uint32              phyData;
    int                 retVal = RT_ERR_OK;
    
    if ((retVal = rtl8367c_setAsicPHYReg(port,PHY_PAGE_ADDRESS,0))!=RT_ERR_OK)
    {
        printk(KERN_ERR "%s: rtl8370_setAsicPHYReg failed! port=%d retVal=0x%x\n", __func__, port, retVal);
        return;
    }

    if ((retVal = rtl8367c_getAsicPHYReg(port,PHY_CONTROL_REG,&phyData))!=RT_ERR_OK)
    {
        printk(KERN_ERR "%s: rtl8370_getAsicPHYReg failed! port=%d retVal=0x%x\n", __func__, port, retVal);
        return;
    }

    if (bEnable)
    {
        phyData |= (1 << 11);
    }
    else
    {
        phyData &= (~(1 << 11));
    }
    if ((retVal = rtl8367c_setAsicPHYReg(port,PHY_CONTROL_REG,phyData))!=RT_ERR_OK)
    {
        printk(KERN_ERR "%s: rtl8370_setAsicPHYReg failed! port=%d phyData=0x%x retVal=0x%x\n", __func__, port, phyData, retVal);
    }
    else
    {
        printk(KERN_ERR "%s: rtl8370_setAsicPHYReg succ! port=%d phyData=0x%x\n", __func__, port, phyData);
    }
}

static void test_smi_signal_and_wait(void)
{
    int i;

    for (i=0; i<100; i++)
    {
        unsigned int data;
        rtk_api_ret_t retVal;

        if((retVal = rtl8367c_getAsicReg(0x1202, &data)) != RT_ERR_OK)
        {
            printk(KERN_ERR "error = %d\n", retVal);
        }

        //printf("data = %x\n", data);

        if (data == 0x88a8)
            break;

        CLK_DURATION(50000);
    }
}

static int set_ext_mode(void)
{
    int retVal;
    rtk_port_mac_ability_t mac_cfg;
    rtk_mode_ext_t mode;

    mode = MODE_EXT_RGMII;
    mac_cfg.forcemode = MAC_FORCE;
    mac_cfg.speed = SPD_1000M;
    mac_cfg.duplex = FULL_DUPLEX;
    mac_cfg.link = PORT_LINKUP;
    mac_cfg.nway = DISABLED;
    mac_cfg.txpause = ENABLED;
    mac_cfg.rxpause = ENABLED;

    retVal = rtk_port_macForceLinkExt_set(EXT_PORT0, mode, &mac_cfg);
    //printf("rtk_port_macForceLinkExt0_set:set ext0 force 1000M speed,retVal %d\n", retVal);

    retVal = rtk_port_macForceLinkExt_set(EXT_PORT1, mode, &mac_cfg);
    //printf("rtk_port_macForceLinkExt1_set:set ext1 force 1000M speed,retVal %d\n", retVal);

    retVal = rtk_port_macForceLinkExt_set(EXT_PORT2, mode, &mac_cfg);
    //printf("rtk_port_macForceLinkExt1_set:set ext1 force 1000M speed,retVal %d\n", retVal);
    return retVal;
}

static int init_ext_interface(void)
{
   int retVal;

   /* set external interface 0 rgmii mode*/
   retVal = rtl8367c_setAsicPortExtMode(EXT_PORT0, MODE_EXT_RGMII); //RTK_EXT_0 - PORT9
   //printf("rtl8370_setAsicPortExtMode:set ext0 RGMII mode,retVal %d\n", retVal);

   /* set external interface 0 rgmii mode*/
   retVal = rtl8367c_setAsicPortExtMode(EXT_PORT1, MODE_EXT_RGMII);   //RTK_EXT_1 - PORT8
   //printf("rtl8370_setAsicPortExtMode:set ext1 RGMII mode,retVal %d\n", retVal);

   retVal = rtl8367c_setAsicPortExtMode(EXT_PORT2, MODE_EXT_RGMII);   //RTK_EXT_1 - PORT8

///-
#if 0
   retVal = rtk_port_rgmiiDelayExt0_set(1, 4);
   printf("rtk_port_rgmiiDelayExt0_set:set ext0 RGMII delay tx:1,rx:0,retVal %d\n", retVal);

   retVal = rtk_port_rgmiiDelayExt1_set(1, 4);
   printf("rtk_port_rgmiiDelayExt1_set:set ext1 RGMII delay tx:1,rx:0,retVal %d\n", retVal);
#else
    retVal = rtk_port_rgmiiDelayExt_set(EXT_PORT0, 1, 7);
    //printf("rtk_port_rgmiiDelayExt0_set:set ext0 RGMII delay tx:1,rx:5,retVal %d\n", retVal);

    retVal = rtk_port_rgmiiDelayExt_set(EXT_PORT1, 1, 7);
    //printf("rtk_port_rgmiiDelayExt1_set:set ext1 RGMII delay tx:1,rx:5,retVal %d\n", retVal);

    retVal = rtk_port_rgmiiDelayExt_set(EXT_PORT2, 1, 7);
#endif


   return retVal;
}

int geth_getPortStatus(int port_phy, u32 *pSpeed, u32 *plink, u32 *pduplex, u32 *pnway)
{
    ret_t retVal;
    rtk_uint32 regData;

    if((NULL == pSpeed) || (NULL == plink) || (NULL == pduplex) || (NULL == pnway))
    {
        printk(KERN_ERR "%s %d: invalid args\n", __FUNCTION__, __LINE__);
        return RT_ERR_FAILED;
    }

    mutex_lock(&acl_cb->lock);
    /* Invalid input parameter */
    if(port_phy > RTK_PHY_ID_MAX)
    {
        mutex_unlock(&acl_cb->lock);
        return RT_ERR_PORT_ID;
     }   
    retVal = rtl8367c_getAsicReg(RTL8367C_REG_PORT0_STATUS + port_phy, &regData);
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

int geth_port_phyAutoNegoAbility_get(int port, u32 *mask)
{
    rtk_port_phy_ability_t pAbility;
    u32 retVal;

    if(NULL == mask)
    {
        printk(KERN_ERR "%s %d: invalid args\n", __FUNCTION__, __LINE__);
        return RT_ERR_FAILED;
    }
    
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

static int rtk8367c_phy_status_get(rtk_port_t port, u32 *pphyStatus)
{
    rtk_api_ret_t retVal;
    uint32 phyData;

    if(NULL == pphyStatus)
    {
        printk(KERN_ERR "%s %d: invalid args\n", __FUNCTION__, __LINE__);
        return RT_ERR_FAILED;
    }

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID; 

    retVal = rtl8367c_setAsicPHYReg(port, RTL8367C_PHY_PAGE_ADDRESS, 0);
    if (RT_ERR_OK != retVal)
    {
        printk(KERN_ERR "%s: failed! retVal=0x%x\n", __func__, retVal);
        return retVal;
    } 

    /*Get PHY status register*/
    retVal = rtl8367c_getAsicPHYReg(port,PHY_STATUS_REG,&phyData);
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

    if(NULL == support_mask)
    {
        printk(KERN_ERR "%s %d: invalid args\n", __FUNCTION__, __LINE__);
        return RT_ERR_FAILED;
    }

    *support_mask = 0;
    
    mutex_lock(&acl_cb->lock);	
    retVal = rtk8367c_phy_status_get(port, &port_status);
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

static int rtk8367c_link_status_get(rtk_port_t port, rtk_port_linkStatus_t *pLinkStatus)
{
    rtk_api_ret_t retVal;
    uint32 phyData;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID; 

    retVal = rtl8367c_setAsicPHYReg(port, RTL8367C_PHY_PAGE_ADDRESS, 0);
    if (RT_ERR_OK != retVal)
    {
        printk(KERN_ERR "%s: failed! retVal=0x%x\n", __func__, retVal);
        return retVal;
    } 

    /*Get PHY status register*/
    retVal = rtl8367c_getAsicPHYReg(port,PHY_STATUS_REG,&phyData);
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

    if(NULL == link_status)
    {
        printk(KERN_ERR "%s %d: invalid args\n", __FUNCTION__, __LINE__);
        return RT_ERR_FAILED;
    }
    
    mutex_lock(&acl_cb->lock);
    retVal = rtk8367c_link_status_get(port,&phy_status); 
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

int rtk8367c_mirror_add2cpu(rtk_mirror_port_t *user_mirrorport)
{
    rtk_port_t          rtkport;
    rtk_portmask_t      rx_portmask;
    rtk_portmask_t      tx_portmask;
    int                 retVal = RT_ERR_OK;
    int src_port = user_mirrorport->mirror_src_port;
    printk(KERN_ERR "%s  dst =%d src=%d\n",
                        __FUNCTION__, user_mirrorport->mirror_dst_port, user_mirrorport->mirror_src_port);
    if(src_port > RTK_MAX_PROT)
    {
        printk(KERN_ERR "%s invalid src_port %d\n", __FUNCTION__, src_port);
        return RT_ERR_FAILED;
    }

    retVal = rtk_mirror_portBased_get(&rtkport, &rx_portmask, &tx_portmask);
    if( retVal != RT_ERR_OK )
	{
		printk("  %s: rtk_mirror_portBased_set failed! src_port=%d retVal=0x%x\n", __func__, src_port, retVal);
        goto err;
	}
    else
    {
        printk("  %s: src_port:%d rtkport:%u rx_portmask:%u tx_portmask:%u\n", __func__, src_port, rtkport, rx_portmask.bits[0], tx_portmask.bits[0]);
    }
    printk("  %s: mirror_direct:%d \n", __func__, user_mirrorport->mirror_direct);

    if(user_mirrorport->mirror_direct ==MIRROR_DIREXT_RX)
    {
        rx_portmask.bits[0] = ((uint32)1 << src_port);
        tx_portmask.bits[0] = 0;
    }
    else if(user_mirrorport->mirror_direct==MIRROR_DIREXT_TX)
    {
        tx_portmask.bits[0] = ((uint32)1 << src_port);
        rx_portmask.bits[0] = 0;
    }
    else if(user_mirrorport->mirror_direct==MIRROR_DIREXT_BOTH)
    {
        rx_portmask.bits[0] = ((uint32)1 << src_port);
        tx_portmask.bits[0] = ((uint32)1 << src_port);
    }
   /* The mirror port can only be set to one port */
    retVal = rtk_mirror_portBased_set(user_mirrorport->mirror_dst_port, &rx_portmask, &tx_portmask);
    if( retVal != RT_ERR_OK )
	{
		printk("  %s: rtk_mirror_portBased_set failed! src_port=%d retVal=0x%x rx_portmask:0x%x tx_portmask:0x%x\n", __func__,
            src_port, retVal, rx_portmask.bits[0], tx_portmask.bits[0]);
        goto err;
	}

    printk("  %s: src_port:%d mirror add succ!\n", __func__, src_port);
    return RT_ERR_OK;

 err:
    return retVal;
}

int rtk8367c_mirror_del2cpu(rtk_mirror_port_t *user_mirrorport)
{
    rtk_port_t          rtkport ;
    rtk_portmask_t      rx_portmask;
    rtk_portmask_t      tx_portmask;
    int                 retVal = RT_ERR_OK;
    int src_port = user_mirrorport->mirror_src_port;

    if(src_port > RTK_MAX_PROT)
    {
        printk(KERN_ERR "%s invalid src_port %d\n", __FUNCTION__, src_port);
        return RT_ERR_FAILED;
    }

    retVal = rtk_mirror_portBased_get(&rtkport, &rx_portmask, &tx_portmask);
    if( retVal != RT_ERR_OK )
    {
		printk("  %s: rtk_mirror_portBased_set failed! src_port=%d retVal=0x%x\n", __func__, src_port, retVal);
        goto err;
    }
    else
    {
        printk("  %s: src_port:%d rtkport:%u rx_portmask:%u tx_portmask:%u\n", __func__, src_port, rtkport, rx_portmask.bits[0], tx_portmask.bits[0]);
    }

    rx_portmask.bits[0] = 0x0;
    tx_portmask.bits[0] = 0x0;
    retVal = rtk_mirror_portBased_set(user_mirrorport->mirror_dst_port, &rx_portmask, &tx_portmask);
    if( retVal != RT_ERR_OK )
	{
		printk("  %s: rtk_mirror_portBased_set failed! src_port=%d retVal=0x%x rx_portmask:0x%x tx_portmask:0x%x\n", __func__,
            src_port, retVal, rx_portmask.bits[0], tx_portmask.bits[0]);
        goto err;
	}

    printk("  %s: src_port:%d mirror del succ!\n", __func__, src_port);
    return RT_ERR_OK;

 err:
    return retVal;
}

static void reset_rtl8367(void)
{
    u32 reg_val;

    reg_val = readl(SUNXI_PIO_BASE + 0x94);
    reg_val &= ~(0xf << 4);
    reg_val |= (0x1 << 4);
    writel(reg_val, SUNXI_PIO_BASE + 0x94);
    reg_val = readl(SUNXI_PIO_BASE + 0xac);
    reg_val &= ~(0x3 << 18);
    reg_val |= (0x1 << 18);
    writel(reg_val, SUNXI_PIO_BASE + 0xac);
    
    reg_val = readl(SUNXI_PIO_BASE + 0xa0);
    reg_val &= ~(1 << 9);
    writel(reg_val, SUNXI_PIO_BASE + 0xa0);

    /*延时至少10ms*/
    mdelay(20);
    
    reg_val = readl(SUNXI_PIO_BASE + 0xa0);
    reg_val |= (1 << 9);
    writel(reg_val, SUNXI_PIO_BASE + 0xa0);

    /*复位信号拉高后，要延迟至少78ms*/
    mdelay(100);
}

static int link_func(void *data)
{
    rtl8367c_port_status_t   portstatus;
    int                     retVal = 0;
    int i, ret;
    rtk_portmask_t          portmask;

#if defined(PRODUCT_SBCUSER)
    /**用户板用gpio点speed灯*/
    back_board_type = board_type_val;//板子类型
	
    if(back_board_type == 2)
    {
        ret = gpio_request(gpio_port0_led0, "gpio_port0_led0");
        if (ret < 0)
        	return -1;
        ret = gpio_request(gpio_port1_led0, "gpio_port1_led0");
        if (ret < 0)
        {
            gpio_free(gpio_port0_led0);
        	return -1;
        }
        gpio_direction_output(gpio_port0_led0, 1);
        gpio_direction_output(gpio_port1_led0, 1);
    }
#endif
    while(!kthread_should_stop())
    {
        mutex_lock(&acl_cb->lock);
        for(i=UTP_PORT0; i<=UTP_PORT4; i++)
        {
            retVal = rtl8367c_getAsicPortStatus(i, &portstatus);
            if (RT_ERR_OK == retVal)
            {
                rtk_led_enable_get(LED_GROUP_0, &portmask);
                if (portstatus.speed != SPD_1000M)
                {
#if defined(PRODUCT_SBCUSER)
                    if(back_board_type == 2)
                    {
                        if(i == UTP_PORT0)
                        {
                            gpio_set_value(gpio_port0_led0, 1);
                        }
                        else if(i == UTP_PORT1)
                        {
                            gpio_set_value(gpio_port1_led0, 1);
                        }
                    }
#else
					portmask.bits[0] &= ~(1 << i);
#endif
                }
                else
                {
#if defined(PRODUCT_SBCUSER)
                    if(back_board_type == 2)
                    {
                        if(i == UTP_PORT0)
                        {
                            gpio_set_value(gpio_port0_led0, 0);
                        }
                        else if(i == UTP_PORT1)
                        {
                            gpio_set_value(gpio_port1_led0, 0);
                        }
                    }
#else
					portmask.bits[0] |= (1 << i);
#endif
                }
#if !defined(PRODUCT_SBCUSER)
                retVal = rtk_led_enable_set(LED_GROUP_0, &portmask);
#endif
            }
        }
        mutex_unlock(&acl_cb->lock);
        msleep(100);
        
        //mod_timer(&link_timer, jiffies + 100);
    }
    return 0;
}

static rtk_api_ret_t init_leds(void)
{
#if defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBCUSER) || defined(PRODUCT_UC200) || \
	defined(PRODUCT_SBC1000MAIN)
    rtl8367c_port_status_t   portstatus;
    rtk_portmask_t          portmask;
    int                     retVal = 0;
    int i;

///-
    printk(KERN_ERR "###################%s %d\n", __FUNCTION__, __LINE__);

    /* enable 绿灯+黄灯 */
    portmask.bits[0] = 0x1f; //port0+port1+port2+port3+port4
    retVal = rtk_led_enable_set(LED_GROUP_1, &portmask);
    if (RT_ERR_OK != retVal)
    {
        printk("%s: rtk_led_enable_set LED_GROUP_1 failed! retVal=0x%x\n", __func__, retVal);
        return RT_ERR_FAILED;
    }
    portmask.bits[0] = 0x1f; //port0+port1+port2+port3+port4
    retVal = rtk_led_enable_set(LED_GROUP_2, &portmask);
    if (RT_ERR_OK != retVal)
    {
        printk("%s: rtk_led_enable_set LED_GROUP_0 failed! retVal=0x%x\n", __func__, retVal);
        return RT_ERR_FAILED;
    }

    retVal = rtk_led_operation_set(LED_OP_PARALLEL);
    if (RT_ERR_OK != retVal)
    {
        printk("%s: rtk_led_operation_set failed! retVal=0x%x\n", __func__, retVal);
        return RT_ERR_FAILED;
    }
#if 0
    retVal = rtl8367c_setAsicLedGroupMode(RTL8367C_LED_MODE_1);
    if (RT_ERR_OK != retVal)
    {
        printk("%s: rtk_led_mode_set failed! retVal=0x%x\n", __func__, retVal);
        return RT_ERR_FAILED;
    }
#endif

    retVal = rtk_led_groupConfig_set(LED_GROUP_1, LED_CONFIG_LINK_ACT);
    if (RT_ERR_OK != retVal)
    {
        printk("%s: rtk_led_groupConfig_set LED_GROUP_0 failed! retVal=0x%x\n", __func__, retVal);
        return RT_ERR_FAILED;
    }
    /*灯态是反的，当速率满足要求时，芯片输出的是高电平。所以为了在1000M能点灯，
      这里不能设置成指示1000M，而是设置成指示100M或10M，当速率不是1000M时，将io
      disable，强制灯灭，当为1000M时，由于设置的是100M或10M时才会输出高电平，所以
      1000M时输出的就变成了低电平，反而将灯点亮*/
    retVal = rtk_led_groupConfig_set(LED_GROUP_2, LED_CONFIG_SPD1000);
    if (RT_ERR_OK != retVal)
    {
        printk("%s: rtk_led_groupConfig_set LED_GROUP_2 failed! retVal=0x%x\n", __func__, retVal);
        return RT_ERR_FAILED;
    }
#if defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_UC200) || defined(PRODUCT_AG) || \
	defined(PRODUCT_SBC1000MAIN)
    for(i=UTP_PORT0; i<=UTP_PORT4; i++)
    {
        retVal = rtl8367c_getAsicPortStatus(i, &portstatus);
        if (RT_ERR_OK != retVal)
        {
            printk("%s: rtl8370_getAsicPortStatus failed! retVal=0x%x\n", __func__, retVal);
        }
        else
        {
            /* GE0未工作在1000M，强制P0黄灯为1(因为P0LED0黄灯设置为反的)，
               如果工作在千兆，使能P0，但因为模式设置的是100M才会输出高电平，
               所以1000M时输出的低电平，灯会点亮*/
            rtk_led_enable_get(LED_GROUP_0, &portmask);
            if (portstatus.speed != SPD_1000M)
            {
                portmask.bits[0] &= ~(1 << i);
            }
            else
            {
                portmask.bits[0] |= (1 << i);
            }
            retVal = rtk_led_enable_set(LED_GROUP_0, &portmask);
        }
    }
#endif
    led_run_tsk = kthread_run(link_func, NULL, "1000M_link_led");

    if(IS_ERR(led_run_tsk)){
        printk(KERN_INFO "create 1000M_link_led kthread failed!\n");  
    }  
    else 
    {  
        printk(KERN_INFO "create 1000M_link_led ktrhead ok!\n");  
    }  
#endif

    return RT_ERR_OK;
}

static void mii_phy_reset(int hard_reset)
{
    unsigned int data = 0;
    unsigned int retVal;
    u16 phy_val;
    
    rtk_acl_exit();

    if(hard_reset)
    {
        reset_rtl8367();
    }

    rtl8367c_setAsicPortEnableAll(DISABLED);
    rtl8367c_setAsicPortEnableAll(DISABLED);
    rtl8367c_getAsicPortEnableAll(&data);
    //printf("disable getAsicPortEnableAll:0x%x\n", data);    

    /*after reset, switch need to delay 1 ms
        if not, SMI may send out unknown data
    */
    udelay(1000);

    retVal = rtk_switch_init();
    if (retVal  != RT_ERR_OK)
    {
         return retVal;
    }

    /* 1) 隔离接口板与外网 */
    //rtk8370_isolation_cfg();
    //rtk8370_print_isolation_cfg();

    test_smi_signal_and_wait();

    /* tom: for test */
    //smi_write(0x13a0, 0x123);
    data = geth_phy_read(0, 0x2022);
    //printf("debug,reg PHY ID value=0x%x\n", data);

    /* led enable */
    //init_and_enable_leds();

    /* force cpu 1000M mode */
    // bit13 要为0 6282 page 579 
    //MV_REG_WRITE(0x7243C, 0x00a4060f);//rnii0 ge1 1000m

    /* force 8370M 1000M node */
    set_ext_mode();

    init_ext_interface();
#if defined(PRODUCT_UC200)
    {
#if 0
        rtk_portmask_t mbrmsk, untagmask;
        rtk_vlan_cfg_t vlan_cfg;
        
        rtk_vlan_init();

        memset(&vlan_cfg, 0, sizeof(vlan_cfg));
        vlan_cfg.mbr.bits[0] = (1 << EXT_PORT0) | 0x1;
        vlan_cfg.untag.bits[0] = 0x1;
        if(RT_ERR_OK != rtk_vlan_set(1, &vlan_cfg))
        {
            printk(KERN_ERR "%s %d\n", __FUNCTION__, __LINE__);
        }
        
        memset(&vlan_cfg, 0, sizeof(vlan_cfg));
        vlan_cfg.mbr.bits[0] = (1 << EXT_PORT0) | 0x1e;
        vlan_cfg.untag.bits[0] = 0x1e;
        if(RT_ERR_OK != rtk_vlan_set(2, &vlan_cfg))
        {
            printk(KERN_ERR "%s %d\n", __FUNCTION__, __LINE__);
        }

        rtk_vlan_portPvid_set(0, 1, 0);
        rtk_vlan_portPvid_set(1, 2, 0);
        rtk_vlan_portPvid_set(2, 2, 0);
        rtk_vlan_portPvid_set(3, 2, 0);
        rtk_vlan_portPvid_set(4, 2, 0);
        //rtl8367c_setAsicVlanPortBasedVID(0, 1, 0);
        //rtl8367c_setAsicVlanPortBasedVID(1, 2, 0);

        //rtk_vlan_portAcceptFrameType_set(0, ACCEPT_FRAME_TYPE_UNTAG_ONLY);
        //rtk_vlan_portAcceptFrameType_set(1, ACCEPT_FRAME_TYPE_UNTAG_ONLY);
        rtk_vlan_portAcceptFrameType_set(EXT_PORT0, ACCEPT_FRAME_TYPE_TAG_ONLY);

        rtk_vlan_tagMode_set(EXT_PORT0, VLAN_TAG_MODE_ORIGINAL);
#else
		ra_sw_probe();
#endif

        /*禁止RMII口的mac地址学习功能。如果开启了rmii口的学习功能，当不同的vlan
          接口设置成桥模式时，一个vlan中的广播从rmii发送出来后，会被linux的桥
          又发回去，这时rmii口会学习到不正确的源mac，如果再次向改来源主机发送
          报文，会因为端口记录的源mac和报文的目的相同产生冲突，导致报文丢弃*/
        rtl8367c_setAsicReg(RTL8367C_REG_LUT_PORT6_LEARN_LIMITNO, 0);
    }
#endif
    // 使能所有网口 前面被powerdowm的不会工作
    rtl8367c_setAsicPortEnableAll(ENABLED);
    rtl8367c_setAsicPortEnableAll(ENABLED);
    rtl8367c_getAsicPortEnableAll(&data);
    //printf("Enable getAsicPortEnableAll:0x%x\n", data);

    /* ACL配置为8370核心功能 */
    retVal = rtk8367_acl_cfg();
    if( retVal != RT_ERR_OK )
    {   
        printk(KERN_ERR "RTK8367c config acl failed !!! .\n");
    }
    if(RT_ERR_OK != init_leds())
    {
        printk(KERN_ERR "init_leds failed\n");
    }
}

static int rtl_reset_open(struct inode *ino, struct file *filp)
{
    return 0;
}

static int rtl_reset_close(struct inode *ino, struct file *filp)
{
    return 0;
}

static ssize_t rtl_reset_write(struct file *filp, const char __user *buf, size_t count, loff_t *off)
{
	char c[128];
	int rc;

    memset(c, 0, sizeof(c));
	rc = copy_from_user(c, buf, sizeof(c) > count ? count : sizeof(c));
	if (rc)
		return rc;
	if (0 == strncmp("hr", c, 2))
    {
        mii_phy_reset(1);
    }
    else if(0 == strncmp("sr", c, 2))
    {
        mii_phy_reset(0);
    }

	return count;
}

static struct file_operations rtl_reset_fops =
{
    .open = rtl_reset_open,
    .release = rtl_reset_close,
    .write = rtl_reset_write,
};

/*************************************************************************************
* Function: rtk8367m_open()
* Desc: driver device ioctl interface
*
* Return: No
**************************************************************************************/
int rtk8367m_open(struct inode *inode, struct file *file)
{
    /* do nothing */
	return 0;
}

/*************************************************************************************
* Function: rtk8370m_release()
* Desc: driver device release interface
*
* Return: No
**************************************************************************************/
int rtk8367m_release(struct inode *inode, struct file *file)
{
    /* do nothing */
	return 0;
}
static long rtk8367_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	rtk_api_ret_t           retVal = RT_ERR_OK;
	int              port;
	rtk_stat_port_cntr_t    pPort_cntrs;
    rtk_port_mib_t          stPortMib;
    struct reg_param_s      param;
    void __user             *argp = (void __user*)arg;
    
    switch(cmd) {

        case RTK_PORT_GET_MIB:             
            if ( copy_from_user(&stPortMib, (rtk_port_mib_t *)arg, sizeof(rtk_port_mib_t)) )
            {
                printk("%s: line:%d retVal:0x%x port:%d sizeof:%d\n", __func__, __LINE__, retVal, stPortMib.port, sizeof(rtk_port_mib_t) );
                return -EFAULT;
            }
                       
            mutex_lock(&acl_cb->lock);
            retVal = rtk_stat_port_get(stPortMib.port,STAT_IfInOctets,&pPort_cntrs.ifInOctets);
            
            if (RT_ERR_OK == retVal)
            {                
                retVal = rtk_stat_port_get(stPortMib.port,STAT_IfOutOctets,&pPort_cntrs.ifOutOctets);
            } 
            
            //除收发报文统计 还要读取主要mib信息
            if ((stPortMib.mib_reset&0x02) != 0)
            {
                if (RT_ERR_OK == retVal)
                {
                    retVal = rtk_stat_port_get(stPortMib.port,STAT_Dot3StatsFCSErrors,&pPort_cntrs.dot3StatsFCSErrors);
                }
                
                if (RT_ERR_OK == retVal)
                {
                    retVal = rtk_stat_port_get(stPortMib.port,STAT_Dot3StatsSymbolErrors,&pPort_cntrs.dot3StatsSymbolErrors);
                }  

                if (RT_ERR_OK == retVal)
                {
                    retVal = rtk_stat_port_get(stPortMib.port,STAT_Dot3InPauseFrames,&pPort_cntrs.dot3InPauseFrames);
                }         

                if (RT_ERR_OK == retVal)
                {
                    retVal = rtk_stat_port_get(stPortMib.port,STAT_Dot3OutPauseFrames,&pPort_cntrs.dot3OutPauseFrames);
                } 
            }
            //是否清零mib
            if (stPortMib.mib_reset&0x01)
            {
                rtk_stat_port_reset(stPortMib.port);
            }
            
            if (RT_ERR_OK == retVal)
            {
                stPortMib.portcntr.ifInOctets            = (uint32)pPort_cntrs.ifInOctets;
                stPortMib.portcntr.ifOutOctets           = (uint32)pPort_cntrs.ifOutOctets;
                if ((stPortMib.mib_reset&0x02) != 0)
                {
                    stPortMib.portcntr.dot3StatsFCSErrors    = pPort_cntrs.dot3StatsFCSErrors;
                    stPortMib.portcntr.dot3StatsSymbolErrors = pPort_cntrs.dot3StatsSymbolErrors;
                    stPortMib.portcntr.dot3InPauseFrames     = pPort_cntrs.dot3InPauseFrames;            
                    stPortMib.portcntr.dot3OutPauseFrames    = pPort_cntrs.dot3OutPauseFrames;
                }
                else
                {
                    stPortMib.portcntr.dot3StatsFCSErrors    = 0;
                    stPortMib.portcntr.dot3StatsSymbolErrors = 0;
                    stPortMib.portcntr.dot3InPauseFrames     = 0;            
                    stPortMib.portcntr.dot3OutPauseFrames    = 0;
                }
                
                if ( copy_to_user(argp, (void *)&stPortMib, sizeof(rtk_port_mib_t)) )
                {
                    printk("%s: line:%d retVal:0x%x port:%d\n", __func__, __LINE__, retVal, stPortMib.port);
                }
            }
            
            mutex_unlock(&acl_cb->lock);
            break;
        case RTK_PORT_ALL_STAT_GET:
             
            if (copy_from_user(&port, (int __user *)arg, sizeof(int)))
            {
            	    return -EFAULT;
            }
            mutex_lock(&acl_cb->lock);
            
            retVal = rtk_stat_port_getAll(port, &pPort_cntrs);
            if (retVal == RT_ERR_OK)
            {
			printk("ifInOctets:%lld \ndot3StatsFCSErrors:%d \ndot3StatsSymbolErrors:%d \n"\
				   "dot3InPauseFrames:%d \ndot3ControlInUnknownOpcodes:%d \netherStatsFragments:%d \n"\
				   "etherStatsJabbers:%d \nifInUcastPkts:%d \netherStatsDropEvents:%d \netherStatsOctets:%lld \n"\
				   "etherStatsUndersizePkts:%d \netherStatsOversizePkts:%d \netherStatsPkts64Octets:%d \n"\
				   "etherStatsPkts65to127Octets:%d \netherStatsPkts128to255Octets:%d \netherStatsPkts256to511Octets:%d \n"\
				   "etherStatsPkts512to1023Octets:%d \netherStatsPkts1024toMaxOctets:%d \n"\
				   "etherStatsMcastPkts:%d \netherStatsBcastPkts:%d \nifOutOctets:%lld \ndot3StatsSingleCollisionFrames:%d \n"\
				   "dot3StatsMultipleCollisionFrames:%d \ndot3StatsDeferredTransmissions:%d \n"\
				   "dot3StatsLateCollisions:%d \netherStatsCollisions:%d \ndot3StatsExcessiveCollisions:%d \n"\
				   "dot3OutPauseFrames:%d \ndot1dBasePortDelayExceededDiscards:%d \ndot1dTpPortInDiscards:%d \n"\
				   "ifOutUcastPkts:%d \nifOutMulticastPkts:%d \nifOutBrocastPkts:%d \noutOampduPkts:%d \n"\
				   "inOampduPkts:%d \npktgenPkts:%d\n\n",
				   pPort_cntrs.ifInOctets, pPort_cntrs.dot3StatsFCSErrors,
				   pPort_cntrs.dot3StatsSymbolErrors, pPort_cntrs.dot3InPauseFrames,
				   pPort_cntrs.dot3ControlInUnknownOpcodes, pPort_cntrs.etherStatsFragments,
				   pPort_cntrs.etherStatsJabbers, pPort_cntrs.ifInUcastPkts,
				   pPort_cntrs.etherStatsDropEvents, pPort_cntrs.etherStatsOctets,
  	               pPort_cntrs.etherStatsUndersizePkts, pPort_cntrs.etherStatsOversizePkts,
				   pPort_cntrs.etherStatsPkts64Octets, pPort_cntrs.etherStatsPkts65to127Octets,
				   pPort_cntrs.etherStatsPkts128to255Octets, pPort_cntrs.etherStatsPkts256to511Octets,
				   pPort_cntrs.etherStatsPkts512to1023Octets, pPort_cntrs.etherStatsPkts1024toMaxOctets,
				   pPort_cntrs.etherStatsMcastPkts, pPort_cntrs.etherStatsBcastPkts,	
                   pPort_cntrs.ifOutOctets, pPort_cntrs.dot3StatsSingleCollisionFrames,
				   pPort_cntrs.dot3StatsMultipleCollisionFrames, pPort_cntrs.dot3StatsDeferredTransmissions,
				   pPort_cntrs.dot3StatsLateCollisions, pPort_cntrs.etherStatsCollisions,
				   pPort_cntrs.dot3StatsExcessiveCollisions, pPort_cntrs.dot3OutPauseFrames,
				   pPort_cntrs.dot1dBasePortDelayExceededDiscards, pPort_cntrs.dot1dTpPortInDiscards,
				   pPort_cntrs.ifOutUcastPkts, pPort_cntrs.ifOutMulticastPkts,
				   pPort_cntrs.ifOutBrocastPkts, pPort_cntrs.outOampduPkts,
				   pPort_cntrs.inOampduPkts, pPort_cntrs.pktgenPkts);
			/* 清零mib统计 */	   
            retVal = rtk_stat_port_reset(port);
            
            mutex_unlock(&acl_cb->lock);
            }
            else
            {
            	    printk("rtk port %d all stat get return %d\n", port, retVal);
            }		
            break;
            case RTK_READ_REGISTER:
    		   /* copy  data from user */
                if( copy_from_user(&param, (struct reg_param_s __user *)arg, 
                               sizeof(struct reg_param_s)) )
                {
                    printk(KERN_ERR "%s: copy param from user failed.\n", __func__);
                    return -EFAULT;
                }
                mutex_lock(&acl_cb->lock);
                /* read register */
                simple_phy_read(param.reg, &param.value);
                printk(KERN_ERR "%s: reg=%x value=%x\n", __func__,param.reg, param.value);
                mutex_unlock(&acl_cb->lock);
                /*copy data to user */
                if( copy_to_user((struct reg_param_s __user *)arg, &param, 
                	              sizeof(struct reg_param_s)))
                {
                    printk(KERN_ERR "%s: copy param to user failed.\n", __func__);
                    return -EFAULT;
                }
            break;
            
            case RTK_WRITE_REGISTER:
    		   /* copy  data from user */
                if( copy_from_user(&param, (struct reg_param_s __user *)arg, 
                	               sizeof(struct reg_param_s)) )
                {
                       printk(KERN_ERR "%s: copy param from user failed.\n", __func__);
                    return -EFAULT;
                }

                /* write register */
                mutex_lock(&acl_cb->lock);
                printk(KERN_ERR "%s: reg=%x value=%x\n", __func__,param.reg, param.value);
                simple_phy_write(param.reg, param.value);
                mutex_unlock(&acl_cb->lock);		
                break;     
            default:
                return -ENOIOCTLCMD;
        }
        
        if (retVal != RT_ERR_OK)
        {
            printk(KERN_ERR "%s: failed:0x%x req:0x%x\n", __func__, retVal, cmd);
        }
        return retVal;

}
struct file_operations rtk8367m_fops =
{
	owner:		THIS_MODULE,
	unlocked_ioctl:		rtk8367_ioctl,
	open:		rtk8367m_open,
	release:	rtk8367m_release,
};
static int __init mii_phy_init(void)
{
    unsigned int data = 0;
    unsigned int retVal;
    u16 phy_val;
    int r = 0,res;
    int i = 0;
    
    gpiobase = (void __iomem *)SUNXI_PIO_BASE;
    
    phy_val = geth_phy_read(4, 0x130f) & 0xffff;
    printk(KERN_WARNING "phy 8367 reg 0x%x val 0x%x\n", 0x130f, phy_val);

#if defined(PRODUCT_SBCUSER)
    /*关闭板间通信的phy口*/
    for(i=0; i<=UTP_PORT4; i++)
#elif defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_UC200) || defined(PRODUCT_AG)
    for(i=UTP_PORT0; i<UTP_PORT5; i++)
    {
        rtk8367_set_phy_powerdown(i, 1);
    }
#endif
    
    rtl8367c_setAsicPortEnableAll(DISABLED);
    rtl8367c_setAsicPortEnableAll(DISABLED);
    rtl8367c_getAsicPortEnableAll(&data);
    //printf("disable getAsicPortEnableAll:0x%x\n", data);    
    
    /*after reset, switch need to delay 1 ms
        if not, SMI may send out unknown data
    */
    udelay(1000);
    
    retVal = rtk_switch_init();
    if (retVal  != RT_ERR_OK)
    {
         return retVal;
    }
    
    /* 1) 隔离接口板与外网 */
    //rtk8370_isolation_cfg();
    //rtk8370_print_isolation_cfg();
    
    test_smi_signal_and_wait();
    
    /* tom: for test */
    //smi_write(0x13a0, 0x123);
    data = geth_phy_read(0, 0x2022);
    //printf("debug,reg PHY ID value=0x%x\n", data);
    
    /* led enable */
    //init_and_enable_leds();
    
    /* force cpu 1000M mode */
    // bit13 要为0 6282 page 579 
    //MV_REG_WRITE(0x7243C, 0x00a4060f);//rnii0 ge1 1000m
    
    /* force 8370M 1000M node */
    set_ext_mode();
    
    init_ext_interface();

#if defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC1000MAIN)
    rtk_cpu_enable_set(ENABLED);
    rtk_cpu_tagPort_set(EXT_PORT0, CPU_INSERT_TO_ALL);
#endif
#if defined(PRODUCT_UC200)
    {
#if 0    
        rtk_portmask_t mbrmsk, untagmask;
        rtk_vlan_cfg_t vlan_cfg;
        
        rtk_vlan_init();

        memset(&vlan_cfg, 0, sizeof(vlan_cfg));
        vlan_cfg.mbr.bits[0] = (1 << EXT_PORT0) | 0x1;
        vlan_cfg.untag.bits[0] = 0x1;
        if(RT_ERR_OK != rtk_vlan_set(2, &vlan_cfg))
        {
            printk(KERN_ERR "%s %d\n", __FUNCTION__, __LINE__);
        }
        
        memset(&vlan_cfg, 0, sizeof(vlan_cfg));
        vlan_cfg.mbr.bits[0] = (1 << EXT_PORT0) | 0x1e;
        vlan_cfg.untag.bits[0] = 0x1e;
        if(RT_ERR_OK != rtk_vlan_set(1, &vlan_cfg))
        {
            printk(KERN_ERR "%s %d\n", __FUNCTION__, __LINE__);
        }

        rtk_vlan_portPvid_set(0, 2, 0);
        rtk_vlan_portPvid_set(1, 1, 0);
        rtk_vlan_portPvid_set(2, 1, 0);
        rtk_vlan_portPvid_set(3, 1, 0);
        rtk_vlan_portPvid_set(4, 1, 0);
        //rtl8367c_setAsicVlanPortBasedVID(0, 1, 0);
        //rtl8367c_setAsicVlanPortBasedVID(1, 2, 0);

        //rtk_vlan_portAcceptFrameType_set(0, ACCEPT_FRAME_TYPE_UNTAG_ONLY);
        //rtk_vlan_portAcceptFrameType_set(1, ACCEPT_FRAME_TYPE_UNTAG_ONLY);
        rtk_vlan_portAcceptFrameType_set(EXT_PORT0, ACCEPT_FRAME_TYPE_TAG_ONLY);

        rtk_vlan_tagMode_set(EXT_PORT0, VLAN_TAG_MODE_ORIGINAL);
#else
		ra_sw_probe();
#endif

        /*禁止RMII口的mac地址学习功能。如果开启了rmii口的学习功能，当不同的vlan
          接口设置成桥模式时，一个vlan中的广播从rmii发送出来后，会被linux的桥
          又发回去，这时rmii口会学习到不正确的源mac，如果再次向改来源主机发送
          报文，会因为端口记录的源mac和报文的目的相同产生冲突，导致报文丢弃*/
        rtl8367c_setAsicReg(RTL8367C_REG_LUT_PORT6_LEARN_LIMITNO, 0);
    }
#endif
    // 使能所有网口 前面被powerdowm的不会工作
    rtl8367c_setAsicPortEnableAll(ENABLED);
    rtl8367c_setAsicPortEnableAll(ENABLED);
    rtl8367c_getAsicPortEnableAll(&data);
    //printf("Enable getAsicPortEnableAll:0x%x\n", data);

    /* ACL配置为8370核心功能 */
    retVal = rtk8367_acl_cfg();
    if( retVal != RT_ERR_OK )
    {	
    	printk(KERN_ERR "RTK8367c config acl failed !!! .\n");
    }

#if defined(PRODUCT_UC200) || defined(PRODUCT_AG)
    for(i=UTP_PORT0; i<UTP_PORT5; i++)
    {
        rtk8367_set_phy_powerdown(i, 0);
    }
#endif

    if(RT_ERR_OK != init_leds())
    {
        printk(KERN_ERR "init_leds failed\n");
    }

    //proc_create("gpio_test", 0, NULL, &gpio_test_fops);
    proc_create("rtl_reset", 0, NULL, &rtl_reset_fops);
	r = register_chrdev(rtk8367m_major, RTL8367M_DEVNAME,
		&rtk8367m_fops);
    if (r < 0) {
        printk(KERN_ERR NAME ": unable to register character device\n");
        return r;
    }
    

        /* 创建节点 */
    rtk_dev_class = class_create(THIS_MODULE, "rtk8367");
    
    if (IS_ERR(rtk_dev_class))
    {
        printk("ERROR: Create Dev Class Fail.\r\n");
        return -1;
    }
    rtk_device = device_create(rtk_dev_class, NULL, MKDEV(rtk8367m_major, 0), NULL, "rtk8367");
    if(IS_ERR(rtk_device))
    {
        printk("ERROR: Create Device Fail.\r\n");
        return -1;
    }

    return 0;
}

static void __exit mii_phy_exit(void)
{

}

module_init(mii_phy_init);
module_exit(mii_phy_exit);
MODULE_LICENSE("Dual BSD/GPL");


