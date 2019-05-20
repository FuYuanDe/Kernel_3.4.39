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
void quadFALC_e1_elasticbuffer_cfg (u8 port, u8 buffer_tx, u8 buffer_rx)
{

    QFALC_REG_MAP*     falc;

    //printk(KERN_ERR "%s: port:%d\n", __FUNCTION__, port);

    falc = ptrreg[port];

    /*clear buffersize rc and tx */
    CLEAR_BIT8(&falc->rdWr.sic1,0x33);

    /*sets the buffersize for tx and rx */
    SET_BIT8(&falc->rdWr.sic1 ,buffer_tx ? TX_BUFFER : 0 | buffer_rx ? (RX_BUFFER<< 4) : 0x30);

}


/*----------------------------------------------------------------
Receiver Reset
------------------------------------------------------------------*/
void quadFALC_e1_receiver_reset(u8 port)
{
    u32 cnt;
    u8  sis;
    QFALC_REG_MAP  *falc;

    falc = ptrreg[port];

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
        debug(" E1 22554 channel(%d) Receiver reset SiS_CEC failed!!",port);
	    mdelay(100);
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
void quadFALC_e1_framer_cfg (u8 port, u8 framing_tx ,u8 framing_rx)
{
    QFALC_REG_MAP   *falc;
    u8    mode1,mode2,sis;
    u8    fram_tx;
    u8    fram_rx;
    u32   cnt;

    fram_tx = 0x00;
    fram_rx = 0x00;

    switch (framing_tx)
    {
	    case FRAMING_DOUBLEFRAME:
	        fram_tx = 0x00;
	        break;

	    case FRAMING_MULTIFRAME_TX:
	        fram_tx = 0x01;
	        break;

	    default:
	        break;
    }


    switch (framing_rx)
    {
	    case FRAMING_DOUBLEFRAME:
	        fram_rx = 0x00;
	        break;

	    case FRAMING_MULTIFRAME_RX:
	        fram_rx = 0x02;
	        break;

	    case FRAMING_MULTIFRAME_MOD:
	        fram_rx = 0x03;
	        break;

	    default:
	        break;
    }

    falc = ptrreg[port];


    /*
    XFS Transmit Framing Select
        Selection of the transmit framing format can be done independently
        of the receive framing format.
        0 Doubleframe format enabled.
        1 CRC4-multiframe format enabled
    */
    mode1 = fram_tx<<3;
    CLEAR_BIT8(&falc->rdWr.fmr1 ,0x08);
    SET_BIT8(&falc->rdWr.fmr1 ,mode1);
    CLEAR_BIT8(&falc->rdWr.xsp_fmr5 ,XSP_TT0);

    /*
      RFS(1:0) Receive Framing Select
        00 Doubleframe format
        01 Doubleframe format
        10 CRC4 Multiframe format
        11 CRC4 Multiframe format with modified CRC4 Multiframe
        alignment algorithm (Interworking according to ITU-T G.706
        Annex B). Setting of FMR3.EXTIW changes the reaction after
        the 400 ms time-out.
    */
    mode2 = fram_rx<<6;
    CLEAR_BIT8(&falc->rdWr.fmr2 ,0xC0);
    SET_BIT8(&falc->rdWr.fmr2 ,mode2);
    CLEAR_BIT8(&falc->rdWr.fmr2, (FMR2_DAIS | FMR2_RTM));


    SET_BIT8(&falc->rdWr.fmr2 , FMR2_AXRA   /* Automatic Transmit Remote Alarm*/
                     |  FMR2_ALMF);        /* Automatic Loss of Multiframe   */
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

    SET_BIT8(&falc->rdWr.fmr1 , FMR1_ECM             /* Latch Error Counter every second  */
    | FMR1_AFR);                              /* Automatic Force Resynchronisation */

                                             /*-------------------------------------------------------------------------
                                             FMR1.AFR - Automatic Force Resync
                                             If the FALC cannot reach multiframe synchronous state it will automatically
                                             resync after 8ms. This function is enabled after the FALC has reached
                                             asynchronous state before. In combination with FMR2.AXRA the RAI is sent
                                             if LFA=1 and reset again if LFA=0. (Important for ETS300011 4.3 - 4.5) */


    SET_BIT8(&falc->rdWr.xsw_fmr4 , XSW_XSIS           /* Y0, Y1 and Y3-Bits and the spare Bit are fixed to ??*/
                         |  XSW_XY0 | XSW_XY1
                         |  XSW_XY2 | XSW_XY3
                         |  XSW_XY4);

   SET_BIT8(&falc->rdWr.xsp_fmr5 , XSP_XSIF             /* Spare Bits are fixed to 1        */
                         | XSP_AXS             /* Automatic Transmisson of E-Bits  */
                         | XSP_EBP);            /* E bits =1 in asynchronous state  */
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
	 debug(" E1 22554 channel(%d) Framer Cfg SiS_CEC failed!!",port);
    }

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
	 debug(" E1 22554 channel(%d) Framer Cfg SiS_CEC failed!!",port);
    }

    quadFALC_e1_receiver_reset(port);

}

u32 quadFALC_e1_chip_is_exist(u32 chipno)
{
     if( chipno < MAX_PEF22554_NUM )
         return QuadFALCDev.pef22554info[chipno].g_bE1ChipStatus;

     return FALSE;
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

  Example:     e1_clocking_cfg(1)
                                    1,
  See Also:    e1_systeminterface_cfg();

  Remarks:      Please note: Clocking rates are configured in the function pef22554_systeminterface_cfg().
-------------------------------------------------------------------------*/
void quadFALC_e1_clocking_cfg(u8 port)
{
    QFALC_REG_MAP    *falc;
    u8   clocking_io,clocking_sync_int_ext,active_edge;
    u8   buffer_transparent_mode,tlck_config;

    
    falc = ptrreg[port];

    /* Pins: SCLKR & RCLK  Configuration */
    clocking_io = 0x01; // SCLKR: Input, RCLK: Output
    CLEAR_BIT8(&falc->rdWr.pc5 ,0x07);
    SET_BIT8(&falc->rdWr.pc5 , clocking_io & 0x07);

    /* Clock Mode Register 2 */
    clocking_sync_int_ext = 0x00;
    CLEAR_BIT8(&falc->rdWr.cmr2 , 0x0f);
    SET_BIT8(&falc->rdWr.cmr2 , clocking_sync_int_ext & 0x0f);

    /*
        Clocked or sampled with the first falling edge of the selected system interface clock.Clocked or sampled with the first falling edge of the selected system interface clock.
     */
    active_edge = 0x0;
    CLEAR_BIT8(&falc->rdWr.sic3 , 0x0C);
    SET_BIT8(&falc->rdWr.sic3 , (active_edge & 0x03) << 2);

    /*
        0x00 (RX_FRAME_SYNC|TX_FRAME_SYNC):
        0x01 (RX_SYNC_CLEAR|TX_FRAME_SYNC):
        0x02 (RX_FRAME_SYNC|TX_FRAME_CLEAR):
        0x03 (RX_SYNC_CLEAR|TX_FRAME_CLEAR):
     */
    buffer_transparent_mode = 0x00;
    CLEAR_BIT8(&falc->rdWr.fmr2 , 0x20);
    CLEAR_BIT8(&falc->rdWr.xsw_fmr4 , 0x40);
    SET_BIT8(&falc->rdWr.fmr2 , (buffer_transparent_mode & 0x01) << 5);
    SET_BIT8(&falc->rdWr.xsw_fmr4 , (buffer_transparent_mode & 0x02) << 5);

    /*
        0x00  TCLK_IGNORE:
        0x01  TCLK_DEJITTERED:
        0x02  TCLK_DIRECT:
     */
    tlck_config = 0x00;
    CLEAR_BIT8(&falc->rdWr.cmr1 , 0x03);
    SET_BIT8(&falc->rdWr.cmr1 , (tlck_config & 0x03));

    /*
        Select RCLK Source Channel
        00 Receive reference clock generated by channel 1
        01 Receive reference clock generated by channel 2.
        10 Receive reference clock generated by channel 3.
        11 Receive reference clock generated by channel 4.
     */
    SET_BIT8(&falc->rdWr.cmr1 , ((port % 4) << 6));


    /*
        Select RCLK Source
        00 Clock recovered from the line through the DPLL drives RCLK
        01 Clock recovered from the line through the DPLL drives RCLK and in case of an active LOS alarm RCLK pin is set high (ored with LOS).
        10 Clock recovered from the line is de-jittered by DCO-R to drive a 2.048 MHz clock on RCLK.
        11 Clock recovered from the line is de-jittered by DCO-R to drive a 8.192 MHz clock on RCLK.
     */
    SET_BIT8(&falc->rdWr.cmr1 , 0x3<<4);

    
    /*
        Disable Clock-Switching
        In Slave mode (LIM0.MAS = 0B) the DCO-R is synchronized on the
        recovered route clock. In case of loss-of-signal LOS the DCO-R switches
        automatically to the clock sourced by port SYNC:
        0 automatic switching from RCLK to SYNC is enabled
        1 automatic switching from RCLK to SYNC is disabled
     */
    CLEAR_BIT8(&falc->rdWr.cmr1 , 0x1<<3);


    /*
         DCO-R Center- Frequency Disabled
         
         0 The DCO-R circuitry is frequency centered in master mode if no
         2.048 MHz reference clock on pin SYNC is provided or in slave
         mode if a loss-of-signal occurs in combination with no 2.048 MHz
         clock on pin SYNC or a gapped clock is provided on pin RCLKI and
         this clock is inactive or stopped.
         
         1 The center function of the DCO-R circuitry is disabled. The
         generated clock (DCO-R) is frequency frozen in that moment when
         no clock is available on pin SYNC or pin RCLKI. The DCO-R
         circuitry starts synchronization as soon as a clock appears on pins
         SYNC or RCLKI.
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
void quadFALC_e1_lineinterface_cfg (u8 port, u8 clocking, u8 line_code)
{
    QFALC_REG_MAP   *falc;
    u8 mode1,mode2, line_co_tx, line_co_rx;
    falc = ptrreg[port];
    mode1 = 0x00;
    mode2 = 0x00;
    line_co_tx = 0x00;
    line_co_rx = 0x00;


    CLEAR_BIT8(&falc->rdWr.fmr1 , (FMR1_PMOD));         // PCM 30 or E1 mode
    SET_BIT8(&falc->rdWr.fmr1, FMR1_XFS);               // 0: Doubleframe format enabled, 1: CRC4-multiframe format enabled.
    MEM_WRITE8(&falc->rdWr.xpm0 , E1_XPM0_0DB);         // values for 120 Ohm interface and
    MEM_WRITE8(&falc->rdWr.xpm1 , E1_XPM1_0DB);         // 75 Ohm serial resistor
    MEM_WRITE8(&falc->rdWr.xpm2 , E1_XPM2_0DB);
    
    
    SET_BIT8(&falc->rdWr.lim1 , LIM1_RIL1 | LIM1_RIL0 | LIM1_RIL2) ;
    MEM_WRITE8(&falc->rdWr.pcd , (QFALC_RAIL_INTERFACE) ? 0xFF : 0x0A); // LOS Detection after 176 consecutive 0s
    MEM_WRITE8(&falc->rdWr.pcr , (QFALC_RAIL_INTERFACE) ? 0x00 : 0x15); // LOS Recovery after 22 ones, Additional hints
    SET_BIT8(&falc->rdWr.lim1 , (QFALC_RAIL_INTERFACE) ? LIM1_DRS : 0);

    /*
        00B NRZ
        01B CMI
        10B AMI
        11B HDB3
    */
    line_co_tx = line_code & 0x03;
    line_co_rx = line_code & 0x03;

    mode1 = line_co_tx<<6 | line_co_rx<<4;
    mode2 = mode1 | 0xF;
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
void quadFALC_e1_systeminterface_cfg(u8 port)
{
    u8      clock_rate,data_rate;
    u8      rx_offset_high,rx_offset_low;
    u8      tx_offset_high,tx_offset_low;
    u8      xmfp_A, xmfp_B, xmfp_C, xmfp_D;
    u8      rmfp_A, rmfp_B, rmfp_C, rmfp_D;
    u8      mfp_A, mfp_B, mfp_C, mfp_D;
    u8      channel_phase;
    QFALC_REG_MAP     *falc;

    
    falc = ptrreg[port];

    /*
        Select System Clock
        SIC1.SSC1 and SIC1.SSC0 define the clocking rate on the system interface
        00 2.048 MHz
        01 4.096 MHz
        10 8.192 MHz
        11 16.384 MHz
    */
    clock_rate = CR_E1_8192;                                // PCM clock is 8.192MHz
    CLEAR_BIT8(&falc->rdWr.sic1 , 0x88);                    // Clear SSC1 & SSC0 bits
    SET_BIT8(&falc->rdWr.sic1 , (clock_rate & 0x02) << 6);  // Set SSC1 bit
    SET_BIT8(&falc->rdWr.sic1 , (clock_rate & 0x01) << 3);  // Set SSC0 bit


    /*
        Select System Data Rate 1
        SIC1.SSD1 and FMR1.SSD0 define the data rate on the system interface.
        00 2.048 Mbit/s
        01 4.096 Mbit/s
        10 8.192 Mbit/s
        11 16.384 Mbit/s
    */
    data_rate = DR_E1_8192;                                 // PCM data rate is  8.192Mbps
    CLEAR_BIT8(&falc->rdWr.sic1 , 0x40);                    // Clear SSD1 Bits
    CLEAR_BIT8(&falc->rdWr.fmr1 , 0x02);                    // Clear SSD0 Bits
    SET_BIT8(&falc->rdWr.sic1 , (data_rate & 0x02) << 5);   // Set SSD1 bit    
    SET_BIT8(&falc->rdWr.fmr1 , (data_rate & 0x01) << 1);   // Set SSD0 bit

    
    /*
        Multiframe Force Resynchronization
        Only valid if CRC multiframe format is selected (FMR2.RFS(1:0) = 10B).
        A transition from low to high initiates the resynchronization procedure for
        CRC-multiframe alignment without influencing doubleframe synchronous
        state. In case, “Automatic Force Resynchronization” (FMR1.AFR) is
        enabled and multiframe alignment cannot be regained, a new search of
        doubleframe (and CRC multiframe) is automatically initiated.
    */
    CLEAR_BIT8(&falc->rdWr.fmr1 , 0x80);
    SET_BIT8(&falc->rdWr.fmr1 , 0x80);

    /*
        System Interface Channel Select
        Only applicable if the system clock rate is greater than 2.048 MHz.Received data is transmitted on pin RDO/RSIG or received on XDI/XSIG with the selected system data rate. If the data rate is greater than 2.048 Mbit/s the data is output or sampled in half, a quarter or one eighth of the time slot. Data is not repeated. The time while data is active during a 8 x 488 ns time slot is called a channel phase. RDO/RSIG are cleared (driven to low level) while XDI/XSIG are ignored for the remaining time of the 8 x 488 ns or for the remaining channel phases. The channel phases are selectable with these bits. See Chapter 4.6.1.
        000B Data active in channel phase 1, valid if system data rate is16/8/4 Mbit/s
        001B Data active in channel phase 2, valid if system data rate is16/8/4 Mbit/s
        010B Data active in channel phase 3, valid if data rate is 16/8 Mbit/s
        011B Data active in channel phase 4, valid if data rate is 16/8 Mbit/s
        100B Data active in channel phase 5, valid if data rate is 16 Mbit/s
        101B Data active in channel phase 6, valid if data rate is 16 Mbit/s
        110B Data active in channel phase 7, valid if data rate is 16 Mbit/s
        111B Data active in channel phase 8, valid if data rate is 16 Mbit/s
    */
    channel_phase = port % 4;
    CLEAR_BIT8(&falc->rdWr.sic2 , 0x0E);    // Clear channel phase
    SET_BIT8(&falc->rdWr.sic2 , (channel_phase & 0x07) << 1);

    
    /* Receive Offset/Receive Frame Marker Offset */
    rx_offset_high = 0;
    rx_offset_low = 4;
    CLEAR_BIT8(&falc->rdWr.rc0  , 0x07);
    SET_BIT8(&falc->rdWr.rc0  , (rx_offset_high & 0x07));
    MEM_WRITE8(&falc->rdWr.rc1  , rx_offset_low);

    /* Transmit Offset */
    tx_offset_high = 0;
    tx_offset_low = 3;
    CLEAR_BIT8(&falc->rdWr.xc0  , 0xFF);
    SET_BIT8(&falc->rdWr.xc0  , tx_offset_high);
    MEM_WRITE8(&falc->rdWr.xc1  , tx_offset_low);

    /* Multi Function Port Selection */
    xmfp_A = MFP_SYPXQ;  // SYPX: Synchronous Pulse Transmit (Input, low active)
    rmfp_A = MFP_SYPRQ;  // SYPR: Synchronous Pulse Receive (Input, low active)
    xmfp_B = MFP_SYPXQ;  // SYPX: Synchronous Pulse Transmit (Input, low active)
    rmfp_B = MFP_SYPRQ;  // SYPR: Synchronous Pulse Receive (Input, low active)
    xmfp_C = MFP_SYPXQ;  // SYPX: Synchronous Pulse Transmit (Input, low active)
    rmfp_C = MFP_SYPRQ;  // SYPR: Synchronous Pulse Receive (Input, low active)
    xmfp_D = MFP_SYPXQ;  // SYPX: Synchronous Pulse Transmit (Input, low active)
    rmfp_D = MFP_SYPRQ;  // SYPR: Synchronous Pulse Receive (Input, low active)
    mfp_A = xmfp_A | (rmfp_A << 4);
    mfp_B = xmfp_B | (rmfp_B << 4);
    mfp_C = xmfp_C | (rmfp_C << 4);
    mfp_D = xmfp_D | (rmfp_D << 4);
    MEM_WRITE8(&falc->rdWr.pc1, mfp_A); // pin RPA & XPA
    MEM_WRITE8(&falc->rdWr.pc2, mfp_B); // pin RPB & XPB
    MEM_WRITE8(&falc->rdWr.pc3, mfp_C); // pin RPC & XPC
    MEM_WRITE8(&falc->rdWr.pc4, mfp_D); // pin RPD & XPD

    quadFALC_e1_clocking_cfg(port);

}


/*group=Clocking
------------------------------------------------------------------------
  Description:  Clocking configuration.
                Please note that clocking rates are configured in the system interface part!

  Arguments:
                e1no - the QuadFALCDev.port
  Setting:
                clocking_io - Configuration of Clock-Pins SCLKR, SCLKX, RCLK
                clocking_sync_int_ext - Configuration of the used Clock and Sync Inputs (used or unused)
                active_edge - Configuration of the active Clock edges
                buffer_transparent_mode - Configuration of the transparent Buffer modes -> Sync pulses independancy
                tlck_config - Usage of the TCLK signal
                tlck_mode - Type of TCLK signal

    The parameter clocking_io must be a logical or of three defines, one for each group to program.
    The parameter clocking_sync_int_ext must be a logical or of four defines, one for each group.
    The parameter active_edge must be a logical or of two defines, one for each group.

  Return Value:    none

  Example:     e1_rclk_select(0)

  See Also:

  Remarks: Please note: Clocking rates are configured in the function pef22554_systeminterface_cfg().
-------------------------------------------------------------------------*/
void quadFALC_e1_rclk_select(u8 port)
{
    QFALC_REG_MAP    *falc;
    u8   temp;

    falc = ptrreg[port];
    temp = port%4;

    // first set the pef22554 register
    CLEAR_BIT8(&falc->rdWr.gpc1, 0x03);
    SET_BIT8(&falc->rdWr.gpc1, temp);

    // 逻辑上选择具体的22554芯片
}


/*
   E1芯片是否处于自由振荡状态?
  */
u32 quadFALC_e1_rclk_is_surge(void)
{
    QFALC_REG_MAP   *falc;
    u8 temp,chipno;

    chipno =0;
    falc = ptrreg[NUM_OF_FALC*chipno];

    MEM_READ8(&falc->rdWr.lim0,&temp);
    if(temp & 0x01)
    {
       return TRUE;
    }

    return FALSE;
}


/*group=Error Counter
------------------------------------------------------------------------
  Description:  Polls the value of an error counter
  Arguments:
                port - the QuadFALCDev.port
                type - type of error counter
  Settings:     Possible settings for type are:

                ERROR_COUNTER_FEC - framing error counter
                ERROR_COUNTER_CVC - code violation counter
                ERROR_COUNTER_CEC - E1: CRC1 error counter 1;
                ERROR_COUNTER_CEC2_EBC - E1: CRC2 error counter;
                ERROR_COUNTER_CEC3_BEC - E1: CRC3 error counter;
                ERROR_COUNTER_EBC_COEC - E1: E-Bit error counter;

 Possible settings for p_counter_value
                receiving the value of the specified counter

  Return Value:     u16

  Example:      e1_errorcounter_statusreq(0, ERROR_COUNTER_CVC,u16* p_counter_value);
                polls the value of the code violation counter on Port 0
  Remarks:
-------------------------------------------------------------------------*/
void quadFALC_e1_errorcounter_statusreq (u8 port, u8 type, u16* p_counter_value)
{
    QFALC_REG_MAP    *falc;
    u8  regl,regh;

    falc = ptrreg[port];

    switch (type)
    {
        case ERROR_COUNTER_FEC:
            //MEM_READ16(&falc->rd.fec,p_counter_value);
            MEM_READ8(&falc->rd.fecl,&regl);
            MEM_READ8(&falc->rd.fech,&regh);
            break;

        case ERROR_COUNTER_CVC:
            //MEM_READ16(&falc->rd.cvc,p_counter_value);
            MEM_READ8(&falc->rd.cvcl,&regl);
            MEM_READ8(&falc->rd.cvch,&regh);
            break;

        case ERROR_COUNTER_CEC:
            //MEM_READ16(&falc->rd.cec1,p_counter_value);
            MEM_READ8(&falc->rd.cec1l,&regl);
            MEM_READ8(&falc->rd.cec1h,&regh);
            break;

        case ERROR_COUNTER_CEC2_EBC:
            //MEM_READ16(&falc->rd.cec2,p_counter_value);
            MEM_READ8(&falc->rd.cec2l,&regl);
            MEM_READ8(&falc->rd.cec2h,&regh);
            break;

        case ERROR_COUNTER_CEC3_BEC:
            //MEM_READ16(&falc->rd.cec3,p_counter_value);
            MEM_READ8(&falc->rd.cec3l,&regl);
            MEM_READ8(&falc->rd.cec3h,&regh);
            break;

        case ERROR_COUNTER_EBC_COEC:
            //MEM_READ16(&falc->rd.ebc,p_counter_value);
            MEM_READ8(&falc->rd.ebcl,&regl);
            MEM_READ8(&falc->rd.ebch,&regh);
            break;

        default:
            regh=0;
            regl=0;
            break;
    }

    *p_counter_value = (u16)((regh<<8)+regl);
    return;
}


u8 quadFALC_e1_geterrstatus( u8 port )
{
    u16                 counter;
    E1_ERR_STATUS       *pBitmap;
    u8                  result = 0;

    pBitmap = (E1_ERR_STATUS *)&result;

    quadFALC_e1_errorcounter_statusreq( port, ERROR_COUNTER_FEC, &counter );
    if( counter )
    {
        pBitmap->ucFEC = 1;
    }

    quadFALC_e1_errorcounter_statusreq( port, ERROR_COUNTER_CVC, &counter );
    if( counter )
    {
        pBitmap->ucCVC = 1;
    }

    quadFALC_e1_errorcounter_statusreq( port, ERROR_COUNTER_EBC_COEC, &counter );
    if( counter )
    {
        pBitmap->ucEBC = 1;
    }

    quadFALC_e1_errorcounter_statusreq( port, ERROR_COUNTER_CEC, &counter );
    if( counter )
    {
        pBitmap->ucCEC = 1;
    }

    return result;

}

/*group=SSM support functions
------------------------------------------------------------------------
  Description:  Configures the device for SSM support. The SSM information 'Quality unknown'
                is transmitted by default.

  Arguments:
                port - the QuadFALCDev.port
                mode - enable disable SSM support
                sabit - the SA-Bit to use in E1 mode.


  Settings:  Possible settings for mode are:
                ON - switches SSM support on
                OFF - switches SSM support off

    Possible settings for sabit are:

                SSM_SA4 - SSM uses SA4 bit
                SSM_SA5 - SSM uses SA5 bit
                SSM_SA6 - SSM uses SA6 bit
                SSM_SA7 - SSM uses SA7 bit
                SSM_SA8 - SSM uses SA8 bit

  Return Value: void

  Example:  e1_ssm_cfg(0, ON, SSM_SA8 )
            configures Port 0 for SSM transmisssion and receiption (via SA8 in E1)

  Remarks:
-------------------------------------------------------------------------*/
void quadFALC_e1_ssm_cfg (u8 port, u8 mode, u8 sabit, u8 ssm)
{
    QFALC_REG_MAP       *falc;

    falc = ptrreg[port];

    if (mode)
    {
        SET_BIT8(&falc->rdWr.fmr1, FMR1_ENSA);
        switch(sabit)
        {
	        case SSM_SA4:
	            MEM_WRITE8(&falc->rdWr.xsa4_xdl1,ssm);
	            break;

	        case SSM_SA5:
	            MEM_WRITE8(&falc->rdWr.xsa5_xdl2,ssm);
	            break;

	        case SSM_SA6:
	            MEM_WRITE8(&falc->rdWr.xsa6_xdl3,ssm);
	            break;

	        case SSM_SA7:
	            MEM_WRITE8(&falc->rdWr.xsa7_ccb1,ssm);
	            break;

	        case SSM_SA8:
	            MEM_WRITE8(&falc->rdWr.xsa8_ccb2,ssm);
	            break;

	         default:
	            break;
        }
    }
    else
    {
        CLEAR_BIT8(&falc->rdWr.fmr1, FMR1_ENSA);
    }
  }

/*group=SSM support functions
------------------------------------------------------------------------
  Description:  Extracts the received SSM data.

  Arguments:
                port - the QuadFALCDev.port
                ssmdata - the SSM data that shall be transmitted

  Settings:

  Return Value: u8 - possible returnvalues for ssmdata are:

                SSM_E1_QUALITY_UNKNOWN
                SSM_E1_REC_G_811
                SSM_E1_SSU_A
                SSM_E1_SSU_B
                SSM_E1_SETS
                SSM_E1_DO_NOT_USE

  Example:   e1_ssm_rxstatusreq(0, SSM_SA7)
                 requests current received SSM on Port 0

  Remarks:
-------------------------------------------------------------------------*/
u8 quadFALC_e1_ssm_rxstatusreq (u8 port, u8 sabit)
{
    QFALC_REG_MAP    *falc;
    u8               ssm;

    falc = ptrreg[port];

    switch(sabit)
    {
	    case SSM_SA4:
	        MEM_READ8(&falc->rd.rsa4_rdl1,&ssm);
	        break;

	    case SSM_SA5:
	        MEM_READ8(&falc->rd.rsa5_rdl2,&ssm);
	        break;

	    case SSM_SA6:
	        MEM_READ8(&falc->rd.rsa6_rdl3,&ssm);
	        break;

	    case SSM_SA7:
	        MEM_READ8(&falc->rd.rsa7,&ssm);
	        break;

	    case SSM_SA8:
	        MEM_READ8(&falc->rd.rsa8,&ssm);
	        break;

	    default:
	        ssm=SSM_E1_QUALITY_UNKNOWN;
	        break;
    }

    return(ssm&0x0f);
}

u8 quadFALC_e1_getsa7status( u8 port )
{
   return quadFALC_e1_ssm_rxstatusreq( port, SSM_SA7 );
}

u8 quadFALC_e1_port_is_crc4(u8 port)
{
    QFALC_REG_MAP  *falc;
    u8   temp;

    if( !QuadFALCDev.pef22554info[port/4].g_bE1ChipStatus )
        return FALSE;

    falc = ptrreg[port];
    MEM_READ8(&falc->rdWr.fmr2 ,&temp);
    if (temp &0x80)
    {
        return TRUE;
    }

    return FALSE;
}

u32 quadFALC_e1_get_active_num(void)
{
    return QuadFALCDev.g_ulE1ActiveNum;
}

// read the clk is from which port
u8 quadFALC_e1_rclk_read_port(void)
{
    QFALC_REG_MAP   *falc;
    u8   temp;
    u8 chipno=0;

    /*first choose the clock from chich chip,read logic register */

    /*second read the pef22554 gpc1 register*/
    falc = ptrreg[NUM_OF_FALC*chipno];

    MEM_READ8(&falc->rdWr.gpc1, &temp);
    return ((temp&0x03)-1);
}

void quadFALC_e1_rclk_select_surge(void)
{
     QFALC_REG_MAP   *falc;
     u8 port =0;

    if( !QuadFALCDev.pef22554info[port/4].g_bE1ChipStatus )
        return;

     /*first select the port surge*/
     falc = ptrreg[port];
     SET_BIT8(&falc->rdWr.lim0,0x01);
     quadFALC_e1_rclk_select(port);

     /*second set the chip 0 to logic register*/
     /*drv_set_clock_source(CLK_PEF1);*/
}

u8 quadFALC_e1_interface_statusreq (u8 port)
{
    QFALC_REG_MAP  *falc;
    u8 status;

    falc=ptrreg[port];
    /*bit 0  null
       bit 1  loss of multiframe alignment  LMFA
       bit 2  no of multiframe alignment found NMF
       bit 3  null
       bit 4  receive remote alarm RRA
       bit 5  loss of frame alignment LFA
       bit 6  alarm indication signal  AIS
       bit 7  loss of signal  LOS
    */
    MEM_READ8(&falc->rd.frs0,&status);

    status = status&0xF6;
    return (status);

}


u8 quadFALC_e1_loop_ctrl_read( u8 port )
{
    QFALC_REG_MAP      *falc;
    u8                  state;

    if( !QuadFALCDev.pef22554info[port/4].g_bE1ChipStatus )
       return E1_NO_LOOP;

    falc = ptrreg[port];

    /* judge local loop */
    MEM_READ8( &falc->rdWr.lim0, &state );
    if( state & LIM0_LL )
    {
        return E1_LOCAL_LOOP;
    }

    /* judge remote loop */
    MEM_READ8( &falc->rdWr.lim1, &state );
    if( state & LIM1_RL )
    {
        return E1_REMOTE_LOOP;
    }

    return E1_NO_LOOP;
}

/*Group=Loop
------------------------------------------------------------------------
  Description:  Turns on/off the remote, local, payload or channelwise loop.

  Arguments:
                port - the QuadFALCDev.port
                type - specifies the type of loop
                mode - turns on /off the loop
                ts_no - for channel loop only: the loopback is programmed for this timeslot

  Settings:     Possible settings for type are:

                REMOTE_LOOP - In the remote loopback mode the clock and data
                              recovered from the line inputs RL1/2 or
                              RDIP/RDIN are routed back to the line outputs
                              XL1/2 or XDOP/XDON via the analog or digital
                              transmitter.

                LOCAL_LOOP - Instead of the signals coming from the line the
                             data provided by the system interface is routed
                             through the analog receiver back to the system
                             interface.

                PAYLOAD_LOOP - The payload loopback (FMR2.PLB) loops the data
                               stream from the receiver section back to
                               transmitter section. The looped data passes
                               the complete receiver including the wander and
                               jitter compensation in the receive elastic
                               store and is output on pin RDO.

                CHANNEL_LOOP - Each of the 32 timeslots can be selected for
                               loopback from the system PCM input (XDI) to the
                               system PCM output (RDO). This loopback is
                               programmed for one timeslot at a time selected
                               by register LOOP. During loopback, an idle
                               channel code programmed in register IDLE is
                               transmitted to the remote end in the
                               corresponding PCM route timeslot.

    Possible settings for mode are:

                ON - turns on the specified loop
                OFF - turns off the specified loop

    Possible settings for ts_no are:

                1-32 - selects the timeslot that should be looped

  Return Value:    void

  Example:        e1_loop_ctrl_set(3, CHANNEL_LOOP, ON, 16)
                  turns on the channel loop for Timeslot 16 on Port 3

  Remarks:
-------------------------------------------------------------------------*/
void quadFALC_e1_loop_ctrl_set (u8 port, u8 type, u8 mode, u8 ts_no)
{
    QFALC_REG_MAP   *falc;

    // selects the falc base address
    falc = ptrreg[port];

    switch (type)
    {
        case E1_NO_LOOP:
            CLEAR_BIT8(&falc->rdWr.lim0, LIM0_LL);
            CLEAR_BIT8(&falc->rdWr.fmr2 ,FMR2_PLB);
            // Disable  ALL Channel Loop Back
            CLEAR_BIT8(&falc->rdWr.loop , LOOP_ECLB);
            CLEAR_BIT8(&falc->rdWr.loop ,(LOOP_CLA4 | LOOP_CLA3 | LOOP_CLA2 | LOOP_CLA1 | LOOP_CLA0));
            break;

        case E1_LOCAL_LOOP:
            if( mode==TRUE )
            {
                SET_BIT8(&falc->rdWr.lim0, LIM0_LL);
            }
            else
            {
                CLEAR_BIT8(&falc->rdWr.lim0, LIM0_LL);
            }
            break;

        case E1_PAYLOAD_LOOP:
            if (mode==TRUE )
            {
                SET_BIT8(&falc->rdWr.fmr2 , FMR2_PLB);
            }
            else
            {
                CLEAR_BIT8(&falc->rdWr.fmr2 , FMR2_PLB);
            }
            break;

        case E1_REMOTE_LOOP:
            if( mode==TRUE )
            {
                SET_BIT8(&falc->rdWr.lim1 , LIM1_RL);
            }
            else
            {
                CLEAR_BIT8(&falc->rdWr.lim1, LIM1_RL);
            }
            break;

        case E1_CHANNEL_LOOP:
            if( mode==TRUE )
            {
                // Enable Channel Loop Back
                SET_BIT8(&falc->rdWr.loop , LOOP_ECLB);
                CLEAR_BIT8(&falc->rdWr.loop , (LOOP_CLA4 | LOOP_CLA3 | LOOP_CLA2 | LOOP_CLA1 | LOOP_CLA0));
                SET_BIT8(&falc->rdWr.loop , ts_no & 0x1F);
            }
            else
            {
                // Disable Channel Loop Back
                CLEAR_BIT8(&falc->rdWr.loop , LOOP_ECLB);
                CLEAR_BIT8(&falc->rdWr.loop ,(LOOP_CLA4 | LOOP_CLA3 | LOOP_CLA2 | LOOP_CLA1 | LOOP_CLA0));
            }
            break;

        default:
            break;
    }

}

/*group=Z_Example for API applications
------------------------------------------------------------------------
  Description:  Standard E1 configuration.

  Arguments:
                port - the QuadFALCDev.port

  Settings:

  Return Value: void

  Source code:   void e1_standard_config(u8 port)

  Remarks:
-------------------------------------------------------------------------*/
void quadFALC_e1_standard_config(u8 port)
{
    quadFALC_e1_systeminterface_cfg(port);

    quadFALC_e1_lineinterface_cfg(port, SLAVE_MODE, HDM3);

    quadFALC_e1_elasticbuffer_cfg(port, ELASTIC_BUF_OFF, ELASTIC_BUF_ON);

    quadFALC_e1_framer_cfg(port, FRAMING_MULTIFRAME_TX, FRAMING_MULTIFRAME_RX);
}



/*
------------------------------------------------------------------------
  Description:      Basic Initialization of QuadFALCDev.chip.
  Include:
  Arguments:   chipno - the QuadFALCDev.chip number
  Return Value:     u32
  Example:        u32 e1_chip_init (0);
  Remarks:
-------------------------------------------------------------------------*/
///-
//u32 e1_chip_init (u8 chipno)
//int e1_chip_init( cmd_tbl_t *cmdtp, int flag, int argc, char * argv[] )
int quadFALC_e1_chip_init(int chipno)
{
    QFALC_REG_MAP  *falc;
    u16 i;
    u8  cis, tmp;

    
    if( chipno > MAX_PEF22554_NUM)
    {
        printk(KERN_ERR "%s: chipno:%d Invalid\n", __FUNCTION__, chipno);
        return -1;
    }
    
    falc = (QFALC_REG_MAP*)NULL;


    /*
        Clock Mode Register Settings for E1 or T1/J1 Table
        ==========================================================================
        MCLK [MHz]    GCM1    GCM2    GCM3    GCM4    GCM5    GCM6    GCM7    GCM8
        --------------------------------------------------------------------------
        1.5440        0x00    0x15    0x00    0x08    0x00    0x3F    0x9C    0xDF
        --------------------------------------------------------------------------
        2.0480        0x00    0x18    0xFB    0x0B    0x00    0x2F    0xDB    0xDF
        --------------------------------------------------------------------------
        8.1920        0x00    0x18    0xFB    0x0B    0x00    0x0B    0xDB    0xDF
        --------------------------------------------------------------------------
        16.3840       0x00    0x18    0xFB    0x0B    0x01    0x0B    0xDB    0xDF
        ==========================================================================
    */
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
        debug("E1 chipno=%d is exist.\n",chipno);
    }
    else
    {
        debug("E1 chipno=%d is not in exist.\n",chipno);
        QuadFALCDev.pef22554info[chipno].g_bE1ChipStatus = FALSE;
        QuadFALCDev.g_ulE1ActiveNum = chipno;
        return -1;
    }

    mdelay(10);
    
    MEM_READ8(&falc->rd.cis,&cis);
    
    if( (cis&CIS_PLLL)== 0x00 )
    {
        debug(" E1 22554 chip(%d) PLL init unlocked!\n",chipno);
    }

    /*
        Configure SEC/FSC Port
        00 SEC: Input, active high
        01 SEC: Output, active high
        10 FSC: Output, active high
        11 FSC: Output, active low
    */
    SET_BIT8(&falc->rdWr.gpc1, QFALC_GLOBAL_PORT<<5);

    
    // Enable System Interface Multiplex Mode
    /*
        4 个通道E1 帧复用为数据率为16.384Mbit/s 或8.192Mbit/s 的数据流
        0：正常模式
        1：系统接口复用模式
        4 个通道的接收系统接口的工作时钟和帧同步脉冲由第一通道的SCLKR 和SYPR 提供，发
        送系统接口的工作时钟和帧同步脉冲由第一通道的SCLKX 和SYPX 提供。数据会以字节间
        插或位间插格式接收或发送。在复用模式下，下列配置必须进行且4个通道必须一致：
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

    /*
        Define the function of the interrupt output pin INT
        X0 Open drain output
        01 Push/pull output, active low
        11 Push/pull output, active high
    */
    SET_BIT8(&falc->rdWr.ipc, QFALC_INTERRUPT); // pin INT : Push/pull output, active low

    for (i = 0; i < NUM_OF_FALC; i++)
    {
        falc = ptrreg[NUM_OF_FALC*chipno+i];


        SET_BIT8(&falc->rdWr.gcr, GCR_SCI); // The following interrupts are activated both on activation and deactivation of the internal interrupt source: ISR2.LOS, ISR2.AIS,ISR3.LMFA16.
        SET_BIT8(&falc->rdWr.gcr, GCR_VIS); // Masked interrupt status bits are visible in ISR

        /*
            Interrupt Mask Registers Table
            =========================================================================
            Bit number  7       6       5       4       3       2       1       0
            -------------------------------------------------------------------------
            IMR0        RME     RFS     T8MS    RMB     CASC    CRC4    SA6SC   RPF
            -------------------------------------------------------------------------
            IMR1        LLBSC   RDO     ALLS    XDU     XMB     SUEX    XLSC    XPR
            -------------------------------------------------------------------------
            IMR2        FAR     LFA     MFAR    T400MS  AIS     LOS     RAR     RA
            -------------------------------------------------------------------------
            IMR3        ES      SEC     LMFA16  AIS16   RA16    LTC     RSN     RSP
            -------------------------------------------------------------------------
            IMR4        XSP     XSN     RME2    RFS2    RDO2    ALLS2   XDU2    RPF2
            -------------------------------------------------------------------------
            IMR5        XPR2    XPR3    RME3    RFS3    RDO3    ALLS3   XDU3    RPF3
            -------------------------------------------------------------------------
            IMR6        SOLSU   SOLSD   LOLSU   LOLSD   SILSU   SILSD   LILSU   LILSD
            -------------------------------------------------------------------------
            IMR7        -       -       -       XCLKSS1 XCLKSS0 -       -       -
            =========================================================================
        */
        MEM_WRITE8(&falc->rdWr.imr0 , 0xFF);
        MEM_WRITE8(&falc->rdWr.imr1 , 0xFF);
        MEM_WRITE8(&falc->rdWr.imr2 , 0xFF);
        MEM_WRITE8(&falc->rdWr.imr3 , 0xFF);
        MEM_WRITE8(&falc->rdWr.imr4 , 0xFF);

        quadFALC_e1_standard_config( (u8)(NUM_OF_FALC*chipno+i) );
    }

    //Receiver Sensitivity optimization
    quadFALC_e1_receiver_reset(chipno);


    return 0;
}

