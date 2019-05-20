/*******************************************************************************
 * Copyright 漏 2012-2014, Shuge
 *		Author: Sugar <shugeLinux@gmail.com>
 *
 * This file is provided under a dual BSD/GPL license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 ********************************************************************************/
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
#include <linux/scatterlist.h>
#include <linux/types.h>


#include <linux/clk/sunxi_name.h>
#include <linux/regulator/consumer.h>

#include <mach/sys_config.h>
#include <mach/gpio.h>
#include <mach/sunxi-chip.h>

#include "sunxi_geth.h"
#include "sunxi_geth_status.h"

#include <linux/ethtool.h>
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
#include <net/ip.h>
#include <linux/kthread.h>
#endif

extern int  geth_getPortStatus(int port_phy, u32 *pSpeed, u32 *plink, u32 *pduplex, u32 *pnway);
extern int geth_port_phyAutoNegoAbility_get(int port, u32 *mask);
extern int geth_phy_status_get(int port, u32 *port_status);
extern int geth_link_status_get(int port, int *link_status);
#undef CONFIG_GETH_CLK_SYS

extern rwlock_t mr_lock_vid;

extern  u16 GTH0_vid;
extern  u16 GTH1_vid;
char GTH0_vid1,GTH0_vid2,GTH1_vid1,GTH1_vid2;


#ifndef GMAC_CLK
#define GMAC_CLK "gmac"
#endif

#ifndef EPHY_CLK
#define EPHY_CLK "ephy"
#endif

#define DMA_DESC_RX	256
#define DMA_DESC_TX	256
#define BUDGET		(dma_desc_rx/4)
#define TX_THRESH	(dma_desc_tx/4)

#define HASH_TABLE_SIZE	64
#define MAX_BUF_SZ	(SZ_2K - 1)

#if (!defined(PRODUCT_SBCUSER)) && (!defined(PRODUCT_SBC300USER)) && \
    (!defined(PRODUCT_SBC1000USER))
#if defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC1000MAIN)
#define MAX_NET_DEVICE  6
/*逻辑接口指的实际上是接口的初始化顺序，而不是iface的名称*/
static int eth_map_phy2logic[MAX_NET_DEVICE] = {0, 1, 2, 3, 4, 5};
#elif defined(PRODUCT_SBCMAIN)
#define MAX_NET_DEVICE  5
/*物理port0-netdev[eth_map_phy2logic[0]]，物理port1-netdev[eth_map_phy2logic[1]]，
  物理port3-netdev[eth_map_phy2logic[3]]和netdev[eth_map_phy2logic[3]+1]，
  其他物理port-netdev[eth_map_phy2logic[4]]*/
static int eth_map_phy2logic[MAX_NET_DEVICE] = {0, 1, -1, 2, 4};
#else
#define MAX_NET_DEVICE  3
/*物理port0-netdev[eth_map_phy2logic[0]]，物理port1-netdev[eth_map_phy2logic[1]]，
  其他物理port-netdev[eth_map_phy2logic[2]]*/
static int eth_map_phy2logic[MAX_NET_DEVICE] = {0, 1, 2};
#endif
#endif

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
#define GMAC_USE_RX_TIMER      0
#define GMAC_USE_RX_TSKLET     1			// tsklert or task tsklet 为1，task为0
#define TS_REV_TIMER_PERIOD           (60000) /*0.06ms/byte*/
#define TS_XMITTIMER_PERIOD           (500000) /*0.5ms/byte*/
#define GETH_MAX_CPU          16	// 驱动适配最大cpu数
#define GETH_INPUT_ONECE_MAX   1024	  //一次处理的接收报文个数，设置这个主要防止cpu独占和长时间处理而没有分配内存
#define GETH_INPUT_BUFFER_MAX   20000	  //一次处理的接收报文个数，设置这个主要防止cpu独占
#define GETH_INPUT_SMP        1
#define GETH_DEBUG_SPARK      1
#endif

#undef PKT_DEBUG
#undef DESC_PRINT

///-
//#define USE_PHY_8201
//#define USE_MDIO

#define circ_cnt(head, tail, size) (((head) > (tail)) ? \
					((head) - (tail)) : \
					((head) - (tail)) & ((size)-1))

#define circ_space(head, tail, size) circ_cnt((tail), ((head)+1), (size))

#define circ_inc(n, s) (((n) + 1) % (s))

#define GETH_MAC_ADDRESS "00:00:00:00:00:00"
static char *mac_str = GETH_MAC_ADDRESS;
module_param(mac_str, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mac_str, "MAC Address String.(xx:xx:xx:xx:xx:xx)");

static int rxmode = 1;
module_param(rxmode, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(rxmode, "DMA threshold control value");

static int txmode = 1;
module_param(txmode, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(txmode, "DMA threshold control value");

static int pause = 0x400;
module_param(pause, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pause, "Flow Control Pause Time");

#define TX_TIMEO	5000
static int watchdog = TX_TIMEO;
module_param(watchdog, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(watchdog, "Transmit timeout in milliseconds");

static int dma_desc_rx = DMA_DESC_RX;
module_param(dma_desc_rx, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(watchdog, "The number of receive's descriptors");

static int dma_desc_tx = DMA_DESC_TX;
module_param(dma_desc_tx, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(watchdog, "The number of transmit's descriptors");

/*
 * - 0: Flow Off
 * - 1: Rx Flow
 * - 2: Tx Flow
 * - 3: Rx & Tx Flow
 */
static int flow_ctrl = 0;
module_param(flow_ctrl, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(flow_ctrl, "Flow control [0: off, 1: rx, 2: tx, 3: both]");

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
#if GETH_DEBUG_SPARK
struct geth_extra_debug {

	/* input buffer debug */
	unsigned long buffer_error_cnt;
	unsigned long buffer_512_cnt;
	unsigned long buffer_1024_cnt;
	unsigned long buffer_2048_cnt;
	unsigned long buffer_4096_cnt;
	unsigned long buffer_more_4096_cnt;

	/*input thread debug*/
	unsigned long thread_0_cnt;
	unsigned long thread_16_cnt;
	unsigned long thread_128_cnt;
	unsigned long thread_256_cnt;
	unsigned long thread_512_cnt;
	unsigned long thread_1024_cnt;
	unsigned long thread_2048_cnt;
	unsigned long thread_4096_cnt;
	unsigned long thread_more_4096_cnt;

	/*input malloc debug*/
	unsigned long malloc_total_cnt;
	unsigned long malloc_recycle_cnt;


	/*input process debug*/ //协议栈接受处理延时
	unsigned long process_512_cnt;
	unsigned long process_1024_cnt;
	unsigned long process_2048_cnt;
	unsigned long process_4096_cnt;
	unsigned long process_more_4096_cnt;	

	unsigned long input_total_cnt;
};
#endif
#endif


struct geth_priv {
	struct dma_desc *dma_tx;
	struct sk_buff **tx_sk;
	unsigned int tx_clean;
	unsigned int tx_dirty;
	dma_addr_t dma_tx_phy;

	unsigned long buf_sz;

	struct dma_desc *dma_rx;
	struct sk_buff **rx_sk;
	unsigned int rx_clean;
	unsigned int rx_dirty;
	dma_addr_t dma_rx_phy;

	struct sk_buff_head rx_recycle;
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct sk_buff_head rx_prealloc;
#endif

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct net_device *ndev;
#endif
	struct device *dev;
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	#if GETH_INPUT_SMP
	struct napi_struct napi[16];
	#else
	struct napi_struct napi;
	#endif
#else
	struct napi_struct napi;
#endif

	struct geth_extra_stats xstats;
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct geth_extra_debug debug_info;
#endif

	struct mii_bus *mii;
	int link;
	int speed;
	int duplex;
#define INT_PHY 0
#define EXT_PHY 1
	int phy_ext;
	int phy_interface;

	void __iomem *base;
///-
//#ifndef CONFIG_GETH_SCRIPT_SYS
#if 1
	void __iomem *gpiobase;
#else
	struct pinctrl *pinctrl;
#endif
#ifndef CONFIG_GETH_CLK_SYS
	void __iomem *clkbase;
#else
	struct clk *geth_clk;
	struct clk *ephy_clk;
#endif
	void __iomem *geth_extclk;
	struct regulator **power;
	bool is_suspend;
#if (!defined(PRODUCT_SBCUSER)) && (!defined(PRODUCT_SBC300USER)) && \
    (!defined(PRODUCT_SBC1000USER))
    int irq;
#endif

	spinlock_t lock;
	spinlock_t tx_lock;
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	spinlock_t rx_lock;
	struct sk_buff_head	input_pkt_queue;
	struct sk_buff_head	output_pkt_queue;
	struct sk_buff_head	input_process_queue;
	atomic_t input_process_cnt;
	#if GMAC_USE_RX_TSKLET
	struct tasklet_struct input_tsklet;
	#else
	struct task_struct 	*input_task;
	#endif
	struct hrtimer           timer;		// 接收timer
	int			(*recv_dispatch)(struct sk_buff *skb);
	
	struct tasklet_struct xmit_tsklet;
	struct hrtimer           xmit_timer;		// xmit timer
	int   irq_recv_cnt;			//单个中断收到的报文数据，用于中断优化
#endif
};

#ifdef CONFIG_GETH_PHY_POWER
struct geth_power {
	unsigned int vol;
	const char *name;
};

struct geth_power power_tb[5] = {};
#endif

///-
static int phy_init_flag = 0;
#if (!defined(PRODUCT_SBCUSER)) && (!defined(PRODUCT_SBC300USER)) && \
    (!defined(PRODUCT_SBC1000USER))
static struct geth_priv *g_eth_priv = NULL;
static struct net_device *g_ndev[MAX_NET_DEVICE];
static u8 boardcast_mac[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
#endif


static int init_vid()
{
	 GTH0_vid1=GTH0_vid&0xff;
	 GTH0_vid2=(GTH0_vid>>8)&0x0f;
	 //printk(KERN_ALERT "GTH0_vid1==%d\tGTH0_vid2==%d\n",GTH0_vid1,GTH0_vid2);

	 GTH1_vid1=GTH1_vid&0xff;
	 GTH1_vid2=(GTH1_vid>>8)&0x0f;
	 //printk(KERN_ALERT "GTH1_vid1==%d\tGTH1_vid2==%d\n",GTH1_vid1,GTH1_vid2);
	 return 0;
}

extern int mii_phy_init(struct net_device *ndev);

void sunxi_udelay(int n)
{
	udelay(n);
}

static int geth_stop(struct net_device *ndev);
static int geth_open(struct net_device *ndev);
static void geth_tx_complete(struct geth_priv *priv);
static void geth_rx_refill(struct net_device *ndev);
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
static int geth_rx(struct geth_priv *priv, int limit);
static int geth_input_mm(struct geth_priv *priv);
static int geth_input_thread(void *data);
static void wakeup_inputd(struct geth_priv *priv);
static int geth_free_input_mm(struct geth_priv *priv);



static int rx_tsk_cnt = 0;
static int rx_int_cnt = 0;
static int rx_timer_cnt = 0;
static int rx_intterput_cnt = 0;

static int rx_buffer_error_cnt = 0;
static struct dma_desc debug_desc[DMA_DESC_RX];
#endif

///-
static int geth_debug_print_on = 0;
static int icmp_seq = -1;

#ifdef DEBUG
static void desc_print(struct dma_desc *desc, int size)
{
#ifdef DESC_PRINT
	int i;
	for (i = 0; i < size; i++) {
		struct dma_desc *x = (desc + i);
		pr_info("\t%d [0x%08x]: %08x %08x %08x %08x\n",
		       i, (unsigned int)(&desc[i]),
		       x->desc[0], x->desc[1], x->desc[2], x->desc[3]);
	}
	pr_info("\n");
#endif
}
#endif

static int geth_power_on(struct geth_priv *priv)
{
	int value;
#ifdef CONFIG_GETH_PHY_POWER
	struct regulator **regu;
	int ret = 0, i = 0;

	regu = kmalloc(ARRAY_SIZE(power_tb) *
			sizeof(struct regulator *), GFP_KERNEL);
	if (!regu)
		return -1;

	/* Set the voltage */
	for (i = 0; i < ARRAY_SIZE(power_tb) && power_tb[i].name; i++) {
		regu[i] = regulator_get(NULL, power_tb[i].name);
		if (IS_ERR(regu[i])) {
			ret = -1;
			goto err;
		}

		if (power_tb[i].vol != 0) {
			ret = regulator_set_voltage(regu[i], power_tb[i].vol,
					power_tb[i].vol);
			if (ret)
				goto err;
		}

		ret = regulator_enable(regu[i]);
		if (ret) {
			goto err;
		}
		mdelay(3);
	}

	msleep(300);
	priv->power = regu;
#endif

	value = readl(priv->geth_extclk + GETH_CLK_REG);
	if (priv->phy_ext == INT_PHY) {
		value |= (1 << 15);
		value &= ~(1 << 16);
		value |= (3 << 17);
	} else {
		value &= ~(1 << 15);
		value |= (1 << 16);
	}
	writel(value, priv->geth_extclk + GETH_CLK_REG);

	return 0;

#ifdef CONFIG_GETH_PHY_POWER
err:
	for(; i > 0; i--) {
		regulator_disable(regu[i - 1]);
		regulator_put(regu[i - 1]);
	}
	kfree(regu);
	priv->power = NULL;
	return ret;
#endif
}

static void geth_power_off(struct geth_priv *priv)
{
	int value;
#ifdef CONFIG_GETH_PHY_POWER
	struct regulator **regu = priv->power;
	int i = 0;

	if (priv->power == NULL)
		return;

	for (i = 0; i < ARRAY_SIZE(power_tb) && power_tb[i].name; i++) {
		regulator_disable(regu[i]);
		regulator_put(regu[i]);
	}
	kfree(regu);
#endif

	if (priv->phy_ext == INT_PHY) {
		value = readl(priv->geth_extclk + GETH_CLK_REG);
		value |= (1 << 16);
		writel(value, priv->geth_extclk + GETH_CLK_REG);
	}
}

/*
 * PHY interface operations
 */
static int geth_mdio_read(struct mii_bus *bus, int phyaddr, int phyreg)
{
	struct net_device *ndev = bus->priv;
	struct geth_priv *priv = netdev_priv(ndev);

	return (int)sunxi_mdio_read(priv->base,  phyaddr, phyreg);
}

static int geth_mdio_write(struct mii_bus *bus, int phyaddr,
				int phyreg, u16 data)
{
	struct net_device *ndev = bus->priv;
	struct geth_priv *priv = netdev_priv(ndev);

	sunxi_mdio_write(priv->base, phyaddr, phyreg, data);

	return 0;
}

static int geth_mdio_reset(struct mii_bus *bus)
{
	struct net_device *ndev = bus->priv;
	struct geth_priv *priv = netdev_priv(ndev);

	return sunxi_mdio_reset(priv->base);
}

static void geth_adjust_link(struct net_device *ndev)
{
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct geth_priv *priv = netdev_priv(ndev);
#else
	struct geth_priv *priv = g_eth_priv;
#endif
	struct phy_device *phydev = ndev->phydev;
	unsigned long flags;
	int new_state = 0;

	if (phydev == NULL)
		return;

	spin_lock_irqsave(&priv->lock, flags);
	if (phydev->link) {
		/* Now we make sure that we can be in full duplex mode.
		 * If not, we operate in half-duplex mode. */
		if (phydev->duplex != priv->duplex) {
			new_state = 1;
			priv->duplex = phydev->duplex;
		}
		/* Flow Control operation */
		if (phydev->pause)
			sunxi_flow_ctrl(priv->base, phydev->duplex,
						 flow_ctrl, pause);

		if (phydev->speed != priv->speed) {
			new_state = 1;
			priv->speed = phydev->speed;
		}


		if (priv->link == 0) {
			new_state = 1;
			priv->link = phydev->link;
		}

		/* Fix the A version chip mode, it not work at 1000M mode */
		if (sunxi_get_soc_ver() == SUN9IW1P1_REV_A
				&& priv->speed == SPEED_1000
				&& phydev->link == 1){
			priv->speed = 0;
			priv->link = 0;
			priv->duplex = -1;
			phydev->speed = SPEED_100;
			phydev->autoneg = AUTONEG_DISABLE;
			phydev->advertising &= ~ADVERTISED_Autoneg;
			phydev->state = PHY_UP;
			new_state = 0;
		}

		if (new_state)
			sunxi_set_link_mode(priv->base, priv->duplex, priv->speed);

#ifdef LOOPBACK_DEBUG
		phydev->state = PHY_FORCING;
#endif

	} else if (priv->link != phydev->link) {
		new_state = 1;
		priv->link = 0;
		priv->speed = 0;
		priv->duplex = -1;
	}

	if (new_state)
		phy_print_status(phydev);

	spin_unlock_irqrestore(&priv->lock, flags);
}

#if 0
static int geth_phy_init(struct net_device *ndev)
{
#ifdef USE_PHY_8201
	int value;
	struct mii_bus *new_bus;
	struct geth_priv *priv = netdev_priv(ndev);
	struct phy_device *phydev = NULL;

	new_bus = mdiobus_alloc();
	if (new_bus == NULL) {
		netdev_err(ndev, "Failed to alloc new mdio bus\n");
		return -ENOMEM;
	}

	new_bus->name = dev_name(priv->dev);
	new_bus->read = &geth_mdio_read;
	new_bus->write = &geth_mdio_write;
	new_bus->reset = &geth_mdio_reset;
	snprintf(new_bus->id, MII_BUS_ID_SIZE, "%s-%x",
		new_bus->name, 0);

	new_bus->parent = priv->dev;
	new_bus->priv = ndev;

	if (mdiobus_register(new_bus)) {
		printk(KERN_ERR "%s: Cannot register as MDIO bus\n", new_bus->name);
		goto reg_fail;
	}

	priv->mii = new_bus;

	phydev = phy_find_first(new_bus);
	if (!phydev) {
		netdev_err(ndev, "No PHY found!\n");
		goto err;
	}

	phydev->irq = PHY_POLL;

	phydev = phy_connect(ndev, dev_name(&phydev->dev), &geth_adjust_link,
			0, priv->phy_interface);
	if (IS_ERR(phydev)) {
		netdev_err(ndev, "Could not attach to PHY\n");
		goto err;
	} else {
		netdev_info(ndev, "%s: PHY ID %08x at %d IRQ %s (%s)\n",
				ndev->name, phydev->phy_id, phydev->addr,
				"poll", dev_name(&phydev->dev));
#if defined(CONFIG_ARCH_SUN8IW8) || defined(CONFIG_ARCH_SUN8IW7)
		if (priv->phy_ext == INT_PHY) {
			phy_write(phydev, 0x1f, 0x013d);
			phy_write(phydev, 0x10, 0x3ffe);
			phy_write(phydev, 0x1f, 0x063d);
			phy_write(phydev, 0x13, 0x8000);
			phy_write(phydev, 0x1f, 0x023d);
			phy_write(phydev, 0x18, 0x1000);
			phy_write(phydev, 0x1f, 0x063d);
			phy_write(phydev, 0x15, 0x132c);
			phy_write(phydev, 0x1f, 0x013d);
			phy_write(phydev, 0x13, 0xd602);
			phy_write(phydev, 0x17, 0x003b);
			phy_write(phydev, 0x1f, 0x063d);
			phy_write(phydev, 0x14, 0x7088);
			phy_write(phydev, 0x1f, 0x033d);
			phy_write(phydev, 0x11, 0x8530);
			phy_write(phydev, 0x1f, 0x003d);
		}

#endif
		phy_write(phydev, MII_BMCR, BMCR_RESET);
		while (BMCR_RESET & phy_read(phydev, MII_BMCR))
			msleep(30);

		value = phy_read(phydev, MII_BMCR);
		phy_write(phydev, MII_BMCR, (value & ~BMCR_PDOWN));

	}

	phydev->supported &= PHY_GBIT_FEATURES;
	phydev->advertising = phydev->supported;


	return 0;

err:
	mdiobus_unregister(new_bus);
reg_fail:
	mdiobus_free(new_bus);

	return -EINVAL;
#else
    struct geth_priv *priv = g_eth_priv;
///-
    /* init phy */
    if(0 == phy_init_flag)
    {
        phy_init_flag = 1;
        mii_phy_init(priv->gpiobase);
        msleep(2000);
    }
    
    return 0;
#endif
}
#endif

static int geth_phy_release(struct net_device *ndev)
{
///-
#ifdef USE_PHY_8201

	struct geth_priv *priv = netdev_priv(ndev);
	struct phy_device *phydev = ndev->phydev;
	int value = 0;

	/* Stop and disconnect the PHY */
	if (phydev) {
		phy_stop(phydev);
		value = phy_read(phydev, MII_BMCR);
		phy_write(phydev, MII_BMCR, (value | BMCR_PDOWN));
		phy_disconnect(phydev);
		ndev->phydev = NULL;
	}

	if (priv->mii) {
		mdiobus_unregister(priv->mii);
		priv->mii->priv = NULL;
		mdiobus_free(priv->mii);
		priv->mii = NULL;
	}
	priv->link = PHY_DOWN;
	priv->speed = 0;
	priv->duplex = -1;
#endif

	return 0;
}


/*****************************************************************************
 *
 *
 ****************************************************************************/
static void geth_rx_refill(struct net_device *ndev)
{
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct geth_priv *priv = netdev_priv(ndev);
#else
	struct geth_priv *priv = g_eth_priv;
#endif
	struct dma_desc *desc;
	struct sk_buff *sk = NULL;
	dma_addr_t paddr;

	while (circ_space(priv->rx_clean, priv->rx_dirty, dma_desc_rx) > 0) {
		int entry = priv->rx_clean;

		/* Find the dirty's desc and clean it */
		desc = priv->dma_rx + entry;

		if (priv->rx_sk[entry] == NULL) {
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
			/*  skb_buffer from pre alloc  queue */
			sk = skb_dequeue(&priv->rx_prealloc);
#else
			/* reclaim the skb_buffer from transmit queue */
			sk = skb_dequeue(&priv->rx_recycle);

			if (sk == NULL)
				sk = netdev_alloc_skb_ip_align(ndev, priv->buf_sz);
#endif

			if (unlikely(sk == NULL))
			{
				// 异常log
				printk(KERN_WARNING "geth_rx_refill err\n");
				break;
			}

			priv->rx_sk[entry] = sk;
			paddr = dma_map_single(priv->dev, sk->data,
					priv->buf_sz, DMA_FROM_DEVICE);
			desc_buf_set(desc, paddr, priv->buf_sz);

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
			desc_buf_set(&debug_desc[entry],paddr,priv->buf_sz);
#endif
		}

#if (!defined(PRODUCT_SBCUSER)) && (!defined(PRODUCT_SBC300USER)) && \
    (!defined(PRODUCT_SBC1000USER))
		wmb();
#endif
		desc_set_own(desc);
		priv->rx_clean = circ_inc(priv->rx_clean, dma_desc_rx);
	}
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	wmb();
#endif
}

/*
 * geth_dma_desc_init - initialize the RX/TX descriptor list
 * @ndev: net device structure
 * Description: initialize the list for dma.
 */
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
static int geth_dma_desc_init(struct net_device *ndev)
#else
static int geth_dma_desc_init(void)
#endif
{
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct geth_priv *priv = netdev_priv(ndev);
#else
	struct geth_priv *priv = g_eth_priv;
#endif
	unsigned int buf_sz;

	priv->rx_sk = kzalloc(sizeof(struct sk_buff*) * dma_desc_rx,
				GFP_KERNEL);
	if (!priv->rx_sk)
		return -ENOMEM;

	priv->tx_sk = kzalloc(sizeof(struct sk_buff*) * dma_desc_tx,
				GFP_KERNEL);
	if (!priv->tx_sk)
		goto tx_sk_err;

	/* Set the size of buffer depend on the MTU & max buf size */
	buf_sz = MAX_BUF_SZ;

	priv->dma_tx = dma_alloc_coherent(priv->dev,
					dma_desc_tx *
					sizeof(struct dma_desc),
					&priv->dma_tx_phy,
					GFP_KERNEL);
	if (!priv->dma_tx)
		goto dma_tx_err;

	priv->dma_rx = dma_alloc_coherent(priv->dev,
					dma_desc_rx *
					sizeof(struct dma_desc),
					&priv->dma_rx_phy,
					GFP_KERNEL);
	if (!priv->dma_rx)
		goto dma_rx_err;

	priv->buf_sz = buf_sz;

	return 0;

dma_rx_err:
	dma_free_coherent(priv->dev, dma_desc_rx * sizeof(struct dma_desc),
			priv->dma_tx, priv->dma_tx_phy);
dma_tx_err:
	kfree(priv->tx_sk);
tx_sk_err:
	kfree(priv->rx_sk);

	return -ENOMEM;

}

static void geth_free_rx_sk(struct geth_priv *priv)
{
	int i;

	for (i = 0; i < dma_desc_rx; i++) {
		if (priv->rx_sk[i] != NULL) {
			struct dma_desc *desc = priv->dma_rx + i;
			dma_unmap_single(priv->dev, desc_buf_get_addr(desc),
					 desc_buf_get_len(desc),
					 DMA_FROM_DEVICE);
			dev_kfree_skb_any(priv->rx_sk[i]);
			priv->rx_sk[i] = NULL;
		}
	}
}

static void geth_free_tx_sk(struct geth_priv *priv)
{
	int i;

	for (i = 0; i < dma_desc_tx; i++) {
		if (priv->tx_sk[i] != NULL) {
			struct dma_desc *desc = priv->dma_tx + i;
			if (desc_buf_get_addr(desc))
				dma_unmap_single(priv->dev, desc_buf_get_addr(desc),
						 desc_buf_get_len(desc),
						 DMA_TO_DEVICE);
			dev_kfree_skb_any(priv->tx_sk[i]);
			priv->tx_sk[i] = NULL;
		}
	}
}

static void geth_free_dma_desc(struct geth_priv *priv)
{
	/* Free the region of consistent memory previously allocated for
	 * the DMA */
	dma_free_coherent(NULL, dma_desc_tx * sizeof(struct dma_desc),
			  priv->dma_tx, priv->dma_tx_phy);
	dma_free_coherent(NULL, dma_desc_rx * sizeof(struct dma_desc),
			  priv->dma_rx, priv->dma_rx_phy);

	kfree(priv->rx_sk);
	kfree(priv->tx_sk);
}


/*****************************************************************************
 *
 *
 ****************************************************************************/
#ifdef CONFIG_PM
static int geth_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct geth_priv *priv = netdev_priv(ndev);
#else
	struct geth_priv *priv = g_eth_priv;
#endif

	if (!ndev || !netif_running(ndev))
		return 0;

	priv->is_suspend = true;

	spin_lock(&priv->lock);
	netif_device_detach(ndev);
	spin_unlock(&priv->lock);

	geth_stop(ndev);

	device_enable_async_suspend(dev);

	return 0;
}

static int geth_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct geth_priv *priv = netdev_priv(ndev);
#else
	struct geth_priv *priv = g_eth_priv;
#endif
	int ret;

	if (!netif_running(ndev))
		return 0;

	spin_lock(&priv->lock);
	netif_device_attach(ndev);
	spin_unlock(&priv->lock);

	ret = geth_open(ndev);
	priv->is_suspend = false;

	device_disable_async_suspend(dev);

	return ret;
}

static int geth_freeze(struct device *dev)
{
	return 0;
}

static int geth_restore(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops geth_pm_ops = {
	.suspend = geth_suspend,
	.resume = geth_resume,
	.freeze = geth_freeze,
	.restore = geth_restore,
};
#else
static const struct dev_pm_ops geth_pm_ops;
#endif /* CONFIG_PM */


/*****************************************************************************
 *
 *
 ****************************************************************************/
extern int sunxi_get_soc_chipid(uint8_t *chip_id);
static void geth_chip_hwaddr(u8 *addr)
{
#define MD5_SIZE	16
#define CHIP_SIZE	16

	struct crypto_hash *tfm;
	struct hash_desc desc;
	struct scatterlist sg;
	u8 result[MD5_SIZE];
	u8 chipid[CHIP_SIZE];
	int i = 0;
	int ret = -1;

	memset(chipid, 0, sizeof(chipid));
	memset(result, 0, sizeof(result));

	sunxi_get_soc_chipid((u8 *)chipid);

	tfm = crypto_alloc_hash("md5", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		pr_err("Failed to alloc md5\n");
		return;
	}
	desc.tfm = tfm;
	desc.flags = 0;

	ret = crypto_hash_init(&desc);
	if (ret < 0) {
		pr_err("crypto_hash_init() failed\n");
		goto out;
	}

	sg_init_one(&sg, chipid, sizeof(chipid) - 1);
	ret = crypto_hash_update(&desc, &sg, sizeof(chipid) - 1);
	if (ret < 0) {
		pr_err("crypto_hash_update() failed for id\n");
		goto out;
	}

	crypto_hash_final(&desc, result);
	if (ret < 0) {
		pr_err("crypto_hash_final() failed for result\n");
		goto out;
	}

	/* Choose md5 result's [0][2][4][6][8][10] byte as mac address */
	for (i = 0; i < ETH_ALEN; i++) {
		addr[i] = result[2*i];
	}
	addr[0] &= 0xfe;     /* clear multicast bit */
	addr[0] |= 0x02;     /* set local assignment bit (IEEE802) */

out:
	crypto_free_hash(tfm);
}

static void geth_check_addr(struct net_device *ndev, unsigned char *mac)
{
	int i;
	char *p = mac;

	if (!is_valid_ether_addr(ndev->dev_addr)) {
		for (i=0; i<ETH_ALEN; i++, p++)
			ndev->dev_addr[i] = simple_strtoul(p, &p, 16);

		if (!is_valid_ether_addr(ndev->dev_addr)) {
			geth_chip_hwaddr(ndev->dev_addr);
		}

		if (!is_valid_ether_addr(ndev->dev_addr)) {
			random_ether_addr(ndev->dev_addr);
			printk(KERN_WARNING "%s: Use random mac address\n", ndev->name);
		}
	}
}

static void geth_clk_enable(struct geth_priv *priv)
{
	int phy_interface = 0;
	u32 clk_value;
#ifndef CONFIG_GETH_CLK_SYS
	int value;

	value = readl(priv->clkbase + AHB1_GATING);
	value |= GETH_AHB_BIT;
	writel(value, priv->clkbase + AHB1_GATING);

	value = readl(priv->clkbase + AHB1_MOD_RESET);
	value |= GETH_RESET_BIT;
	writel(value, priv->clkbase + AHB1_MOD_RESET);

    ///-
    value = readl(priv->clkbase + 0x5C);
    printk(KERN_ERR "%s %d: AHB2:0x%x\n", __FUNCTION__, __LINE__, value);
#else
	if (priv->phy_ext == INT_PHY)
		clk_prepare_enable(priv->ephy_clk);
	clk_prepare_enable(priv->geth_clk);
#endif

	phy_interface = priv->phy_interface;

	clk_value = readl(priv->geth_extclk + GETH_CLK_REG);
	if (phy_interface == PHY_INTERFACE_MODE_RGMII)
		clk_value |= 0x00000004;
	else
		clk_value &= (~0x00000004);

	clk_value &= (~0x00000003);
	if (phy_interface == PHY_INTERFACE_MODE_RGMII
			|| phy_interface == PHY_INTERFACE_MODE_GMII)
		clk_value |= 0x00000002;
	writel(clk_value, priv->geth_extclk + GETH_CLK_REG);
///-
	clk_value = readl(priv->geth_extclk + GETH_CLK_REG);
    printk(KERN_WARNING "%s %d:clk_value:0x%x\n", __FUNCTION__, __LINE__, clk_value);
}

static void geth_clk_disable(struct geth_priv *priv)
{
#ifndef CONFIG_GETH_CLK_SYS
	int value;

	value = readl(priv->clkbase + AHB1_GATING);
	value &= ~GETH_AHB_BIT;
	writel(value, priv->clkbase + AHB1_GATING);

	value = readl(priv->clkbase + AHB1_MOD_RESET);
	value &= ~GETH_RESET_BIT;
	writel(value, priv->clkbase + AHB1_MOD_RESET);
#else
	if (priv->phy_ext == INT_PHY)
		clk_disable_unprepare(priv->ephy_clk);
	clk_disable_unprepare(priv->geth_clk);
#endif
}

static void geth_tx_err(struct geth_priv *priv)
{
#if (!defined(PRODUCT_SBCUSER)) && (!defined(PRODUCT_SBC300USER)) && \
    (!defined(PRODUCT_SBC1000USER))
    int i;
    
    for(i=0; i<MAX_NET_DEVICE; i++)
    {
    	netif_stop_queue(g_ndev[i]);
    }
#else
	netif_stop_queue(priv->ndev);
#endif

	sunxi_stop_tx(priv->base);

	geth_free_tx_sk(priv);
	memset(priv->dma_tx, 0, dma_desc_tx * sizeof(struct dma_desc));
	desc_init_chain(priv->dma_tx, priv->dma_tx_phy, dma_desc_tx);
	priv->tx_dirty = 0;
	priv->tx_clean = 0;
	sunxi_start_tx(priv->base, priv->dma_tx_phy);

#if (!defined(PRODUCT_SBCUSER)) && (!defined(PRODUCT_SBC300USER)) && \
    (!defined(PRODUCT_SBC1000USER))
	g_ndev[0]->stats.tx_errors++;
    for(i=0; i<MAX_NET_DEVICE; i++)
    {
    	netif_wake_queue(g_ndev[i]);
    }
#else
	priv->ndev->stats.tx_errors++;
	netif_wake_queue(priv->ndev);
#endif
}

#if (!defined(PRODUCT_SBCUSER)) && (!defined(PRODUCT_SBC300USER)) && \
    (!defined(PRODUCT_SBC1000USER))
static inline void geth_schedule(struct geth_priv *priv)
{
	sunxi_int_disable(priv->base);
	if(likely(napi_schedule_prep(&priv->napi)))
		__napi_schedule(&priv->napi);
}
#endif

static irqreturn_t geth_interrupt(int irq, void *dev_id)
{
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct net_device *ndev = (struct net_device *)dev_id;
	struct geth_priv *priv = netdev_priv(ndev);
#else
	struct geth_priv *priv = g_eth_priv;
#endif
	int status;

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	if (unlikely(!ndev)) {
#else
	if (unlikely(!g_ndev[0])) {
#endif
		pr_err("%s: invalid ndev pointer\n", __func__);
		return IRQ_NONE;
	}

	status = sunxi_int_status(priv->base, (void *)(&priv->xstats));

	if (likely(status == handle_tx_rx))
	{
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
		sunxi_int_disable(priv->base);
		rx_intterput_cnt++;
		//if(geth_rx(priv,DMA_DESC_RX-1) > 0)
		#if GMAC_USE_RX_TIMER
		hrtimer_start(&priv->timer, ktime_set(0,TS_REV_TIMER_PERIOD), HRTIMER_MODE_REL);
		#endif
#else
		geth_schedule(priv);
#endif
	}
	else if (unlikely(status == tx_hard_error_bump_tc)) {
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
		netdev_info(ndev, "Do nothing for bump tc\n");
#else
		netdev_info(g_ndev[0], "Do nothing for bump tc\n");
#endif
	} else if(unlikely(status == tx_hard_error)){
		geth_tx_err(priv);
	}

	return IRQ_HANDLED;
}

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
#if GMAC_USE_RX_TIMER
/**
 * geth_rcv_timer_func
 * @pdev: struct geth_priv *priv
 * Description: 接收中断可以避免网口中断过于频繁
 */
static enum hrtimer_restart geth_rcv_timer_func(struct hrtimer *timer)
{
	struct geth_priv *priv = container_of(timer, struct geth_priv, timer);
	static int run_cnt = 0;
	int cnt;
	//printk(KERN_ERR"geth_rcv_timer_func %d.\n",jiffies);
	spin_lock(&priv->input_pkt_queue.lock);
	rx_timer_cnt++;
	cnt = geth_rx(priv,DMA_DESC_RX-1);
	#if GETH_DEBUG_SPARK
	if(cnt > 0)
		priv->debug_info.input_total_cnt += cnt;
	#endif
	if(cnt == 0)
	{
		if(skb_queue_len(&priv->input_pkt_queue) > 0)
		{
			wakeup_inputd(priv);
		}
		spin_unlock(&priv->input_pkt_queue.lock);
		sunxi_int_enable(priv->base);
		run_cnt = 0;
	}
	else
	{
		spin_unlock(&priv->input_pkt_queue.lock);	
		// 降低运行频率，以达到cpu减少损耗
		if(++run_cnt > 10|| cnt < 0)
		{
			wakeup_inputd(priv);
			run_cnt = 0;
		}		
		//tasklet_hi_schedule(&priv->input_tsklet);
		hrtimer_forward_now(timer, ns_to_ktime(TS_REV_TIMER_PERIOD));
		return HRTIMER_RESTART;
	}	
	return HRTIMER_NORESTART;
	
}


/**
 * geth_rcv_timer
 * @pdev: struct geth_priv *priv
 * Description: 接收中断可以避免网口中断过于频繁
 */
static void geth_rcv_timer_init(struct geth_priv *priv)
{
	hrtimer_init(&priv->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    priv->timer.function = geth_rcv_timer_func;
    //hrtimer_start(&priv->timer, ktime_set(0,TS_REV_TIMER_PERIOD), HRTIMER_MODE_REL);
    return;
}
#endif


/**
 * geth_rcv_timer_func
 * @pdev: struct geth_priv *priv
 * Description: 接收中断可以避免网口中断过于频繁
 */
void geth_rcv_hardtimer_func(void *dev_id)
{
	struct geth_priv *priv = (struct geth_priv *)dev_id;
	static int run_cnt = 0;
	int cnt;
	//printk(KERN_ERR"geth_rcv_timer_func %d.\n",jiffies);
	//spin_lock(&priv->input_pkt_queue.lock);
	rx_timer_cnt++;
	cnt = geth_rx(priv,DMA_DESC_RX-1);
	#if GETH_DEBUG_SPARK
	if(cnt > 0)
		priv->debug_info.input_total_cnt += cnt;
	#endif
	if(cnt == 0)
	{
		if(skb_queue_len(&priv->input_pkt_queue) > 0)
		{
			wakeup_inputd(priv);
		}
		//spin_unlock(&priv->input_pkt_queue.lock);
		//sunxi_int_enable(priv->base);
		run_cnt = 0;
	}
	else
	{
		//spin_unlock(&priv->input_pkt_queue.lock);	
		// 降低运行频率，以达到cpu减少损耗
		if(++run_cnt > 10|| cnt < 0)
		{
			wakeup_inputd(priv);
			run_cnt = 0;
		}		
	}	
	return;
	
}

static void geth_xmit_cpu(void *data)
{
	struct tasklet_struct *xmit_tsklet = data;
	
	tasklet_schedule(xmit_tsklet);
}

static enum hrtimer_restart geth_xmit_timer_func(struct hrtimer *timer)
{
	struct geth_priv *priv = container_of(timer, struct geth_priv, xmit_timer);

	//printk(KERN_ERR"geth_rcv_timer_func %d.\n",jiffies);

///-
	//smp_call_function_single(1, geth_xmit_cpu,&priv->xmit_tsklet,0);
	smp_call_function_single(0, geth_xmit_cpu,&priv->xmit_tsklet,0);
	//tasklet_schedule(&priv->xmit_tsklet);

	//tasklet_hi_schedule(&priv->input_tsklet);
	hrtimer_forward_now(timer, ns_to_ktime(TS_XMITTIMER_PERIOD));
	return HRTIMER_RESTART;
}
#endif
static int geth_open(struct net_device *ndev)
{
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct geth_priv *priv = netdev_priv(ndev);
	int ret = 0;

	ret = geth_power_on(priv);
	if (ret) {
		netdev_err(ndev, "Power on is failed\n");
		ret = -EINVAL;
	}

	geth_clk_enable(priv);

///-
#if 0
	ret = geth_phy_init(ndev);
	if (ret)
		goto err;
#endif

	ret = sunxi_mac_reset((void *)priv->base, &sunxi_udelay, 10000);
	if (ret) {
		netdev_err(ndev, "Initialize hardware error\n");
		goto desc_err;
	}

	sunxi_mac_init(priv->base, txmode, rxmode);
	sunxi_set_umac(priv->base, ndev->dev_addr, 0);

	if (!priv->is_suspend) {
		skb_queue_head_init(&priv->rx_recycle);
		skb_queue_head_init(&priv->rx_prealloc);
		ret = geth_dma_desc_init(ndev);
		if (ret) {
			ret = -EINVAL;
			goto desc_err;
		}
	}

	memset(priv->dma_tx, 0, dma_desc_tx * sizeof(struct dma_desc));
	memset(priv->dma_rx, 0, dma_desc_rx * sizeof(struct dma_desc));

	desc_init_chain(priv->dma_rx, priv->dma_rx_phy, dma_desc_rx);
	desc_init_chain(priv->dma_tx, priv->dma_tx_phy, dma_desc_tx);

	priv->rx_clean = priv->rx_dirty = 0;
	priv->tx_clean = priv->tx_dirty = 0;
	geth_input_mm(priv);
	geth_rx_refill(ndev);

	/* Extra statistics */
	memset(&priv->xstats, 0, sizeof(struct geth_extra_stats));
	memset(&priv->debug_info,0,sizeof(struct geth_extra_debug));

	if (ndev->phydev)
		phy_start(ndev->phydev);

	sunxi_start_rx(priv->base, (unsigned long)((struct dma_desc *)
				priv->dma_rx_phy + priv->rx_dirty));
	sunxi_start_tx(priv->base, (unsigned long)((struct dma_desc *)
				priv->dma_tx_phy + priv->tx_clean));
	#if GETH_INPUT_SMP
	{
		int cpu;
		
		for_each_possible_cpu(cpu)
		{		
			napi_enable(&priv->napi[cpu]);
		}	
	}
	#else
	napi_enable(&priv->napi);
	#endif
	netif_start_queue(ndev);
	
	//tx timer init
	#if GMAC_USE_RX_TIMER
	geth_rcv_timer_init(priv);
	#endif

///-
	#if 0
	// xmit timer init
	hrtimer_init(&priv->xmit_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    priv->xmit_timer.function = geth_xmit_timer_func;
	hrtimer_start(&priv->xmit_timer, ktime_set(0,TS_XMITTIMER_PERIOD), HRTIMER_MODE_REL);
	#endif

	#if !GMAC_USE_RX_TSKLET
	priv->input_task= kthread_create(geth_input_thread, priv, "pkg input task");
	#endif
	dispatch_init();
	/* Enable the Rx/Tx */
	sunxi_mac_enable(priv->base);
	sunxi_timer_init(priv);

	return 0;

desc_err:
	geth_phy_release(ndev);
err:
	geth_clk_disable(priv);
	if (priv->is_suspend)
	{
		#if GETH_INPUT_SMP
		{
			int cpu;
			
			for_each_possible_cpu(cpu)
			{		
				napi_enable(&priv->napi[cpu]);
			}	
		}
		#else
		napi_enable(&priv->napi);
		#endif
	}

	return ret;
#else
	netif_start_queue(ndev);

	return 0;
#endif
}

static int geth_stop(struct net_device *ndev)
{
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct geth_priv *priv = netdev_priv(ndev);

	#if GMAC_USE_RX_TIMER
	hrtimer_cancel(&priv->timer);
	#endif
	#if 1
	hrtimer_cancel(&priv->xmit_timer);
	if (priv->xmit_tsklet.func)
		tasklet_kill(&priv->xmit_tsklet);
	
	#if GMAC_USE_RX_TSKLET
	if(priv->input_tsklet.func)
		tasklet_kill(&priv->input_tsklet);
	#else
	kthread_stop(priv->input_task);
	#endif
	
	#endif
	netif_stop_queue(ndev);
	#if GETH_INPUT_SMP
	{
		int cpu;
		
		for_each_possible_cpu(cpu)
		{		
			napi_disable(&priv->napi[cpu]);
		}	
	}
	#else
	napi_disable(&priv->napi);
	#endif

	netif_carrier_off(ndev);

	/* Release PHY resources */
	geth_phy_release(ndev);

	/* Disable Rx/Tx */
	sunxi_mac_disable(priv->base);

	geth_clk_disable(priv);
	geth_power_off(priv);

	netif_tx_lock_bh(ndev);
	/* Release the DMA TX/RX socket buffers */
	geth_free_rx_sk(priv);
	geth_free_tx_sk(priv);
	
	netif_tx_unlock_bh(ndev);

	/* Ensure that hareware have been stopped */
	if (!priv->is_suspend) {
		skb_queue_purge(&priv->rx_recycle);
		geth_free_dma_desc(priv);
		geth_free_input_mm(priv);
	}
#else
	//struct geth_priv *priv = g_eth_priv;

	netif_stop_queue(ndev);
    /*不需要调用，否则会导致重新up后，网口不通*/
	//netif_carrier_off(ndev);
    
#endif
	return 0;
}

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
static int xmit_cnt_spark = 0;
static int xmit_fail_spark = 0;
#endif

static void geth_tx_complete(struct geth_priv *priv)
{
	unsigned int entry = 0;
	struct sk_buff *skb = NULL;
	struct dma_desc *desc = NULL;
	int tx_stat;

	spin_lock(&priv->tx_lock);
	while (circ_cnt(priv->tx_dirty, priv->tx_clean, dma_desc_tx) > 0) {

		entry = priv->tx_clean;
		desc = priv->dma_tx + entry;

		/* Check if the descriptor is owned by the DMA. */
		if (desc_get_own(desc))
			break;

		/* Verify tx error by looking at the last segment */
		if (desc_get_tx_ls(desc)) {
#if (!defined(PRODUCT_SBCUSER)) && (!defined(PRODUCT_SBC300USER)) && \
    (!defined(PRODUCT_SBC1000USER))
            struct sk_buff *skb_tmp = priv->tx_sk[entry];
#endif
            
			tx_stat = desc_get_tx_status(desc, (void *)(&priv->xstats));

			if (likely(!tx_stat))
            {     
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
				priv->ndev->stats.tx_packets++;
				xmit_cnt_spark++;
#else
                if(likely(skb_tmp && skb_tmp->dev))
                {
    				skb_tmp->dev->stats.tx_packets++;
                }
#endif
            }
			else
            {    
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
				priv->ndev->stats.tx_errors++;
				xmit_fail_spark++;
#else
                if(likely(skb_tmp && skb_tmp->dev))
                {
    				skb_tmp->dev->stats.tx_errors++;
                }
#endif
            }
		}

		dma_unmap_single(priv->dev, desc_buf_get_addr(desc),
				desc_buf_get_len(desc), DMA_TO_DEVICE);

		skb = priv->tx_sk[entry];
		priv->tx_sk[entry] = NULL;
		desc_init(desc);

		/* Find next dirty desc */
		priv->tx_clean = circ_inc(entry, dma_desc_tx);

		if (unlikely(skb == NULL))
			continue;

		/*
		 * If there's room in the queue (limit it to size)
		 * we add this skb back into the pool,
		 * if it's the right size.
		 */
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
		wmb();
		
		if (skb_recycle_check(skb, priv->buf_sz))
		{
			spin_lock(&priv->rx_recycle.lock);
			__skb_queue_head(&priv->rx_recycle, skb);
			spin_unlock(&priv->rx_recycle.lock);
		}
		else
			dev_kfree_skb(skb);
#else
		if ((skb_queue_len(&priv->rx_recycle) <
			dma_desc_rx) &&
			skb_recycle_check(skb, priv->buf_sz))
        {
            /*改为使用加锁的安全函数，接收处理的时候也会进行
              队列操作，可能导致野指针*/
			//__skb_queue_head(&priv->rx_recycle, skb);
			skb_queue_head(&priv->rx_recycle, skb);
        }
		else
			dev_kfree_skb(skb);
#endif
	}

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	if (unlikely(netif_queue_stopped(priv->ndev)) &&
		circ_space(priv->tx_dirty, priv->tx_clean, dma_desc_tx) >
			TX_THRESH) {
		netif_wake_queue(priv->ndev);
	}
#else
///-
#if 0
	if (unlikely(netif_queue_stopped(napi->dev)) &&
		circ_space(priv->tx_dirty, priv->tx_clean, dma_desc_tx) >
			TX_THRESH) {
		netif_wake_queue(napi->dev);
	}
#else
    if(circ_space(priv->tx_dirty, priv->tx_clean, dma_desc_tx) >
			TX_THRESH)
    {
        int i;
        
        for(i=0; i<MAX_NET_DEVICE; i++)
        {
            if(unlikely(netif_queue_stopped(g_ndev[i])))
            {
                netif_wake_queue(g_ndev[i]);
            }
        }
    }
#endif
#endif
	spin_unlock(&priv->tx_lock);
}

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
static netdev_tx_t geth_xmit_new(struct sk_buff *skb, struct net_device *ndev)
{
	struct geth_priv  *priv = netdev_priv(ndev);
	unsigned int entry;
	struct dma_desc *desc, *first;
	unsigned int len, tmp_len = 0;
	int i, csum_insert;
	int nfrags = skb_shinfo(skb)->nr_frags;
	dma_addr_t paddr;

///-
    geth_tx_complete(priv);

	spin_lock(&priv->tx_lock);
	if (unlikely(circ_space(priv->tx_dirty, priv->tx_clean,
			dma_desc_tx) < (nfrags + 1))) {

		if (!netif_queue_stopped(ndev)) {
			netdev_err(ndev, "%s: BUG! Tx Ring full when queue awake\n", __func__);
			netif_stop_queue(ndev);
		}
		spin_unlock(&priv->tx_lock);

		return NETDEV_TX_BUSY;
	}


	csum_insert = (skb->ip_summed == CHECKSUM_PARTIAL);
	entry = priv->tx_dirty;
	first = desc = priv->dma_tx + entry;

	len = skb_headlen(skb);
	priv->tx_sk[entry] = skb;

#ifdef PKT_DEBUG
	printk("======TX PKT DATA: ============\n");
	/* dump the packet */
	print_hex_dump(KERN_DEBUG, "skb->data: ", DUMP_PREFIX_NONE,
			16, 1, skb->data, 64, true);
#endif
///-
    {
        int cpu;
        cpu = smp_processor_id();
        if(cpu != 0)
        {
            printk(KERN_ERR "======TX PKT DATA: ============ cpu:%d\n", cpu);
        }
    }

	/* Every desc max size is 2K */
	while (len != 0) {
		desc = priv->dma_tx + entry;
		tmp_len = ((len > MAX_BUF_SZ) ?  MAX_BUF_SZ : len);

		paddr = dma_map_single(priv->dev, skb->data, tmp_len, DMA_TO_DEVICE);
		if (dma_mapping_error(priv->dev, paddr)){
			dev_kfree_skb(skb);
			return -EIO;
		}
		desc_buf_set(desc, paddr, tmp_len);
		/* Don't set the first's own bit, here */
		if (first != desc) {
			priv->tx_sk[entry] = NULL;
			desc_set_own(desc);
		}

		entry = circ_inc(entry, dma_desc_tx);
		len -= tmp_len;
	}

	for (i = 0; i <nfrags; i++) {
		const skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
		len = skb_frag_size(frag);

		desc = priv->dma_tx + entry;
		paddr = skb_frag_dma_map(priv->dev, frag, 0, len, DMA_TO_DEVICE);
		if (dma_mapping_error(priv->dev, paddr)) {
			dev_kfree_skb(skb);
			return -EIO;
		}

		desc_buf_set(desc, paddr, len);
		desc_set_own(desc);
		priv->tx_sk[entry] = NULL;
		entry = circ_inc(entry, dma_desc_tx);
	}

	ndev->stats.tx_bytes += skb->len;
	priv->tx_dirty = entry;
	desc_tx_close(first, desc, csum_insert);

	desc_set_own(first);
	spin_unlock(&priv->tx_lock);

	if (circ_space(priv->tx_dirty, priv->tx_clean, dma_desc_tx) <=
			(MAX_SKB_FRAGS + 1)) {
		netif_stop_queue(ndev);
		if (circ_space(priv->tx_dirty, priv->tx_clean, dma_desc_tx) >
				TX_THRESH)
			netif_wake_queue(ndev);
	}

#ifdef DEBUG
	printk("=======TX Descriptor DMA: 0x%08x\n", priv->dma_tx_phy);
	printk("Tx pointor: dirty: %d, clean: %d\n", priv->tx_dirty, priv->tx_clean);
	desc_print(priv->dma_tx, dma_desc_tx);
#endif
	sunxi_tx_poll(priv->base);
///-
	//geth_tx_complete(priv);

	return NETDEV_TX_OK;
}

DEFINE_PER_CPU_ALIGNED(struct sk_buff_head, per_cpu_output_pkt_queue);




static void geth_xmit_tsklet(unsigned long data)
{
	struct sk_buff_head * xmit_head;
	struct geth_priv *priv = (struct geth_priv *)data;
	int cpu;
	struct sk_buff *skb;
		
	for_each_possible_cpu(cpu)
	{
		xmit_head = &per_cpu(per_cpu_output_pkt_queue, cpu);  //1-7
		spin_lock(&xmit_head->lock);
		skb_queue_splice_tail_init(xmit_head,&priv->output_pkt_queue);
		spin_unlock(&xmit_head->lock);
	}

	while ((skb = __skb_dequeue(&priv->output_pkt_queue))) {
		if(geth_xmit_new(skb,priv->ndev) != NETDEV_TX_OK)
		{
			xmit_fail_spark++;
			break;
		}
		//dev_kfree_skb(skb);
		//skb = NULL;
	}
	if(skb != NULL)
		__skb_queue_head( &priv->output_pkt_queue, skb);

///-
	//geth_tx_complete(priv);
	//printk(KERN_ERR"geth_xmit_tsklet\n");
}

///-
#if 0
//add by spark 只添加发送队列，后统一处理
static netdev_tx_t geth_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct sk_buff_head * xmit_head;

	//printk(KERN_ERR"geth_xmit\n");
	xmit_head = &__get_cpu_var(per_cpu_output_pkt_queue);  //1-7
	spin_lock(&xmit_head->lock);
	__skb_queue_tail( xmit_head, skb);
	spin_unlock(&xmit_head->lock);
	
	return NETDEV_TX_OK;
}
#else
static netdev_tx_t geth_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct geth_priv  *priv = netdev_priv(ndev);
	unsigned int entry;
	struct dma_desc *desc, *first;
	unsigned int len, tmp_len = 0;
	int i, csum_insert;
	int nfrags = skb_shinfo(skb)->nr_frags;
	dma_addr_t paddr;

///-
	geth_tx_complete(priv);

	spin_lock(&priv->tx_lock);
	if (unlikely(circ_space(priv->tx_dirty, priv->tx_clean,
			dma_desc_tx) < (nfrags + 1))) {

		if (!netif_queue_stopped(ndev)) {
			netdev_err(ndev, "%s: BUG! Tx Ring full when queue awake\n", __func__);
			netif_stop_queue(ndev);
		}
        ///-
        printk(KERN_ERR "tx Ring full\n");
		spin_unlock(&priv->tx_lock);

		return NETDEV_TX_BUSY;
	}


	csum_insert = (skb->ip_summed == CHECKSUM_PARTIAL);
	entry = priv->tx_dirty;
	first = desc = priv->dma_tx + entry;

	len = skb_headlen(skb);
	priv->tx_sk[entry] = skb;

#ifdef PKT_DEBUG
	printk("======TX PKT DATA: ============\n");
	/* dump the packet */
	print_hex_dump(KERN_DEBUG, "skb->data: ", DUMP_PREFIX_NONE,
			16, 1, skb->data, 64, true);
#endif
///-
#if 0
    {
        int cpu;
        cpu = smp_processor_id();
        if(cpu != 0)
        {
            printk(KERN_ERR "======TX PKT DATA: ============ cpu:%d\n", cpu);
        }
    }
#endif
#if 0
    if(geth_debug_print_on)
    {
        if((len > 35) && (skb->data[23] == 0x1) && (skb->data[34] == 0x00))
        {
            printk(KERN_ERR "======TX PKT DATA: ============\n");
            /* dump the packet */
            print_hex_dump(KERN_ERR, "skb->data: ", DUMP_PREFIX_NONE,
                    16, 1, skb->data, 64, true);
        }
    }
#endif
#if 0
    if((len > 35) && (skb->data[23] == 0x1) && (skb->data[34] == 0x00))
    {
        unsigned short seq;
        
        if(geth_debug_print_on)
        {
            if((len > 35) && (skb->data[23] == 0x1) && (skb->data[34] == 0x00))
            {
                printk(KERN_ERR "======TX PKT DATA: ============\n");
                /* dump the packet */
                print_hex_dump(KERN_ERR, "skb->data: ", DUMP_PREFIX_NONE,
                        16, 1, skb->data, 64, true);
            }
        }
        
        memcpy(&seq, &skb->data[40], sizeof(seq));
        seq = ntohs(seq);
        if(icmp_seq < 0)
        {
            icmp_seq = seq;
        }
        else if((icmp_seq + 1) != seq)
        {
            printk(KERN_ERR "icmp reply seq:%d missing send seq:%d\n", icmp_seq+1, seq);
            icmp_seq = seq;
        }
        else
        {
            icmp_seq = seq;
        }
    }
#endif

	/* Every desc max size is 2K */
	while (len != 0) {
		desc = priv->dma_tx + entry;
		tmp_len = ((len > MAX_BUF_SZ) ?  MAX_BUF_SZ : len);

		paddr = dma_map_single(priv->dev, skb->data, tmp_len, DMA_TO_DEVICE);
		if (dma_mapping_error(priv->dev, paddr)){
			dev_kfree_skb(skb);
			return -EIO;
		}
		desc_buf_set(desc, paddr, tmp_len);
		/* Don't set the first's own bit, here */
		if (first != desc) {
			priv->tx_sk[entry] = NULL;
			desc_set_own(desc);
		}

		entry = circ_inc(entry, dma_desc_tx);
		len -= tmp_len;
	}

	for (i = 0; i <nfrags; i++) {
		const skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
		len = skb_frag_size(frag);

		desc = priv->dma_tx + entry;
		paddr = skb_frag_dma_map(priv->dev, frag, 0, len, DMA_TO_DEVICE);
		if (dma_mapping_error(priv->dev, paddr)) {
			dev_kfree_skb(skb);
			return -EIO;
		}

		desc_buf_set(desc, paddr, len);
		desc_set_own(desc);
		priv->tx_sk[entry] = NULL;
		entry = circ_inc(entry, dma_desc_tx);
	}

	ndev->stats.tx_bytes += skb->len;
	priv->tx_dirty = entry;
	desc_tx_close(first, desc, csum_insert);

	desc_set_own(first);
	spin_unlock(&priv->tx_lock);

	if (circ_space(priv->tx_dirty, priv->tx_clean, dma_desc_tx) <=
			(MAX_SKB_FRAGS + 1)) {
		netif_stop_queue(ndev);
		if (circ_space(priv->tx_dirty, priv->tx_clean, dma_desc_tx) >
				TX_THRESH)
			netif_wake_queue(ndev);
	}

#ifdef DEBUG
	printk("=======TX Descriptor DMA: 0x%08x\n", priv->dma_tx_phy);
	printk("Tx pointor: dirty: %d, clean: %d\n", priv->tx_dirty, priv->tx_clean);
	desc_print(priv->dma_tx, dma_desc_tx);
#endif
	sunxi_tx_poll(priv->base);
	geth_tx_complete(priv);

	return NETDEV_TX_OK;
}
#endif
#else
static netdev_tx_t geth_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct geth_priv  *priv = g_eth_priv;
	unsigned int entry;
	struct dma_desc *desc, *first;
	unsigned int len, tmp_len = 0;
	int i, csum_insert;
	int nfrags = skb_shinfo(skb)->nr_frags;
	dma_addr_t paddr;
#if defined(PRODUCT_UC200)
	if(((*(skb->data+12))!=0x81)||((*(skb->data+13))!=0x00))
	{
		skb=skb_realloc_headroom(skb,4);
		skb_push(skb, 4);
		memmove(skb->data, skb->data+4, 12);
		*(skb->data+12)=0x81;
		*(skb->data+13)=0x00;
		*(skb->data+14)=0x00;
		read_lock(&mr_lock_vid);
		init_vid();
		if(strncmp(ndev->name,"eth1",4)==0){
		*(skb->data+14)|=GTH0_vid2;
		*(skb->data+15)=GTH0_vid1;
	}
			
	if(strncmp(ndev->name,"eth0",4)==0){
		*(skb->data+14)|=GTH1_vid2;
		*(skb->data+15)=GTH1_vid1;
		}
		read_unlock(&mr_lock_vid);
	}

	//81 00 00 01
	//print_hex_dump(KERN_ALERT, "skb->data: ", DUMP_PREFIX_NONE,
	//		16, 1, skb->data, 64, true);
#endif
	spin_lock(&priv->tx_lock);
	if (unlikely(circ_space(priv->tx_dirty, priv->tx_clean,
			dma_desc_tx) < (nfrags + 1))) {

		if (!netif_queue_stopped(ndev)) {
			netdev_err(ndev, "%s: BUG! Tx Ring full when queue awake\n", __func__);
			netif_stop_queue(ndev);
		}
		spin_unlock(&priv->tx_lock);

		return NETDEV_TX_BUSY;
	}


	csum_insert = (skb->ip_summed == CHECKSUM_PARTIAL);
	entry = priv->tx_dirty;
	first = desc = priv->dma_tx + entry;

	len = skb_headlen(skb);
	priv->tx_sk[entry] = skb;

#ifdef PKT_DEBUG
	printk("======TX PKT DATA: ============\n");
	/* dump the packet */
	print_hex_dump(KERN_DEBUG, "skb->data: ", DUMP_PREFIX_NONE,
			16, 1, skb->data, 64, true);
#endif
#if 1
#if 0
    if((skb->data[12] == 0x81) && (skb->data[13] == 0x00) &&
        (skb->data[14] == 0x00) && (skb->data[15] == 0x02))
    {
        spin_unlock(&priv->tx_lock);
        dev_kfree_skb(skb);
        return NETDEV_TX_OK;
    }
#endif
    if(geth_debug_print_on)
    {
        printk("======TX PKT DATA: ============\n");
        /* dump the packet */
        print_hex_dump(KERN_ERR, "skb->data: ", DUMP_PREFIX_NONE,
                16, 1, skb->data, 64, true);
    }
#endif

	/* Every desc max size is 2K */
	while (len != 0) {
		desc = priv->dma_tx + entry;
		tmp_len = ((len > MAX_BUF_SZ) ?  MAX_BUF_SZ : len);

		paddr = dma_map_single(priv->dev, skb->data, tmp_len, DMA_TO_DEVICE);
		if (dma_mapping_error(priv->dev, paddr)){
			dev_kfree_skb(skb);
            spin_unlock(&priv->tx_lock);
			return -EIO;
		}
		desc_buf_set(desc, paddr, tmp_len);
		/* Don't set the first's own bit, here */
		if (first != desc) {
			priv->tx_sk[entry] = NULL;
			desc_set_own(desc);
		}

		entry = circ_inc(entry, dma_desc_tx);
		len -= tmp_len;
	}

	for (i = 0; i <nfrags; i++) {
		const skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
		len = skb_frag_size(frag);

		desc = priv->dma_tx + entry;
		paddr = skb_frag_dma_map(priv->dev, frag, 0, len, DMA_TO_DEVICE);
		if (dma_mapping_error(priv->dev, paddr)) {
			dev_kfree_skb(skb);
			return -EIO;
		}

		desc_buf_set(desc, paddr, len);
		desc_set_own(desc);
		priv->tx_sk[entry] = NULL;
		entry = circ_inc(entry, dma_desc_tx);
	}

	ndev->stats.tx_bytes += skb->len;
	priv->tx_dirty = entry;
	desc_tx_close(first, desc, csum_insert);

	desc_set_own(first);
	spin_unlock(&priv->tx_lock);

	if (circ_space(priv->tx_dirty, priv->tx_clean, dma_desc_tx) <=
			(MAX_SKB_FRAGS + 1)) {
		netif_stop_queue(ndev);
		if (circ_space(priv->tx_dirty, priv->tx_clean, dma_desc_tx) >
				TX_THRESH)
			netif_wake_queue(ndev);
	}

#ifdef DEBUG
	printk("=======TX Descriptor DMA: 0x%08x\n", priv->dma_tx_phy);
	printk("Tx pointor: dirty: %d, clean: %d\n", priv->tx_dirty, priv->tx_clean);
	desc_print(priv->dma_tx, dma_desc_tx);
#endif
	sunxi_tx_poll(priv->base);
	geth_tx_complete(priv);

	return NETDEV_TX_OK;
}
#endif

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
static int recv_cnt_spark = 0;

//此函数不可重入
static int geth_rx(struct geth_priv *priv, int limit)
{
	unsigned int rxcount = 0;
	unsigned int entry;
	struct dma_desc *desc;
	struct sk_buff *skb;
	unsigned int half_dma = dma_desc_rx/2;
	static int in_user = 0;
	int buf_cnt;
	
	if(in_user > 0)
	{
		netdev_err(priv->ndev, "geth_rx in user %d\n",in_user);
		return -3;
	}
	in_user++;
	rx_int_cnt++;
	// 无接收缓存填充dma，放弃接收
	buf_cnt = skb_queue_len(&priv->rx_prealloc);
	#if GETH_DEBUG_SPARK
	if(buf_cnt < limit)
	{
		priv->debug_info.buffer_error_cnt++;
	}
	else if(buf_cnt < 512)
	{
		priv->debug_info.buffer_512_cnt++;
	}
	else if(buf_cnt < 1024)
	{
		priv->debug_info.buffer_1024_cnt++;
	}
	else if(buf_cnt < 2048)
	{
		priv->debug_info.buffer_2048_cnt++;
	}
	else if(buf_cnt < 4096)
	{
		priv->debug_info.buffer_4096_cnt++;
	}
	else
	{
		priv->debug_info.buffer_more_4096_cnt++;
	}
	#endif
	if(buf_cnt < limit )
	{
		//netdev_err(priv->ndev, "rx prealloc Skb is null,int %d,tsk %d\n",rx_int_cnt,rx_tsk_cnt);
		rx_buffer_error_cnt++;
		in_user--;
		return -1;
	}
	while (rxcount < limit) {

		entry = priv->rx_dirty;
		desc = priv->dma_rx + entry;

		if (desc_get_own(desc))   /* Own bit. CPU:0, DMA:1 */		//desc属于cpu
			break;

		rxcount++;
		if(rxcount == half_dma)		// dma
		{
			geth_rx_refill(priv->ndev);
		}
		recv_cnt_spark++;
		priv->rx_dirty = circ_inc(priv->rx_dirty, dma_desc_rx);

		// 添加rx 到接受队列
		skb = priv->rx_sk[entry];
		if (unlikely(!skb)){
			netdev_err(priv->ndev, "Skb 2 is null\n");
			priv->ndev->stats.rx_dropped++;
			break;
		}
		memcpy(skb->cb,desc,sizeof(struct dma_desc));
		dma_unmap_single(priv->dev, desc_buf_get_addr(desc),
				desc_buf_get_len(desc), DMA_FROM_DEVICE);
		priv->rx_sk[entry] = NULL;
		__skb_queue_tail(&priv->input_pkt_queue, skb);
	}

#ifdef DEBUG
	if (rxcount > 0) {
		printk("======RX Descriptor DMA: 0x%08x=\n", priv->dma_rx_phy);
		printk("RX pointor: dirty: %d, clean: %d\n", priv->rx_dirty, priv->rx_clean);
		desc_print(priv->dma_rx, dma_desc_rx);
	}
#endif
	geth_rx_refill(priv->ndev);

	in_user--;
	return rxcount;
}
#else
static int geth_rx(struct geth_priv *priv, int limit)
{
	unsigned int rxcount = 0;
	unsigned int entry;
	struct dma_desc *desc;
	struct sk_buff *skb, *skb_cp;
	int status;
	int frame_len, i;
    uint16_t src_port, logic_port;
	char flag_vid;
	char flag_eth=0;

	while (rxcount < limit) {

		entry = priv->rx_dirty;
		desc = priv->dma_rx + entry;

		if (desc_get_own(desc))
			break;

		rxcount++;
		priv->rx_dirty = circ_inc(priv->rx_dirty, dma_desc_rx);

		/* Get lenght & status from hardware */
		frame_len = desc_rx_frame_len(desc);
		status = desc_get_rx_status(desc, (void *)(&priv->xstats));

		netdev_dbg(g_ndev[0], "Rx frame size %d, status: %d\n",
				frame_len, status);

		skb = priv->rx_sk[entry];
		if (unlikely(!skb)){
			netdev_err(g_ndev[0], "Skb is null\n");
			g_ndev[0]->stats.rx_dropped++;
			break;
		}

///-
#if 0
#ifdef PKT_DEBUG
		printk("======RX PKT DATA: ============\n");
		/* dump the packet */
		print_hex_dump(KERN_DEBUG, "skb->data: ", DUMP_PREFIX_NONE,
				16, 1, skb->data, 64, true);
#endif
#else
#if 1
        if(geth_debug_print_on)
        {
            printk(KERN_ERR "======RX PKT DATA: ============\n");
            /* dump the packet */
            print_hex_dump(KERN_ERR, "skb->data: ", DUMP_PREFIX_NONE,
                    16, 1, skb->data, 64, true);
        }
#endif
#endif

		if (status == discard_frame){
			netdev_dbg(g_ndev[0], "Get error pkt\n");
			g_ndev[0]->stats.rx_errors++;
			continue;
		}

		if (unlikely(status != llc_snap))
			frame_len -= ETH_FCS_LEN;

		priv->rx_sk[entry] = NULL;

		skb_put(skb, frame_len);
        /*sbc交换芯片开启了cpu port功能*/
#if defined(PRODUCT_SBCMAIN) || defined(PRODUCT_MTG2500MAIN) || \
    defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC1000MAIN)
        memcpy(&src_port, skb->data+18, sizeof(src_port));
        src_port = ntohs(src_port);
        //printk(KERN_ERR "src port:%d\n", src_port);
        memmove(skb->data+8, skb->data, 12);
        skb_pull(skb, 8);
        frame_len -= 8;
#endif
		dma_unmap_single(priv->dev, desc_buf_get_addr(desc),
				desc_buf_get_len(desc), DMA_FROM_DEVICE);

#if 0
        for(i=0; i<MAX_NET_DEVICE; i++)
        {
        ///-
            //printk(KERN_ERR "%s %d: eth%d %02x %02x %02x %02x %02x %02x\n",
            //    __FUNCTION__, __LINE__, i, 
            //    g_ndev[i]->dev_addr[0], g_ndev[i]->dev_addr[1], g_ndev[i]->dev_addr[2],
            //    g_ndev[i]->dev_addr[3], g_ndev[i]->dev_addr[4], g_ndev[i]->dev_addr[5]);
            if(0 == memcmp(g_ndev[i]->dev_addr, skb->data, g_ndev[i]->addr_len))
            {
        		skb->protocol = eth_type_trans(skb, g_ndev[i]);
                skb->ip_summed = CHECKSUM_UNNECESSARY;
        		napi_gro_receive(&priv->napi, skb);
                g_ndev[i]->stats.rx_packets++;
                g_ndev[i]->stats.rx_bytes += frame_len;
                break;
            }
#if defined(PRODUCT_SBCMAIN) || defined(PRODUCT_MTG2500MAIN)
            /*用户板会发出源地址为主控板对外接口的mac的包，这种包需要
              抓上来*/
            if(i < (MAX_NET_DEVICE -1))
            {
                if(0 == memcmp(g_ndev[i]->dev_addr, skb->data+6, g_ndev[i]->addr_len))
                {
                    skb->protocol = eth_type_trans(skb, g_ndev[i]);
                    skb->ip_summed = CHECKSUM_UNNECESSARY;
                    napi_gro_receive(&priv->napi, skb);
                    g_ndev[i]->stats.rx_packets++;
                    g_ndev[i]->stats.rx_bytes += frame_len;
                    break;
                }
            }
#endif
        }
        /*广播报文，在没有port vlan的情况下，无法判断来自哪个port，
          同时向所有注册接口递交报文*/
        if((MAX_NET_DEVICE == i) && (0 == memcmp(boardcast_mac, skb->data, ETH_ALEN)))
        {
        ///-
        //printk(KERN_ERR "%s %d\n", __FUNCTION__, __LINE__);
            for(i=1; i<MAX_NET_DEVICE; i++)
            {
                skb_cp = skb_copy(skb, GFP_KERNEL);
                if(skb_cp)
                {
                    skb_cp->protocol = eth_type_trans(skb_cp, g_ndev[i]);
                    skb_cp->ip_summed = CHECKSUM_UNNECESSARY;
                    napi_gro_receive(&priv->napi, skb_cp);
                    g_ndev[i]->stats.rx_packets++;
                    g_ndev[i]->stats.rx_bytes += frame_len;
                }
            }
            
            skb->protocol = eth_type_trans(skb, g_ndev[0]);
            skb->ip_summed = CHECKSUM_UNNECESSARY;
            napi_gro_receive(&priv->napi, skb);
            g_ndev[0]->stats.rx_packets++;
            g_ndev[0]->stats.rx_bytes += frame_len;
        }
        else if(MAX_NET_DEVICE == i) //目的mac非host或非广播，直接丢弃
        {
            dev_kfree_skb(skb);
        }
#else
#if defined(PRODUCT_SBCMAIN) || defined(PRODUCT_MTG2500MAIN) || \
    defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC1000MAIN)

#if defined(PRODUCT_SBCMAIN) || defined(PRODUCT_MTG2500MAIN) || defined(PRODUCT_AG)
        if(src_port < 2)
#elif defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC1000MAIN)
        if(src_port <= 4)
#endif
        {
#if  defined(PRODUCT_MTG2500MAIN)
    /**mtg的eth0和eth1和其他产品是相反*/
            if(src_port == 1)
                src_port = 0;
            else
                src_port = 1;
#endif
            logic_port = eth_map_phy2logic[src_port];
            if((0 == memcmp(g_ndev[logic_port]->dev_addr, skb->data, g_ndev[logic_port]->addr_len)) ||
                (0 == memcmp(g_ndev[logic_port]->dev_addr, skb->data+6, g_ndev[logic_port]->addr_len)) || 
                (0 == memcmp(boardcast_mac, skb->data, ETH_ALEN)))
            {
                skb->protocol = eth_type_trans(skb, g_ndev[logic_port]);
                skb->ip_summed = CHECKSUM_UNNECESSARY;
                napi_gro_receive(&priv->napi, skb);
                g_ndev[logic_port]->stats.rx_packets++;
                g_ndev[logic_port]->stats.rx_bytes += frame_len;
            }
            else
            {
                dev_kfree_skb(skb);
            }
        }
#if defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC1000MAIN)
#if 0
        else if((src_port >= 1) && (src_port <= 4))
        {
            if((0 == memcmp(g_ndev[1]->dev_addr, skb->data, g_ndev[1]->addr_len)) ||
                (0 == memcmp(g_ndev[1]->dev_addr, skb->data+6, g_ndev[1]->addr_len)) || 
                (0 == memcmp(boardcast_mac, skb->data, ETH_ALEN)))
            {
                skb->protocol = eth_type_trans(skb, g_ndev[1]);
                skb->ip_summed = CHECKSUM_UNNECESSARY;
                napi_gro_receive(&priv->napi, skb);
                g_ndev[1]->stats.rx_packets++;
                g_ndev[1]->stats.rx_bytes += frame_len;
            }
            else
            {
                dev_kfree_skb(skb);
            }
        }
#endif
#endif
#if defined(PRODUCT_SBCMAIN)
        else if(3 == src_port)
        {
            logic_port = eth_map_phy2logic[src_port];
            if((0 == memcmp(g_ndev[logic_port]->dev_addr, skb->data, g_ndev[logic_port]->addr_len)) ||
                (0 == memcmp(g_ndev[logic_port]->dev_addr, skb->data+6, g_ndev[logic_port]->addr_len)))
            {
                skb->protocol = eth_type_trans(skb, g_ndev[logic_port]);
                skb->ip_summed = CHECKSUM_UNNECESSARY;
                napi_gro_receive(&priv->napi, skb);
                g_ndev[logic_port]->stats.rx_packets++;
                g_ndev[logic_port]->stats.rx_bytes += frame_len;
            }
            else if((0 == memcmp(g_ndev[logic_port+1]->dev_addr, skb->data, g_ndev[logic_port+1]->addr_len)) ||
                (0 == memcmp(g_ndev[logic_port+1]->dev_addr, skb->data+6, g_ndev[logic_port+1]->addr_len)))
            {
                skb->protocol = eth_type_trans(skb, g_ndev[logic_port+1]);
                skb->ip_summed = CHECKSUM_UNNECESSARY;
                napi_gro_receive(&priv->napi, skb);
                g_ndev[logic_port]->stats.rx_packets++;
                g_ndev[logic_port]->stats.rx_bytes += frame_len;
            }
            else if((0 == memcmp(g_ndev[eth_map_phy2logic[0]]->dev_addr, skb->data, g_ndev[eth_map_phy2logic[0]]->addr_len)) ||
                    (0 == memcmp(g_ndev[eth_map_phy2logic[0]]->dev_addr, skb->data+6, g_ndev[eth_map_phy2logic[0]]->addr_len)))
            {
                /*用户板0上的从eth0/eth1过来后者发往eth0/eth1的rtp抓包报文需要送到eth0/eth1*/
                skb->protocol = eth_type_trans(skb, g_ndev[eth_map_phy2logic[0]]);
                skb->ip_summed = CHECKSUM_UNNECESSARY;
                napi_gro_receive(&priv->napi, skb);
                g_ndev[eth_map_phy2logic[0]]->stats.rx_packets++;
                g_ndev[eth_map_phy2logic[0]]->stats.rx_bytes += frame_len;
            }
            else if((0 == memcmp(g_ndev[eth_map_phy2logic[1]]->dev_addr, skb->data, g_ndev[eth_map_phy2logic[1]]->addr_len)) ||
                    (0 == memcmp(g_ndev[eth_map_phy2logic[1]]->dev_addr, skb->data+6, g_ndev[eth_map_phy2logic[1]]->addr_len)))
            {
                skb->protocol = eth_type_trans(skb, g_ndev[eth_map_phy2logic[1]]);
                skb->ip_summed = CHECKSUM_UNNECESSARY;
                napi_gro_receive(&priv->napi, skb);
                g_ndev[eth_map_phy2logic[1]]->stats.rx_packets++;
                g_ndev[eth_map_phy2logic[1]]->stats.rx_bytes += frame_len;
            }
            else if((0 == memcmp(g_ndev[MAX_NET_DEVICE-1]->dev_addr, skb->data, g_ndev[MAX_NET_DEVICE-1]->addr_len)) ||
                (0 == memcmp(g_ndev[MAX_NET_DEVICE-1]->dev_addr, skb->data+6, g_ndev[MAX_NET_DEVICE-1]->addr_len)))
            {
                skb->protocol = eth_type_trans(skb, g_ndev[MAX_NET_DEVICE-1]);
                skb->ip_summed = CHECKSUM_UNNECESSARY;
                napi_gro_receive(&priv->napi, skb);
                g_ndev[MAX_NET_DEVICE-1]->stats.rx_packets++;
                g_ndev[MAX_NET_DEVICE-1]->stats.rx_bytes += frame_len;
            }
            else if(0 == memcmp(boardcast_mac, skb->data, ETH_ALEN))
            {
#if 0
                skb_cp = skb_copy(skb, GFP_KERNEL);
                if(skb_cp)
                {
                    skb_cp->protocol = eth_type_trans(skb_cp, g_ndev[logic_port]);
                    skb_cp->ip_summed = CHECKSUM_UNNECESSARY;
                    napi_gro_receive(&priv->napi, skb_cp);
                    g_ndev[logic_port]->stats.rx_packets++;
                    g_ndev[logic_port]->stats.rx_bytes += frame_len;
                }

                skb_cp = skb_copy(skb, GFP_KERNEL);
                if(skb_cp)
                {
                    skb_cp->protocol = eth_type_trans(skb_cp, g_ndev[MAX_NET_DEVICE-1]);
                    skb_cp->ip_summed = CHECKSUM_UNNECESSARY;
                    napi_gro_receive(&priv->napi, skb_cp);
                    g_ndev[MAX_NET_DEVICE-1]->stats.rx_packets++;
                    g_ndev[MAX_NET_DEVICE-1]->stats.rx_bytes += frame_len;
                }
                
                skb->protocol = eth_type_trans(skb, g_ndev[logic_port+1]);
                skb->ip_summed = CHECKSUM_UNNECESSARY;
                napi_gro_receive(&priv->napi, skb);
                g_ndev[logic_port+1]->stats.rx_packets++;
                g_ndev[logic_port+1]->stats.rx_bytes += frame_len;
#else
                /*抓一路rtp包的时候，为了绕开acl规则，用户板将抓的rtp包的目的mac
                  都改为广播地址，抓包的时候，要求能够在该包实际收发的外网口抓到
                  该报文，tcpdump不能监听所有的网口，将包发送到所有网口，tcpdump
                  无论在哪个网口监听都可以抓到*/
                for(i=1; i<MAX_NET_DEVICE; i++)
                {
                    skb_cp = skb_copy(skb, GFP_ATOMIC);
                    if(skb_cp)
                    {
                        skb_cp->protocol = eth_type_trans(skb_cp, g_ndev[i]);
                        skb_cp->ip_summed = CHECKSUM_UNNECESSARY;
                        napi_gro_receive(&priv->napi, skb_cp);
                        g_ndev[i]->stats.rx_packets++;
                        g_ndev[i]->stats.rx_bytes += frame_len;
                    }
                }
                skb->protocol = eth_type_trans(skb, g_ndev[0]);
                skb->ip_summed = CHECKSUM_UNNECESSARY;
                napi_gro_receive(&priv->napi, skb);
                g_ndev[0]->stats.rx_packets++;
                g_ndev[0]->stats.rx_bytes += frame_len;
#endif
            }
            else
            {
                dev_kfree_skb(skb);
            }
        }
#endif
        else
        {
#if defined(PRODUCT_SBCMAIN)
            if((0 == memcmp(g_ndev[MAX_NET_DEVICE-1]->dev_addr, skb->data, g_ndev[MAX_NET_DEVICE-1]->addr_len)) ||
                (0 == memcmp(g_ndev[MAX_NET_DEVICE-1]->dev_addr, skb->data+6, g_ndev[MAX_NET_DEVICE-1]->addr_len)))
            {
                skb->protocol = eth_type_trans(skb, g_ndev[MAX_NET_DEVICE-1]);
                skb->ip_summed = CHECKSUM_UNNECESSARY;
                napi_gro_receive(&priv->napi, skb);
                g_ndev[MAX_NET_DEVICE-1]->stats.rx_packets++;
                g_ndev[MAX_NET_DEVICE-1]->stats.rx_bytes += frame_len;
            }
            else
            {
                /*镜像抓包功能，用户板的物理port口在前面的条件可能都不满足，
                  需要在这里处理*/
                if((0 == memcmp(g_ndev[eth_map_phy2logic[0]]->dev_addr, skb->data, g_ndev[eth_map_phy2logic[0]]->addr_len)) ||
                    (0 == memcmp(g_ndev[eth_map_phy2logic[0]]->dev_addr, skb->data+6, g_ndev[eth_map_phy2logic[0]]->addr_len)))
                {
                    skb->protocol = eth_type_trans(skb, g_ndev[eth_map_phy2logic[0]]);
                    skb->ip_summed = CHECKSUM_UNNECESSARY;
                    napi_gro_receive(&priv->napi, skb);
                    g_ndev[eth_map_phy2logic[0]]->stats.rx_packets++;
                    g_ndev[eth_map_phy2logic[0]]->stats.rx_bytes += frame_len;
                }
                else if((0 == memcmp(g_ndev[eth_map_phy2logic[1]]->dev_addr, skb->data, g_ndev[eth_map_phy2logic[1]]->addr_len)) ||
                    (0 == memcmp(g_ndev[eth_map_phy2logic[1]]->dev_addr, skb->data+6, g_ndev[eth_map_phy2logic[1]]->addr_len)))
                {
                    skb->protocol = eth_type_trans(skb, g_ndev[eth_map_phy2logic[1]]);
                    skb->ip_summed = CHECKSUM_UNNECESSARY;
                    napi_gro_receive(&priv->napi, skb);
                    g_ndev[eth_map_phy2logic[1]]->stats.rx_packets++;
                    g_ndev[eth_map_phy2logic[1]]->stats.rx_bytes += frame_len;
                }
                else if((0 == memcmp(g_ndev[eth_map_phy2logic[3]]->dev_addr, skb->data, g_ndev[eth_map_phy2logic[3]]->addr_len)) ||
                    (0 == memcmp(g_ndev[eth_map_phy2logic[3]]->dev_addr, skb->data+6, g_ndev[eth_map_phy2logic[3]]->addr_len)))
                {
                    skb->protocol = eth_type_trans(skb, g_ndev[eth_map_phy2logic[3]]);
                    skb->ip_summed = CHECKSUM_UNNECESSARY;
                    napi_gro_receive(&priv->napi, skb);
                    g_ndev[eth_map_phy2logic[3]]->stats.rx_packets++;
                    g_ndev[eth_map_phy2logic[3]]->stats.rx_bytes += frame_len;
                }
                else if((0 == memcmp(g_ndev[eth_map_phy2logic[3]+1]->dev_addr, skb->data, g_ndev[eth_map_phy2logic[3]+1]->addr_len)) ||
                    (0 == memcmp(g_ndev[eth_map_phy2logic[3]+1]->dev_addr, skb->data+6, g_ndev[eth_map_phy2logic[3]+1]->addr_len)))
                {
                    skb->protocol = eth_type_trans(skb, g_ndev[eth_map_phy2logic[3]+1]);
                    skb->ip_summed = CHECKSUM_UNNECESSARY;
                    napi_gro_receive(&priv->napi, skb);
                    g_ndev[eth_map_phy2logic[3]+1]->stats.rx_packets++;
                    g_ndev[eth_map_phy2logic[3]+1]->stats.rx_bytes += frame_len;
                }
                else if(0 == memcmp(boardcast_mac, skb->data, ETH_ALEN))
                {
                    /*抓一路rtp包的时候，为了绕开acl规则，用户板将抓的rtp包的目的mac
                      都改为广播地址，抓包的时候，要求能够在该包实际收发的外网口抓到
                      该报文，tcpdump不能监听所有的网口，将包发送到所有网口，tcpdump
                      无论在哪个网口监听都可以抓到*/
                    for(i=1; i<MAX_NET_DEVICE; i++)
                    {
                        skb_cp = skb_copy(skb, GFP_ATOMIC);
                        if(skb_cp)
                        {
                            skb_cp->protocol = eth_type_trans(skb_cp, g_ndev[i]);
                            skb_cp->ip_summed = CHECKSUM_UNNECESSARY;
                            napi_gro_receive(&priv->napi, skb_cp);
                            g_ndev[i]->stats.rx_packets++;
                            g_ndev[i]->stats.rx_bytes += frame_len;
                        }
                    }
                    skb->protocol = eth_type_trans(skb, g_ndev[0]);
                    skb->ip_summed = CHECKSUM_UNNECESSARY;
                    napi_gro_receive(&priv->napi, skb);
                    g_ndev[0]->stats.rx_packets++;
                    g_ndev[0]->stats.rx_bytes += frame_len;
                }
                else
                {
                    dev_kfree_skb(skb);
                }
            }
#elif defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC1000MAIN)
            /*镜像抓包功能，用户板的物理port口在前面的条件可能都不满足，
                  需要在这里处理*/
            for(i = 0; i<MAX_NET_DEVICE; i++)
            {
                if((0 == memcmp(g_ndev[eth_map_phy2logic[i]]->dev_addr, skb->data, g_ndev[eth_map_phy2logic[i]]->addr_len)) ||
                    (0 == memcmp(g_ndev[eth_map_phy2logic[i]]->dev_addr, skb->data+6, g_ndev[eth_map_phy2logic[i]]->addr_len)))
                {
                    skb->protocol = eth_type_trans(skb, g_ndev[eth_map_phy2logic[i]]);
                    skb->ip_summed = CHECKSUM_UNNECESSARY;
                    napi_gro_receive(&priv->napi, skb);
                    g_ndev[eth_map_phy2logic[i]]->stats.rx_packets++;
                    g_ndev[eth_map_phy2logic[i]]->stats.rx_bytes += frame_len;
                    break;
                }
            }
            if(i == MAX_NET_DEVICE)
            {
                if(0 == memcmp(boardcast_mac, skb->data, ETH_ALEN))
                {
                    /*抓一路rtp包的时候，为了绕开acl规则，用户板将抓的rtp包的目的mac
                      都改为广播地址，抓包的时候，要求能够在该包实际收发的外网口抓到
                      该报文，tcpdump不能监听所有的网口，将包发送到所有网口，tcpdump
                      无论在哪个网口监听都可以抓到*/
                    for(i=1; i<MAX_NET_DEVICE; i++)
                    {
                        skb_cp = skb_copy(skb, GFP_ATOMIC);
                        if(skb_cp)
                        {
                            skb_cp->protocol = eth_type_trans(skb_cp, g_ndev[i]);
                            skb_cp->ip_summed = CHECKSUM_UNNECESSARY;
                            napi_gro_receive(&priv->napi, skb_cp);
                            g_ndev[i]->stats.rx_packets++;
                            g_ndev[i]->stats.rx_bytes += frame_len;
                        }
                    }
                    skb->protocol = eth_type_trans(skb, g_ndev[0]);
                    skb->ip_summed = CHECKSUM_UNNECESSARY;
                    napi_gro_receive(&priv->napi, skb);
                    g_ndev[0]->stats.rx_packets++;
                    g_ndev[0]->stats.rx_bytes += frame_len;
                }
                else
                {
                    dev_kfree_skb(skb);
                }
            }
#else
            if((0 == memcmp(g_ndev[MAX_NET_DEVICE-1]->dev_addr, skb->data, g_ndev[MAX_NET_DEVICE-1]->addr_len)) ||
                (0 == memcmp(g_ndev[MAX_NET_DEVICE-1]->dev_addr, skb->data+6, g_ndev[MAX_NET_DEVICE-1]->addr_len)) || 
                (0 == memcmp(boardcast_mac, skb->data, ETH_ALEN)))
            {
                skb->protocol = eth_type_trans(skb, g_ndev[MAX_NET_DEVICE-1]);
                skb->ip_summed = CHECKSUM_UNNECESSARY;
                napi_gro_receive(&priv->napi, skb);
                g_ndev[MAX_NET_DEVICE-1]->stats.rx_packets++;
                g_ndev[MAX_NET_DEVICE-1]->stats.rx_bytes += frame_len;
            }
            else
            {
                dev_kfree_skb(skb);
            }
#endif
        }
#else   
	#if defined(PRODUCT_UC200)
	/****************判断是否去vlan头**************/
	if(((*(skb->data+12))==0x81)&&((*(skb->data+13))==0x00))
		{	
			flag_vid=(*(skb->data+14))&0x0f;
			
			read_lock(&mr_lock_vid);
			init_vid();
			if(((*(skb->data+15))==GTH0_vid1)&&(flag_vid==GTH0_vid2))
				{
					memmove(skb->data+4, skb->data, 12);
		        	skb_pull(skb, 4);
		        	frame_len -= 4;
					flag_eth=1;
				}

			else if	(((*(skb->data+15))==GTH1_vid1)&&(flag_vid==GTH1_vid2))
				{					
					memmove(skb->data+4, skb->data, 12);
		        	skb_pull(skb, 4);
		        	frame_len -= 4;
					

				}
			read_unlock(&mr_lock_vid);
		}
		//print_hex_dump(KERN_ALERT, "skb->data: ", DUMP_PREFIX_NONE,
		//		16, 1, skb->data, 64, true);
	if(flag_eth)
	{
		skb->protocol = eth_type_trans(skb, g_ndev[1]);
	    skb->ip_summed = CHECKSUM_UNNECESSARY;
	    napi_gro_receive(&priv->napi, skb);			//将数据提交到内核协议栈
	    g_ndev[1]->stats.rx_packets++;
	    g_ndev[1]->stats.rx_bytes += frame_len;
		
	}

	if(!flag_eth)		
	{
	    skb->protocol = eth_type_trans(skb, g_ndev[0]);
	    skb->ip_summed = CHECKSUM_UNNECESSARY;
	    napi_gro_receive(&priv->napi, skb);			//将数据提交到内核协议栈
	    g_ndev[0]->stats.rx_packets++;
	    g_ndev[0]->stats.rx_bytes += frame_len;
	}
	#else
        skb->protocol = eth_type_trans(skb, g_ndev[0]);
        skb->ip_summed = CHECKSUM_UNNECESSARY;
        napi_gro_receive(&priv->napi, skb);
        g_ndev[0]->stats.rx_packets++;
        g_ndev[0]->stats.rx_bytes += frame_len;
	#endif
#endif
#endif
	}

#ifdef DEBUG
	if (rxcount > 0) {
		printk("======RX Descriptor DMA: 0x%08x=\n", priv->dma_rx_phy);
		printk("RX pointor: dirty: %d, clean: %d\n", priv->rx_dirty, priv->rx_clean);
		desc_print(priv->dma_rx, dma_desc_rx);
	}
#endif
	geth_rx_refill(NULL);

	return rxcount;
}
#endif

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
DEFINE_PER_CPU_ALIGNED(struct sk_buff_head, per_cpu_input_pkt_queue);
DEFINE_PER_CPU_ALIGNED(struct sk_buff_head, per_cpu_process_queue);

static int geth_poll(struct napi_struct *napi, int budget)
{
	struct sk_buff_head * rev_head;
	struct sk_buff_head * process_head;
	struct sk_buff *skb;
	int rxcount = 0;
	int cpu = smp_processor_id();
	struct geth_priv *priv;

	priv = container_of(napi, struct geth_priv, napi[cpu]);
        
///-
    //geth_tx_complete(priv);
    
	process_head = &__get_cpu_var(per_cpu_process_queue);
	//if(skb_queue_len(process_head) < budget)
	{
		rev_head = &__get_cpu_var(per_cpu_input_pkt_queue);
		spin_lock(&rev_head->lock);
		skb_queue_splice_tail_init(rev_head,process_head);
		spin_unlock(&rev_head->lock);
	}
	
	while ((rxcount < budget) && (skb = __skb_dequeue(process_head))) 
	{
		//if(tx_test(skb,priv) == 0)
		{
			napi_gro_receive(napi, skb);
		}
		
		rxcount++;
		atomic_dec(&priv->input_process_cnt);
	}
	
	if (rxcount < budget) {
		napi_complete(napi);
	}
	return rxcount;
}

static void geth_dispatch_cpu(void *data)
{
	struct napi_struct *n = data;
	
	if(likely(napi_schedule_prep(n)))
	{
		__napi_schedule(n);
	}
}
//测试中断效率
static unsigned int in_cnt[64];


static int geth_input_statistics(struct geth_priv *priv,struct sk_buff *skb)
{
	int status;
	struct dma_desc *desc;
	int frame_len;
	
	if (unlikely(!skb)){
		netdev_err(priv->ndev, "Skb is null\n");
		priv->ndev->stats.rx_dropped++;
		return -1;
	}

	desc = (struct dma_desc *)skb->cb;

	//dma_unmap_single(priv->dev, desc_buf_get_addr(desc),
			//desc_buf_get_len(desc), DMA_FROM_DEVICE);
	
	/* Get lenght & status from hardware */
	frame_len = desc_rx_frame_len(desc);
	status = desc_get_rx_status(desc, (void *)(&priv->xstats));

	memset(skb->cb,0,sizeof(struct dma_desc));

	netdev_dbg(priv->ndev, "Rx frame size %d, status: %d\n",
			frame_len, status);

#ifdef PKT_DEBUG
	printk("======RX PKT DATA: ============\n");
	/* dump the packet */
	print_hex_dump(KERN_DEBUG, "skb->data: ", DUMP_PREFIX_NONE,
			16, 1, skb->data, 64, true);
#endif

	if (status == discard_frame){
		netdev_dbg(priv->ndev, "Get error pkt\n");
		priv->ndev->stats.rx_errors++;
		return -2;
	}

	if (unlikely(status != llc_snap))
		frame_len -= ETH_FCS_LEN;

	skb_put(skb, frame_len);
	
	{
		struct ethhdr *eth;

		skb_reset_mac_header(skb);
		eth = eth_hdr(skb);
		memcpy(eth->h_dest,priv->ndev->dev_addr,6);
	}
	skb->protocol = eth_type_trans(skb, priv->ndev);

	skb->ip_summed = CHECKSUM_UNNECESSARY;

	priv->ndev->stats.rx_packets++;
	priv->ndev->stats.rx_bytes += frame_len;
	return 1;
}


// 获取cpu,从0开始
static int geth_default_dispatch(struct sk_buff *skb)
{
	static int toggle = 0;
	#if 1
	if(toggle == 0)
	{
		toggle = 1;
		return 2;
	}
	else if(toggle == 1)
	{
		toggle = 2;
		return 3;
	}
	else
	{
		toggle = 0;
		return 4;
	}
	#endif
	//return 2;
}

static int geth_dispatch(struct geth_priv *priv, int budget)
{
	int work_done = 0;
	unsigned long flags;
	int cpu = 0;
	struct sk_buff *skb;
	struct sk_buff_head rev_head[GETH_MAX_CPU];
	struct sk_buff_head * rev_head_tmp;

	for_each_possible_cpu(cpu)
	{
		skb_queue_head_init(&rev_head[cpu]);
	}
	// 在中断中进行接收处理，关中断防止冲突
	spin_lock_irqsave(&priv->input_pkt_queue.lock, flags);
	skb_queue_splice_tail_init(&priv->input_pkt_queue,&priv->input_process_queue);
	spin_unlock_irqrestore(&priv->input_pkt_queue.lock,flags);

	//进行dispatch 处理，处理最大值为budget
	while((skb = __skb_dequeue(&priv->input_process_queue)) && --budget > 0)
	{
		if(geth_input_statistics(priv,skb) < 0)
		{
			dev_kfree_skb(skb);
		}
		else
		{
			cpu = priv->recv_dispatch(skb);
			__skb_queue_tail(&rev_head[cpu], skb);
			atomic_inc(&priv->input_process_cnt);
		}
		work_done++;
	}
	// 发送softirq
	for_each_possible_cpu(cpu)
	{
		if(skb_queue_len(&rev_head[cpu]))
		{
			rev_head_tmp = &per_cpu(per_cpu_input_pkt_queue, cpu);  
			spin_lock(&rev_head_tmp->lock);			
			skb_queue_splice_tail_init(&rev_head[cpu],rev_head_tmp);
			#if GETH_INPUT_SMP	
			smp_call_function_single(cpu, geth_dispatch_cpu,&priv->napi[cpu],0);
			#else
			smp_call_function_single(cpu, geth_dispatch_cpu,&priv->napi,0);
			#endif
			spin_unlock(&rev_head_tmp->lock);			
			//printk(KERN_ERR"geth_poll rx %d,%d,cpu %d\n",work_done,budget,cpu+1);
		}	
	}
	
	return work_done;
}

static int geth_input_mm(struct geth_priv *priv)
{
	int buffer_cnt;
	int alloc_cnt = 0; 
	int free_cnt =0;
	struct sk_buff_head *head_tmp;
	struct sk_buff *skb;
	struct sk_buff_head  buffer_head;
	unsigned long flags;
	int cpu;
	int input_porcess_len,rx_prealoc_len;
	//static int debug_cnt = 0;

	skb_queue_head_init(&buffer_head);
	// 发多收少情况，需要释放部分发送buffer
	spin_lock(&priv->rx_recycle.lock);
	skb_queue_splice_tail_init(&priv->rx_recycle,&buffer_head);
	spin_unlock(&priv->rx_recycle.lock);


	spin_lock_irqsave(&priv->input_pkt_queue.lock, flags);
	input_porcess_len = skb_queue_len(&priv->input_process_queue);
	rx_prealoc_len = skb_queue_len(&priv->rx_prealloc);
	spin_unlock_irqrestore(&priv->input_pkt_queue.lock,flags);
	// get cur buffer cnt

	//spin_lock(&priv->rx_recycle.lock);
	buffer_cnt = skb_queue_len(&buffer_head) + input_porcess_len+ rx_prealoc_len;

	#if 0
	debug_cnt++;
	if(debug_cnt %5000 == 0)
		printk(KERN_ERR"input buffer is %d,error cnt %d,recycle %d,process %d\n",buffer_cnt,rx_buffer_error_cnt
			,skb_queue_len(&buffer_head),input_porcess_len);
	#endif
	buffer_cnt += atomic_read(&priv->input_process_cnt);
	
	if(buffer_cnt > GETH_INPUT_BUFFER_MAX *2 || buffer_cnt < 0)
	{
		printk(KERN_ERR"input buffer is %d,error cnt %d,recycle %d,process %d,input_porcess cnt %d\n"
			,buffer_cnt,rx_buffer_error_cnt
			,skb_queue_len(&buffer_head),input_porcess_len,atomic_read(&priv->input_process_cnt));
	}
	//spin_unlock(&priv->rx_recycle.lock);
	#if 0
	for_each_possible_cpu(cpu)
	{ 
		//head_tmp = &per_cpu(per_cpu_process_queue, cpu); 
		//spin_lock(&rev_head_tmp->lock);			
		buffer_cnt += skb_queue_len(&per_cpu(per_cpu_input_pkt_queue, cpu)); 
		buffer_cnt += skb_queue_len(&per_cpu(per_cpu_process_queue, cpu));
		//spin_unlock(&rev_head_tmp->lock);
	}
	#endif
	#if 0
	if(debug_cnt %5000 == 0)
	{
		printk(KERN_ERR"2 input buffer is %d,prealloc cnt %d\n",buffer_cnt,skb_queue_len(&priv->rx_prealloc));
		printk(KERN_ERR"cpu 2 input %d,process %d,cpu 3 input %d,process %d\n",skb_queue_len(&per_cpu(per_cpu_input_pkt_queue, 2))
			,skb_queue_len(&per_cpu(per_cpu_process_queue, 2)),skb_queue_len(&per_cpu(per_cpu_input_pkt_queue, 3))
			,skb_queue_len(&per_cpu(per_cpu_process_queue, 3)));
	}
	#endif
	if(buffer_cnt > (GETH_INPUT_BUFFER_MAX + 4096))
	{
		free_cnt = buffer_cnt -(GETH_INPUT_BUFFER_MAX + 512);
		while((--free_cnt > 0) && (skb = skb_dequeue(&buffer_head)))
		{
			dev_kfree_skb(skb);
		}
	}
	#if GETH_DEBUG_SPARK
	priv->debug_info.malloc_recycle_cnt += skb_queue_len(&buffer_head);
	#endif
	//增加内存分配
	while ((buffer_cnt + alloc_cnt) < GETH_INPUT_BUFFER_MAX)
	{
		skb = netdev_alloc_skb_ip_align(priv->ndev, priv->buf_sz+16);
		if (unlikely(skb == NULL))
		{
			break;
		}
        skb_reserve(skb, 16);
		__skb_queue_tail(&buffer_head, skb);
		alloc_cnt++;
		#if GETH_DEBUG_SPARK
		priv->debug_info.malloc_total_cnt++;
		#endif
	}
	spin_lock_irqsave(&priv->input_pkt_queue.lock, flags);
	skb_queue_splice_tail_init(&buffer_head,&priv->rx_prealloc);
	spin_unlock_irqrestore(&priv->input_pkt_queue.lock,flags);
	#if 0
	if(debug_cnt %5000 == 0)
		printk(KERN_ERR"3 input buffer is %d,prealloc cnt %d\n",buffer_cnt,skb_queue_len(&priv->rx_prealloc));
	#endif
	return alloc_cnt;
}

static void geth_input_tsklet(unsigned long data)
{
	struct geth_priv *priv = (struct geth_priv *)data;
	int cnt;
	// 分发
	cnt = geth_dispatch(priv,GETH_INPUT_ONECE_MAX);
	#if GETH_DEBUG_SPARK
	cnt += skb_queue_len(&priv->input_process_queue);
	if(cnt == 0)
	{
		priv->debug_info.thread_0_cnt++;
	}
	else if(cnt < 16)
	{
		priv->debug_info.thread_16_cnt++;
	}
	else if(cnt < 128)
	{
		priv->debug_info.thread_128_cnt++;
	}
	else if(cnt < 256)
	{
		priv->debug_info.thread_256_cnt++;
	}
	else if(cnt < 512)
	{
		priv->debug_info.thread_512_cnt++;
	}
	else if(cnt < 1024)
	{
		priv->debug_info.thread_1024_cnt++;
	}
	else if(cnt < 2048)
	{
		priv->debug_info.thread_2048_cnt++;
	}
	else if(cnt < 4096)
	{
		priv->debug_info.thread_4096_cnt++;
	}
	else
	{
		priv->debug_info.thread_more_4096_cnt++;
	}
	#endif
	// 申请dma内存
	geth_input_mm(priv);
	rx_tsk_cnt++;
}


static int geth_input_thread(void *data)
{
	struct geth_priv *priv = (struct geth_priv *)data;
	cpumask_var_t mask;
	int cnt;

	cpumask_clear(&mask);
	cpumask_set_cpu(0,&mask);
	/*
	 * root may have changed our (kthreadd's) priority or CPU mask.
	 * The kernel thread should not inherit these properties.
	 */
	//sched_setscheduler_nocheck(current, SCHED_NORMAL, &param);
	set_cpus_allowed_ptr(current, mask);
		
	while (!kthread_should_stop()) {

		__set_current_state(TASK_RUNNING);

		geth_input_tsklet(data);
		#if GETH_DEBUG_SPARK
		cnt = atomic_read(&priv->input_process_cnt);
		if(cnt < 512)
		{
			priv->debug_info.process_512_cnt++;
		}
		else if(cnt < 1024)
		{
			priv->debug_info.process_1024_cnt++;
		}
		else if(cnt < 2048)
		{
			priv->debug_info.process_2048_cnt++;
		}
		else if(cnt < 4096)
		{
			priv->debug_info.process_4096_cnt++;
		}
		else
		{
			priv->debug_info.process_more_4096_cnt++;
		}
		#endif
		cnt = skb_queue_len(&priv->input_process_queue);
		if(cnt == 0)
		{
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
		}
	}
	__set_current_state(TASK_RUNNING);
	return 0;
}


static void wakeup_inputd(struct geth_priv *priv)
{
	/* Interrupts are disabled: no need to stop preemption */
	#if GMAC_USE_RX_TSKLET
	tasklet_hi_schedule(&priv->input_tsklet);
	#else
	struct task_struct *tsk = priv->input_task;

	if (tsk && tsk->state != TASK_RUNNING)
		wake_up_process(tsk);
	#endif
}

static int geth_free_input_mm(struct geth_priv *priv)
{
	int cpu;
	struct sk_buff *skb;
	struct sk_buff_head buffer_head;


	skb_queue_head_init(&buffer_head);
	for_each_possible_cpu(cpu)
	{ 
		skb_queue_splice_tail_init(&per_cpu(per_cpu_input_pkt_queue, cpu),&buffer_head);
		skb_queue_splice_tail_init(&per_cpu(per_cpu_process_queue, cpu),&buffer_head);
		skb_queue_splice_tail_init(&priv->rx_recycle,&buffer_head);
		skb_queue_splice_tail_init(&priv->rx_prealloc,&buffer_head);
		skb_queue_splice_tail_init(&priv->input_pkt_queue,&buffer_head);
		skb_queue_splice_tail_init(&priv->input_process_queue,&buffer_head);
		skb_queue_splice_tail_init(&priv->output_pkt_queue,&buffer_head);
	}
	while (skb = __skb_dequeue(&buffer_head)) 
	{
		dev_kfree_skb(skb);
	}
	atomic_set(&priv->input_process_cnt,0);
}
#else
static int geth_poll(struct napi_struct *napi, int budget)
{
	int work_done = 0;

    if(!g_eth_priv)
    {
        return work_done;
    }

	geth_tx_complete(g_eth_priv);
	work_done = geth_rx(g_eth_priv, budget);

	if (work_done < budget) {
		napi_complete(napi);
		sunxi_int_enable(g_eth_priv->base);
	}

	return work_done;
}
#endif

static int geth_change_mtu(struct net_device *ndev, int new_mtu)
{
	int max_mtu;

	if (netif_running(ndev)) {
		pr_err("%s: must be stopped to change its MTU\n", ndev->name);
		return -EBUSY;
	}

	max_mtu = SKB_MAX_HEAD(NET_SKB_PAD + NET_IP_ALIGN);

	if ((new_mtu < 46) || (new_mtu > max_mtu)) {
		pr_err("%s: invalid MTU, max MTU is: %d\n", ndev->name, max_mtu);
		return -EINVAL;
	}

	ndev->mtu = new_mtu;
	netdev_update_features(ndev);

	return 0;
}

static netdev_features_t geth_fix_features(struct net_device *ndev,
	netdev_features_t features)
{
	return features;
}

static void geth_set_rx_mode(struct net_device *ndev)
{
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct geth_priv *priv = netdev_priv(ndev);
#else
	struct geth_priv *priv = g_eth_priv;
#endif
	unsigned int value = 0;

///-
	pr_debug(KERN_INFO "%s: # mcasts %d, # unicast %d\n",
		 __func__, netdev_mc_count(ndev), netdev_uc_count(ndev));
	printk(KERN_ERR "%s: # mcasts %d, # unicast %d\n",
		 __func__, netdev_mc_count(ndev), netdev_uc_count(ndev));

	spin_lock(&priv->lock);
	if (ndev->flags & IFF_PROMISC)
		value = GETH_FRAME_FILTER_PR;
	else if ((netdev_mc_count(ndev) > HASH_TABLE_SIZE)
		   || (ndev->flags & IFF_ALLMULTI)) {
		value = GETH_FRAME_FILTER_PM;	/* pass all multi */
		sunxi_hash_filter(priv->base, ~0UL, ~0UL);
	} else if (!netdev_mc_empty(ndev)) {
		u32 mc_filter[2];
		struct netdev_hw_addr *ha;

		/* Hash filter for multicast */
		value = GETH_FRAME_FILTER_HMC;

		memset(mc_filter, 0, sizeof(mc_filter));
		netdev_for_each_mc_addr(ha, ndev) {
			/* The upper 6 bits of the calculated CRC are used to
			   index the contens of the hash table */
			int bit_nr =
			    bitrev32(~crc32_le(~0, ha->addr, 6)) >> 26;
			/* The most significant bit determines the register to
			 * use (H/L) while the other 5 bits determine the bit
			 * within the register. */
			mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
		}
		sunxi_hash_filter(priv->base, mc_filter[0], mc_filter[1]);
	}

	/* Handle multiple unicast addresses (perfect filtering)*/
	if (netdev_uc_count(ndev) > 16)
		/* Switch to promiscuous mode is more than 8 addrs
		   are required */
		value |= GETH_FRAME_FILTER_PR;
	else {
		int reg = 1;
		struct netdev_hw_addr *ha;

		netdev_for_each_uc_addr(ha, ndev) {
			sunxi_set_umac(priv->base, ha->addr, reg);
			reg++;
		}
	}

#ifdef FRAME_FILTER_DEBUG
	/* Enable Receive all mode (to debug filtering_fail errors) */
	value |= GETH_FRAME_FILTER_RA;
#endif
///-
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
    /*tcpdump抓包关闭后，会关闭混杂模式，这里要避免这种情况
      SBCMAIN无论在什么情况下都要开启混杂模式*/
	sunxi_set_filter(priv->base, value);
#endif
	spin_unlock(&priv->lock);
}

static void geth_tx_timeout(struct net_device *ndev)
{
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct geth_priv *priv = netdev_priv(ndev);
#else
	struct geth_priv *priv = g_eth_priv;
#endif

	geth_tx_err(priv);
}

static int geth_ioctl(struct net_device *ndev, struct ifreq *rq, int cmd)
{
	if (!netif_running(ndev))
		return -EINVAL;

	if (!ndev->phydev)
		return -EINVAL;

	return phy_mii_ioctl(ndev->phydev, rq, cmd);
}

/* Configuration changes (passed on by ifconfig) */
static int geth_config(struct net_device *ndev, struct ifmap *map)
{
	if (ndev->flags & IFF_UP)	/* can't act on a running interface */
		return -EBUSY;

	/* Don't allow changing the I/O address */
	if (map->base_addr != ndev->base_addr) {
		printk(KERN_WARNING "%s: can't change I/O address\n",
			ndev->name);
		return -EOPNOTSUPP;
	}

	/* Don't allow changing the IRQ */
	if (map->irq != ndev->irq) {
		printk(KERN_WARNING "%s: can't change IRQ number %d\n",
		       ndev->name, ndev->irq);
		return -EOPNOTSUPP;
	}

	return 0;
}

static int geth_set_mac_address(struct net_device *ndev, void *p)
{
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct geth_priv *priv = netdev_priv(ndev);
#else
	struct geth_priv *priv = g_eth_priv;
#endif
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(ndev->dev_addr, addr->sa_data, ndev->addr_len);
	sunxi_set_umac(priv->base, ndev->dev_addr, 0);

	return 0;
}

int geth_set_features(struct net_device *ndev, netdev_features_t features)
{
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct geth_priv *priv = netdev_priv(ndev);
#else
	struct geth_priv *priv = g_eth_priv;
#endif

	if (features & NETIF_F_LOOPBACK && netif_running(ndev))
		sunxi_mac_loopback(priv->base, 1);
	else
		sunxi_mac_loopback(priv->base, 0);

	return 0;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
/* Polling receive - used by NETCONSOLE and other diagnostic tools
 * to allow network I/O with interrupts disabled. */
static void geth_poll_controller(struct net_device *dev)
{
	disable_irq(dev->irq);
	geth_interrupt(dev->irq, dev);
	enable_irq(dev->irq);
}
#endif


static const struct net_device_ops geth_netdev_ops = {
	.ndo_init = NULL,
	.ndo_open = geth_open,
	.ndo_start_xmit = geth_xmit,
	.ndo_stop = geth_stop,
	.ndo_change_mtu = geth_change_mtu,
	.ndo_fix_features = geth_fix_features,
	.ndo_set_rx_mode = geth_set_rx_mode,
	.ndo_tx_timeout = geth_tx_timeout,
	.ndo_do_ioctl = geth_ioctl,
	.ndo_set_config = geth_config,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller = geth_poll_controller,
#endif
	.ndo_set_mac_address = geth_set_mac_address,
	.ndo_set_features = geth_set_features,
};

/*****************************************************************************
 *
 *
 ****************************************************************************/
static int geth_check_if_running(struct net_device *ndev)
{
	if (!netif_running(ndev))
		return -EBUSY;
	return 0;
}

static int geth_get_sset_count(struct net_device *netdev, int sset)
{
	int len;

	switch (sset) {
	case ETH_SS_STATS:
		len = 0;
		return len;
	default:
		return -EOPNOTSUPP;
	}
}

static int geth_ethtool_getsettings(struct net_device *ndev,
				      struct ethtool_cmd *cmd)
{

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct geth_priv *priv = netdev_priv(ndev);
#else
	struct geth_priv *priv = g_eth_priv;
#endif
#if 0
	struct phy_device *phy = ndev->phydev;
	int rc;

	if (phy == NULL) {
     ///-
		//netdev_err(ndev, "%s: %s: PHY is not registered\n",
		//       __func__, ndev->name);
		//return -ENODEV;
	}

	if (!netif_running(ndev)) {
		pr_err("%s: interface is disabled: we cannot track "
		"link speed / duplex setting\n", ndev->name);
		return -EBUSY;
	}

	cmd->transceiver = XCVR_INTERNAL;
	spin_lock_irq(&priv->lock);
	rc = phy_ethtool_gset(phy, cmd);
	spin_unlock_irq(&priv->lock);
#endif
#if defined(PRODUCT_SBCMAIN) || defined(PRODUCT_MTG2500MAIN) || \
    defined(PRODUCT_UC200) || defined(PRODUCT_SBC300MAIN) || \
    defined(PRODUCT_SBC1000MAIN)
    int port_phy, pLinkStatus, ret;
    u32 plink, pduplex, pnway, pSpeed, adver_mask, port_status;
    u32 support_mask = 0;

#if defined(PRODUCT_SBCMAIN) || defined(PRODUCT_MTG2500MAIN) || \
        defined(PRODUCT_UC200)
    if(!strncmp(ndev->name, "eth0", sizeof("eth0")))
    {
        port_phy = eth_map_phy2logic[0];
        cmd->phy_address = 0x00;
    }
    else if(!strncmp(ndev->name, "eth1", sizeof("eth1")))
    {
        port_phy = eth_map_phy2logic[1];
        cmd->phy_address = 0x01;
    }
#endif
#if defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC1000MAIN)
    if(!strncmp(ndev->name, "eth90", sizeof("eth90")))
    {
        port_phy = eth_map_phy2logic[0];
        cmd->phy_address = 0x00;
    }
    else if(!strncmp(ndev->name, "eth0", sizeof("eth0")))
    {
        port_phy = eth_map_phy2logic[1];
        cmd->phy_address = 0x01;
    }
    else if(!strncmp(ndev->name, "eth1", sizeof("eth1")))
    {
         port_phy = eth_map_phy2logic[2];
         cmd->phy_address = 0x02;
    }
    else if(!strncmp(ndev->name, "eth2", sizeof("eth2")))
    {
         port_phy = eth_map_phy2logic[3];
         cmd->phy_address = 0x03;
    }
    else if(!strncmp(ndev->name, "eth3", sizeof("eth3")))
    {
         port_phy = eth_map_phy2logic[4];
         cmd->phy_address = 0x04;
    }
#endif
    else
    {
        printk("%s:%d  %s get setting not support\n",__func__, __LINE__, ndev->name);
        return -1;
    }
    //spin_lock_irq(&priv->lock);
    ret =geth_getPortStatus(port_phy,&pSpeed, &plink, &pduplex, &pnway);
    if(ret != 0)
    {
        printk("geth_getPortStatus error\n");
        return -1;
    }
              
    if(pSpeed == 0)
    {
        cmd->speed = SPEED_10;
    }
    else if(pSpeed == 1)
    {
        cmd->speed = SPEED_100;
    }
    else if(pSpeed == 2)
    {
        cmd->speed = SPEED_1000;
    }
    else
    {
        printk("speed isn't unknown\n");
    }

    cmd->duplex = pduplex;       
    cmd->autoneg = pnway;
    cmd->port = PORT_MII;
    cmd->transceiver = XCVR_EXTERNAL;

    ret = geth_port_phyAutoNegoAbility_get(port_phy, &adver_mask);
    if(ret != 0)
    {
        printk("geth_port_phyAutoNegoAbility_get error\n");
        return -1;
    }
    ret = geth_phy_status_get(port_phy, &support_mask);
    if(ret != 0)
    {
        printk("geth_phy_status_get error\n");
        return -1;
    }

    cmd->advertising = adver_mask;      
    cmd->supported = support_mask;

    //spin_unlock_irq(&priv->lock);
#endif
	return 0;
}

static int geth_ethtool_setsettings(struct net_device *ndev,
				      struct ethtool_cmd *cmd)
{
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct geth_priv *priv = netdev_priv(ndev);
#else
	struct geth_priv *priv = g_eth_priv;
#endif
	struct phy_device *phy = ndev->phydev;
	int rc;

	spin_lock(&priv->lock);
	rc = phy_ethtool_sset(phy, cmd);
	spin_unlock(&priv->lock);

	return rc;
}

static void geth_ethtool_getdrvinfo(struct net_device *ndev,
				      struct ethtool_drvinfo *info)
{
	strlcpy(info->driver, "sunxi_geth", sizeof(info->driver));

#define DRV_MODULE_VERSION "SUNXI Gbgit driver V1.1"

	strcpy(info->version, DRV_MODULE_VERSION);
	info->fw_version[0] = '\0';
}

 u32 geth_ethtool_op_get_link(struct net_device *ndev)
{
#if defined(PRODUCT_SBCMAIN) || defined(PRODUCT_MTG2500MAIN) || \
     defined(PRODUCT_UC200) || defined(PRODUCT_SBC300MAIN) || \
     defined(PRODUCT_SBC1000MAIN)
       int port_phy, pLinkStatus, ret;

#if defined(PRODUCT_SBCMAIN) || defined(PRODUCT_MTG2500MAIN) || \
            defined(PRODUCT_UC200)
       if(!strncmp(ndev->name, "eth0", sizeof("eth0")))
       {
            port_phy = eth_map_phy2logic[0];
       }
       else if(!strncmp(ndev->name, "eth1", sizeof("eth1")))
       {
            port_phy = eth_map_phy2logic[1];
       }
#endif
#if defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC1000MAIN)
       if(!strncmp(ndev->name, "eth90", sizeof("eth90")))
       {
            port_phy = eth_map_phy2logic[0];
       }
       else if(!strncmp(ndev->name, "eth0", sizeof("eth0")))
       {
            port_phy = eth_map_phy2logic[1];
       }
       else if(!strncmp(ndev->name, "eth1", sizeof("eth1")))
       {
            port_phy = eth_map_phy2logic[2];
       }
       else if(!strncmp(ndev->name, "eth2", sizeof("eth2")))
       {
            port_phy = eth_map_phy2logic[3];
       }
       else if(!strncmp(ndev->name, "eth3", sizeof("eth3")))
       {
            port_phy = eth_map_phy2logic[4];
       }
#endif
       else
       {
            printk("%s:%d  %s get setting not support\n",__func__, __LINE__, ndev->name);
            return -1;
       }
       
       ret = geth_link_status_get(port_phy, &pLinkStatus);  
       if(ret != 0)
       {
            printk("geth_phy_status_get error\n");
            return -1;
       }
       return pLinkStatus;
#else 
        return 0;
#endif
}
static const struct ethtool_ops geth_ethtool_ops = {
	.begin = geth_check_if_running,
	.get_settings = geth_ethtool_getsettings,
	.set_settings = geth_ethtool_setsettings,
	.get_link = geth_ethtool_op_get_link,
	.get_pauseparam = NULL,
	.set_pauseparam = NULL,
	.get_ethtool_stats = NULL,
	.get_strings = NULL,
	.get_wol = NULL,
	.set_wol = NULL,
	.get_sset_count = geth_get_sset_count,
	.get_drvinfo = geth_ethtool_getdrvinfo,
};


/*****************************************************************************
 *
 *
 ****************************************************************************/
static int geth_script_parse(struct platform_device *pdev)
{
#ifdef CONFIG_GETH_SCRIPT_SYS
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct geth_priv *priv = netdev_priv(ndev);
#else
	struct geth_priv *priv = platform_get_drvdata(pdev);
#endif
	script_item_value_type_e  type;
	script_item_u item;
#ifdef CONFIG_GETH_PHY_POWER
	char power[20];
	int cnt;
#endif

///-
#if 1
	type = script_get_item((char *)dev_name(&pdev->dev), "gmac_used", &item);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type)
		return -EINVAL;

	switch(item.val) {
	case 0:
		printk(KERN_WARNING "%s not be used\n", dev_name(&pdev->dev));
		return -EINVAL;
	case 1:
		priv->phy_ext = EXT_PHY;
		break;
	case 2:
	default:
		priv->phy_ext = INT_PHY;
		break;
	}
#endif

#ifdef CONFIG_GETH_PHY_POWER
	memset(power_tb, 0, sizeof(power_tb));
	for (cnt = 0; cnt < ARRAY_SIZE(power_tb); cnt++) {
		char *vol;
		size_t len;
		snprintf(power, 15, "gmac_power%u", (cnt+1));
		type = script_get_item((char *)dev_name(&pdev->dev), power, &item);
		if(SCIRPT_ITEM_VALUE_TYPE_STR != type)
			continue;

		len = strlen((char *)item.val);
		vol = strnchr((const char *)item.val, len, ':');
		if (vol) {
			len = (size_t)(vol - (char *)item.val);
			power_tb[cnt].vol = simple_strtoul(++vol, NULL, 0);
		}

		power_tb[cnt].name = kstrndup((char *)item.val, len, GFP_KERNEL);
	}
#endif

	/* Default mode is PHY_INTERFACE_MODE_RGMII */
	priv->phy_interface = PHY_INTERFACE_MODE_RGMII;
///-
#if 1
	type = script_get_item((char *)dev_name(&pdev->dev), "gmac_mode", &item);
	if (SCIRPT_ITEM_VALUE_TYPE_STR == type) {
		if (!strncasecmp((char *)item.val, "MII", 3))
			priv->phy_interface = PHY_INTERFACE_MODE_MII;
		else if(!strncasecmp((char *)item.val, "GMII", 4))
			priv->phy_interface = PHY_INTERFACE_MODE_GMII;
		else if(!strncasecmp((char *)item.val, "RMII", 4))
			priv->phy_interface = PHY_INTERFACE_MODE_RMII;
	}

	if (priv->phy_ext == INT_PHY)
		priv->phy_interface = PHY_INTERFACE_MODE_MII;
#endif
#endif
	return 0;
}

static int geth_sys_request(struct platform_device *pdev)
{
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct geth_priv *priv = netdev_priv(ndev);
#else
	struct geth_priv *priv = platform_get_drvdata(pdev);
#endif
	int ret = 0;
	struct resource *res;
///-
    uint32_t reg_val;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "geth_extclk");
	if (unlikely(!res)){
		ret = -ENODEV;
		printk(KERN_ERR "Failed to get gmac clk reg!\n");
		goto out;
	}

	priv->geth_extclk = ioremap(res->start, resource_size(res));
	if (unlikely(!priv->geth_extclk)) {
		ret = -ENOMEM;
		printk(KERN_ERR "Failed to ioremap the address of gmac register\n");
		goto out;
	}

#ifndef CONFIG_GETH_CLK_SYS
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "geth_clk");
	if (unlikely(!res)){
		ret = -ENODEV;
		printk(KERN_ERR "Failed to get gmac clk bus!\n");
		goto clk_err;
	}
	priv->clkbase = ioremap(res->start, resource_size(res));
	if (unlikely(!priv->clkbase)) {
		ret = -ENOMEM;
		goto clk_err;
	}
#else
	priv->geth_clk = clk_get(&pdev->dev, GMAC_CLK);
	if (unlikely(!priv->geth_clk || IS_ERR(priv->geth_clk))) {
		printk(KERN_ERR "ERROR: Get clock is failed!\n");
		ret = -EINVAL;
		goto out;
	}

	if (priv->phy_ext == INT_PHY) {
		priv->ephy_clk = clk_get(&pdev->dev, EPHY_CLK);
		if (unlikely(!priv->ephy_clk || IS_ERR(priv->ephy_clk))) {
			printk(KERN_ERR "ERROR: Get ephy clock is failed\n");
			ret = -EINVAL;
			clk_put(priv->geth_clk);
			goto out;
		}
	}
#endif

///-
//#ifndef CONFIG_GETH_SCRIPT_SYS
#if 1
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "geth_pio");
	if (unlikely(!res)){
		ret = -ENODEV;
		goto pin_err;
	}

	priv->gpiobase = ioremap(res->start, resource_size(res));
	if (unlikely(!priv->gpiobase)) {
		printk(KERN_ERR "%s: ERROR: memory mapping failed", __func__);
		ret = -ENOMEM;
		goto pin_err;
	}
///-
#if 0
	writel(0x22222222, priv->gpiobase + PA_CFG0);
	writel(0x22222222, priv->gpiobase + PA_CFG1);
	writel(0x00000022 |
		((readl(priv->gpiobase + PA_CFG2) >> 8) << 8),
		priv->gpiobase + PA_CFG2);
#endif
///-
    writel(0x44444477, priv->gpiobase + 0x6C);
    writel(0x74444777, priv->gpiobase + 0x70);
#ifdef USE_MDIO
    writel(0x44444477, priv->gpiobase + 0x74);
#else
    writel(0x11444477, priv->gpiobase + 0x74);
    reg_val = readl(priv->gpiobase + 0x8C);
    reg_val &= ~((0x3 << 12) | (0x3 << 14));
    reg_val |= (0x1 << 12) | (0x1 << 14);
    writel(reg_val, priv->gpiobase + 0x8C);
#endif
#else
	if (priv->phy_ext != INT_PHY) {
		priv->pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
		if (IS_ERR_OR_NULL(priv->pinctrl)) {
			printk(KERN_ERR "gmac: devm_pinctrl is failed\n");
			ret = -EINVAL;
			goto pin_err;
		}
	}
#endif

///-
    /*rtl8370 up*/
#ifndef USE_PHY_8201
   reg_val = readl(priv->gpiobase + 0x94);
   reg_val &= ~(0xf << 4);
   reg_val |= (0x1 << 4);
   writel(reg_val, priv->gpiobase + 0x94);
   reg_val = readl(priv->gpiobase + 0xAC);
   reg_val &= ~(0x3 << 18);
   reg_val |= (1 << 18);
   writel(reg_val, priv->gpiobase + 0xAC);
   reg_val = readl(priv->gpiobase + 0xA0);
   reg_val |= (0x1 << 9);
   writel(reg_val, priv->gpiobase + 0xA0);
#endif

	return 0;

pin_err:
#ifndef CONFIG_GETH_CLK_SYS
	iounmap(priv->clkbase);
clk_err:
#endif
	iounmap(priv->geth_extclk);
out:
	return ret;
}

static void geth_sys_release(struct platform_device *pdev)
{
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct geth_priv *priv = netdev_priv(ndev);
#else
	struct geth_priv *priv = platform_get_drvdata(pdev);
#endif

///-
//#ifndef CONFIG_GETH_SCRIPT_SYS
#if 1
	iounmap((void *)priv->gpiobase);
#else
	if (!IS_ERR_OR_NULL(priv->pinctrl))
		devm_pinctrl_put(priv->pinctrl);
#endif

	iounmap(priv->geth_extclk);

#ifndef CONFIG_GETH_CLK_SYS
	iounmap((void *)priv->clkbase);
#else
	if (priv->phy_ext == INT_PHY && priv->ephy_clk)
		clk_put(priv->ephy_clk);

	if (priv->geth_clk)
		clk_put(priv->geth_clk);
#endif
}

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
static int geth_statistics_show_proc(struct seq_file *m, void *v)
{
	struct inode *inode = m->private;

	struct proc_dir_entry *dp = PDE(inode);
	struct geth_priv *priv = dp->data;
	int i;

	/* Transmit errors */
	seq_printf(m, "\n\nTransmit errors\n");
	seq_printf(m, "tx_underflow:               %ld\n", priv->xstats.tx_underflow);
	seq_printf(m, "tx_carrier:                 %ld\n", priv->xstats.tx_carrier);
	seq_printf(m, "tx_losscarrier:             %ld\n", priv->xstats.tx_losscarrier);
	seq_printf(m, "vlan_tag:                   %ld\n", priv->xstats.vlan_tag);
	seq_printf(m, "tx_deferred:                %ld\n", priv->xstats.tx_deferred);
	seq_printf(m, "tx_vlan:                    %ld\n", priv->xstats.tx_vlan);
	seq_printf(m, "tx_jabber:                  %ld\n", priv->xstats.tx_jabber);
	seq_printf(m, "tx_frame_flushed:           %ld\n", priv->xstats.tx_frame_flushed);
	seq_printf(m, "tx_payload_error:           %ld\n", priv->xstats.tx_payload_error);
	seq_printf(m, "tx_ip_header_error:         %ld\n", priv->xstats.tx_ip_header_error);

	/* Receive errors */
	seq_printf(m, "\n\nReceive errors\n");
	seq_printf(m, "rx_desc:                    %ld\n", priv->xstats.rx_desc);
	seq_printf(m, "sa_filter_fail:             %ld\n", priv->xstats.sa_filter_fail);
	seq_printf(m, "overflow_error:             %ld\n", priv->xstats.overflow_error);
	seq_printf(m, "ipc_csum_error:             %ld\n", priv->xstats.ipc_csum_error);
	seq_printf(m, "rx_collision:               %ld\n", priv->xstats.rx_collision);
	seq_printf(m, "rx_crc:                     %ld\n", priv->xstats.rx_crc);
	seq_printf(m, "dribbling_bit:              %ld\n", priv->xstats.dribbling_bit);
	seq_printf(m, "rx_length:                  %ld\n", priv->xstats.rx_length);
	seq_printf(m, "rx_mii:                     %ld\n", priv->xstats.rx_mii);
	seq_printf(m, "rx_multicast:               %ld\n", priv->xstats.rx_multicast);
	seq_printf(m, "rx_gmac_overflow:           %ld\n", priv->xstats.rx_gmac_overflow);
	seq_printf(m, "rx_watchdog:                %ld\n", priv->xstats.rx_watchdog);
	seq_printf(m, "da_rx_filter_fail:          %ld\n", priv->xstats.da_rx_filter_fail);
	seq_printf(m, "sa_rx_filter_fail:          %ld\n", priv->xstats.sa_rx_filter_fail);
	seq_printf(m, "rx_missed_cntr:             %ld\n", priv->xstats.rx_missed_cntr);
	seq_printf(m, "rx_overflow_cntr:           %ld\n", priv->xstats.rx_overflow_cntr);
	seq_printf(m, "rx_vlan:                    %ld\n", priv->xstats.rx_vlan);

	/* Tx/Rx IRQ errors */
	seq_printf(m, "\n\nTx/Rx IRQ errors\n");
	seq_printf(m, "tx_undeflow_irq:            %ld\n", priv->xstats.tx_undeflow_irq);
	seq_printf(m, "tx_process_stopped_irq:     %ld\n", priv->xstats.tx_process_stopped_irq);
	seq_printf(m, "tx_jabber_irq:              %ld\n", priv->xstats.tx_jabber_irq);
	seq_printf(m, "rx_overflow_irq:            %ld\n", priv->xstats.rx_overflow_irq);
	seq_printf(m, "rx_buf_unav_irq:            %ld\n", priv->xstats.rx_buf_unav_irq);
	seq_printf(m, "rx_process_stopped_irq:     %ld\n", priv->xstats.rx_process_stopped_irq);
	seq_printf(m, "rx_watchdog_irq:            %ld\n", priv->xstats.rx_watchdog_irq);
	seq_printf(m, "tx_early_irq:               %ld\n", priv->xstats.tx_early_irq);
	seq_printf(m, "fatal_bus_error_irq:        %ld\n", priv->xstats.fatal_bus_error_irq);

	/* Extra info */
	seq_printf(m, "\n\nExtra info\n");
	seq_printf(m, "threshold:                  %ld\n", priv->xstats.threshold);
	seq_printf(m, "tx_pkt_n:                   %ld\n", priv->xstats.tx_pkt_n);
	seq_printf(m, "rx_pkt_n:                   %ld\n", priv->xstats.rx_pkt_n);
	seq_printf(m, "poll_n:                     %ld\n", priv->xstats.poll_n);
	seq_printf(m, "sched_timer_n:              %ld\n", priv->xstats.sched_timer_n);
	seq_printf(m, "normal_irq_n:               %ld\n", priv->xstats.normal_irq_n);



	/* Extra info */
	seq_printf(m, "\n\nExtra info\n");
	seq_printf(m, "threshold:                  %ld\n", priv->xstats.threshold);
	seq_printf(m, "tx_pkt_n:                   %ld\n", priv->xstats.tx_pkt_n);
	seq_printf(m, "rx_pkt_n:                   %ld\n", priv->xstats.rx_pkt_n);
	seq_printf(m, "poll_n:                     %ld\n", priv->xstats.poll_n);
	seq_printf(m, "sched_timer_n:              %ld\n", priv->xstats.sched_timer_n);
	seq_printf(m, "normal_irq_n:               %ld\n", priv->xstats.normal_irq_n);


	seq_printf(m, "\n\eth irq\n");
	for(i=0;i<64;i++)
	{
		seq_printf(m, "irq recv %d,cnt %d:\n", i,in_cnt[i]);
	}

	return 0;
}


static int geth_statistics_open(struct inode *inode, struct file *file)
{
	return single_open(file, geth_statistics_show_proc, inode);
}


static const struct file_operations geth_statistics_op = {
	.open		= geth_statistics_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

#if GETH_DEBUG_SPARK
static int geth_debug_info_show_proc(struct seq_file *m, void *v)
{
	struct inode *inode = m->private;

	struct proc_dir_entry *dp = PDE(inode);
	struct geth_priv *priv = dp->data;

	seq_printf(m, "input_total_cnt:            %ld\n", priv->debug_info.input_total_cnt);
	/* input buffer debug */
	seq_printf(m, "\n\ninput buffer debug\n");
	seq_printf(m, "buffer_error_cnt:           %ld\n", priv->debug_info.buffer_error_cnt);
	seq_printf(m, "buffer_512_cnt:             %ld\n", priv->debug_info.buffer_512_cnt);
	seq_printf(m, "buffer_1024_cnt:            %ld\n", priv->debug_info.buffer_1024_cnt);
	seq_printf(m, "buffer_2048_cnt:            %ld\n", priv->debug_info.buffer_2048_cnt);
	seq_printf(m, "buffer_4096_cnt:            %ld\n", priv->debug_info.buffer_4096_cnt);
	seq_printf(m, "buffer_more_4096_cnt:       %ld\n", priv->debug_info.buffer_more_4096_cnt);

	/* input thread debug */
	seq_printf(m, "\n\ninput thread debug\n");
	seq_printf(m, "thread_0_cnt:               %ld\n", priv->debug_info.thread_0_cnt);
	seq_printf(m, "thread_16_cnt:              %ld\n", priv->debug_info.thread_16_cnt);
	seq_printf(m, "thread_128_cnt:             %ld\n", priv->debug_info.thread_128_cnt);
	seq_printf(m, "thread_256_cnt:             %ld\n", priv->debug_info.thread_256_cnt);
	seq_printf(m, "thread_512_cnt:             %ld\n", priv->debug_info.thread_512_cnt);
	seq_printf(m, "thread_1024_cnt:            %ld\n", priv->debug_info.thread_1024_cnt);
	seq_printf(m, "thread_2048_cnt:            %ld\n", priv->debug_info.thread_2048_cnt);
	seq_printf(m, "thread_4096_cnt:            %ld\n", priv->debug_info.thread_4096_cnt);
	seq_printf(m, "thread_more_4096_cnt:       %ld\n", priv->debug_info.thread_more_4096_cnt);
	

	/* input malloc debug */
	seq_printf(m, "\n\ninput malloc debug\n");
	seq_printf(m, "malloc_total_cnt:           %ld\n", priv->debug_info.malloc_total_cnt);
	seq_printf(m, "malloc_recycle_cnt:         %ld\n", priv->debug_info.malloc_recycle_cnt);

	
	/* input process debug */
	seq_printf(m, "\n\nExtra info\n");
	seq_printf(m, "process_512_cnt:             %ld\n", priv->debug_info.process_512_cnt);
	seq_printf(m, "process_1024_cnt:            %ld\n", priv->debug_info.process_1024_cnt);
	seq_printf(m, "process_2048_cnt:            %ld\n", priv->debug_info.process_2048_cnt);
	seq_printf(m, "process_4096_cnt:            %ld\n", priv->debug_info.process_4096_cnt);
	seq_printf(m, "process_more_4096_cnt:       %ld\n", priv->debug_info.process_more_4096_cnt);
	return 0;
}



static int geth_debug_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, geth_debug_info_show_proc, inode);
}


static const struct file_operations geth_debug_info_op = {
	.open		= geth_debug_info_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};
#endif
#endif

static int geth_debug_print_open(struct inode *ino, struct file *file)
{
    return 0;
}

static ssize_t geth_debug_print_write(struct file *file, const char __user *buf, size_t size, loff_t *ofs)
{
    if(size > 0)
    {
        if(buf[0] == '1')
        {
            geth_debug_print_on = 1;
        }
        else if(buf[0] == '0')
        {
            geth_debug_print_on = 0;
        }
    }
    return size;
}

static int geth_debug_print_release(struct inode *ino, struct file *file)
{
    return 0;
}

static const struct file_operations geth_debug_print = {
	.open		= geth_debug_print_open,
	.write		= geth_debug_print_write,
	.release	= geth_debug_print_release,
};
/**
 * geth_probe
 * @pdev: platform device pointer
 * Description: the driver is initialized through platform_device.
 */
static int geth_probe(struct platform_device *pdev)
{
///-
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	int ret = 0;
	int irq = 0;
	struct resource *res;
	struct net_device *ndev = NULL;
	struct geth_priv *priv;

	ndev = alloc_etherdev(sizeof(struct geth_priv));
	if (!ndev) {
		printk(KERN_ERR "Error: Failed to alloc netdevice\n");
		return -ENOMEM;
	}
	SET_NETDEV_DEV(ndev, &pdev->dev);
	priv = netdev_priv(ndev);
	platform_set_drvdata(pdev, ndev);

	/* Must set private data to pdev, before call it */
	ret = geth_script_parse(pdev);
	if (ret)
		goto out_err;

	ret = geth_sys_request(pdev);
	if (ret)
		goto out_err;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "geth_io");
	if (!res) {
		ret =  -ENODEV;
		goto res_err;
	}

	if (!request_mem_region(res->start, resource_size(res), pdev->name)) {
		pr_err("%s: ERROR: memory allocation failed"
		       "cannot get the I/O addr 0x%x\n",
		       __func__, (unsigned int)res->start);
		ret = -EBUSY;
		goto res_err;
	}

	priv->base = ioremap(res->start, resource_size(res));
	if (!priv->base) {
		pr_err("%s: ERROR: memory mapping failed", __func__);
		ret = -ENOMEM;
		goto map_err;
	}

	/* Get the MAC information */
	irq = platform_get_irq_byname(pdev, "geth_irq");
	if (irq == -ENXIO) {
		printk(KERN_ERR "%s: ERROR: MAC IRQ configuration "
		       "information not found\n", __func__);
		ret = -ENXIO;
		goto irq_err;
	}
	ret = request_irq(irq, geth_interrupt, IRQF_SHARED,
			dev_name(&pdev->dev), ndev);
	if (unlikely(ret < 0)) {
		netdev_err(ndev, "Could not request irq %d, error: %d\n",
				ndev->irq, ret);
		goto irq_err;
	}

	/* setup the netdevice, fill the field of netdevice */
	ether_setup(ndev);
	ndev->netdev_ops = &geth_netdev_ops;
	SET_ETHTOOL_OPS(ndev, &geth_ethtool_ops);
	ndev->base_addr = (unsigned long)priv->base;
	ndev->irq = irq;

	priv->ndev = ndev;
	priv->dev = &pdev->dev;

	/* TODO: support the VLAN frames */
	ndev->hw_features = NETIF_F_SG | NETIF_F_HIGHDMA | NETIF_F_IP_CSUM |
				NETIF_F_IPV6_CSUM | NETIF_F_RXCSUM;

	ndev->features |= ndev->hw_features;
	ndev->hw_features |= NETIF_F_LOOPBACK;
	ndev->priv_flags |= IFF_UNICAST_FLT;

	ndev->watchdog_timeo = msecs_to_jiffies(watchdog);
	#if GETH_INPUT_SMP	
	{
		int cpu;
		
		for_each_possible_cpu(cpu)
		{		
			netif_napi_add(ndev, &priv->napi[cpu], geth_poll,  BUDGET);
		}	
	}
	#else
	netif_napi_add(ndev, &priv->napi, geth_poll,  BUDGET);
	#endif

	spin_lock_init(&priv->lock);
	spin_lock_init(&priv->tx_lock);
	spin_lock_init(&priv->rx_lock);

	/* The last val is mdc clock ratio */
	sunxi_geth_register((void *)ndev->base_addr, HW_VERSION, 0x03);

	skb_queue_head_init(&priv->input_pkt_queue);
	skb_queue_head_init(&priv->input_process_queue);
	skb_queue_head_init(&priv->output_pkt_queue);
	atomic_set(&priv->input_process_cnt,0);
	#if 1
	/* initialize tasklet for xmit */
	tasklet_init(&priv->xmit_tsklet, geth_xmit_tsklet,
		     (unsigned long) priv);

	
	/* initialize tasklet for input */
	priv->recv_dispatch = dispatch_match;//geth_default_dispatch;
	
	#if GMAC_USE_RX_TSKLET
	tasklet_init(&priv->input_tsklet, geth_input_tsklet,
		     (unsigned long) priv);
	#endif
	
	#endif
	{
		int cpu;
		
		for_each_possible_cpu(cpu)
		{
			skb_queue_head_init(&per_cpu(per_cpu_input_pkt_queue, cpu));
			skb_queue_head_init(&per_cpu(per_cpu_process_queue, cpu));
			skb_queue_head_init(&per_cpu(per_cpu_output_pkt_queue, cpu));
		}
	}
	ret = register_netdev(ndev);
	if (ret) {
		#if GETH_INPUT_SMP	
		{
			int cpu;
			
			for_each_possible_cpu(cpu)
			{		
				netif_napi_del(&priv->napi[cpu]);
			}	
		}
		#else
		netif_napi_del(&priv->napi);
		#endif
		printk(KERN_ERR "Error: Register %s failed\n", ndev->name);
		goto reg_err;
	}

	/* Before open the device, the mac address is be set */
	geth_check_addr(ndev, mac_str);


	proc_create_data("sunxi_geth", S_IRUGO, NULL, &geth_statistics_op,priv);
	#if GETH_DEBUG_SPARK
	proc_create_data("sunxi_geth_debug", S_IRUGO, NULL, &geth_debug_info_op,priv);
	#endif
    //proc_create_data("sunxi_dbg_print", S_IRUGO, NULL, &geth_debug_print, priv);
	proc_create("sunxi_dbg_print", 0, NULL, &geth_debug_print);
	return 0;

reg_err:
	free_irq(irq, ndev);
irq_err:
	iounmap(priv->base);
map_err:
	release_mem_region(res->start, resource_size(res));
res_err:
	geth_sys_release(pdev);
out_err:
	platform_set_drvdata(pdev, NULL);
	free_netdev(ndev);

	return ret;
#else
    int ret = 0, i;
    int irq = 0;
    struct resource *res;

    g_eth_priv = kmalloc(sizeof(struct geth_priv), GFP_KERNEL);
    if(NULL == g_eth_priv)
    {
		printk(KERN_ERR "Error: Failed to alloc g_eth_priv\n");
        return ENOMEM;
    }
    memset(g_eth_priv, 0, sizeof(struct geth_priv));
	platform_set_drvdata(pdev, g_eth_priv);
    
	/* Must set private data to pdev, before call it */
	ret = geth_script_parse(pdev);
	if (ret)
		goto out_err;
    
	ret = geth_sys_request(pdev);
	if (ret)
		goto out_err;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "geth_io");
	if (!res) {
		ret =  -ENODEV;
		goto res_err;
	}

	if (!request_mem_region(res->start, resource_size(res), pdev->name)) {
		pr_err("%s: ERROR: memory allocation failed"
		       "cannot get the I/O addr 0x%x\n",
		       __func__, (unsigned int)res->start);
		ret = -EBUSY;
		goto res_err;
	}

	g_eth_priv->base = ioremap(res->start, resource_size(res));
	if (!g_eth_priv->base) {
		pr_err("%s: ERROR: memory mapping failed", __func__);
		ret = -ENOMEM;
		goto map_err;
	}

	/* Get the MAC information */
	irq = platform_get_irq_byname(pdev, "geth_irq");
	if (irq == -ENXIO) {
		printk(KERN_ERR "%s: ERROR: MAC IRQ configuration "
		       "information not found\n", __func__);
		ret = -ENXIO;
		goto irq_err;
	}
	ret = request_irq(irq, geth_interrupt, IRQF_SHARED,
			dev_name(&pdev->dev), &pdev->dev);
	if (unlikely(ret < 0)) {
		pr_err("Could not request irq %d, error: %d\n",
				irq, ret);
		goto irq_err;
	}

	g_eth_priv->dev = &pdev->dev;
    g_eth_priv->irq = irq;
	spin_lock_init(&g_eth_priv->lock);
	spin_lock_init(&g_eth_priv->tx_lock);

///-
    ret = geth_power_on(g_eth_priv);
    if (ret) {
        printk(KERN_ERR "Power on is failed\n");
        goto reg_err;
    }

    geth_clk_enable(g_eth_priv);

///-
#if 0
    ret = geth_phy_init(NULL);
    if (ret)
    {   
        goto desc_err;
    }
#endif

    ret = sunxi_mac_reset((void *)g_eth_priv->base, &sunxi_udelay, 10000);
    if (ret) {
        printk(KERN_ERR "Initialize hardware error\n");
        goto desc_err;
    }

    sunxi_mac_init(g_eth_priv->base, txmode, rxmode);

    if (!g_eth_priv->is_suspend) {
        skb_queue_head_init(&g_eth_priv->rx_recycle);
        ret = geth_dma_desc_init();
        if (ret) {
            ret = -EINVAL;
            goto desc_err;
        }
    }

    memset(g_eth_priv->dma_tx, 0, dma_desc_tx * sizeof(struct dma_desc));
    memset(g_eth_priv->dma_rx, 0, dma_desc_rx * sizeof(struct dma_desc));

    desc_init_chain(g_eth_priv->dma_rx, g_eth_priv->dma_rx_phy, dma_desc_rx);
    desc_init_chain(g_eth_priv->dma_tx, g_eth_priv->dma_tx_phy, dma_desc_tx);

    g_eth_priv->rx_clean = g_eth_priv->rx_dirty = 0;
    g_eth_priv->tx_clean = g_eth_priv->tx_dirty = 0;
    geth_rx_refill(NULL);

    /* Extra statistics */
    memset(&g_eth_priv->xstats, 0, sizeof(struct geth_extra_stats));

///-
#if 0
    if (ndev->phydev)
        phy_start(ndev->phydev);
#endif

    sunxi_start_rx(g_eth_priv->base, (unsigned long)((struct dma_desc *)
                g_eth_priv->dma_rx_phy + g_eth_priv->rx_dirty));
    sunxi_start_tx(g_eth_priv->base, (unsigned long)((struct dma_desc *)
                g_eth_priv->dma_tx_phy + g_eth_priv->tx_clean));

    for(i=0; i<MAX_NET_DEVICE; i++)
    {
///-
        //g_ndev[i] = alloc_etherdev(0);
        if((MAX_NET_DEVICE-1) == i)
        {
            g_ndev[i] = alloc_netdev_mqs(0, "eth99", ether_setup, 1, 8);
        }
#if defined(PRODUCT_SBC300MAIN) || defined(PRODUCT_SBC1000MAIN)
        else if(0 == i)
        {
            g_ndev[i] = alloc_netdev_mqs(0, "eth90", ether_setup, 1, 8);
        }
#endif
        else
        {
            g_ndev[i] = alloc_etherdev_mqs(0, 1, 8);
        }
        if (!g_ndev[i]) {
            printk(KERN_ERR "Error: Failed to alloc netdevice\n");
            goto reg_err;
        }
        SET_NETDEV_DEV(g_ndev[i], &pdev->dev);
        
        /* setup the netdevice, fill the field of netdevice */
        ether_setup(g_ndev[i]);
        g_ndev[i]->netdev_ops = &geth_netdev_ops;
        SET_ETHTOOL_OPS(g_ndev[i], &geth_ethtool_ops);
        g_ndev[i]->base_addr = (unsigned long)g_eth_priv->base;
        g_ndev[i]->irq = irq;
        
        /* TODO: support the VLAN frames */
        g_ndev[i]->hw_features = NETIF_F_SG | NETIF_F_HIGHDMA | NETIF_F_IP_CSUM |
                    NETIF_F_IPV6_CSUM | NETIF_F_RXCSUM;
        
        g_ndev[i]->features |= g_ndev[i]->hw_features;
        g_ndev[i]->hw_features |= NETIF_F_LOOPBACK;
        g_ndev[i]->priv_flags |= IFF_UNICAST_FLT;
        
        g_ndev[i]->watchdog_timeo = msecs_to_jiffies(watchdog);
                        
        ret = register_netdev(g_ndev[i]);
        if (ret) {
            printk(KERN_ERR "Error: Register %s failed\n", g_ndev[i]->name);
            goto reg_err;
        }
        
        /* Before open the device, the mac address is be set */
        geth_check_addr(g_ndev[i], mac_str);
    }

    /* The last val is mdc clock ratio */
    sunxi_geth_register((void *)g_eth_priv->base, HW_VERSION, 0x03);
    netif_napi_add(g_ndev[0], &g_eth_priv->napi, geth_poll,  BUDGET);

    napi_enable(&g_eth_priv->napi);

    /*将mac设置成混杂模式，接收所有报文*/
    sunxi_set_filter(g_eth_priv->base, GETH_FRAME_FILTER_PR);

    /* Enable the Rx/Tx */
    sunxi_mac_enable(g_eth_priv->base);
	proc_create("sunxi_dbg_print", 0, NULL, &geth_debug_print);
    return 0;
    
desc_err:
    geth_clk_disable(g_eth_priv);
reg_err:
	free_irq(irq, NULL);
irq_err:
	iounmap(g_eth_priv->base);
map_err:
	release_mem_region(res->start, resource_size(res));
res_err:
	geth_sys_release(pdev);
out_err:
    platform_set_drvdata(pdev, NULL);

    return ret;
#endif
}

static int geth_remove(struct platform_device *pdev)
{
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct geth_priv *priv = netdev_priv(ndev);
#else
	struct geth_priv *priv = g_eth_priv;
#endif
	struct resource *res;
    int i;
#if defined(CONFIG_GETH_PHY_POWER)

	for (i=0; i < ARRAY_SIZE(power_tb); i++) {
		if (power_tb[i].name)
			kfree(power_tb[i].name);
	}
#endif

#if (!defined(PRODUCT_SBCUSER)) && (!defined(PRODUCT_SBC300USER)) && \
    (!defined(PRODUCT_SBC1000USER))
	napi_disable(&priv->napi);

///-
#if 0
	/* Release PHY resources */
	geth_phy_release(NULL);
#endif

	/* Disable Rx/Tx */
	sunxi_mac_disable(priv->base);

	geth_clk_disable(priv);
	geth_power_off(priv);

	/* Release the DMA TX/RX socket buffers */
	geth_free_rx_sk(priv);
	geth_free_tx_sk(priv);

	/* Ensure that hareware have been stopped */
	if (!priv->is_suspend) {
		skb_queue_purge(&priv->rx_recycle);
		geth_free_dma_desc(priv);
	}
#endif

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	#if GETH_INPUT_SMP	
	{
		int cpu;
		
		for_each_possible_cpu(cpu)
		{		
			netif_napi_del(&priv->napi[cpu]);
		}	
	}
	#else
	netif_napi_del(&priv->napi);
	#endif
#else
	netif_napi_del(&priv->napi);
#endif
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	unregister_netdev(ndev);
#else
    for(i=0; i<MAX_NET_DEVICE; i++)
    {
    	unregister_netdev(g_ndev[i]);
    }
#endif

	iounmap(priv->base);
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	free_irq(ndev->irq, ndev);
#else
	free_irq(priv->irq, NULL);
#endif
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "geth_io");
	release_mem_region(res->start, resource_size(res));

	geth_sys_release(pdev);

	platform_set_drvdata(pdev, NULL);
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	free_netdev(ndev);
#else
    for(i=0; i<MAX_NET_DEVICE; i++)
    {
    	free_netdev(g_ndev[i]);
    }
#endif

	return 0;
}

static struct resource geth_resources[] = {
	{
		.name	= "geth_io",
		.start	= GETH_BASE,
		.end	= GETH_BASE + 0x1054,
		.flags	= IORESOURCE_MEM,
	},
#ifndef CONFIG_GETH_CLK_SYS
	{
		.name	= "geth_clk",
		.start	= CCMU_BASE,
		.end	= CCMU_BASE + 1024,
		.flags	= IORESOURCE_MEM,
	},
#endif
///-
//#ifndef CONFIG_GETH_SCRIPT_SYS
#if 1
	{
		.name	= "geth_pio",
		.start	= GPIO_BASE,
		.end	= GPIO_BASE + 0x200,
		.flags	= IORESOURCE_MEM,
	},
#endif
	{
		.name	= "geth_extclk",
		.start	= SYS_CTL_BASE,
		.end	= SYS_CTL_BASE + GETH_CLK_REG,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "geth_irq",
		.start	= SUNXI_IRQ_GMAC,
		.end	= SUNXI_IRQ_GMAC,
		.flags	= IORESOURCE_IRQ,
	}
};

static void geth_device_release(struct device *dev)
{
}

static u64 geth_dma_mask = 0xffffffffUL;
static struct platform_device geth_device = {
	.name = "gmac0",
	.id = -1,
	.resource = geth_resources,
	.num_resources = ARRAY_SIZE(geth_resources),
	.dev = {
		.release = geth_device_release,
		.platform_data = NULL,
		.dma_mask = &geth_dma_mask,
		.coherent_dma_mask = 0xffffffffUL,
	},
};

static struct platform_driver geth_driver = {
	.probe	= geth_probe,
	.remove = geth_remove,
	.driver = {
		   .name = "gmac0",
		   .owner = THIS_MODULE,
#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
		   .pm = &geth_pm_ops,
#endif
	},
};

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
static struct timer_list relayin_ticktimer;

/*
 * 
 * handler for tick_timer
 */
static void relayin_tick_timer(unsigned long date)
{ 
	int recv_cnt = 0;
	int xmit_cnt = 0;
	static int recv_old_cnt = 0;
	static int xmit_old_cnt =0;
	int i;
	relayin_ticktimer.expires	= jiffies + 100;
	add_timer(&relayin_ticktimer);
	recv_cnt = recv_cnt_spark;
	xmit_cnt = xmit_cnt_spark;
	printk(KERN_ERR"sunxi recv %d,total %d.xmit %d ,total %d.fail tx %d.tsk %d,rx %d,timer %d,int %d, at time %ld\n"
		,recv_cnt -recv_old_cnt,recv_cnt,xmit_cnt - xmit_old_cnt,xmit_cnt,xmit_fail_spark
		,rx_tsk_cnt,rx_int_cnt,rx_timer_cnt,rx_intterput_cnt,jiffies);
	recv_old_cnt = recv_cnt;
	xmit_old_cnt = xmit_cnt;
    return;
}
#endif

static int __init geth_init(void)
{
	int ret;

	ret = platform_device_register(&geth_device);
	if (ret)
		return ret;

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
	init_timer(&relayin_ticktimer);
    relayin_ticktimer.data     = 0;
	relayin_ticktimer.expires  = jiffies + 100;		// 1000ms
	relayin_ticktimer.function = relayin_tick_timer;
	//add_timer(&relayin_ticktimer);
	//测试中断效率
	{
		int i;
		
		for(i=0;i<64;i++)
		{
			in_cnt[i] = 0; 
		}
	}
#endif
	return platform_driver_register(&geth_driver);
}

static void __exit geth_exit(void)
{
	platform_driver_unregister(&geth_driver);
	platform_device_unregister(&geth_device);
}

#ifndef MODULE
static int __init set_mac_addr(char *str)
{
	char *p = str;

	if (str != NULL && strlen(str))
		memcpy(mac_str, p, 18);

	return 0;
}
__setup("mac_addr=", set_mac_addr);
#endif

module_init(geth_init);
module_exit(geth_exit);

MODULE_DESCRIPTION("SUNxI Gigabit Ethernet driver");
MODULE_AUTHOR("Sugar <shugeLinux@gmail.com>");
MODULE_LICENSE("Dual BSD/GPL");
