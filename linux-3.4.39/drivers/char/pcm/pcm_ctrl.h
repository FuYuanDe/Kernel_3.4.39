#ifndef __RALINK_PCM_H_
#define __RALINK_PCM_H_


/* use dma or irq */
#define USE_DMA                             1


/* module name and version */
#define PCM_MOD_TDMNAME                     "tdm1"
#define PCM_MOD_DEVNAME                     "pcm0"
#define PCM_MOD_LICENSE                     "GPL"
#define PCM_MOD_VERSION                     "0.8.9"
#define PCM_MOD_AUTHOR                      "Dinstar"


/* driver i/o control command */
#define PCM_IOC_MAGIC                       0xE0
#define PCM_IOC_INIT                        _IO(PCM_IOC_MAGIC, 0)
#define PCM_IOC_ALLOC_MEM                   _IO(PCM_IOC_MAGIC, 1)
#define PCM_IOC_FREE_MEM                    _IO(PCM_IOC_MAGIC, 2)
#define PCM_IOC_ENABLE                      _IO(PCM_IOC_MAGIC, 3)
#define PCM_IOC_DISABLE                     _IO(PCM_IOC_MAGIC, 4)
#define PCM_IOC_GET_TX_SEQ                  _IO(PCM_IOC_MAGIC, 5)
#define PCM_IOC_GET_RX_SEQ                  _IO(PCM_IOC_MAGIC, 6)
#define PCM_IOC_DMA_START                   _IO(PCM_IOC_MAGIC, 7)
#define PCM_IOC_DMA_STOP                    _IO(PCM_IOC_MAGIC, 8)
#define PCM_IOC_DUMP_PCM_REG                _IO(PCM_IOC_MAGIC, 9)
#define PCM_IOC_DUMP_DMA_REG                _IO(PCM_IOC_MAGIC, 10)



/* Digital Audio Register Address */
#define SUNXI_DAUDIOBASE                    (0x01c23000)
#define SUNXI_DAUDIO_VBASE                  (0xf1c23000)


/* Digital Audio Control Register */
#define SUNXI_DAUDIOCTL                     (0x00)      // Register Offset
#define SUNXI_DAUDIOCTL_BCLKOUT             (1<<18)     // BCLK output or input
#define SUNXI_DAUDIOCTL_LRCKOUT             (1<<17)     // LRCK output or input
#define SUNXI_DAUDIOCTL_LRCKROUT            (1<<16)     // LRCKR output or input
#define SUNXI_DAUDIOCTL_SDO3EN              (1<<11)     // SDO3 Enable
#define SUNXI_DAUDIOCTL_SDO2EN              (1<<10)     // SDO2 Enable
#define SUNXI_DAUDIOCTL_SDO1EN              (1<<9)      // SDO1 Enable
#define SUNXI_DAUDIOCTL_SDO0EN              (1<<8)      // SDO0 Enable
#define SUNXI_DAUDIOCTL_OUTMUTE             (1<<6)      // Out Mute
#define SUNXI_DAUDIOCTL_MODESEL(v)          ((v)<<4)    // Mode Selection
#define SUNXI_DAUDIOCTL_LOOP                (1<<3)      // Loop back test
#define SUNXI_DAUDIOCTL_TXEN                (1<<2)      // Transmitter Block Enable 
#define SUNXI_DAUDIOCTL_RXEN                (1<<1)      // Receiver Block Enable
#define SUNXI_DAUDIOCTL_GEN                 (1<<0)      // Global Enable


/* Digital Audio Format Register0 */
#define SUNXI_DAUDIOFAT0                    (0x04)      // Register Offset
#define SUNXI_DAUDIOFAT0_SDI_SYNC_SEL       (1<<31)     // SDI SYNC use LRCK or LRCKR
#define SUNXI_DAUDIOFAT0_LRCK_WIDTH         (1<<30)     // LRCK width
#define SUNXI_DAUDIOFAT0_LRCKR_PERIOD(v)    ((v)<<20)   // The number of BCLKs per channel of sample frame
#define SUNXI_DAUDIOFAT0_LRCK_POLAYITY      (1<<19)     // LRCK/LRCKR polarity
#define SUNXI_DAUDIOFAT0_LRCK_PERIOD(v)     ((v)<<8)    // The number of BCLKs per channel of sample frame
#define SUNXI_DAUDIOFAT0_BCLK_POLAYITY      (1<<7)      // BCLK polarity
#define SUNXI_DAUDIOFAT0_SAMPLE_RES(v)      ((v)<<4)    // Sample Resolution
#define SUNXI_DAUDIOFAT0_EDGE_TRANSFER      (1<<3)      // Edge Transfer
#define SUNXI_DAUDIOFAT0_SLOT_WIDTH(v)      ((v)<<0)    // Slot Width Select


/* Digital Audio Format Register1 */
#define SUNXI_DAUDIOFAT1                    (0x08)      // Register Offset
#define SUNXI_DAUDIOFAT1_RX_MLS             (1<<7)      // RX MSB LSB First Select
#define SUNXI_DAUDIOFAT1_TX_MLS             (1<<6)      // TX MSB LSB First Select
#define SUNXI_DAUDIOFAT1_SEXT(v)            ((v)<<4)    // Sign Extend in slot
#define SUNXI_DAUDIOFAT1_RX_PDM(v)          ((v)<<2)    // RX PCM Data Mode
#define SUNXI_DAUDIOFAT1_TX_PDM(v)          ((v)<<0)    // TX PCM Data Mode


/* Digital Audio Interrupt Status Register */
#define SUNXI_DAUDIOISTA                    (0x0c)      // Register Offset
#define SUNXI_DAUDIOSTA_TXU_INT             (1<<6)      // TX FIFO Under run Pedding Interrupt
#define SUNXI_DAUDIOSTA_TXO_INT             (1<<5)      // TX FIFO Overrun Pedding Interrupt
#define SUNXI_DAUDIOSTA_TXE_INT             (1<<4)      // TX FIFO Empty Pedding Interrupt
#define SUNXI_DAUDIOSTA_RXU_INT             (1<<2)      // RX FIFO Under run Pedding Interrupt
#define SUNXI_DAUDIOSTA_RXO_INT             (1<<1)      // RX FIFO Overrun Pedding Interrupt
#define SUNXI_DAUDIOSTA_RXA_INT             (1<<0)      // RX FIFO Data Available Pedding Interrupt


/* Digital Audio RX FIFO Register */
#define SUNXI_DAUDIORXFIFO                  (0x10)      // Register Offset


/* Digital Audio FIFO Control Register */
#define SUNXI_DAUDIOFCTL                    (0x14)      // Register Offset
#define SUNXI_DAUDIOFCTL_HUBEN              (1<<31)     // Audio hub enable
#define SUNXI_DAUDIOFCTL_FTX                (1<<25)     // Write '1' to flush TX FIFO
#define SUNXI_DAUDIOFCTL_FRX                (1<<24)     // Write '1' to flush RX FIFO
#define SUNXI_DAUDIOFCTL_TXTL(v)            ((v)<<12)   // TX FIFO Trigger Level
#define SUNXI_DAUDIOFCTL_RXTL(v)            ((v)<<4)    // RX FIFO Trigger Level
#define SUNXI_DAUDIOFCTL_TXIM               (1<<2)      // TX FIFO Input Mode
#define SUNXI_DAUDIOFCTL_RXOM(v)            ((v)<<0)    // RX FIFO Output Mode


/* Digital Audio FIFO Status Register */
#define SUNXI_DAUDIOFSTA                    (0x18)      // Register Offset
#define SUNXI_DAUDIOFSTA_TXE                (1<<28)     // TX FIFO Empty
#define SUNXI_DAUDIOFSTA_TXECNT(v)          ((v)<<16)   // TX FIFO Empty Space Word Counter
#define SUNXI_DAUDIOFSTA_RXA                (1<<8)      // RX FIFO Available
#define SUNXI_DAUDIOFSTA_RXACNT(v)          ((v)<<0)    // RX FIFO Available Sample Word Counter


/* Digital Audio DMA & Interrupt Control Register */
#define SUNXI_DAUDIOINT                     (0x1c)      // Register Offset
#define SUNXI_DAUDIOINT_TXDRQEN             (1<<7)      // TX FIFO Empty DRQ Enable
#define SUNXI_DAUDIOINT_TXUIEN              (1<<6)      // TX FIFO Under Interrupt Enable
#define SUNXI_DAUDIOINT_TXOIEN              (1<<5)      // TX FIFO Overrun Interrupt Enable
#define SUNXI_DAUDIOINT_TXEIEN              (1<<4)      // TX FIFO Empty Interrupt Enable
#define SUNXI_DAUDIOINT_RXDRQEN             (1<<3)      // RX FIFO Data Available DRQ Enable
#define SUNXI_DAUDIOINT_RXUIEN              (1<<2)      // RX FIFO Under run Interrupt Enable
#define SUNXI_DAUDIOINT_RXOIEN              (1<<1)      // RX FIFO Overrun Interrupt Enable
#define SUNXI_DAUDIOINT_RXAIEN              (1<<0)      // RX FIFO Data Available Interrupt Enable


/* Digital Audio TX FIFO Register */
#define SUNXI_DAUDIOTXFIFO                  (0x20)      // Register Offset


/* Digital Audio Clock Divide Register */
#define SUNXI_DAUDIOCLKD                    (0x24)      // Register Offset
#define SUNXI_DAUDIOCLKD_MCLKOEN            (1<<8)      // MCLK Output Enable
#define SUNXI_DAUDIOCLKD_BCLKDIV(v)         ((v)<<4)    // BCLK Divide Ratio from PLL2
#define SUNXI_DAUDIOCLKD_MCLKDIV(v)         ((v)<<0)    // MCLK Divide Ratio from PLL2 Output 


/* Digital Audio TX Counter Register */
#define SUNXI_DAUDIOTXCNT                   (0x28)      // Register Offset


/* Digital Audio RX Counter Register */
#define SUNXI_DAUDIORXCNT                   (0x2C)      // Register Offset


/* Digital Audio Channel Configuration Register */
#define SUNXI_DAUDIOCHCFG                   (0x30)      // Register Offset
#define SUNXI_DAUDIOCHCFG_TX_SLOT_HIZ       (1<<9)      // Normal or hi-z state for the last half cycle of BCLK in the slot
#define SUNXI_DAUDIOCHCFG_TX_STATE          (1<<8)      // Level 0 or hi-z state when not transferring slot
#define SUNXI_DAUDIOCHCFG_RX_SLOT_NUM(v)    ((v)<<4)    // RX Channel/Slot Number which between CPU/DMA and FIFO
#define SUNXI_DAUDIOCHCFG_TX_SLOT_NUM(v)    ((v)<<0)    // TX Channel/Slot Number which between CPU/DMA and FIFO


/* Digital Audio TXn Channel Configuration Register */
#define SUNXI_DAUDIOTX0CHSEL                (0x34)      // Register Offset
#define SUNXI_DAUDIOTX1CHSEL                (0x38)      // Register Offset
#define SUNXI_DAUDIOTX2CHSEL                (0x3C)      // Register Offset
#define SUNXI_DAUDIOTX3CHSEL                (0x40)      // Register Offset
#define SUNXI_DAUDIOTXn_OFFSET(v)           ((v)<<12)   // TX data offset to LRCK
#define SUNXI_DAUDIOTXn_CHEN(v)             ((v)<<4)    // TX Channel(slot) enable
#define SUNXI_DAUDIOTXn_CHSEL(v)            ((v)<<0)    // TX Channel(slot) number Select for each output


/* Digital Audio TXn Channel Mapping Register */
#define SUNXI_DAUDIOTX0CHMAP                (0x44)      // Register Offset
#define SUNXI_DAUDIOTX1CHMAP                (0x48)      // Register Offset
#define SUNXI_DAUDIOTX2CHMAP                (0x4C)      // Register Offset
#define SUNXI_DAUDIOTX3CHMAP                (0x50)      // Register Offset
#define SUNXI_DAUDIOTXn_MAP_CH7(v)          ((v)<<28)   // TX Channel 7 Mapping
#define SUNXI_DAUDIOTXn_MAP_CH6(v)          ((v)<<24)   // TX Channel 6 Mapping
#define SUNXI_DAUDIOTXn_MAP_CH5(v)          ((v)<<20)   // TX Channel 5 Mapping
#define SUNXI_DAUDIOTXn_MAP_CH4(v)          ((v)<<16)   // TX Channel 4 Mapping
#define SUNXI_DAUDIOTXn_MAP_CH3(v)          ((v)<<12)   // TX Channel 3 Mapping
#define SUNXI_DAUDIOTXn_MAP_CH2(v)          ((v)<<8)    // TX Channel 2 Mapping
#define SUNXI_DAUDIOTXn_MAP_CH1(v)          ((v)<<4)    // TX Channel 1 Mapping
#define SUNXI_DAUDIOTXn_MAP_CH0(v)          ((v)<<0)    // TX Channel 0 Mapping


/* Digital Audio RX Channel Select Register */
#define SUNXI_DAUDIORXCHSEL                 (0x54)      // Register Offset
#define SUNXI_DAUDIORXCHSEL_RXOFFSET(v)     ((v)<<12)   // RX data offset to LRCK
#define SUNXI_DAUDIORXCHSEL_RXCHSEL(v)      ((v)<<0)    // RX Channel(slot) number Select for input


/* Digital Audio RX Channel Mapping Register */
#define SUNXI_DAUDIORXCHMAP                 (0x58)      // Register Offset
#define SUNXI_DAUDIORXCHMAP_CH7(v)          ((v)<<28)   // RX Channel 7 Mapping
#define SUNXI_DAUDIORXCHMAP_CH6(v)          ((v)<<24)   // RX Channel 6 Mapping
#define SUNXI_DAUDIORXCHMAP_CH5(v)          ((v)<<20)   // RX Channel 5 Mapping
#define SUNXI_DAUDIORXCHMAP_CH4(v)          ((v)<<16)   // RX Channel 4 Mapping
#define SUNXI_DAUDIORXCHMAP_CH3(v)          ((v)<<12)   // RX Channel 3 Mapping
#define SUNXI_DAUDIORXCHMAP_CH2(v)          ((v)<<8)    // RX Channel 2 Mapping
#define SUNXI_DAUDIORXCHMAP_CH1(v)          ((v)<<4)    // RX Channel 1 Mapping
#define SUNXI_DAUDIORXCHMAP_CH0(v)          ((v)<<0)    // RX Channel 0 Mapping



#define SUNXI_DAUDIOCLKD_MCLK_MASK          0x0F
#define SUNXI_DAUDIOCLKD_MCLK_OFFS          0
#define SUNXI_DAUDIOCLKD_BCLK_MASK          0x070
#define SUNXI_DAUDIOCLKD_BCLK_OFFS          4
#define SUNXI_DAUDIOCLKD_MCLKEN_OFFS        7

#define SUNXI_DAUDIO_DIV_MCLK               (0)
#define SUNXI_DAUDIO_DIV_BCLK               (1)

#define DMA_SEG_NUM                         (512)
#define DMA_SEG_SIZE                        (40)
#define PCM_NUM_PER_DMA                     (128)

#define SEND_MAX_BYTES                      (PCM_NUM_PER_DMA * DMA_SEG_SIZE * DMA_SEG_NUM)
#define RECV_MAX_BYTES                      SEND_MAX_BYTES


/* DMA Register */
#define SUNXI_DMA_BASE                      (0x01c02000)
#define SUNXI_DMA_IRQ_EN                    (0x0)
#define SUNXI_DMA_IRQ_PEND                  (0x10)
#define SUNXI_DMA_AUTO_GATE                 (0x20)
#define SUNXI_DMA_SECURE                    (0x28)
#define SUNXI_DMA_STA                       (0x30)
#define SUNXI_DMA_CHN_EN(n)                 (0x100 + n*0x40 + 0x00)
#define SUNXI_DMA_CHN_PAU(n)                (0x100 + n*0x40 + 0x04)
#define SUNXI_DMA_CHN_DESC_ADDR(n)          (0x100 + n*0x40 + 0x08)
#define SUNXI_DMA_CHN_CFG(n)                (0x100 + n*0x40 + 0x0C)
#define SUNXI_DMA_CHN_SRC(n)                (0x100 + n*0x40 + 0x10)
#define SUNXI_DMA_CHN_DST(n)                (0x100 + n*0x40 + 0x14)
#define SUNXI_DMA_CHN_BCNT_LEFT(n)          (0x100 + n*0x40 + 0x18)
#define SUNXI_DMA_CHN_PARA(n)               (0x100 + n*0x40 + 0x1C)


#define SUNXI_DMA_SRC_WIDTH(x)	            ((x) << 9)
#define SUNXI_DMA_SRC_BURST(x)	            ((x) << 7)
#define SUNXI_DMA_SRC_IO_MODE	            (0x01 << 5)
#define SUNXI_DMA_SRC_LINEAR_MODE	        (0x00 << 5)
#define SUNXI_DMA_SRC_DRQ(x)	            ((x) << 0)

#define SUNXI_DMA_DST_WIDTH(x)	            ((x) << 25)
#define SUNXI_DMA_DST_BURST(x)	            ((x) << 23)
#define SUNXI_DMA_DST_IO_MODE	            (0x01 << 21)
#define SUNXI_DMA_DST_LINEAR_MODE	        (0x00 << 21)
#define SUNXI_DMA_DST_DRQ(x)	            ((x) << 16)
#define SUNXI_DMA_NORMAL_WAIT	            (8 << 0)

#define SUNXI_DMA_CHAN_START	            1
#define SUNXI_DMA_CHAN_STOP	                0



struct pcm_dma_info
{
    uint32_t num;
};

struct pcm_global_t
{
    void __iomem   *pcm_regs;
    void __iomem   *dma_regs;
    u32 mode;				// 0: slave, 1: master
    u32 lrckrsel;			// 0: no lrckr   1: use lrckr
    u32 lrckrdir;			// 0: input	     1: output
    u32 doutnum;            // 1: 1 dout    2: 2 dout 	3: 3 dout	4: 4 dout
    u32 transfermode;		// 0: pcm mode		1: left mode	2: right mode
    u32 loopback;			// 0: no loopback	1: loopback
    u32 frametype;			// 0: short frame	1: long frame
    u32 lrckpolarity;		// 0 or 1
    u32 bclkpolarity;		// 0 or 1
    u32 edge;				// 0 or  1
    u32 samp_res;			// sample resolution: from 8-bit to 32-bit
    u32 slot_width;			// slot width: from 8-bit to 32-bit
    u32 msb;                // 0: msb	1: lsb
    u32 transfertype;       // 0: linear		2: 8-bit u-law	3: 8-bit a-law
    u32 signext;            // 0: zero padding	1: sign extension	3: transfer 0 after sample
    u32 huben;				// 0: no use hub		1: use hub
    u32 txtl;
    u32 rxtl;
    u32 fifo_txim;			// TX FIFO input mode (0: valid data at MSB, 1: vallid data at LSB)
    u32 fifo_rxom;			// RX FIFO output mode
    u32 pcm_lrck_period;		//even num
    u32 pcm_lrckr_period;	// even num
    u32 pcm_lsb_first;
    u32 pcm_start_slot;
    u32 ws_size;
    u32 pcm_sw;
    //lock divider setup
    u32 bclk_rate;            // 2.048M, 4.096M, 8.192M
    u32 mclk_rate;            // 2.048M, 4.096M, 8.192M
    u32 mclkout;              // 0: disable mclk output  		1: enable mclk output
    u32 sampling_rate;		// 8000 to 192000
    u32 mclk_fs;            // 128fs, 192fs, 256fs, 384fs, 512fs, 768fs

    //channel slot number setup
    u32 tx_slot_num;        // tx channel slot number between cpu/dma and fifo
    u32 tx_state;			// 0: trasfer 0 when not transferring slot      1: turn to hi-state(TDM) when not transferring slot
    u32 tx_slot_hiz;        // 0: normal mode for the last half cycle of BCLK in the slot       1: turn to hi-z state for the last half cycle of BCLK in the slot
    u32 tx0_chslotoffset;
    u32 tx1_chslotoffset;
    u32 tx2_chslotoffset;
    u32 tx3_chslotoffset;
    u32 tx0_chsloten;
    u32 tx1_chsloten;
    u32 tx2_chsloten;
    u32 tx3_chsloten;
    u32 tx0_chslotsel;
    u32 tx1_chslotsel;
    u32 tx2_chslotsel;
    u32 tx3_chslotsel;
    u32 tx0_chslotmap;
    u32 tx1_chslotmap;
    u32 tx2_chslotmap;
    u32 tx3_chslotmap;

    u32 rx_slot_num;        // rx channel slot number between cpu/dma and fifo
    u32 rx_chslotoffset;
    u32 rx_chslotsel;
    u32 rx_chslotmap;

    /* DMA */
    u32 send_max_bytes;
    u32 recv_max_bytes;
    u32 dma_alloc_size;
    
    u8 *dma_area;
    u8 *send_area;
    u8 *recv_area;
    
    dma_addr_t dma_addr;
    dma_addr_t send_dma_addr;
    dma_addr_t recv_dma_addr;

    struct dma_chan *send_dma_chan;
    struct dma_chan *recv_dma_chan;

    dma_cookie_t send_cookie;
    dma_cookie_t recv_cookie;

    struct pcm_dma_info send_dma_info;
    struct pcm_dma_info recv_dma_info;

    dma_addr_t tx_desc_phy_addr;
    u8        *tx_desc_vir_addr;
    dma_addr_t rx_desc_phy_addr;
    u8        *rx_desc_vir_addr;
};

struct sunxi_dma_desc
{
	u32		    cfg;		        /* DMA configuration */
	dma_addr_t	src;		        /* Source address */
	dma_addr_t	dst;		        /* Destination address */
	u32		    len;		        /* Length of buffers */
	u32		    para;		        /* Parameter register */
    dma_addr_t  next;               /* Next desc addr */ 
}__attribute__((packed));


/* driver ioctl data type */
typedef struct pcm_ioctl_data
{
    u8   pcm_chan;
    u8   pcm_mode;
    u16  cur_segment;
}pcm_ioctl_data_type;


#endif
