#ifndef _PEF22554_DRV_H
#define _PEF22554_DRV_H

#include <linux/cdev.h>
#include <linux/semaphore.h>


/*-------------------------- debug --------------------------*/

#ifdef DEBUG
#define debug(format, args...)	    printk(KERN_WARNING format, ##args)
#else
#define debug(format, args...)      { }
#endif

typedef int             BOOL;

#if !defined(TRUE)
#define TRUE                        1
#endif
#if !defined(FALSE)
#define FALSE                       0
#endif



/*-------------------------- Basic QuadFALC device settings --------------------------*/
#define MEM_READ8   quadFALC_mem_read8
#define MEM_WRITE8  quadFALC_mem_write8
#define CLEAR_BIT8  quadFALC_clear_bit8
#define SET_BIT8    quadFALC_set_bit8


#define MAX_PEF22554_NUM            1
#define NUM_OF_FALC                 4
#define QFALC_OFFSET                0x100 /*size in BYTE */
#define QFALC_OFFSET_DUINT          0x40  /*size in DUINT*/

/* clocking modes */
#define SLAVE_MODE                  0x00
#define MASTER_MODE                 0x01

#define MODVERSION                  1
#define MAX_TS_NUM		            32
#define CHANNEL_UP			        0
#define CHANNEL_DOWN		        1
#define CHANNEL_UNKNOWN	            2
#define E1_NUM_MASK                 0xf
#define MAX_R2_E1_CASREG_NUM        16
#define MAX_R2_E1_CHANNEL           30
#define MAX_R2_T1_CASREG_NUM        12
#define MAX_R2_T1_CHANNEL           24

#define MOD_R2_DEBUG0               0


#define MAX_DATA_LEN                360
#define MAX_DATA_LEN_R2             32
#define MAX_DATA_NUM                30


#define CPLD_BASE_ADDR				0x11400000
#define PACKET_TOOBIG               0x1
#define NONBLOCK                    0
#define NUM_OF_LED                  (MAX_PEF22554_NUM * NUM_OF_FALC)
#define DATA_LEN_LIMIT              320
#define MAX_SEND_LEN                32

/* buffer size */
#define RX_BUFFER_2_FRAME           0x00
#define RX_BUFFER_1_FRAME           0x01
#define RX_BUFFER_96_bits           0x02
#define RX_BUFFER_BYPASS            0x03
#define TX_BUFFER_2_FRAME           0x02
#define TX_BUFFER_1_FRAME           0x01
#define TX_BUFFER_96_bits           0x03
#define TX_BUFFER_BYPASS            0x00

#define TX_BUFFER                   TX_BUFFER_2_FRAME  /* 0x01 - TX_BUFFER_1_FRAME; 0x03 - TX_BUFFER_96_bits; 0x00 - TX_BUFFER_BYPASS 0x02 - TX_BUFFER_2_FRAME */
#define RX_BUFFER                   RX_BUFFER_2_FRAME  /* 0x01 - RX_BUFFER_1_FRAME; 0x02 - RX_BUFFER_96_bits; 0x03 - RX_BUFFER_BYPASS 0x00 - RX_BUFFER_2_FRAME */


/* framing modes */
#define FRAMING_NO                  0x08
/* E1 */
#define FRAMING_DOUBLEFRAME         0x00
#define FRAMING_MULTIFRAME_TX       0x01
#define FRAMING_MULTIFRAME_RX       0x02
#define FRAMING_MULTIFRAME_MOD      0x03

#define RCLK_SOURCE1                0x0 /* 0x0 - port#0; 0x01-port#1; 0x02-port#2; 0x03-port#3; */
#define RCLK_SOURCE2                0x1 /* 0x0 - port#0; 0x01-port#1; 0x02-port#2; 0x03-port#3; */
#define RCLK_SOURCE3                0x2 /* 0x0 - port#0; 0x01-port#1; 0x02-port#2; 0x03-port#3; */
#define RCLK_SOURCE4                0x3 /* 0x0 - port#0; 0x01-port#1; 0x02-port#2; 0x03-port#3; */

#define E1_XPM0_0DB                 0x52
#define E1_XPM1_0DB                 0x02
#define E1_XPM2_0DB                 0x00

#define QFALC_RAIL_INTERFACE        0x0  /* 0-single rail; 1-dual rail interface */
#define QFALC_INTERRUPT             0x1  /* 0-open drain; 1-push/pull, active low; 2-push/pull, active high */
#define QFALC_GLOBAL_PORT           0x03 /* 00 = SEC: Input, active high; 01 = SEC: Output, active high; 02 = FSC: Output, active high; 03 = FSC: Output, active low */

/* system data rate, clock rate, channel translation mode */
#define CR_E1_2048                  0x00
#define CR_E1_4096                  0x01
#define CR_E1_8192                  0x02
#define CR_E1_16384                 0x03

#define DR_E1_2048                  0x00
#define DR_E1_4096                  0x01
#define DR_E1_8192                  0x02
#define DR_E1_16384                 0x03

#define MFP_SYPXQ                   0x00
#define MFP_XMFS                    0x01
#define MFP_XSIG                    0x02
#define MFP_TCLK                    0x03
#define MFP_XMFB                    0x04
#define MFP_XSIGM                   0x05
#define MFP_DLX                     0x06
#define MFP_XCLK                    0x07
#define MFP_XLT                     0x08

#define MFP_SYPRQ                   0x00
#define MFP_RFM                     0x10
#define MFP_RMFB                    0x20
#define MFP_RSIGM                   0x30
#define MFP_RSIG                    0x40
#define MFP_DLR                     0x50
#define MFP_FREEZ                   0x60
#define MFP_RFSPQ                   0x70

#define ERROR_COUNTER_FEC           0x00
#define ERROR_COUNTER_CVC           0x01
#define ERROR_COUNTER_CEC           0x02
#define ERROR_COUNTER_CEC2_EBC      0x03
#define ERROR_COUNTER_CEC3_BEC      0x04
#define ERROR_COUNTER_EBC_COEC      0x05

#define SSM_E1_QUALITY_UNKNOWN      0
#define SSM_E1_REC_G_811            2
#define SSM_E1_SSU_A                4
#define SSM_E1_SSU_B                8
#define SSM_E1_SETS                 11
#define SSM_E1_DO_NOT_USE           15

#define ELASTIC_BUF_ON              0x01
#define ELASTIC_BUF_OFF             0x00

#define T1_FRAMING_F4               0x01 //4-frame multiframe
#define T1_FRAMING_F12              0x00 //12-frame multiframe (D4)
#define T1_FRAMING_ESF              0x02 //Extended Superframe (F24)
#define T1_FRAMING_F72              0x03 //72-frame multiframe (SLC96)

#define T1_XPM0_xDB                 0x9C
#define T1_XPM1_xDB                 0x03
#define T1_XPM2_xDB                 0x00

#define T1_XPM0_0DB                 0xD7
#define T1_XPM1_0DB                 0x22
#define T1_XPM2_0DB                 0x01

#define T1_XPM0_7_5DB               0x51
#define T1_XPM1_7_5DB               0x02
#define T1_XPM2_7_5DB               0x20

#define T1_XPM0_15DB                0x51
#define T1_XPM1_15DB                0x01
#define T1_XPM2_15DB                0x20

#define T1_XPM0_22_5DB              0x50
#define T1_XPM1_22_5DB              0x01
#define T1_XPM2_22_5DB              0x20


/*-------------------------- ioctl cmd --------------------------*/
#define E1_CMD_IOC_MAGIC            0xE1
#define E1LOOP                      _IO(E1_CMD_IOC_MAGIC, 0)
#define E1FRAMEFORMAT               _IO(E1_CMD_IOC_MAGIC, 1)
#define E1SELECTCHANNEL             _IO(E1_CMD_IOC_MAGIC, 2)
#define E1READDATA                  _IO(E1_CMD_IOC_MAGIC, 3)
#define E1FRAMEERRCNT               _IO(E1_CMD_IOC_MAGIC, 4)
#define E1CODEVIOCNT                _IO(E1_CMD_IOC_MAGIC, 5)
#define E1FRAMECRCERRCNT            _IO(E1_CMD_IOC_MAGIC, 6)
#define E1EBITERRCNT                _IO(E1_CMD_IOC_MAGIC, 7)
#define E1INFOCNTALL                _IO(E1_CMD_IOC_MAGIC, 8)
#define E1INFOCNT                   _IO(E1_CMD_IOC_MAGIC, 9)
#define E1WRITECAS                  _IO(E1_CMD_IOC_MAGIC, 10)
#define E1READCAS                   _IO(E1_CMD_IOC_MAGIC, 11)
#define E1MODE                      _IO(E1_CMD_IOC_MAGIC, 12)
#define E1CHANGETS                  _IO(E1_CMD_IOC_MAGIC, 13)
#define E1CLOSETS                   _IO(E1_CMD_IOC_MAGIC, 14)
#define E1RESET                     _IO(E1_CMD_IOC_MAGIC, 15)
#define E1LINKSTATUS                _IO(E1_CMD_IOC_MAGIC, 16)
#define E1GETSTATUS                 _IO(E1_CMD_IOC_MAGIC, 17)
#define E1GETSA4STATUS              _IO(E1_CMD_IOC_MAGIC, 18)
#define E1GETSA5STATUS              _IO(E1_CMD_IOC_MAGIC, 19)
#define E1GETSA6STATUS              _IO(E1_CMD_IOC_MAGIC, 20)
#define E1GETSA7STATUS              _IO(E1_CMD_IOC_MAGIC, 21)
#define E1GETSA8STATUS              _IO(E1_CMD_IOC_MAGIC, 22)
#define E1NUM                       _IO(E1_CMD_IOC_MAGIC, 23)
#define E1ISSURGE                   _IO(E1_CMD_IOC_MAGIC, 24)
#define E1SSMCFG                    _IO(E1_CMD_IOC_MAGIC, 25)
#define E1PORTISCRC4                _IO(E1_CMD_IOC_MAGIC, 26)
#define E1ACTNUM                    _IO(E1_CMD_IOC_MAGIC, 27)
#define E1RCLKSEL                   _IO(E1_CMD_IOC_MAGIC, 28)
#define E1RCLKSELSURGE              _IO(E1_CMD_IOC_MAGIC, 29)
#define E1CLOCKCFG                  _IO(E1_CMD_IOC_MAGIC, 30)
#define E1LOOPMODE                  _IO(E1_CMD_IOC_MAGIC, 31)
#define E1RCLKRDPORT                _IO(E1_CMD_IOC_MAGIC, 32)
#define E1FDSTAT                    _IO(E1_CMD_IOC_MAGIC, 33)
#define E1LED                       _IO(E1_CMD_IOC_MAGIC, 34)
#define E1SENDDATA                  _IO(E1_CMD_IOC_MAGIC, 35)
#define E1GETBOARDVER               _IO(E1_CMD_IOC_MAGIC, 36)
#define E1GETCPLDVER                _IO(E1_CMD_IOC_MAGIC, 37)
#define E1SETCLKSRC                 _IO(E1_CMD_IOC_MAGIC, 38)
#define E1STARTFISU                 _IO(E1_CMD_IOC_MAGIC, 39)
#define E1STOPFISU                  _IO(E1_CMD_IOC_MAGIC, 40)
#define E1SETREGVALUE               _IO(E1_CMD_IOC_MAGIC, 41)
#define E1READREGVALUE              _IO(E1_CMD_IOC_MAGIC, 42)
#define CHIPMODE                    _IO(E1_CMD_IOC_MAGIC, 43)
#define E1GETDAGPORTNUM             _IO(E1_CMD_IOC_MAGIC, 44)
#define E1GETPRODUCTTYPE            _IO(E1_CMD_IOC_MAGIC, 45)
#define E1GETMODVER                 _IO(E1_CMD_IOC_MAGIC, 46)
#define E1GETMODE                   _IO(E1_CMD_IOC_MAGIC, 47)
#define E1SENDR2SIG                 _IO(E1_CMD_IOC_MAGIC, 48)
#define E1RECVR2SIG                 _IO(E1_CMD_IOC_MAGIC, 49)
#define E1LINECODE                  _IO(E1_CMD_IOC_MAGIC, 50)
#define E1CMDBUTT                   _IO(E1_CMD_IOC_MAGIC, 51)


typedef enum en_pef22554_mode {
    MODE_E1,
    MODE_T1,
} PEF22554_MODE_EN;

typedef enum en_buf_status {
	E1_RX_BUFF_CLEAN,
	E1_RX_BUFF_SETTLE,
} EN_BUF_STATUS;

typedef enum en_fd_use_stat {
    FDUNUSED,
    FDUSED,
} EN_FD_USE_STAT;

typedef enum tag_en_type
{
    EN_NOTEND,
    EN_END,
} EN_TYPE;

typedef enum tag_en_mode {
	E10,
	E11,
	PCM0,
	PCM1,
	NOLOOP,
	DOUBLEFRAME,
	MULTIFRAME,
	MODMULTIFRAME,
	SS7,
	SS1,
	PRI,
} EN_MODE;

typedef enum tag_enable {
	E1_ON,
	E1_OFF,
} EN_ENABLE;

typedef enum tag_e1_loop_mode
{
	E1_NO_LOOP,
	E1_LOCAL_LOOP,
	E1_PAYLOAD_LOOP,
	E1_REMOTE_LOOP,
	E1_CHANNEL_LOOP,
} E1_LOOP_MODE;

typedef enum tag_e1_line_code
{
	NRZ,
	CMI,
	AMI,
	HDM3
} E1_LINE_CODE;

typedef enum tag_cpld_clk_src
{
	CLK_MASTER,  //clk from 16.384M clock
	CLK_PEF1,
} CLK_SOURCE_E;



/*-------------------------- write only registers --------------------------*/
typedef struct tag_qfalc_write_register
{
    u16 xfifo;                /* 00-01    Transmission FIFO                        */
    u8  cmdr;                 /* 02       Command Register                         */
    u8  unused_area_1[93];    /* 03-5F    Gap within address range                 */
    u8  dec;                  /* 60       Disable Error Counter                    */
    u8  unused_byte_1;        /* 61       Gap within address range                 */
    u8  unused_byte_2;        /* 62       Gap within address range                 */
    u8  unused_area_2[13];    /* 63-6F    Gap within address range                 */
    u8  xs[16];               /* 70-7F    Transmit CAS Register                    */
    u8  unused_area_3[7];     /* 80-86    Gap within address range                 */
    u8  cmdr2;                /* 87       Command Register no.2                    */
} QFALC_WRITE_REGISTER;

/*-------------------------- read only registers --------------------------*/
typedef struct tag_qfalc_read_register
{
    u16 rfifo;                /* 00/01    Receive FIFO                             */
    u8  unused_area_1[71];    /* 02-48    Gap within address range                 */
    u8  rbd;                  /* 49       Receive Buffer Delay                     */
    u8  vstr;                 /* 4A       Version Status                           */
    u8  res;                  /* 4B       Receive Equalizer Status                 */
    u8  frs0;                 /* 4C       Framer Receive Status 0                  */
    u8  frs1;                 /* 4D       Framer Receive Status 1                  */
    u8  rsw_frs2;             /* 4E       Receive Service U                        */
                              /*          Framer Receive Status 2                  */
    u8  rsp;                  /* 4F       Receive Spare Bits                       */
    u8  fecl;                 /* 50/51    Framing Error Counter                    */
    u8  fech;
    u8  cvcl;                 /* 52/53    Code Violation Counter                   */
    u8  cvch;
    u8  cec1l;                /* 54/55    CRC Error Counter 1                      */
    u8  cec1h;
    u8  ebcl;                 /* 56/57    E-Bit Error Counter                      */
    u8  ebch;
    u8  cec2l;                /* 58/59    CRC Error Counter 2   (E1)               */
    u8  cec2h;
    u8  cec3l;                /* 5A/5B    CRC Error Counter 3 (E1)                 */
    u8  cec3h;
    u8  rsa4_rdl1;            /* 5C       Receive SA4 Bit Register                 */
                              /*          Receive DL-Bit Register 1                */
    u8  rsa5_rdl2;            /* 5D       Receive SA5 Bit Register                 */
                              /*          Receive DL-Bit Register 2                */
    u8  rsa6_rdl3;            /* 5E       Receive SA6 Bit Register                 */
                              /*          Receive DL-Bit Register 3                */
    u8  rsa7;                 /* 5F       Receive SA7 Bit Register                 */
    u8  rsa8;                 /* 60       Receive SA8 Bit Register                 */
    u8  rsa6s;                /* 61       Receive Sa6 Bit Status Register          */
    u8  rsp1;                 /* 62       Receive Signaling Pointer 1              */
    u8  rsp2;                 /* 63       Receive Signaling Pointer 2              */
    u8  sis;                  /* 64       Signaling Status Register                */
    u8  rsis;                 /* 65       Receive Signaling Status Register        */
    u16 rbc;                  /* 66/67    Receive Byte Control                     */
    u8  isr0;                 /* 68       Interrupt Status Register 0              */
    u8  isr1;                 /* 69       Interrupt Status Register 1              */
    u8  isr2;                 /* 6A       Interrupt Status Register 2              */
    u8  isr3;                 /* 6B       Interrupt Status Register 3              */
    u8  isr4;                 /* 6C       Interrupt Status Register 4              */
    u8  unused_byte_2;        /* 6D       Gap within address range                 */
    u8  gis;                  /* 6E       Global Interrupt Status                  */
    u8  cis;                  /* 6F       Channel Interrupt Status                 */
    u8  rs[16];               /* 70-7F    Receive CAS Register   1...16            */
} QFALC_READ_REGISTER;

/*-------------------------- read and write registers --------------------------*/
typedef struct tag_qfalc_read_write_register
{
    u16 unused_U16_1;        /* 00/01     Gap within address range                 */
    u8  unused_byte_1;        /* 02       Gap within address range                 */
    u8  mode;                 /* 03       Mode Register                            */
    u8  rah1;                 /* 04       Receive Address High 1                   */
    u8  rah2;                 /* 05       Receive Address High 2                   */
    u8  ral1;                 /* 06       Receive Address Low 1                    */
    u8  ral2;                 /* 07       Receive Address Low 2                    */
    u8  ipc;                  /* 08       Interrupt Port Configuration             */
    u8  ccr1;                 /* 09       Common Configuration Register 1          */
    u8  ccr2;                 /* 0A       Common Configuration Register 3          */
    u8  pre;                  /* 0B       Preamble Register                        */
    u8  rtr1;                 /* 0C       Receive Timeslot Register 1              */
    u8  rtr2;                 /* 0D       Receive Timeslot Register 2              */
    u8  rtr3;                 /* 0E       Receive Timeslot Register 3              */
    u8  rtr4;                 /* 0F       Receive Timeslot Register 4              */
    u8  ttr1;                 /* 10       Transmit Timeslot Register 1             */
    u8  ttr2;                 /* 11       Transmit Timeslot Register 2             */
    u8  ttr3;                 /* 12       Transmit Timeslot Register 3             */
    u8  ttr4;                 /* 13       Transmit Timeslot Register 4             */
    u8  imr0;                 /* 14       Interrupt Mask Register 0                */
    u8  imr1;                 /* 15       Interrupt Mask Register 1                */
    u8  imr2;                 /* 16       Interrupt Mask Register 2                */
    u8  imr3;                 /* 17       Interrupt Mask Register 3                */
    u8  imr4;                 /* 18       Interrupt Mask Register 4                */
    u8  unused_area_1[2];     /* 19-1A    Gap within address range                 */
    u8  ierr;                 /* 1B       Single Bit Insertion Register            */
    u8  fmr0;                 /* 1C       Framer Mode Register 0                   */
    u8  fmr1;                 /* 1D       Framer Mode Register 1                   */
    u8  fmr2;                 /* 1E       Framer Mode Register 2                   */
    u8  loop;                 /* 1F       Channel Loop Back                        */
    u8  xsw_fmr4;             /* 20       Transmit Service UINT                    */
                              /*          Framer Mode Reigster 4                   */
    u8  xsp_fmr5;             /* 21       Transmit Spare Bits                      */
                              /*          Framer Mode Reigster 5                   */
    u8  xc0;                  /* 22       Transmit Control 0                       */
    u8  xc1;                  /* 23       Transmit Control 1                       */
    u8  rc0;                  /* 24       Receive Control 0                        */
    u8  rc1;                  /* 25       Receive Control 1                        */
    u8  xpm0;                 /* 26       Transmit Pulse Mask 0                    */
    u8  xpm1;                 /* 27       Transmit Pulse Mask 1                    */
    u8  xpm2;                 /* 28       Transmit Pulse Mask 2                    */
    u8  tswm;                 /* 29       Transparent Service UINT Mask            */
    u8  unused_byte_2;        /* 2A       Gap within address range                 */
    u8  idle;                 /* 2B       Idle Channel Code                        */
    u8  xsa4_xdl1;            /* 2C       Transmit SA4 Bit Register                */
                              /*          Fransmit DL-Bit Register 1               */
    u8  xsa5_xdl2;            /* 2D       Transmit SA5 Bit Register                */
                              /*          Fransmit DL-Bit Register 2               */
    u8  xsa6_xdl3;            /* 2E       Transmit SA6 Bit Register                */
                              /*          Fransmit DL-Bit Register 3               */
    u8  xsa7_ccb1;            /* 2F       Transmit SA7 Bit Register                */
                              /*          Clear Channel Register 1                 */
    u8  xsa8_ccb2;            /* 30       Transmit SA8 Bit Register                */
                              /*          Clear Channel Register 2                 */
    u8  fmr3_ccb3;            /* 31       Framer Mode Reg. 3                       */
                              /*          Clear Channel Register 3                 */
    u8  icb1;                 /* 32       Idle Channel Register 1                  */
    u8  icb2;                 /* 33       Idle Channel Register 2                  */
    u8  icb3;                 /* 34       Idle Channel Register 3                  */
    u8  icb4;                 /* 35       Idle Channel Register 4                  */
    u8  lim0;                 /* 36       Line Interface Mode 0                    */
    u8  lim1;                 /* 37       Line Interface Mode 1                    */
    u8  pcd;                  /* 38       Pulse Count Detection                    */
    u8  pcr;                  /* 39       Pulse Count Recovery                     */
    u8  lim2;                 /* 3A       Line Interface Mode Register 2           */
    u8  lcr1;                 /* 3B       Line Code Register 1                     */
    u8  lcr2;                 /* 3C       Line Code Register 2                     */
    u8  lcr3;                 /* 3D       Line Code Register 3                     */
    u8  sic1;                 /* 3E       System Interface Control 1               */
    u8  sic2;                 /* 3F       System Interface Control 2               */
    u8  sic3;                 /* 40       System Interface Control 3               */
    u8  unused_area_2[3];     /* 41-43    Gap within address range                 */
    u8  cmr1;                 /* 44       Clock Mode Register 1                    */
    u8  cmr2;                 /* 45       Clock Mode Register 2                    */
    u8  gcr;                  /* 46       Global Configuration Register            */
    u8  esm;                  /* 47       Errored Second Mask                      */
    u8  unused_area_3[56];    /* 48-7F    Gap within address range                 */
    u8  pc1;                  /* 80       Port Configuration 1                     */
    u8  pc2;                  /* 81       Port Configuration 2                     */
    u8  pc3;                  /* 82       Port Configuration 3                     */
    u8  pc4;                  /* 83       Port Configuration 4                     */
    u8  pc5;                  /* 84       Port Configuration 5                     */
    u8  gpc1;                 /* 85       Global Port Configuration 1              */
    u8  unused_byte_3;        /* 86       Gap within address range                 */
    u8  unused_area_4[6];     /* 87-8C    Gap within address range                 */
    u8  ccr5;                 /* 8D       Common Configuration Register 5          */
    u8  unused_area_5[4];     /* 8E-91    Gap within address range                 */
    u8  gcm1;                 /* 92       Global Clocking Modes                    */
    u8  gcm2;                 /* 93       Channel Interrupt Status                 */
    u8  gcm3;                 /* 94       Global Clocking Modes                    */
    u8  gcm4;                 /* 95       Channel Interrupt Status                 */
    u8  gcm5;                 /* 96       Global Clocking Modes                    */
    u8  gcm6;                 /* 97       Global Clocking Modes                    */
    u8  gcm7;                 /* 98       Global Clocking Modes                    */
    u8  gcm8;                 /* 99       Global Clocking Modes                    */
    u8  unused_area_6[6];     /* 9A-9F    Gap within address range                 */
    u8  tseo;                 /* A0       Time Slot Even/Odd Select                */
    u8  tsbs1;                /* A1       Time Slot Bit Select 1                   */
    u8  unused_area_7[6];     /* A2-A7    Gap within address range                 */
    u8  tpc0;                 /* A8       Test Pattern Control 0                   */
    u8  unused_area_8[18];    /* A9-BA    Gap within address range                 */
    u8  BB;                   /* BB       Receiver Sensitivity optimizatoin        */
    u8  BC;                   /* BC       Receiver Sensitivity optimization        */
} QFALC_READ_WRITE_REGISTER;

/*-------------------------- reg mapping --------------------------*/
typedef union tag_qfalc_reg_map
{
    QFALC_WRITE_REGISTER         wr;   /* write register                           */
    QFALC_READ_REGISTER          rd;   /* read register                            */
    QFALC_READ_WRITE_REGISTER    rdWr; /* read/write register                      */
} QFALC_REG_MAP;


struct st_data
{
	int  len;
	int  cur;
	char buf[MAX_DATA_LEN];
};

typedef struct tag_buffer
{
	struct st_data *head;
	struct st_data *tail;
	int size;
	int len;
	struct st_data *databuf;
} ST_BUFFER;

struct quadFALC_dev_info {
    PEF22554_MODE_EN chipmode;
    unsigned long physical_base_addr;
    unsigned long virtual_base_addr;
    struct resource *virtual_base_source;
    u32 g_bE1ChipStatus;
    struct semaphore sem[NUM_OF_FALC];
	struct semaphore channel_sem[NUM_OF_FALC];
    volatile int sendfrmerr[NUM_OF_FALC], sendfrmfail[NUM_OF_FALC];
    volatile int recvfrmerr[NUM_OF_FALC], recvfrmfail[NUM_OF_FALC];
    volatile int freezecnt[NUM_OF_FALC];
    volatile int intdeadcnt[NUM_OF_FALC];
    unsigned long infocnt[NUM_OF_FALC];
    unsigned long rpfcnt[NUM_OF_FALC], rmecnt[NUM_OF_FALC];
    unsigned long casccnt[NUM_OF_FALC], xmbcnt[NUM_OF_FALC];
    wait_queue_head_t rq[NUM_OF_FALC],wq[NUM_OF_FALC];
    struct timer_list timer[NUM_OF_FALC];
    unsigned long mode[NUM_OF_FALC];
	ST_BUFFER aos_buf_receive[NUM_OF_FALC];
	ST_BUFFER aos_buf_send[NUM_OF_FALC];
	u8 e1_use_state[NUM_OF_FALC];
};

struct quadFALC_dev {
	unsigned long virtual_gpio_base_addr,virtual_cpld_base_addr;
	struct resource *virtual_gpio_source, *virtual_cpld_source;
	u32 g_ulE1ActiveNum;
	spinlock_t lock;

	unsigned long cnt;
	unsigned long errcnt;
	unsigned long intcnt;
	unsigned long rpfcnt, casccnt;
	unsigned long rmecnt, xmbcnt;
	unsigned long infocntall;

	wait_queue_head_t q;
	struct cdev quadFALC_cdev;
	struct task_struct *quadFALC_task;
	unsigned long irq_handle_state;
    struct quadFALC_dev_info pef22554info[MAX_PEF22554_NUM];
};

typedef struct tag_file_private {
	struct quadFALC_dev *falc;
	int  channel;
    u32  major;
    u32  minor;
} ST_FILE_PRIVATE;


typedef struct tag_e1_receive_data {
	u32  enBuffSta;
	int  ptSend;
	char ucBuffer[380];
} ST_E1_RECEIVE_DATA;

typedef struct r2_msg_st
{
    u8 channel;
    u8 cas;
}R2_MSG_ST;

typedef struct read_cas_st
{
	u8 max_num;
	u8 ret_num;
	u8 buf[0];
}READ_CAS_ST;

typedef struct tag_e1_err_status
{
	u8 ucFEC:1;
	u8 ucCVC:1;
	u8 ucCEC:1;
	u8 ucCEC2EBC:1;
	u8 ucCEC3BEC:1;
	u8 ucEBC:1;
} E1_ERR_STATUS;

typedef struct st_chip_mode_cmd {
    u32 chipno;
    PEF22554_MODE_EN chipmode;
} ST_CHIP_MODE_CMD;

typedef struct tag_ioctl_send_msg
{
    int len;
    int ret;
    u8  buf[360];
} ST_IO_SEND;

typedef struct st_e1_cmd_arg {
	u32 e1no;
    PEF22554_MODE_EN chipmode;
	EN_MODE mode;
	EN_ENABLE enable;
	u32 tsno;
} ST_E1_CMD_ARG;

typedef struct st_e1_err_statics
{
    int sendfrmerr;
    int sendfrmfail;
    int recvfrmerr;
    int recvfrmfail;
    int intdeadcnt;
    int freezecnt;
    int rpfcnt;
    int rmecnt;
    int casccnt;
    int xmbcnt;
} ST_E1_ERR_STATICS;

typedef struct tag_ssm_cfg {
    u8 port;
    u8 mode;
    u8 sabit;
    u8 ssm;
} ST_SSM_CFG;


typedef struct st_e1_set_reg_arg {
    u8  e1no;
    u32 regaddr;
    u8  regvalue;
} ST_E1_REG;


/*-------------------------- functions --------------------------*/
void quadFALC_mem_read8(u8 *a, u8 *d_ptr);
void quadFALC_mem_write8(u8 *a, u8 d);
void quadFALC_set_bit8(u8 *a, u8 b);
void quadFALC_clear_bit8(u8 *a, u8 b);


void quadFALC_pef_read_reg(u32 addr, u8* data);
void quadFALC_pef_write_reg(u32 addr, u8 data);
void quadFALC_cpld_read_reg(u32 addr, u8* data);
void quadFALC_cpld_write_reg(u32 addr, u8 data);
int  quadFALC_gpio_led_cfg(u8 led_idx, u8 led_cfg);
u8   quadFALC_gpio_get_board_mark(void);
u8   quadFALC_gpio_get_board_type(void);
u8   quadFALC_gpio_get_hard_id(void);
u8   quadFALC_gpio_get_board_ver(void);
void quadFALC_gpio_reset_chip(int chipno);
u8   quadFALC_gpio_get_cpld_ver(void);
int  quadFALC_gpio_init(void);
void quadFALC_gpio_exit(void);

void quadFALC_irq_cfg(int chipno);
irqreturn_t quadFALC_irq_handler(int irq, void *dev_id);
int  quadFALC_irq_thread_create(void);
void quadFALC_irq_thread_destroy(void);

int  quadFALC_buffer_is_empty(ST_BUFFER *buffer);
int  quadFALC_buffer_is_full(ST_BUFFER *buffer);
int  quadFALC_buffer_add(ST_BUFFER *buffer, char *srcbuf, int length, EN_TYPE type);
int  quadFALC_buffer_fetch(ST_BUFFER *buffer, struct st_data *decbuf);
int  quadFALC_buffer_clear(ST_BUFFER *buffer);
struct st_data * quadFALC_buffer_get_empty_node(ST_BUFFER *buffer);
void quadFALC_buffer_get_next_node(ST_BUFFER *buffer);
int  quadFALC_buffer_init(void);
int  quadFALC_buffer_free(void);



u32  quadFALC_e1_rclk_is_surge(void);
void quadFALC_e1_ssm_cfg (u8 port, u8 mode, u8 sabit, u8 ssm);
u8   quadFALC_e1_port_is_crc4(u8 port);
void quadFALC_e1_rclk_select(u8 port);
void quadFALC_e1_rclk_select_surge(void);
void quadFALC_e1_clocking_cfg(u8 port);
u8   quadFALC_e1_loop_ctrl_read( u8 port );
u8   quadFALC_e1_rclk_read_port(void);
u32  quadFALC_e1_get_active_num(void);
void quadFALC_e1_loop_ctrl_set (u8 port, u8 type, u8 mode, u8 ts_no);
void quadFALC_e1_lineinterface_cfg (u8 port, u8 clocking, u8 line_code);
void quadFALC_e1_framer_cfg (u8 port, u8 framing_tx ,u8 framing_rx);
int  quadFALC_e1_chip_init(int chipno);
int  quadFALC_t1_chip_init(int chipno);

#endif

