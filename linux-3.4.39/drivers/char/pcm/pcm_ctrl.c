#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf-sunxi.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <asm/dma.h>
#include <mach/sunxi-smc.h>
#include <mach/hardware.h>
#include <mach/sys_config.h>
#include <mach/platform.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/dma-mapping.h>
#include <linux/dma/sunxi-dma.h>
#include <linux/dmaengine.h>
#include <linux/pci.h>


#include "pcm_ctrl.h"

static struct class *pcmdrv_class;
static int pcmdrv_major =  233;

static struct pcm_global_t *pcm_global;
static struct pinctrl *daudio_pinctrl;
static script_item_u  *pin_daudio0_list;
unsigned int pin_count = 0;

extern int board_cpuid;


static struct gpio tdm_bus_gpios[] = {
    { GPIOB(4),  GPIOF_DIR_OUT, "TDM_LRCK" },
    { GPIOB(5),  GPIOF_DIR_OUT, "TDM_BCLK" },
    { GPIOB(6),  GPIOF_DIR_OUT, "TDM_DOUT" },
    { GPIOB(7),  GPIOF_DIR_OUT, "TDM_DIN" },
    { GPIOB(8),  GPIOF_DIR_OUT, "TDM_MCLK" },
};


static int pcm_global_config(void)
{
    if(NULL == pcm_global)
    {
        return -1;
    }

    pcm_global->mode = 0;
    pcm_global->loopback = 0;
    pcm_global->frametype = 0;

    pcm_global->pcm_lrckr_period = 128;
    pcm_global->pcm_lrck_period = 128;

    pcm_global->samp_res = 16;
    pcm_global->slot_width = 16;

    pcm_global->bclk_rate = 8192000;
    pcm_global->mclk_rate = 8192000;

    return 0;
}

static int pcm_reg_config(void)
{
    u32 reg_val;
    u32 mclk_div = 0;
	u32 bclk_div = 0;

    
    if(NULL == pcm_global)
    {
        return -1;
    }
    
    /* Digital Audio Control Register */
    reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOCTL);

    if(pcm_global->mode)
    {
        reg_val |= SUNXI_DAUDIOCTL_BCLKOUT;
        reg_val |= SUNXI_DAUDIOCTL_LRCKOUT;
        //reg_val |= SUNXI_DAUDIOCTL_LRCKROUT;
    }
    else
    {
        reg_val &= ~SUNXI_DAUDIOCTL_BCLKOUT;
        reg_val &= ~SUNXI_DAUDIOCTL_LRCKOUT;
        //reg_val &= ~SUNXI_DAUDIOCTL_LRCKROUT;
    }

    reg_val &= ~SUNXI_DAUDIOCTL_SDO3EN;
    reg_val &= ~SUNXI_DAUDIOCTL_SDO2EN;
    reg_val &= ~SUNXI_DAUDIOCTL_SDO1EN;
    reg_val &= ~SUNXI_DAUDIOCTL_SDO0EN;
    reg_val |= SUNXI_DAUDIOCTL_SDO0EN | SUNXI_DAUDIOCTL_SDO1EN | SUNXI_DAUDIOCTL_SDO2EN | SUNXI_DAUDIOCTL_SDO3EN;

    reg_val &= ~SUNXI_DAUDIOCTL_OUTMUTE;
    reg_val &= ~SUNXI_DAUDIOCTL_MODESEL(3);
    reg_val &= ~SUNXI_DAUDIOCTL_LOOP;
    if (pcm_global->loopback)
    {
        reg_val |= SUNXI_DAUDIOCTL_LOOP;
    }
    reg_val |= SUNXI_DAUDIOCTL_TXEN;
    reg_val |= SUNXI_DAUDIOCTL_RXEN;

    sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOCTL);


    /* Digital Audio Format Register0 */
    reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOFAT0);
    
    reg_val &= ~SUNXI_DAUDIOFAT0_SDI_SYNC_SEL;
    reg_val &= ~SUNXI_DAUDIOFAT0_LRCK_WIDTH;
    if(pcm_global->frametype)
        reg_val |= SUNXI_DAUDIOFAT0_LRCK_WIDTH;

    reg_val &= ~SUNXI_DAUDIOFAT0_LRCKR_PERIOD(0x3FF);
    reg_val |= SUNXI_DAUDIOFAT0_LRCKR_PERIOD(pcm_global->pcm_lrckr_period-1);

    reg_val |= SUNXI_DAUDIOFAT0_LRCK_POLAYITY;
    
    reg_val &= ~SUNXI_DAUDIOFAT0_LRCK_PERIOD(0x3FF);
    reg_val |= SUNXI_DAUDIOFAT0_LRCK_PERIOD(pcm_global->pcm_lrck_period-1);
    
    reg_val |= SUNXI_DAUDIOFAT0_BCLK_POLAYITY;
    
    /* sample reslotion */
	reg_val &= ~SUNXI_DAUDIOFAT0_SAMPLE_RES(7);
	if(pcm_global->samp_res == 16)
		reg_val |= SUNXI_DAUDIOFAT0_SAMPLE_RES(3);
	else if(pcm_global->samp_res == 20)
		reg_val |= SUNXI_DAUDIOFAT0_SAMPLE_RES(4);
	else if(pcm_global->samp_res == 24)
		reg_val |= SUNXI_DAUDIOFAT0_SAMPLE_RES(5);
	else if(pcm_global->samp_res == 32)
		reg_val |= SUNXI_DAUDIOFAT0_SAMPLE_RES(7);
	else
        reg_val |= SUNXI_DAUDIOFAT0_SAMPLE_RES(1);

    /* slot width select */
	reg_val &= ~SUNXI_DAUDIOFAT0_SLOT_WIDTH(7);
	if(pcm_global->slot_width == 16)
        reg_val |= SUNXI_DAUDIOFAT0_SLOT_WIDTH(3);
	else if(pcm_global->slot_width == 20)
		reg_val |= SUNXI_DAUDIOFAT0_SLOT_WIDTH(4);
	else if(pcm_global->slot_width == 24)
		reg_val |= SUNXI_DAUDIOFAT0_SLOT_WIDTH(5);
	else if(pcm_global->slot_width == 32)
		reg_val |= SUNXI_DAUDIOFAT0_SLOT_WIDTH(7);
	else
		reg_val |= SUNXI_DAUDIOFAT0_SLOT_WIDTH(1);

	sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOFAT0);


    /* Digital Audio Format Register1 */
    reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOFAT1);
    
    reg_val &= SUNXI_DAUDIOFAT1_RX_MLS;
    reg_val &= SUNXI_DAUDIOFAT1_TX_MLS;
    reg_val &= ~SUNXI_DAUDIOFAT1_SEXT(3);
    reg_val &= ~SUNXI_DAUDIOFAT1_RX_PDM(3);
    reg_val &= ~SUNXI_DAUDIOFAT1_TX_PDM(3);
    
    sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOFAT1);


    /* Digital Audio FIFO Control Register */
    reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOFCTL);

    reg_val &= ~SUNXI_DAUDIOFCTL_TXIM;
    reg_val &= ~SUNXI_DAUDIOFCTL_RXOM(3);
    reg_val |= SUNXI_DAUDIOFCTL_TXIM;
    reg_val |= SUNXI_DAUDIOFCTL_RXOM(1);
    
    sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOFCTL);


    /* Digital Audio Clock Divide Register */
	reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOCLKD);
    
	reg_val |= SUNXI_DAUDIOCLKD_MCLKOEN;

    bclk_div = ((8192000/pcm_global->bclk_rate));
    mclk_div = ((8192000/pcm_global->mclk_rate));

    reg_val &= ~SUNXI_DAUDIOCLKD_BCLKDIV(0xF);
    reg_val |= SUNXI_DAUDIOCLKD_BCLKDIV((bclk_div>>1) + 1);

    reg_val &= ~SUNXI_DAUDIOCLKD_MCLKDIV(0xF);
    reg_val |= SUNXI_DAUDIOCLKD_MCLKDIV((bclk_div>>1) + 1);
    
	sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOCLKD);


    /* Digital Audio Channel Configuration Register */
    reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOCHCFG);

    //reg_val |= SUNXI_TXCHCFG_TX_SLOT_HIZ;
    reg_val |= SUNXI_DAUDIOCHCFG_TX_STATE;
    
    reg_val &= ~SUNXI_DAUDIOCHCFG_RX_SLOT_NUM(7);
    reg_val |= SUNXI_DAUDIOCHCFG_RX_SLOT_NUM(7);

    reg_val &= ~SUNXI_DAUDIOCHCFG_TX_SLOT_NUM(7);
    reg_val |= SUNXI_DAUDIOCHCFG_TX_SLOT_NUM(7);

    sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOCHCFG);


    /* Digital Audio TXn Channel Configuration Register */
    /**** TX0 ****/
    reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOTX0CHSEL);
    
	reg_val &= ~SUNXI_DAUDIOTXn_OFFSET(3);
    reg_val |= SUNXI_DAUDIOTXn_OFFSET(1);

    reg_val &= ~SUNXI_DAUDIOTXn_CHEN(0xFF);

 #if defined(PRODUCT_MTG2500USER)
    printk(KERN_ERR "Product is MTG2500USER, board_cpuid = %d\n", board_cpuid);

    if (board_cpuid)
    {
        reg_val |= SUNXI_DAUDIOTXn_CHEN(0xAA);
    }
    else
    {
        reg_val |= SUNXI_DAUDIOTXn_CHEN(0x55);
    }
 #else
	reg_val |= SUNXI_DAUDIOTXn_CHEN(0xFF);
 #endif
    
    reg_val &= ~SUNXI_DAUDIOTXn_CHSEL(7);
    reg_val |= SUNXI_DAUDIOTXn_CHSEL(7);

    sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOTX0CHSEL);


    /* Digital Audio TXn Channel Mapping Register */
    /****  TX0 ****/
    reg_val = SUNXI_DAUDIOTXn_MAP_CH0(0) |
    		  SUNXI_DAUDIOTXn_MAP_CH1(1) |
    		  SUNXI_DAUDIOTXn_MAP_CH2(2) |
    		  SUNXI_DAUDIOTXn_MAP_CH3(3) |
    		  SUNXI_DAUDIOTXn_MAP_CH4(4) |
    		  SUNXI_DAUDIOTXn_MAP_CH5(5) |
    		  SUNXI_DAUDIOTXn_MAP_CH6(6) |
    		  SUNXI_DAUDIOTXn_MAP_CH7(7);
    sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOTX0CHMAP);

    
    /* Digital Audio RX Channel Select Register */
    reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIORXCHSEL);

    reg_val &= ~SUNXI_DAUDIORXCHSEL_RXOFFSET(3);
    reg_val |=  SUNXI_DAUDIORXCHSEL_RXOFFSET(1);
    
    reg_val &= ~SUNXI_DAUDIORXCHSEL_RXCHSEL(7);
    reg_val |=  SUNXI_DAUDIORXCHSEL_RXCHSEL(7);
    
    sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIORXCHSEL);


    /* Digital Audio RX Channel Mapping Register */
    reg_val = SUNXI_DAUDIORXCHMAP_CH0(0) |
    	 	  SUNXI_DAUDIORXCHMAP_CH1(1) |
    	 	  SUNXI_DAUDIORXCHMAP_CH2(2) |
    	 	  SUNXI_DAUDIORXCHMAP_CH3(3) |
    	 	  SUNXI_DAUDIORXCHMAP_CH4(4) |
    	 	  SUNXI_DAUDIORXCHMAP_CH5(5) |
    	 	  SUNXI_DAUDIORXCHMAP_CH6(6) |
    	 	  SUNXI_DAUDIORXCHMAP_CH7(7);
    sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIORXCHMAP);

    return 0;
}

static int pcm_rxctrl(int on)
{
	u32 reg_val;

    
	/*flush RX FIFO*/
	reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOFCTL);
	reg_val |= SUNXI_DAUDIOFCTL_FRX;
	sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOFCTL);
    
	/*clear RX counter*/
	sunxi_smc_writel(0, pcm_global->pcm_regs + SUNXI_DAUDIORXCNT);

#ifdef USE_DMA    
    if (on) {
        writel(SUNXI_DMA_CHAN_START, (pcm_global->dma_regs + SUNXI_DMA_CHN_EN(1)));

        /* enable DMA DRQ mode for record */
        reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOINT);
        reg_val |= SUNXI_DAUDIOINT_RXDRQEN;
        sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOINT);    /* DAUDIO RX ENABLE */
    } else {
        writel(SUNXI_DMA_CHAN_STOP, (pcm_global->dma_regs + SUNXI_DMA_CHN_EN(1)));

        /* DISBALE dma DRQ mode */
        reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOINT);
        reg_val &= ~SUNXI_DAUDIOINT_RXDRQEN;
        sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOINT);
    }
#else
    /*中断使能和关闭*/
    if (on)
    {
        reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOINT);
        reg_val &= ~0xf;
        reg_val |= (1<<0) | (1<<1) | (1<<2);
        sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOINT);
    }
    else
    {
        reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOINT);
        reg_val &= ~0xf;
        sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOINT);
    }
#endif

    return 0;
}

static int pcm_txctrl(int on)
{
	u32 reg_val;

    
	/*flush TX FIFO*/
	reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOFCTL);
	reg_val |= SUNXI_DAUDIOFCTL_FTX;
	sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOFCTL);
    
	/*clear TX counter*/
	sunxi_smc_writel(0, pcm_global->pcm_regs + SUNXI_DAUDIOTXCNT);

#ifdef USE_DMA   
    if (on) {
        writel(SUNXI_DMA_CHAN_START, (pcm_global->dma_regs + SUNXI_DMA_CHN_EN(0)));
        
        /* enable DMA DRQ mode for play */
        reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOINT);
        reg_val |= SUNXI_DAUDIOINT_TXDRQEN;
        sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOINT);
    } else {
        writel(SUNXI_DMA_CHAN_STOP, (pcm_global->dma_regs + SUNXI_DMA_CHN_EN(0)));

        /* DISBALE dma DRQ mode */
        reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOINT);
        reg_val &= ~SUNXI_DAUDIOINT_TXDRQEN;
        sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOINT);
    }
#else
    /*中断使能和关闭*/
	if (on)
	{
        reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOINT);
        reg_val &= ~(0xf << 4);
        reg_val |= (1<<4) | (1<<5) | (1<<6);
        sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOINT);
	}
	else
	{
        reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOINT);
        reg_val &= ~(0xf << 4);
        sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOINT);
	}
#endif
    
	return 0;
}

static int pcm_enable(void)
{
	u32 reg_val;

    pcm_rxctrl(1);
    pcm_txctrl(1);

    /*Global Enable Digital Audio Interface*/
    reg_val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOCTL);
    reg_val |= SUNXI_DAUDIOCTL_GEN;
    sunxi_smc_writel(reg_val, pcm_global->pcm_regs + SUNXI_DAUDIOCTL);
    
    return 0;
}

static int pcm_disable(void)
{
    pcm_rxctrl(0);
    pcm_txctrl(0);
    
    return 0;
}

static int dma_reg_dump(void)
{
    unsigned int val = 0, i;

    
    if (pcm_global->dma_regs == NULL) {
		return -ENXIO;
	}

    printk(KERN_ERR "DMA register dump:\n");
    
    val = readl(pcm_global->dma_regs + SUNXI_DMA_IRQ_EN);
    printk(KERN_ERR "  DMA_IRQ_EN            = 0x%08X\n", val);

    val = readl(pcm_global->dma_regs + SUNXI_DMA_IRQ_PEND);
    printk(KERN_ERR "  DMA_IRQ_PEND          = 0x%08X\n", val);

    val = readl(pcm_global->dma_regs + SUNXI_DMA_AUTO_GATE);
    printk(KERN_ERR "  DMA_AUTO_GATE         = 0x%08X\n", val);

    val = readl(pcm_global->dma_regs + SUNXI_DMA_SECURE);
    printk(KERN_ERR "  DMA_SECURE            = 0x%08X\n", val);

    val = readl(pcm_global->dma_regs + SUNXI_DMA_STA);
    printk(KERN_ERR "  DMA_STA               = 0x%08X\n", val);

    for(i=0; i<2; i++)
    {
        val = readl(pcm_global->dma_regs + SUNXI_DMA_CHN_EN(i));
        printk(KERN_ERR "  DMA_CHN%d_EN           = 0x%08X\n", i, val);

        val = readl(pcm_global->dma_regs + SUNXI_DMA_CHN_PAU(i));
        printk(KERN_ERR "  DMA_CHN%d_PAU          = 0x%08X\n", i, val);

        val = readl(pcm_global->dma_regs + SUNXI_DMA_CHN_DESC_ADDR(i));
        printk(KERN_ERR "  DMA_CHN%d_DESC_ADDR    = 0x%08X\n", i, val);

        val = readl(pcm_global->dma_regs + SUNXI_DMA_CHN_CFG(i));
        printk(KERN_ERR "  DMA_CHN%d_CFG          = 0x%08X\n", i, val);
        
        val = readl(pcm_global->dma_regs + SUNXI_DMA_CHN_SRC(i));
        printk(KERN_ERR "  DMA_CHN%d_CUR_SRC      = 0x%08X\n", i, val);

        val = readl(pcm_global->dma_regs + SUNXI_DMA_CHN_DST(i));
        printk(KERN_ERR "  DMA_CHN%d_CUR_DST      = 0x%08X\n", i, val);

        val = readl(pcm_global->dma_regs + SUNXI_DMA_CHN_BCNT_LEFT(i));
        printk(KERN_ERR "  DMA_CHN%d_BCNT_LEFT    = 0x%08X\n", i, val);

        val = readl(pcm_global->dma_regs + SUNXI_DMA_CHN_PARA(i));
        printk(KERN_ERR "  DMA_CHN%d_PARA         = 0x%08X\n", i, val);
    }

    return 0;
}

static int pcm_reg_dump(void)
{
    unsigned int val = 0;

    
    if (pcm_global->pcm_regs == NULL) {
		return -ENXIO;
	}

    printk(KERN_ERR "PCM register dump:\n");
    
    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOCTL);
    printk(KERN_ERR "  SUNXI_DAUDIOCTL       = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOFAT0);
    printk(KERN_ERR "  SUNXI_DAUDIOFAT0      = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOFAT1);
    printk(KERN_ERR "  SUNXI_DAUDIOFAT1      = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOISTA);
    printk(KERN_ERR "  SUNXI_DAUDIOISTA      = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIORXFIFO);
    printk(KERN_ERR "  SUNXI_DAUDIORXFIFO    = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOFCTL);
    printk(KERN_ERR "  SUNXI_DAUDIOFCTL      = 0x%08X\n", val);
    
    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOFSTA);
    printk(KERN_ERR "  SUNXI_DAUDIOFSTA      = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOINT);
    printk(KERN_ERR "  SUNXI_DAUDIOINT       = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOTXFIFO);
    printk(KERN_ERR "  SUNXI_DAUDIOTXFIFO    = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOCLKD);
    printk(KERN_ERR "  SUNXI_DAUDIOCLKD      = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOTXCNT);
    printk(KERN_ERR "  SUNXI_DAUDIOTXCNT     = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIORXCNT);
    printk(KERN_ERR "  SUNXI_DAUDIORXCNT     = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOCHCFG);
    printk(KERN_ERR "  SUNXI_DAUDIOCHCFG     = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOTX0CHSEL);
    printk(KERN_ERR "  SUNXI_DAUDIOTX0CHSEL  = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOTX1CHSEL);
    printk(KERN_ERR "  SUNXI_DAUDIOTX1CHSEL  = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOTX2CHSEL);
    printk(KERN_ERR "  SUNXI_DAUDIOTX2CHSEL  = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOTX3CHSEL);
    printk(KERN_ERR "  SUNXI_DAUDIOTX3CHSEL  = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOTX0CHMAP);
    printk(KERN_ERR "  SUNXI_DAUDIOTX0CHMAP  = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOTX1CHMAP);
    printk(KERN_ERR "  SUNXI_DAUDIOTX1CHMAP  = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOTX2CHMAP);
    printk(KERN_ERR "  SUNXI_DAUDIOTX2CHMAP  = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOTX3CHMAP);
    printk(KERN_ERR "  SUNXI_DAUDIOTX3CHMAP  = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIORXCHSEL);
    printk(KERN_ERR "  SUNXI_DAUDIORXCHSEL   = 0x%08X\n", val);

    val = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIORXCHMAP);
    printk(KERN_ERR "  SUNXI_DAUDIORXCHMAP   = 0x%08X\n\n", val);

    return 0;
}

static int pcm_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long page;
	unsigned long start = (unsigned long)vma->vm_start;
	unsigned long size =  (unsigned long)(vma->vm_end - vma->vm_start);

    
    page = pcm_global->dma_addr;

	vma->vm_flags |= VM_IO;
	vma->vm_flags |= (VM_DONTEXPAND | VM_NODUMP);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if(remap_pfn_range(vma, start, page>>PAGE_SHIFT, size, vma->vm_page_prot))
	{
		pr_err("remap_pfn_range failed\n");
		return -1;
	}
    
	return 0;
}

static long pcm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    pcm_ioctl_data_type *p_ioctl_config;

    
    switch(cmd)
    {
        case PCM_IOC_INIT:
        case PCM_IOC_ALLOC_MEM:
        case PCM_IOC_FREE_MEM:
            break;
        case PCM_IOC_ENABLE:
            pcm_enable();
            break;
        case PCM_IOC_DISABLE:
            pcm_disable();
            break;
        case PCM_IOC_DMA_START:
        case PCM_IOC_DMA_STOP:
            break;
        case PCM_IOC_DUMP_PCM_REG:
            pcm_reg_dump();
            break;
        case PCM_IOC_DUMP_DMA_REG:
            dma_reg_dump();
            break;
        case PCM_IOC_GET_TX_SEQ:
            {
                u32 reg_val;
                
                p_ioctl_config = (pcm_ioctl_data_type *)arg;
                reg_val = readl(pcm_global->dma_regs + SUNXI_DMA_CHN_BCNT_LEFT(0));
                p_ioctl_config->cur_segment =  (SEND_MAX_BYTES - reg_val) / (PCM_NUM_PER_DMA * DMA_SEG_SIZE); 

            }
            break;
        case PCM_IOC_GET_RX_SEQ:
            {
                u32 reg_val;
                
                p_ioctl_config = (pcm_ioctl_data_type *)arg;
                reg_val = readl(pcm_global->dma_regs + SUNXI_DMA_CHN_BCNT_LEFT(1));
                p_ioctl_config->cur_segment =  (RECV_MAX_BYTES - reg_val) / (PCM_NUM_PER_DMA * DMA_SEG_SIZE); 
            }
            break;
        default:
            printk(KERN_WARNING "Warning: ioctl cmd[%d] undefined!\n", cmd);
            break;
    }

	return 0;
}


struct file_operations pcmdrv_fops = {
    mmap:           pcm_mmap,
	unlocked_ioctl: pcm_ioctl,
};

#ifdef USE_DMA
static int pcm_alloc_dma_desc(void)
{
	struct sunxi_dma_desc *tx_desc, *rx_desc;
    

    /* TX DMA */
    pcm_global->tx_desc_vir_addr = pci_alloc_consistent(NULL, sizeof(struct sunxi_dma_desc), &pcm_global->tx_desc_phy_addr);
    if(!pcm_global->tx_desc_vir_addr)
    {
        printk(KERN_ERR "%s: DMA tx pci_alloc_consistent\n", __FUNCTION__);
        return -1;
    }

    //printk(KERN_ERR "tx dma desc phy addr = %p\n", (void *)pcm_global->tx_desc_phy_addr);
    
    tx_desc = (struct sunxi_dma_desc *)pcm_global->tx_desc_vir_addr;

    tx_desc->cfg = SUNXI_DMA_SRC_BURST(2)       // burst length = 8
                   | SUNXI_DMA_SRC_WIDTH(2)     // DMA source data width = 32-bit
                   | SUNXI_DMA_DST_BURST(2)     // burst length = 8
                   | SUNXI_DMA_DST_WIDTH(1)     // DMA destination data width = 16-bit
                   | SUNXI_DMA_DST_DRQ(DRQDST_TDMRX)
				   | SUNXI_DMA_SRC_LINEAR_MODE
				   | SUNXI_DMA_DST_IO_MODE
				   | SUNXI_DMA_SRC_DRQ(DRQSRC_SDRAM);
    
	tx_desc->src  = pcm_global->send_dma_addr;
	tx_desc->dst  = SUNXI_DAUDIOBASE + SUNXI_DAUDIOTXFIFO;
	tx_desc->len  = pcm_global->send_max_bytes;
    tx_desc->para = SUNXI_DMA_NORMAL_WAIT;
    tx_desc->next = pcm_global->tx_desc_phy_addr;
    
    writel(pcm_global->tx_desc_phy_addr, (pcm_global->dma_regs + SUNXI_DMA_CHN_DESC_ADDR(0)));

    
    /* RX DMA */
    pcm_global->rx_desc_vir_addr = pci_alloc_consistent(NULL, sizeof(struct sunxi_dma_desc), &pcm_global->rx_desc_phy_addr);
    if(!pcm_global->tx_desc_vir_addr)
    {
        dma_free_writecombine(NULL, sizeof(struct sunxi_dma_desc), pcm_global->tx_desc_vir_addr, pcm_global->tx_desc_phy_addr);
        printk(KERN_ERR "%s: DMA rx pci_alloc_consistent\n", __FUNCTION__);
        return -1;
    }

    //printk(KERN_ERR "rx dma desc phy addr = %p\n", (void *)pcm_global->rx_desc_phy_addr);
    
    rx_desc = (struct sunxi_dma_desc *)pcm_global->rx_desc_vir_addr;

    rx_desc->cfg = SUNXI_DMA_SRC_BURST(2)       // burst length = 8
                   | SUNXI_DMA_SRC_WIDTH(1)     // DMA source data width = 16-bit
                   | SUNXI_DMA_DST_BURST(2)     // burst length = 8
                   | SUNXI_DMA_DST_WIDTH(2)     // DMA destination data width = 32-bit
                   | SUNXI_DMA_SRC_DRQ(DRQDST_TDMRX)
    			   | SUNXI_DMA_DST_LINEAR_MODE
    			   | SUNXI_DMA_SRC_IO_MODE
    			   | SUNXI_DMA_DST_DRQ(DRQDST_SDRAM);

    rx_desc->src  = SUNXI_DAUDIOBASE + SUNXI_DAUDIORXFIFO;
    rx_desc->dst  = pcm_global->recv_dma_addr;
    rx_desc->len  = pcm_global->recv_max_bytes;
    rx_desc->para = SUNXI_DMA_NORMAL_WAIT;
    rx_desc->next = pcm_global->rx_desc_phy_addr;

    writel(pcm_global->rx_desc_phy_addr, (pcm_global->dma_regs + SUNXI_DMA_CHN_DESC_ADDR(1)));

	return 0;
}

static int pcm_dma_config(struct platform_device *pdev)
{
    int ret = 0;
	u32 dma_mem_size;

    
    // alloc dma mem
    pcm_global->send_max_bytes = SEND_MAX_BYTES;
    pcm_global->recv_max_bytes = RECV_MAX_BYTES;
    dma_mem_size = SEND_MAX_BYTES + RECV_MAX_BYTES;
    pcm_global->dma_alloc_size = ((dma_mem_size / PAGE_SIZE) + 1) * PAGE_SIZE;

    // debug
    printk(KERN_ERR "dma send size = %d\n", pcm_global->send_max_bytes);
    printk(KERN_ERR "dma recv size = %d\n", pcm_global->recv_max_bytes);
    printk(KERN_ERR "dma mem alloc size = %d\n", pcm_global->dma_alloc_size);
    
    
    pcm_global->dma_area = pci_alloc_consistent(NULL, pcm_global->dma_alloc_size, &pcm_global->dma_addr);
    if(IS_ERR(pcm_global->dma_area))
    {
        pr_err("send dma buffer request error\n");
        return -ENOMEM;
    }

    pcm_global->send_area = pcm_global->dma_area;
    pcm_global->recv_area = pcm_global->dma_area + pcm_global->send_max_bytes;
    pcm_global->send_dma_addr = pcm_global->dma_addr;
    pcm_global->recv_dma_addr = pcm_global->dma_addr + pcm_global->send_max_bytes;

    // debug
    //printk(KERN_ERR "send dma addr = %p\n", (void *)pcm_global->send_dma_addr);
    //printk(KERN_ERR "recv dma addr = %p\n", (void *)pcm_global->recv_dma_addr);

    ret = pcm_alloc_dma_desc();
    
    return ret;
}
#else
static irqreturn_t pcm_irq_handle(int irq, void *dev_id)
{
    u32 irq_status, i;
    u32 tx_val[8], rx_val[8];

    
    irq_status = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIOISTA);

    if(irq_status & SUNXI_DAUDIOSTA_TXE_INT)
    {
        for(i=0; i<8; i++)
        {
            tx_val[i] = 0x12345678;
            sunxi_smc_writel(tx_val[i], pcm_global->pcm_regs + SUNXI_DAUDIOTXFIFO);
        }
    }

    if(irq_status & SUNXI_DAUDIOSTA_RXA_INT)
    {
        rx_val[0] = sunxi_smc_readl(pcm_global->pcm_regs + SUNXI_DAUDIORXFIFO);

        if(printk_ratelimit())
            printk(KERN_ERR "**SUNXI_DAUDIORXFIFO     = 0x%08X\n", rx_val[0]);
        
    }
    
    sunxi_smc_writel(irq_status, pcm_global->pcm_regs + SUNXI_DAUDIOISTA);

    return IRQ_HANDLED;
}
#endif

static int pcm_clk_set(void)
{
    struct clk *daudio_pll2clk 	= NULL;
    struct clk *daudio_pllx8 	= NULL;
    struct clk *daudio_moduleclk	= NULL;
    

	daudio_pllx8 = clk_get(NULL, "pll_audiox8");
	if ((!daudio_pllx8)||(IS_ERR(daudio_pllx8))) {
		pr_err("try to get daudio_pllx8 failed\n");
	}
	if (clk_prepare_enable(daudio_pllx8)) {
		pr_err("enable daudio_pll2clk failed; \n");
	}

	/*daudio pll2clk*/
	daudio_pll2clk = clk_get(NULL, "pll2");
	if ((!daudio_pll2clk)||(IS_ERR(daudio_pll2clk))) {
		pr_err("try to get daudio_pll2clk failed\n");
	}
	if (clk_prepare_enable(daudio_pll2clk)) {
		pr_err("enable daudio_pll2clk failed; \n");
	}

	daudio_moduleclk = clk_get(NULL, "tdm");
	if ((!daudio_moduleclk)||(IS_ERR(daudio_moduleclk))) {
		pr_err("try to get daudio_moduleclk failed\n");
	}

	if (clk_set_parent(daudio_moduleclk, daudio_pll2clk)) {
		pr_err("try to set parent of daudio_moduleclk to daudio_pll2ck failed! line = %d\n",__LINE__);
	}
    
	if (clk_set_rate(daudio_moduleclk, 8192000)) {
		pr_err("set daudio_moduleclk clock freq to 24576000 failed! line = %d\n", __LINE__);
	}
	if (clk_prepare_enable(daudio_moduleclk)) {
		pr_err("open daudio_moduleclk failed! line = %d\n", __LINE__);
	}    

    return 0;
}

static int pcm_pin_config(void)
{
    u32 reg_val;
    int ret;


    ret = gpio_request_array(tdm_bus_gpios, ARRAY_SIZE(tdm_bus_gpios));
    if (IS_ERR_VALUE(ret))
    {
        printk(KERN_ERR "%s: request tdm gpio, ret:%d\n", __FUNCTION__, ret);
        return ret;
    }

    //printk(KERN_ERR "%s: request tdm gpio succ\n", __FUNCTION__);    

    /*
       PB4_SELECT: TDM_LRCK
       PB5_SELECT: TDM_BCLK
       PB6_SELECT: TDM_DOUT
       PB7_SELECT: TDM_DIN
     */
    reg_val = sunxi_smc_readl(SUNXI_PIO_VBASE + 0x24);
    reg_val &= ~((0x7 << 16) | (0x7 << 20) | (0x7 << 24) | (0x7 << 28));
    reg_val |=  ((0x3 << 16) | (0x3 << 20) | (0x3 << 24) | (0x3 << 28));
    sunxi_smc_writel(reg_val, SUNXI_PIO_VBASE + 0x24);

    /*
       PB8_SELECT: TDM_MCLK
     */
    reg_val = sunxi_smc_readl(SUNXI_PIO_VBASE + 0x28);
    reg_val &= ~(0x7 << 0);
    reg_val |=  (0x3 << 0);
    sunxi_smc_writel(reg_val, SUNXI_PIO_VBASE + 0x28);

    return 0;
}

static int __init pcm_probe(struct platform_device *pdev)
{
    int result = 0;
    struct device *class_device;


    pcm_global = kmalloc(sizeof(struct pcm_global_t), GFP_KERNEL);
    if(IS_ERR(pcm_global))
    {
        printk(KERN_ERR "kmalloc error\n");
        return -ENOMEM;
    }
    memset(pcm_global, 0, sizeof(struct pcm_global_t));

    
    result = register_chrdev(pcmdrv_major, PCM_MOD_DEVNAME, &pcmdrv_fops);
    if (result < 0) {
        printk(KERN_WARNING "pcm: can't get major %d\n", pcmdrv_major);
        return result;
    }

    if (pcmdrv_major == 0) {
        pcmdrv_major = result; /* dynamic */
    }

    pcmdrv_class = class_create(THIS_MODULE, PCM_MOD_DEVNAME);
    if(IS_ERR(pcmdrv_class))
    {
        printk(KERN_WARNING "class_create %s\n", PCM_MOD_DEVNAME);
        return -EFAULT;
    }
    class_device = device_create(pcmdrv_class, NULL, MKDEV(pcmdrv_major, 0), NULL, PCM_MOD_DEVNAME);
    if(IS_ERR(class_device))
    {
        printk(KERN_WARNING "device_create %s\n", PCM_MOD_DEVNAME);
        return -EFAULT;
    }

    pcm_global->pcm_regs = ioremap(SUNXI_DAUDIOBASE, 0x100);
	if (pcm_global->pcm_regs == NULL) {
		return -ENXIO;
	}
    pcm_global->dma_regs = ioremap(SUNXI_DMA_BASE, 0x500);
    
	daudio_pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR_OR_NULL(daudio_pinctrl)) {
		dev_warn(&pdev->dev,
			"pins are not configured from the driver\n");
	}
	if (IS_ERR_OR_NULL(daudio_pinctrl)) {
		dev_warn(&pdev->dev,
			"pins are not configured from the driver\n");
	}
	pin_count = script_get_pio_list (PCM_MOD_TDMNAME, &pin_daudio0_list);
	if (pin_count == 0) {
		/* "daudio0" have no pin configuration */
		pr_err("daudio0 have no pin configuration\n");
	}

    pcm_pin_config();
    pcm_clk_set();
    pcm_global_config();
    pcm_reg_config();
    
#ifdef USE_DMA
    pcm_dma_config(pdev);
#else
    /*中断申请*/
	result = request_irq(SUNXI_IRQ_TDM, pcm_irq_handle, 0, dev_name(&pdev->dev), NULL);
	if(result < 0)
	{
	    pr_err("request irq failed\n");
        return -EINVAL;
	}
#endif

	pcm_enable();

    //dma_reg_dump();
    //pcm_reg_dump();

    printk(KERN_ERR "pcm module version: %s\n", PCM_MOD_VERSION);

	return 0;
}

static int __exit pcm_remove(struct platform_device *pdev)
{

    printk(KERN_ERR "%s\n", __FUNCTION__);

    pcm_disable();

    gpio_free_array(tdm_bus_gpios, ARRAY_SIZE(tdm_bus_gpios));

    if(pcm_global->send_dma_chan)
        dma_release_channel(pcm_global->send_dma_chan);
    if(pcm_global->recv_dma_chan)
        dma_release_channel(pcm_global->recv_dma_chan);

    if(pcm_global->dma_area)
    {
        dma_free_writecombine(&pdev->dev, pcm_global->dma_alloc_size, pcm_global->dma_area, pcm_global->dma_addr);
    }

    if(pcm_global->tx_desc_vir_addr)
    {
        dma_free_writecombine(&pdev->dev, sizeof(struct sunxi_dma_desc), pcm_global->tx_desc_vir_addr, pcm_global->tx_desc_phy_addr);
    }

    if(pcm_global->rx_desc_vir_addr)
    {
        dma_free_writecombine(&pdev->dev, sizeof(struct sunxi_dma_desc), pcm_global->rx_desc_vir_addr, pcm_global->rx_desc_phy_addr);
    }
    
    device_destroy(pcmdrv_class, MKDEV(pcmdrv_major, 0));
    class_destroy(pcmdrv_class);
    unregister_chrdev(pcmdrv_major, PCM_MOD_DEVNAME);
    
    kfree(pcm_global);
    pcm_global = NULL;

	return 0;
}

static void pcm_release(struct device * dev)
{
    return;
}

static struct platform_device pcm_device = {
	.name 	= PCM_MOD_TDMNAME,
	.id 	= PLATFORM_DEVID_NONE,
	.dev = {
        .release = pcm_release,
    }
};

static struct platform_driver pcm_driver = {
	.probe = pcm_probe,
	.remove = __exit_p(pcm_remove),
	.driver = {
		.name  = PCM_MOD_TDMNAME,
		.owner = THIS_MODULE,
	},
};

static int __init pcm_init(void)
{
	int err = 0;

    if((err = platform_device_register(&pcm_device)) < 0)
        return err;

    if((err = platform_driver_register(&pcm_driver)) < 0)
        return err;

    return 0;
}

static void __exit pcm_exit(void)
{
    printk(KERN_ERR "%s\n", __FUNCTION__);
    
    platform_driver_unregister(&pcm_driver);
    platform_device_unregister(&pcm_device);
}

module_init(pcm_init);
module_exit(pcm_exit);

MODULE_LICENSE(PCM_MOD_LICENSE);
MODULE_AUTHOR(PCM_MOD_AUTHOR);
MODULE_VERSION(PCM_MOD_VERSION);

