/******************************************************************************
        (c) COPYRIGHT 2002-2003 by Shenzhen Allywll Information Co.,Ltd
                          All rights reserved.
File: pef22554.c
Desc:the source file of user config
Modification history(no, author, date, desc)
1.Holy 2003-04-02 create file
2.luke 2006-04-20 for AOS
******************************************************************************/
#include <linux/interrupt.h>
#include <linux/delay.h>

#include "pef22554_drv.h"
#include "pef22554_reg.h"

extern struct quadFALC_dev QuadFALCDev;
extern QFALC_REG_MAP *ptrreg[MAX_PEF22554_NUM*NUM_OF_FALC];

/*Group=Elastic Buffer
------------------------------------------------------------------------
  Description:  Set buffer size for transmit and receive side.
  Arguments:
                port - the QuadFALCDev.port
                buffer_tx - on/off buffer used for transmit size
                buffer_rx - on/off buffer used for receive size

  Settings:     Possible settings for buffer_tx and buffer_rx are:

                ON - turns on the buffer
                OFF - turns off the buffer


  Return Value: void

  Example:     e1_elasticbuffer_cfg (2, ON, ON)
                 turns on buffer in transmit and receive direction on Port 2


  Remarks:      Buffer sizes are configured in the headerfile pef22554_cfg.h
-------------------------------------------------------------------------*/
void quadFALC_t1_elasticbuffer_cfg (u8 port, u8 buffer_tx, u8 buffer_rx)
{

    QFALC_REG_MAP*     falc;

    falc = ptrreg[port];

    /*clear buffersize rc and tx */
    CLEAR_BIT8(&falc->rdWr.sic1,0x33);

    /*sets the buffersize for tx and rx */
    SET_BIT8(&falc->rdWr.sic1 ,buffer_tx ? TX_BUFFER : 0 | buffer_rx ? (RX_BUFFER<< 4) : 0x30);

}


/*----------------------------------------------------------------
Receiver Reset
------------------------------------------------------------------*/
void quadFALC_t1_receiver_reset(u8 port)
{
    u32 cnt;
    u8  sis;
    QFALC_REG_MAP  *falc;

    falc = ptrreg[port];
    /*
    RRES Receiver Reset
    The receive line interface except the clock and data recovery unit
    (DPLL), the receive framer, the one-second timer and the receive
    signaling controller are reset. However the contents of the control
    registers is not deleted.
    */

    SET_BIT8(&falc->wr.cmdr , CMDR_RRES);

    cnt=0;
	do
	{
		MEM_READ8(&falc->rd.sis,&sis);
        cnt++;
	}
	while( ((sis&SIS_CEC) == 0x04) && (cnt<10000) );

    if( cnt >= 10000 )
    {
        debug(" t1 22554 channel(%d) Receiver reset SiS_CEC failed!!",port);
	 mdelay(200);
        return;
    }

    /*Receiver Sensitivity optimization*/
	MEM_WRITE8(&falc->rdWr.BB , 0x17);
    MEM_WRITE8(&falc->rdWr.BC , 0x55);
    MEM_WRITE8(&falc->rdWr.BB , 0x97);
    MEM_WRITE8(&falc->rdWr.BB , 0x11);
    MEM_WRITE8(&falc->rdWr.BC , 0xAA);
    MEM_WRITE8(&falc->rdWr.BB , 0x91);
    MEM_WRITE8(&falc->rdWr.BB , 0x12);
    MEM_WRITE8(&falc->rdWr.BC , 0x55);
    MEM_WRITE8(&falc->rdWr.BB , 0x92);
    MEM_WRITE8(&falc->rdWr.BB , 0x0C);
    MEM_WRITE8(&falc->rdWr.BC , 0x00);
    MEM_WRITE8(&falc->rdWr.BB , 0x8C);
    MEM_WRITE8(&falc->rdWr.BB , 0x0C);

}


/*Group=Framer
------------------------------------------------------------------------
  Description:  Configures the framer device (framing mode etc.).
  Arguments:
                port - the QuadFALCDev.port
                framing_tx - framing format for the transmit side E1: double frame, CRC4 multiframe,
                framing_rx - framing format for the receive side (see above);only applicable in E1 mode

  Settings:     Possible settings for framing_rx and framing_tx are:
                FRAMING_DOUBLEFRAME - Doubleframe format
                FRAMING_MULTIFRAME_TX - CRC4 Multiframe format for tx
                                        direction
                FRAMING_MULTIFRAME_RX - CRC4 Multiframe format for rx
                                        direction
                FRAMING_NO - transparent mode

    Possible settings for framing_rx are:
                FRAMING_MULTIFRAME_MOD - CRC4 Multiframe format with modified
                                         CRC4 Multiframe alignment algorithm
  Return Value: void

  Example:      e1_framer_cfg(1, FRAMING_MULTIFRAME_TX, FRAMING_MULTIFRAME_RX)
                configures on Port 1:CRC4 Multiframe format for tx and rx side

  Remarks:
-------------------------------------------------*/
void quadFALC_t1_framer_cfg (u8 port, u8 framing_tx ,u8 framing_rx)
{
    QFALC_REG_MAP   *falc;
    u8    mode1,sis;
    u8    fram_tx;
    u8    fram_rx;
    u32   cnt;

    fram_tx = 0x00;
    fram_rx = 0x00;

    switch (framing_tx)
    {
	    case T1_FRAMING_F4:
	        fram_tx = 0x00;
	        break;

	    case T1_FRAMING_F12:
	        fram_tx = 0x01;
	        break;

        case T1_FRAMING_ESF:
            break;
        case T1_FRAMING_F72:
            break;
	    default:
	        break;
    }


    switch (framing_rx)
    {
	    case T1_FRAMING_F4:
	        fram_tx = 0x00;
	        break;

	    case T1_FRAMING_F12:
	        fram_tx = 0x01;
	        break;

        case T1_FRAMING_ESF:
            break;
        case T1_FRAMING_F72:
            break;

	    default:
	        break;
    }

    falc = ptrreg[port];


    /*
        Framer Mode Register 4 (Read/Write)
        Value after reset: 00H
          FMR4 AIS3 TM XRA SSC1 SSC0 AUTO FM1 FM0 (x20)
        FM(1:0) Select Frame Mode
            FM = 0: 12-frame multiframe format (F12, D3/4)
            FM = 1: 4-frame multiframe format (F4)
            FM = 2: 24-frame multiframe format (ESF)
            FM = 3: 72-frame multiframe format (F72, remote switch mode)
    */
    mode1 = fram_tx<<3;
    CLEAR_BIT8(&falc->rdWr.xsw_fmr4 ,0x03);
    SET_BIT8(&falc->rdWr.xsw_fmr4 ,framing_rx);

#ifdef INFINEON_ADVICE_MODE

    CLEAR_BIT8(&falc->rdWr.xsw_fmr4 ,FMR4_TM);
    CLEAR_BIT8(&falc->rdWr.fmr2 ,FMR2_DAIS);
    CLEAR_BIT8(&falc->rdWr.loop ,LOOP_RTM);

    switch(framing_tx)
    {
    case 0x00:
    case 0x03:
        CLEAR_BIT8(&falc->rdWr.fmr2 ,(FMR2_MCSP | FMR2_SSP));
        break;

    case 0x02:
        SET_BIT8(&falc->rdWr.fmr2 ,FMR2_MCSP | FMR2_SSP);
        break;
    }

    CLEAR_BIT8(&falc->rdWr.fmr0 ,0x05);
    SET_BIT8(&falc->rdWr.fmr0 ,FMR0_SRAF);

    SET_BIT8(&falc->rdWr.xsw_fmr4 , FMR4_AUTO | FMR4_SSC0);  /* LFA after 2 out of 5 false framing bits */
#endif

#ifndef INFINEON_ADVICE_MODE
    SET_BIT8(&falc->rdWr.fmr2 , FMR2_AXRA);   /* Automatic Transmit Remote Alarm*/
                    // |  FMR2_ALMF);        /* Automatic Loss of Multiframe   */
                                          /* FMR2.AXRA - Automatic Transmit Remote Alarm
                                             The Remote Alarm bit will be automatically set in the outgoing
                                             data stream if the receiver is in the asynchronous state
                                             (FRS0.LFA bit is set). In synchronous state the remote alarm
                                              bit will be reset.

                                              FMR2.ALMF - Automatic Loss of Multiframe
                                              The receiver will search a new basic- and multiframing if more
                                              than 914 CRC errors have been detected in a time interval of
                                              one second. The internal 914 CRC error counter will be reset
                                              if the multiframe synchronization is found. Incrementing the
                                              counter is only enabled in the multiframe synchronous state.*/

    SET_BIT8(&falc->rdWr.fmr1 , FMR1_ECM   );        /* Latch Error Counter every second  */

    cnt = 0;
    do
    {
        MEM_READ8(&falc->rd.sis,&sis);
        cnt++;
    }
    while( ((sis&SIS_CEC) == 0x04)&&(cnt<10000) );

    ///-
    //clear_watch_dog();
    if( cnt >= 10000 )
    {
	    debug(" t1 22554 channel(%d) Framer Cfg SiS_CEC failed!!",port);
    }
#endif

    MEM_WRITE8(&falc->wr.cmdr, CMDR_XRES);

    cnt = 0;
    do
    {
        MEM_READ8(&falc->rd.sis,&sis);
        cnt++;
    }
    while( ((sis&SIS_CEC) == 0x04)&&(cnt<10000) );

    ///-
    //clear_watch_dog();
    if( cnt >= 10000 )
    {
	    debug(" t1 22554 channel(%d) Framer Cfg SiS_CEC failed!!",port);
    }

    quadFALC_t1_receiver_reset(port);

}




/*group=Clocking
------------------------------------------------------------------------
  Description:  Clocking configuration.
                Please note that clocking rates are configured in the system interface part!

  Arguments:
                port - the QuadFALCDev.port

 Settings specific:
                clocking_io - Configuration of Clock-Pins SCLKR, SCLKX, RCLK
                clocking_sync_int_ext - Configuration of the used Clock and Sync Inputs (used or unused)
                active_edge - Configuration of the active Clock edges
                buffer_transparent_mode - Configuration of the transparent Buffer modes -> Sync pulses independancy
                tlck_config - Usage of the TCLK signal
                tlck_mode - Type of TCLK signal

  Settings:     Possible settings for clocking_io are:

                RCLK_INPUT - RCLK is INPUT
                RCLK_OUTPUT - RCLK is OUTPUT

                SCLKR_INPUT - SCLKR is INPUT
                SCLKR_OUTPUT - SCLKR is OUTPUT

                SCLKX_INPUT - SCLKX is INPUT
                SCLKX_OUTPUT - SCLKX is OUTPUT

    The parameter clocking_io must be a logical or of three defines, one for each group to program.

    Possible settings for clocking_sync_int_ext are:

                SCLKX_EXT - external SCLKX is used
                CLK_INT_IN - internal Clock is used for clocking into the
                             transmit buffer

                SYPXQ_EXT - external SYPXQ is used
                SYNC_TX_INT - internal transmit syncpulse is used for
                              syncronisation

                SCLKR_EXT - external SCLKR is used
                CLK_INT_OUT - internal Clock is used for clocking out off the
                              receive buffer

                SYPRQ_EXT - external SYPRQ is used
                SYNC_RX_INT - internal receive syncpulse is used for
                              syncronisation

    The parameter clocking_sync_int_ext must be a logical or of four defines, one for each group.

    Possible settings for active_edge are:

                CLK_FALLING_OUT - Data is clocked out with the falling edge
                                  of the used receive clock
                CLK_RISING_OUT  - Data is clocked out with the rising edge
                CLK_FALLING_IN - Data is clocked in with the falling edge of
                                 the used transmit clock
                CLK_RISING_IN - Data is clocked in with the rising edge

    The parameter active_edge must be a logical or of two defines, one for each group.

    Possible settings for buffer_transparent_mode are:

                RX_FRAME_SYNC - Receive framesyncs are used for clocking data
                                out of the buffer
                RX_SYNC_CLEAR - The relation between frame syncs and Data is
                                cleared in receive direction
                TX_FRAME_SYNC - Transmit framesyncs are used for clocking
                                data into the buffer
                TX_FRAME_CLEAR - The relation between frame syncs and data is
                                 cleared in transmit direction

    The parameter buffer_transparent_mode must be a logical or of two defines, one for each group.

    Possible settings for tlck_config are:

                TCLK_IGNORE - The signal on pin TCLK is ignored
                TCLK_DEJITTERED - The signal on pin TCLK will be dejittered
                                  and then used as transmit clock on the line
                                  interface
                TCLK_DIRECT - The signal on pin TCLK will directly be used as
                              transmit clock on the line interface

    Possible settings for tlck_mode are:

                TCLK_2048 - Supplied TCLK clock is 2.048 MHz (E1)
                TCLK_8192 - Supplied TCLK clock is 8.192 MHz (E1)

  Return Value:    none

  Example:     T1_clocking_cfg(1)
                                    1,
  See Also:    T1_systeminterface_cfg();

  Remarks:      Please note: Clocking rates are configured in the function pef22554_systeminterface_cfg().
-------------------------------------------------------------------------*/
void quadFALC_t1_clocking_cfg(u8 port)
{
    QFALC_REG_MAP    *falc;
    u8   clocking_io,clocking_sync_int_ext,active_edge;
    u8   buffer_transparent_mode,tlck_config;

    falc = ptrreg[port];

    /*0x00 (RCLK_INPUT|SCLKR_INPUT|SCLKX_INPUT):
       0x01 (RCLK_OUTPUT|SCLKR_INPUT|SCLKX_INPUT):
       0x02 (RCLK_INPUT|SCLKR_OUTPUT|SCLKX_INPUT):
       0x03 (RCLK_OUTPUT|SCLKR_OUTPUT|SCLKX_INPUT):
     */
     clocking_io = 0x01;

    /*
    0x00 (SCLKX_EXT|SYPXQ_EXT|SCLKR_EXT|SYPRQ_EXT):
    0x01 (SCLKX_EXT|SYPXQ_EXT|SCLKR_EXT|CLK_INT_IN):
    0x02 (SCLKX_EXT|SYPXQ_EXT|SYNC_TX_INT|SYPRQ_EXT):
    0x03 (SCLKX_EXT|SYPXQ_EXT|SYNC_TX_INT|CLK_INT_IN):
    0x04 (SCLKX_EXT|CLK_INT_OUT|SCLKR_EXT|SYPRQ_EXT):
    0x05 (SCLKX_EXT|CLK_INT_OUT|SCLKR_EXT|CLK_INT_IN):
    0x06 (SCLKX_EXT|CLK_INT_OUT|SYNC_TX_INT|SYPRQ_EXT):
    0x07 (SCLKX_EXT|CLK_INT_OUT|SYNC_TX_INT|CLK_INT_IN):
    0x08 (SYNC_RX_INT|SYPXQ_EXT|SCLKR_EXT|SYPRQ_EXT):
    0x09 (SYNC_RX_INT|SYPXQ_EXT|SCLKR_EXT|CLK_INT_IN):
    0x0A (SYNC_RX_INT|SYPXQ_EXT|SYNC_TX_INT|SYPRQ_EXT):
    0x0B (SYNC_RX_INT|SYPXQ_EXT|SYNC_TX_INT|CLK_INT_IN):
    0x0C (SYNC_RX_INT|CLK_INT_OUT|SCLKR_EXT|SYPRQ_EXT):
    0x0D (SYNC_RX_INT|CLK_INT_OUT|SCLKR_EXT|CLK_INT_IN):
    0x0E (SYNC_RX_INT|CLK_INT_OUT|SYNC_TX_INT|SYPRQ_EXT):
    0x0F (SYNC_RX_INT|CLK_INT_OUT|SYNC_TX_INT|CLK_INT_IN):
   */
    clocking_sync_int_ext = 0x00;

    /*
    0x00 (CLK_FALLING_OUT|CLK_FALLING_IN):
    0x01 (CLK_FALLING_OUT|CLK_RISING_IN):
    0x02 (CLK_RISING_OUT|CLK_FALLING_IN):
    0x03 (CLK_RISING_OUT|CLK_RISING_IN):
    */
   /*    active_edge =  0x02; for version1*/
    //active_edge =  0x01;  /*for version 2*/
    active_edge = 0x0;


    /*
    0x00 (RX_FRAME_SYNC|TX_FRAME_SYNC):
    0x01 (RX_SYNC_CLEAR|TX_FRAME_SYNC):
    0x02 (RX_FRAME_SYNC|TX_FRAME_CLEAR):
    0x03 (RX_SYNC_CLEAR|TX_FRAME_CLEAR):
    */
    buffer_transparent_mode = 0x00;

    /*
    0x00  TCLK_IGNORE:
    0x01  TCLK_DEJITTERED:
    0x02  TCLK_DIRECT:
    */
    tlck_config = 0x00;

    CLEAR_BIT8(&falc->rdWr.pc5 ,0x07);
    SET_BIT8(&falc->rdWr.pc5 , clocking_io & 0x07);;

    CLEAR_BIT8(&falc->rdWr.cmr2 , 0x0f);
    SET_BIT8(&falc->rdWr.cmr2 , clocking_sync_int_ext & 0x0f);

    CLEAR_BIT8(&falc->rdWr.sic3 , 0x0C);
    SET_BIT8(&falc->rdWr.sic3 , (active_edge & 0x03) << 2);

    CLEAR_BIT8(&falc->rdWr.fmr2 , 0x20);
    /*
     7    6  5   4    3    2   1    0
    AIS3 TM XRA SSC1 SSC0 AUTO FM1 FM0

    TM Transparent Mode
        Setting this bit enables the transparent mode:
        In transmit direction bit 8 of every FS/DL time slot from the system
        internal highway (XDI) is inserted in the F-bit position of the outgoing
        frame. Internal framing generation, insertion of CRC and DL data is
        disabled.
    AUTO Enable Auto Resynchronization
        0 The receiver does not resynchronize automatically. Starting a
        new synchronization procedure is possible by the bits
        FMR0.EXLS or FMR0.FRS.
        1 Auto-resynchronization is enabled.
    */
#ifdef INFINEON_ADVICE_MODE

    CLEAR_BIT8(&falc->rdWr.loop , 0x40);
    CLEAR_BIT8(&falc->rdWr.xsp_fmr5 , 0x04);
    SET_BIT8(&falc->rdWr.loop , ((buffer_transparent_mode & 0x01) << 6));
    SET_BIT8(&falc->rdWr.xsp_fmr5 , ((buffer_transparent_mode & 0x02) << 1));
#else
    SET_BIT8(&falc->rdWr.xsw_fmr4 , 0x04);
    /*使用弹性buffer*/
    SET_BIT8(&falc->rdWr.fmr2 , (buffer_transparent_mode & 0x01) << 5);
#endif
    /*
    XTM：发送透明模式
    0：发送器按照SYPX /XMFS 提供的帧头指示进行成帧。发送时隙分配的改变引起FAS
    比特位置的改变。
    1：解除系统侧接口对发送器的控制，发送器处在自由运行模式。发送器的成帧不受时隙
    分配及等干扰。常应用于Loop_timed 情况。发送弹性buffer 必须处于2 帧模式
    （SIC1.XBS1/0=10）。
    */
    SET_BIT8(&falc->rdWr.xsw_fmr4 , (buffer_transparent_mode & 0x02) << 5);


    /*
    DXJA：禁止内部发送抖动衰减器
    设置此位禁止发送抖动衰减。从发送弹性buffer 读取数据和在XL1/2（XDOP/N/XOID）
    上发送数据由TCLK 触发。在发送弹性buffer 旁路时，发送时钟取自SCLKX，与此位无
    关。
    DXSS：DCO_X 的同步时钟源的选择
    0：DCO_X 电路同步到源自SCLKR/X 或RCLK 的内部参考时钟。由于参考时钟的选择
    有多个寄存器位可以设置，其优先级为：LIM1.RL>CMR2.DXSS>LIM2.ELT>发送系统接
    口的当前工作时钟。
    1：DCO_X 电路同步到由TCLK 提供的外部参考时钟，若远端环回无效，TCLK 由
    PC(1…4).XPC(2…0)=011 配置
    */
    CLEAR_BIT8(&falc->rdWr.cmr1 , 0x03);
    SET_BIT8(&falc->rdWr.cmr1 , (tlck_config & 0x03));

    /*
    cmr1.drss (bit7,6) DCO-R receive reference clock source select
    00 receive reference clock generate by channel 1;
    01 receive reference clock generate by channel 2;
    10 receive reference clock generate by channel 3;
    11 receive reference clock generate by channel 4;
    */

    if (0 == port%4 )  /*port 0*/
    {
        SET_BIT8(&falc->rdWr.cmr1 , RCLK_SOURCE1<<6); /* This command selects the reference clock  */
                                                    /* for the DCO-R circuitry. Here the         */
                                                    /* recovered clock from the corresponding    */
                                                    /* line interface input is used.             */
    }

    if (1 == port%4 )  /*port 1*/
    {
        SET_BIT8(&falc->rdWr.cmr1 , RCLK_SOURCE2<<6); /* This command selects the reference clock  */
                                                  /* for the DCO-R circuitry. Here the         */
                                                    /* recovered clock from the corresponding    */
                                                    /* line interface input is used.             */
    }

    if (2 == port%4 )        /*port 2*/
    {
        SET_BIT8(&falc->rdWr.cmr1 , RCLK_SOURCE3<<6); /* This command selects the reference clock  */
                                                   /* for the DCO-R circuitry. Here the         */
                                                    /* recovered clock from the corresponding    */
                                                    /* line interface input is used.             */
    }

    if (3 == port%4 )        /*port 1*/
    {
        SET_BIT8(&falc->rdWr.cmr1 , RCLK_SOURCE4<<6);  /* This command selects the reference clock  */
                                                   /* for the DCO-R circuitry. Here the         */
                                                    /* recovered clock from the corresponding    */
                                                    /* line interface input is used.             */
    }

    /*
    cmr1.rs    (bit5,4)  select RCLK source
    00  clock recovered from the line through the DPLL drives RCLK
    01 clock recovered from the line through the DPLL drives RCLK
        and in case of an active LOS alarm RCLK pin is set high.
    10 clock recovered from the line is dejittered by DCO-R to drive a 2.048 MHz clock on RCLK.
    11 clock recovered from the line is dejittered by DCO-R to drive a 8.192 MHz clock on RCLK.
    */
    SET_BIT8(&falc->rdWr.cmr1 , 0x3<<4);  /* clock recovered from the line is dejittered
                                                                by DCO-R to drive a 8.192 MHz clock on RCLK  */



    /*
    cmr1.dcs   (bit3)  Disable Clock Switching
    In Slave mode (LIM0.MAS = 0) the DCO-R is synchronized on the recovered route clock.
    In case of loss of signal LOS the DCO-R switches automatically to the clock sourced by port SYNC.
    Setting his bit automatic switching from RCLK to SYNC is disabled.
    */
    CLEAR_BIT8(&falc->rdWr.cmr1 , 0x1<<3);


    /*
     cmr2.dcf (bit 4)DCO-R Center- Frequency Disabled,
     0 The DCO-R circuitry is frequency centered.
     1 The center function of the DCO-R circuitry is disabled.
     */
    CLEAR_BIT8(&falc->rdWr.cmr2 , 0x1<<4);

}



/*Group=Line Interface
------------------------------------------------------------------------
  Description:  Configure the line interface mode (E1), the line coding and clocking mode.
  Arguments:
                port - the QuadFALCDev.port
                clocking - the used clocking mode (master or slave)
  Settings:     Possible settings for mode are:
                MODE_E1 - selects the E1 pcm 30 mode
                MODE_T1 - selects the T1 pcm 24 mode
                MODE_J1 - selects the J1 pcm 24 mode for Japan

    Possible settings for line_coding_tx and line_coding_rx are.
                LINE_CODING_NRZ - selects NRZ line coding
                LINE_CODING_CMI - selects CMI line coding
                LINE_CODING_AMI - selects AMI line coding
                LINE_CODING_B8ZS - selects B8ZS line coding
                LINE_CODING_HDB3 - selects HDB3 line coding

    Possible settings for clocking are:
                SLAVE_MODE - the DCO-R is synchronized on the recovered route
                             clock. In case of LOS the DCO-R switches
                             automatically to master mode
                MASTER_MODE - the jitter attenuator is in free running mode.
                              If an external clock on the SYNC input is
                              applied, the DCO-R synchronizes to this input

  Return Value:    void

  Example:        pef22554_lineinterface_cfg (0, SLAVE_MODE)

  Remarks:
-------------------------------------------------------------------------*/
void quadFALC_t1_lineinterface_cfg (u8 port, u8 clocking)
{
    QFALC_REG_MAP   *falc;
    u8 mode1,mode2, line_co_tx, line_co_rx;

    falc = ptrreg[port];                  /* selects the falc base address */
    mode1 = 0x00;
    mode2 = 0x00;
    line_co_tx = 0x00;
    line_co_rx = 0x00;

    SET_BIT8(&falc->rdWr.fmr1 , (FMR1_PMOD));               /* T1 mode */
#ifdef INFINEON_ADVICE_MODE
    SET_BIT8(&falc->rdWr.lim1 , LIM1_RIL1| LIM1_RIL0 | LIM1_RIL2);

    SET_BIT8(&falc->rdWr.lim2 , LIM2_LOS1);     /* Recovery with additional no more than 15 zeros */
#endif

    //SET_BIT8(&falc->rdWr.fmr1, FMR1_XFS);                          /*0 doubleframe format,1 crc4 multiframe format*/
    MEM_WRITE8(&falc->rdWr.xpm0 , T1_XPM0_0DB);                   /* values for 120 Ohm interface and      */
    MEM_WRITE8(&falc->rdWr.xpm1 , T1_XPM1_0DB);                   /* 75 Ohm serial resistor               */
    MEM_WRITE8(&falc->rdWr.xpm2 , T1_XPM2_0DB);

#ifndef INFINEON_ADVICE_MODE
    SET_BIT8(&falc->rdWr.lim1 , LIM1_RIL0) ;    /* Receive Input Treshold  0.5 V 010 */

#endif
    MEM_WRITE8(&falc->rdWr.pcd , (QFALC_RAIL_INTERFACE) ? 0xFF : 0x0A);                   /* LOS Detection after 176 consecutive 0s       */
    MEM_WRITE8(&falc->rdWr.pcr , (QFALC_RAIL_INTERFACE) ? 0x00 : 0x15);                   /* LOS Recovery after 22 ones, Additional hints */

    SET_BIT8(&falc->rdWr.lim1 ,(QFALC_RAIL_INTERFACE) ? LIM1_DRS : 0);

    /*0x00 LINE_CODING_NRZ:
	0x01 LINE_CODING_CMI:
	0x02 LINE_CODING_AMI:
	0x03 LINE_CODING_B8ZS:
    */
    line_co_tx = 0x03;

    /*  0x00 LINE_CODING_NRZ:
         0x01 LINE_CODING_CMI:
         0x02 LINE_CODING_AMI:
	   0x03 LINE_CODING_B8ZS:
    */
    line_co_rx = 0x03;

    mode1=line_co_tx<<6 | line_co_rx<<4;
    mode2=mode1 | 0xF;
    SET_BIT8(&falc->rdWr.fmr0 ,mode1);
    CLEAR_BIT8(&falc->rdWr.fmr0 ,~mode2);

    if (clocking == MASTER_MODE)
    {
        SET_BIT8(&falc->rdWr.lim0 , LIM0_MAS);           /* Master mode */
    }
    else
    {
        CLEAR_BIT8(&falc->rdWr.lim0 , LIM0_MAS);       /* Slave mode */
    }

#ifdef CLOS
    SET_BIT8(&falc->rdWr.lim1,LIM1_CLOS);
#endif
    SET_BIT8(&falc->rdWr.lim0, (QFALC_RAIL_INTERFACE) ? 0: LIM0_EQON);
    //SET_BIT8( &falc->rdWr.lim0,LIM0_RLM);  //MARTN
}




/*Group=System Interface
------------------------------------------------------------------------
  Description:  Configures of the system interface.
  Arguments:
                port - the QuadFALCDev.port

                clock_rate - Configuration of the clocking rate at the system interface
                data_rate - Configuration of the data rate at the system interface
                channel_phase - Only applicable if datarates greater
                                than the datarates of the line are used
                rx_offset_high - Highbyte of the calculated offset value
                                 in receive direction
                rx_offset_low - Lowbyte of the calculated offset value
                                in receive direction
                tx_offset_high - Highbyte of the calculated offset value
                                 in transmit direction
                tx_offset_low - Lowbyte of the calculated offset value
                                in transmit direction
                rmfp_A - programs the receive multifunction Port A
                xmfp_A - programs the transmit multifunction Port A
                rmfp_B - programs the receive multifunction Port B
                xmfp_B - programs the transmit multifunction Port B
                rmfp_C - programs the receive multifunction Port C
                xmfp_C - programs the transmit multifunction Port C
                rmfp_D - programs the receive multifunction Port D
                xmfp_D - programs the transmit multifunction Port D
                xmfs - Transmit Multiframe Synchronization:
                       select between active high and active low

  Settings:     Possible settings for E1 clock_rate are:
                CR_E1_2048 - 2.048 MHz
                CR_E1_4096 - 4.096 MHz
                CR_E1_8192 - 8.192 MHz
                CR_E1_16384 - 16.384 MHz

    Possible settings for E1 data_rate are:
                DR_E1_2048 - 2.048 Mbit/s
                DR_E1_4096 - 4.096 Mbit/s
                DR_E1_8192 - 8.192 Mbit/s
                DR_E1_16384 - 16.384 Mbit/s

    Possible settings for channel_phase are:

                0..8 - The data of this channel is mapped on the selected
                       channel phase

    Possible settings for xmfs are:

                ACTIVE_HIGH - Transmit Multiframe Synchronization is active
                              high
                ACTIVE_LOW - Transmit Multiframe Synchronization is active
                             low

    Possible settings for rmfp_A..D are:

                MFP_SYPRQ - Synchronous Pulse Receive (Input)
                MFP_RFM - Receive Frame Sync (Output)
                MFP_RMFB - Receive Multiframe Begin (Output)
                MFP_RSIGM - Receive Signaling Marker (Output)
                MFP_RSIG - Receive Signaling data (Output)
                MFP_DLR - Data Link Bit Receive (Output)
                MFP_FREEZ - Freeze Signaling (Output)
                MFP_RFSPQ - Receive Frame Synchronous Pulse (Output)

    Possible settings for xmfp_A..D are:

                MFP_SYPXQ - Synchronous Pulse Transmit (Input)
                MFP_XMFS - Transmit Multiframe Synchronization (Input)
                MFP_XSIG - Transmit Signaling Data (Input)
                MFP_TCLK - Transmit Clock (Input)
                MFP_XMFB - Transmit Multiframe Begin (Output)
                MFP_XSIGM - Transmit Signaling Marker (Output)
                MFP_DLX - Data Link Bit Transmit (Output)
                MFP_XCLK - Transmit Line Clock (Output)
                MFP_XLT - Transmit Line Tristate (Input)

  Return Value:    none

  Example:         e1_systeminterface_cfg(port)

  Remarks:
-------------------------------------------------------------------------*/
void quadFALC_t1_systeminterface_cfg(u8 port)
{
    u8      clock_rate,data_rate;
    u8      rx_offset_high,rx_offset_low;
    u8      tx_offset_high,tx_offset_low;
    u8      xmfp_A, xmfp_B, xmfp_C, xmfp_D;
    u8      rmfp_A, rmfp_B, rmfp_C, rmfp_D;
    u8      mfp_A, mfp_B, mfp_C, mfp_D;
    u8      channel_phase;
    u16     slotoffset;
    QFALC_REG_MAP     *falc;

    falc = ptrreg[port];

    channel_phase = port%4;  /*data active in channel phase 0 for port 0;phase n for port n*/
    slotoffset = 32*port;    /*port n time slot 0 start at (32*port)timeslot for multiplex mode*/

    /*can modified acording 8260 PCM interface
    Receive Offset Programming:
    T: Time between beginning of SYPR pulse and beginning of next frame
    (time slot 0, bit 0), measured in number of SCLKR clock intervals
    maximum delay: Tmax =(256× SC/SD)-1.
    SD: Basic data rate, 2.048 Mbit/s
    SC: System clock rate; 2.048, 4.096, 8.192, or 16.384 MHz
    X: Programming value to be written to registers RC0 and RC1 (see Page 243).
    0 ≤ T ≤ 4:      X = 4 - T
    5 ≤ T ≤ Tmax: X = 2052 - T

    Transmit Offset Programming:
    T: Time between beginning of SYPX pulse and beginning of next frame
    (time slot 0, bit 0), measured in number of SCLKX clock intervals
    maximum delay: Tmax =(256× SC/SD)-1
    SD: Basic data rate, 2.048 Mbit/s
    SC: System clock rate; 2.048, 4.096, 8.192, or 16.384 MHz
    X: Programming value to be written to registers XC0 and XC1 (see Page 241).
    0 ≤ T ≤ 4: X = 4 - T
    5 ≤ T ≤ Tmax: X = 256 × SC/SD - T + 4
    */

    rx_offset_high = 0;
    rx_offset_low = 4;
    tx_offset_high = 0;
    tx_offset_low = 3;

    xmfp_A = MFP_SYPXQ;  /*Port A-D Multiplex mode*/
    rmfp_A = MFP_SYPRQ;
    xmfp_B = MFP_SYPXQ;
    rmfp_B = MFP_SYPRQ;
    xmfp_C = MFP_SYPXQ;
    rmfp_C = MFP_SYPRQ;
    xmfp_D = MFP_SYPXQ;
    rmfp_D = MFP_SYPRQ;

    mfp_A = xmfp_A|rmfp_A;
    mfp_B = xmfp_B|rmfp_B;
    mfp_C = xmfp_C|rmfp_C;
    mfp_D = xmfp_D|rmfp_D;

    clock_rate = CR_E1_8192;   // PCM clock is 8.192MHz
    data_rate = DR_E1_8192;    // PCM data rate is  8.192Mbps
    if (0 == port%4)
    {
        // enable system interface multiplex mode
        /*
        4 个通道E1 帧复用为数据率为16.384Mbit/s 或8.192Mbit/s 的数据流
        0：正常模式
        1：系统接口复用模式
        4 个通道的接收系统接口的工作时钟和帧同步脉冲由第一通道的SCLKR 和SYPR 提供，发
        送系统接口的工作时钟和帧同步脉冲由第一通道的SCLKX 和SYPX 提供。数据会以字节间
        插或位间插格式接收或发送。在复用模式下，下列配置必须进行且4 个通道必须一致：
                SYPR 1 由RPA1 提供
                SYPX 1 由XPA1 提供或
                XMFS 由XPB1 提供
                XSIG 由XPC1 提供
                RSIG 在RPB1 上输出
        每一通道必须配置一致：
                由SIC1.SSC1/0 配置的时钟速率：16.384MHz 或8.192MHz
                由SIC1.SSD1 与FMR1.SSD0 配置的数据率：16.384Mbit/s 或8.192Mbit/s
                由RC1/0、XC1/0 进行的时隙偏移编程
                接收弹性buffer 的大小为2 帧
        */
        SET_BIT8(&falc->rdWr.gpc1, 0x80);
    }


    /*
    Value after reset: 00H
            SSC(1:0) Select System Clock
            SIC1.SSC1/0 and SIC2.SSC2 define the clocking rate on the system
            highway.
            SIC2.SSC2 = 0:
            00 2.048 MHz
            01 4.096 MHz
            10 8.192 MHz
            11 16.384 MHz
            SIC2.SSC2 = 1:
            00 1.544 MHz
            01 3.088 MHz
            10 6.176 MHz
            11 12.352 MHz
    */
    CLEAR_BIT8(&falc->rdWr.sic1 , 0x88);     /* Clear SSC10 Bits */
    SET_BIT8(&falc->rdWr.sic1 , (clock_rate & 0x02) << 6);
    SET_BIT8(&falc->rdWr.sic1 , (clock_rate & 0x01) << 3);   /* Set selected clocking rate */

#ifdef INFINEON_ADVICE_MODE
    CLEAR_BIT8(&falc->rdWr.sic2 , SIC2_SSC2);     /* Clear SSC2 Bit */
    //SET_BIT8(&falc->rdWr.sic2 , (clock_rate & 0x04) << 2);
#endif
    /*
        SIC1.SSD1, FMR1.SSD0 and SIC2.SSC2 define the data rate on the
        system highway. Programming SSD1/SSD0 and corresponding data
        rate is shown below.
        SIC2.SSC2 = 0:
                00 2.048 Mbit/s
                01 4.096 Mbit/s
                10 8.192 Mbit/s
                11 16.384 Mbit/s
        SIC2.SSC2 = 1:
                00 1.544 Mbit/s
                01 3.088 Mbit/s
                10 6.176 Mbit/s
                11 12.352 Mbit/s
    */
    CLEAR_BIT8(&falc->rdWr.sic1 , 0x40);     /* Clear SSD1 Bits */
    CLEAR_BIT8(&falc->rdWr.fmr1 , 0x02);     /* Clear SSD0 Bits */
    SET_BIT8(&falc->rdWr.sic1 , (data_rate & 0x02) << 5);
    SET_BIT8(&falc->rdWr.fmr1 , (data_rate & 0x01) << 1);    /* Set seleccted data rate */
    SET_BIT8(&falc->rdWr.fmr1 , 0x08 );    /* Set enable crc6 */


    /*
       The 24 received time slots (T1/J1) can be translated into the 32 system time
    slots (E1)    in two different channel translation modes (FMR1.CTM).
    */
#ifdef INFINEON_ADVICE_MODE
    CLEAR_BIT8(&falc->rdWr.fmr1 , 0x80);     /* Clear CTM Bit */
#else
    CLEAR_BIT8(&falc->rdWr.fmr1 , 0x80);     /* Clear CTM Bit */
    SET_BIT8(&falc->rdWr.fmr1 , 0x80);
#endif

    /*
        SIC2…0：系统接口通道选择
        仅在系统时钟大于2.048MHz 时有意义。
        接收的数据在RDO/RSIG 上发送或XDI/XSIG 上以系统数据率接收数据。如果数据率大
        于2.048Mbit/s，数据的输出或采样在1/2、1/4、1/8 个2M 时隙，并数据不重复。在
        8x488ns的时隙内数据活动的时间称为1 个channel phase。没有数据的channel phase，
        RDO/RSIG 上清除为0，而XDI/XSIG 上的数据被忽略。
        000：channel phase1 的数据有效，若系统数据率为16/8/4Mbit/s
        001：channel phase2 的数据有效，若系统数据率为16/8/4Mbit/s
        010：channel phase3 的数据有效，若系统数据率为16/8Mbit/s
        011：channel phase4 的数据有效，若系统数据率为16/8Mbit/s
        100：channel phase5 的数据有效，若系统数据率为16Mbit/s
        101：channel phase6 的数据有效，若系统数据率为16Mbit/s
        110：channel phase7 的数据有效，若系统数据率为16Mbit/s
        111：channel phase8 的数据有效，若系统数据率为16Mbit/s
    */
    CLEAR_BIT8(&falc->rdWr.sic2 , 0x0E);     /* Clear channel phase */
    SET_BIT8(&falc->rdWr.sic2 , (channel_phase & 0x07) << 1);

    /*Receive Offset/Receive Frame Marker Offset*/
    CLEAR_BIT8(&falc->rdWr.rc0  , 0x07);
    SET_BIT8(&falc->rdWr.rc0  , (rx_offset_high & 0x07));
    MEM_WRITE8(&falc->rdWr.rc1  , rx_offset_low);

    /*
        Initial value loaded into the transmit bit counter at the trigger edge of
    SCLKX when the synchronous pulse at port SYPX/XMFS is active
    */
    CLEAR_BIT8(&falc->rdWr.xc0  , 0xFF);
    SET_BIT8(&falc->rdWr.xc0  , tx_offset_high);
    MEM_WRITE8(&falc->rdWr.xc1  , tx_offset_low);

    /*
    RPC2…0：接收多功能口配置
        多功能口RP(A…D)是双向端口，复位后初始配置为输入。还可以进行功能选择，输入功
    能SYPR 只能在4 个管脚中选择一个，不能多选。PC1 对应RPA、PC2 对应RPB、PC3
    对应RPC、PC4 对应RPD。
        000：接收同步脉冲SYPR （输入）
            与RC1/0 一起，SYPR 定义接收系统接口上的帧头信号。由于偏移编程，SYPR 与RFM
            不能同时选择。
    XPC3…0：发送多功能口配置
        多功能口XP(A…D)是双向端口，复位后初始配置为输入。还可以进行功能选择，每一通
    道的4 个输入功能SYPX 、XMFS、XSIG、TCLK 只能选择一次，不能多选，并且SYPX
    和XMFS 不能同时出现。PC1 对应XPA、PC2 对应XPB、PC3 对应XPC、PC4 对应
    XPD。
    0000：发送方向同步脉冲SYPX （输入）
    与XC1/0 一起定义发送系统接口XDI 和XSIG 上帧头指示
    */
    MEM_WRITE8(&falc->rdWr.pc1,mfp_A);
    MEM_WRITE8(&falc->rdWr.pc2,mfp_B);
    MEM_WRITE8(&falc->rdWr.pc3,mfp_C);
    MEM_WRITE8(&falc->rdWr.pc4,mfp_D);

    quadFALC_t1_clocking_cfg(port);

}

/*group=Z_Example for API applications
------------------------------------------------------------------------
  Description:  Standard E1 configuration.

  Arguments:
                port - the QuadFALCDev.port

  Settings:

  Return Value: void

  Source code:   void t1_standard_config(u8 port)

  Remarks:
-------------------------------------------------------------------------*/
void quadFALC_t1_standard_config(u8 port)
{
    u32 ulPraE1FmtCfg;
    //u8  tmp_slot,tmp_port;

    /*clock rate=8.192 MHz ; data rate=2.048 Mbit (E1 mode); channel translation mode 0;
    channel_phase=0 (not valid in E1 mode); rc offset=0x24; tx offset=0x4;
    MFPA=RFM and XCLK; MFPB=RMFB and XMFB; MFPC=DLX and DLR; MFPD= RSIGM and XSIGM
    Transmit multiframe sync=active high */
    quadFALC_t1_systeminterface_cfg(port);

    quadFALC_t1_lineinterface_cfg (port, SLAVE_MODE);

    quadFALC_t1_elasticbuffer_cfg (port, ELASTIC_BUF_ON, ELASTIC_BUF_ON);

///-
/*
#if( AOS_INCLUDE_SERVICE_Q931 == TRUE )
*/
/*
    if( q931_is_primary_e1( port ) )
    {
        ulPraE1FmtCfg = db_get_pra_prim_e1_fmt( );
    }
    else
#endif
    {
        ulPraE1FmtCfg = db_get_pra_e1_fmt( );
    }
   */

    ulPraE1FmtCfg = 2;
    switch (ulPraE1FmtCfg)
    {
        case 0:
            quadFALC_t1_framer_cfg(port,T1_FRAMING_F12,T1_FRAMING_F12);
            break;

        case 1:
            quadFALC_t1_framer_cfg(port,T1_FRAMING_F4,T1_FRAMING_F4);
            break;

        case 2:
            quadFALC_t1_framer_cfg(port,T1_FRAMING_ESF,T1_FRAMING_ESF);
            break;

        default:
            quadFALC_t1_framer_cfg(port,T1_FRAMING_F12,T1_FRAMING_F12);
            break;
    }

}


/*
------------------------------------------------------------------------
  Description:      Basic Initialization of QuadFALCDev.chip.
  Include:
  Arguments:   chipno - the QuadFALCDev.chip number
  Return Value:     u32
  Example:        u32 t1_chip_init (0);
  Remarks:
-------------------------------------------------------------------------*/
int quadFALC_t1_chip_init (int chipno)
{
    QFALC_REG_MAP  *falc;
    u16 i;
    u8 cis,tmp;
    ///-
    /*
    u32 immrBase;

    immrBase = vxImmrGet();
    */

    if( chipno > MAX_PEF22554_NUM)
    {
    ///-
        //return AOS_FAIL;
        return -1;
    }

    ///-
    /*
    aos_printf( 0,"E1 chip addr[%d]:0x%08x ,System addr offset:0x%08x",chipno,QuadFALCDev.virtual_base_addr[chipno], CONFIG_NEW_PERIPHERAL_OFFSET);
    QuadFALCDev.virtual_base_addr[chipno] |= CONFIG_NEW_PERIPHERAL_OFFSET;
    */
    //*(volatile u32*)( immrBase + 0x10144) = 0xFFF00E66;///***///
    //*(volatile u32*)(immrBase + 0x1014C) = 0xFFF00E66;

    falc = (QFALC_REG_MAP*)((u32*)QuadFALCDev.pef22554info[chipno].virtual_base_addr);

    MEM_WRITE8(&falc->rdWr.gcm1 , 0x00);
    MEM_WRITE8(&falc->rdWr.gcm2 , 0x18);
    MEM_WRITE8(&falc->rdWr.gcm3 , 0xFB);
    MEM_WRITE8(&falc->rdWr.gcm4 , 0x0B);
    MEM_WRITE8(&falc->rdWr.gcm5 , 0x01);
    MEM_WRITE8(&falc->rdWr.gcm6 , 0x0B);
    MEM_WRITE8(&falc->rdWr.gcm7 , 0xDB);
    MEM_WRITE8(&falc->rdWr.gcm8 , 0xDF);

    MEM_READ8(&falc->rdWr.gcm3,&tmp);
    if( tmp == 0xFB )
    {
        QuadFALCDev.pef22554info[chipno].g_bE1ChipStatus = TRUE;
        QuadFALCDev.g_ulE1ActiveNum = chipno+1;
        debug("t1 chipno=%d is exist.\n",chipno);
    }
    else
    {
        debug("t1 chipno=%d is not in exist.\n",chipno);
        QuadFALCDev.pef22554info[chipno].g_bE1ChipStatus = FALSE;
        QuadFALCDev.g_ulE1ActiveNum = chipno;
	    return -1;
    }

    mdelay(10);
    
    MEM_READ8(&falc->rd.cis,&cis);
    if( (cis&CIS_PLLL)== 0x00 )
    {
        debug(" T1 22554 chip(%d) PLL init unlocked!\n",chipno);
    }


    for (i = 0; i < NUM_OF_FALC; i++)
    {
        falc = ptrreg[NUM_OF_FALC*chipno+i];
        if (0 == i)
        {
            SET_BIT8(&falc->rdWr.gpc1, QFALC_GLOBAL_PORT<<5);
        }

        // configures interrupt
        SET_BIT8(&falc->rdWr.ipc , QFALC_INTERRUPT);
        // Masked interrupts visible
        SET_BIT8(&falc->rdWr.gcr  , GCR_VIS);

        MEM_WRITE8(&falc->rdWr.imr0 , 0xFF);
        MEM_WRITE8(&falc->rdWr.imr1 , 0xFF);
        MEM_WRITE8(&falc->rdWr.imr2 , 0xFF);
        MEM_WRITE8(&falc->rdWr.imr3 , 0xFF);
        MEM_WRITE8(&falc->rdWr.imr4 , 0xFF);

        quadFALC_t1_standard_config( (u8)(NUM_OF_FALC*chipno+i) );
    }

    //Receiver Sensitivity optimization
    quadFALC_t1_receiver_reset(chipno);

    return 0;
}


