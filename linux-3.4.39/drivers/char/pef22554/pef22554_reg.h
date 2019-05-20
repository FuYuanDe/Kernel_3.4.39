/******************************************************************************
        (c) COPYRIGHT 2002-2003 by Shenzhen Allywll Information Co.,Ltd
                          All rights reserved.
File: pef22554_reg.h
Desc:the source file of user config
Modification history(no, author, date, desc)
1.Holy 2003-04-02 create file
******************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _PEF22554_REG_H
#define _PEF22554_REG_H


/* CMDR  (Command Register)

---------------- E1 & T1 ------------------------------*/

#define  CMDR_RMC        0x80
#define  CMDR_RRES       0x40
#define  CMDR_XREP       0x20
#define  CMDR_XRES       0x10
#define  CMDR_XHF        0x08
#define  CMDR_XTF        0x04
#define  CMDR_XME        0x02
#define  CMDR_SRES       0x01


/* CMDR2 (Command Register 2) */

#define  CMDR2_RSUC      0x02
#define  CMDR2_XPPR      0x01


/* MODE  (Mode Register)

----------------- E1 & T1 -----------------------------*/

#define  MODE_MDS2       0x80
#define  MODE_MDS1       0x40
#define  MODE_MDS0       0x20
#define  MODE_BRAC       0x10
#define  MODE_HRAC       0x08
#define  MODE_DIV        0x04

/* IPC   (Interrupt Port Configuration)

----------------- E1 & T1 -----------------------------*/

#define  IPC_SSYF        0x04
#define  IPC_IC1         0x02
#define  IPC_IC0         0x01


/* GPC1   (General Port Configuration)

----------------- E1 & T1 -----------------------------*/

#define  GPC1_SMM         0x80
#define  GPC1_CSFP1       0x40
#define  GPC1_CSFP0       0x20
#define  GPC1_FSS1        0x08
#define  GPC1_FSS0        0x04
#define  GPC1_R1S1        0x02
#define  GPC1_R1S0        0x01

/* GCR  (Global Configuration Register)

----------------- E1 & T1 -----------------------------*/

#define  GCR_VIS         0x80
#define  GCR_SCI         0x40
#define  GCR_SES         0x20
#define  GCR_ECMC        0x10
#define  GCR_PD          0x01

/* CCR1  (Common Configuration Register 1)

----------------- E1 & T1 -----------------------------*/


#define  CCR1_BRM        0x40
#define  CCR1_EDLX       0x20
#define  CCR1_CASM       0x20
#define  CCR1_EITS       0x10
#define  CCR1_ITF        0x08
#define  CCR1_XMFA       0x04
#define  CCR1_RFT1       0x02
#define  CCR1_RFT0       0x01



/* CCR2  (Common Configuration Register 2)

---------------- E1 & T1 ------------------------------*/

#define  CCR2_RADD       0x10
#define  CCR2_RBFE       0x08
#define  CCR2_RCRC       0x04
#define  CCR2_XCRC       0x02

/* CCR5  (Common Configuration Register 5) */

#define  CCR5_SUET       0x20
#define  CCR5_CSF        0x10
#define  CCR5_AFX        0x08
#define  CCR5_CR         0x02
#define  CCR5_EPR        0x01


/* RTR1-4  (Receive Timeslot Register 1-4)

---------------- E1 & T1 ------------------------------*/

#define  RTR1_TS0        0x80
#define  RTR1_TS1        0x40
#define  RTR1_TS2        0x20
#define  RTR1_TS3        0x10
#define  RTR1_TS4        0x08
#define  RTR1_TS5        0x04
#define  RTR1_TS6        0x02
#define  RTR1_TS7        0x01

#define  RTR2_TS8        0x80
#define  RTR2_TS9        0x40
#define  RTR2_TS10       0x20
#define  RTR2_TS11       0x10
#define  RTR2_TS12       0x08
#define  RTR2_TS13       0x04
#define  RTR2_TS14       0x02
#define  RTR2_TS15       0x01

#define  RTR3_TS16       0x80
#define  RTR3_TS17       0x40
#define  RTR3_TS18       0x20
#define  RTR3_TS19       0x10
#define  RTR3_TS20       0x08
#define  RTR3_TS21       0x04
#define  RTR3_TS22       0x02
#define  RTR3_TS23       0x01

#define  RTR4_TS24       0x80
#define  RTR4_TS25       0x40
#define  RTR4_TS26       0x20
#define  RTR4_TS27       0x10
#define  RTR4_TS28       0x08
#define  RTR4_TS29       0x04
#define  RTR4_TS30       0x02
#define  RTR4_TS31       0x01


/* TTR1-4  (Transmit Timeslot Register 1-4)

---------------- E1 & T1 ------------------------------*/

#define  TTR1_TS0        0x80
#define  TTR1_TS1        0x40
#define  TTR1_TS2        0x20
#define  TTR1_TS3        0x10
#define  TTR1_TS4        0x08
#define  TTR1_TS5        0x04
#define  TTR1_TS6        0x02
#define  TTR1_TS7        0x01

#define  TTR2_TS8        0x80
#define  TTR2_TS9        0x40
#define  TTR2_TS10       0x20
#define  TTR2_TS11       0x10
#define  TTR2_TS12       0x08
#define  TTR2_TS13       0x04
#define  TTR2_TS14       0x02
#define  TTR2_TS15       0x01

#define  TTR3_TS16       0x80
#define  TTR3_TS17       0x40
#define  TTR3_TS18       0x20
#define  TTR3_TS19       0x10
#define  TTR3_TS20       0x08
#define  TTR3_TS21       0x04
#define  TTR3_TS22       0x02
#define  TTR3_TS23       0x01

#define  TTR4_TS24       0x80
#define  TTR4_TS25       0x40
#define  TTR4_TS26       0x20
#define  TTR4_TS27       0x10
#define  TTR4_TS28       0x08
#define  TTR4_TS29       0x04
#define  TTR4_TS30       0x02
#define  TTR4_TS31       0x01


/* TSEO    (Time Slot Even/Odd Select) */

#define  TSEO_EO11       0x02
#define  TSEO_EO10       0x01

/* TSBS1   (Time Slot Bit Select 1)    */

#define  TSBS1_TSB17     0x80
#define  TSBS1_TSB16     0x40
#define  TSBS1_TSB15     0x20
#define  TSBS1_TSB14     0x10
#define  TSBS1_TSB13     0x08
#define  TSBS1_TSB12     0x04
#define  TSBS1_TSB11     0x02
#define  TSBS1_TSB10     0x01

/* TPC0    (Test Pattern Control Register 0)    */

#define  TPC0_FRA        0x40


/* IMR0-4  (Interrupt Mask Register 0-4)

----------------- E1 & T1 -----------------------------*/

#define  IMR0_RME        0x80
#define  IMR0_RFS        0x40
#define  IMR0_T8MS       0x20
#define  IMR0_ISF        0x20
#define  IMR0_RMB        0x10
#define  IMR0_CASC       0x08
#define  IMR0_RSC        0x08
#define  IMR0_CRC6       0x04
#define  IMR0_CRC4       0x04    
#define  IMR0_SA6SC      0x02  
#define  IMR0_PDEN       0x02
#define  IMR0_RPF        0x01

#define  IMR1_LLBSC      0x80
#define  IMR1_CASE       0x80
#define  IMR1_RDO        0x40
#define  IMR1_ALLS       0x20
#define  IMR1_XDU        0x10
#define  IMR1_XMB        0x08
#define  IMR1_SUEX       0x04
#define  IMR1_XLSC       0x02
#define  IMR1_XPR        0x01

#define  IMR2_FAR        0x80
#define  IMR2_LFA        0x40
#define  IMR2_MFAR       0x20
#define  IMR2_T400MS     0x10
#define  IMR2_LMFA       0x10
#define  IMR2_AIS        0x08
#define  IMR2_LOS        0x04
#define  IMR2_RAR        0x02
#define  IMR2_RA         0x01

#define  IMR3_ES         0x80
#define  IMR3_SEC        0x40
#define  IMR3_LMFA16     0x20
#define  IMR3_AIS16      0x10
#define  IMR3_RA16       0x08
#define  IMR3_LLBSC      0x08
#define  IMR3_RSN        0x02
#define  IMR3_RSP        0x01

#define  IMR4_XSP        0x80
#define  IMR4_XSN        0x40


/* IERR for E1 and T1 (Single Bit Error Insertion Register) */

#define  IERR_IFASE      0x20
#define  IERR_IMFE       0x10
#define  IERR_ICRCE      0x08
#define  IERR_ICASE      0x04
#define  IERR_IPE        0x02
#define  IERR_IBV        0x01


/* FMR0-5 for E1 and T1  (Framer Mode Register )*/

#define  FMR0_XC1        0x80
#define  FMR0_XC0        0x40
#define  FMR0_RC1        0x20
#define  FMR0_RC0        0x10
#define  FMR0_EXZE       0x08
#define  FMR0_ALM        0x04
#define  E1_FMR0_FRS     0x02
#define  T1_FMR0_FRS     0x08
#define  FMR0_SRAF       0x04
#define  FMR0_EXLS       0x02
#define  FMR0_SIM        0x01

#define  FMR1_MFCS       0x80
#define  FMR1_AFR        0x40
#define  FMR1_ENSA       0x20
#define  FMR1_CTM        0x80
#define  FMR1_EDL        0x20
#define  FMR1_PMOD       0x10
#define  FMR1_XFS        0x08
#define  FMR1_CRC        0x08
#define  FMR1_ECM        0x04
#define  FMR1_SSD0       0x02
#define  FMR1_XAIS       0x01

#define  FMR2_RFS1       0x80
#define  FMR2_RFS0       0x40
#define  FMR2_MCSP       0x40
#define  FMR2_RTM        0x20
#define  FMR2_SSP        0x20
#define  FMR2_DAIS       0x10
#define  FMR2_SAIS       0x08
#define  FMR2_PLB        0x04
#define  FMR2_AXRA       0x02
#define  FMR2_ALMF       0x01
#define  FMR2_EXZE       0x01

/*--------------------- T1 ----------------------------*/
#define  FMR4_AIS3       0x80
#define  FMR4_TM         0x40
#define  FMR4_XRA        0x20
#define  FMR4_SSC1       0x10
#define  FMR4_SSC0       0x08
#define  FMR4_AUTO       0x04
#define  FMR4_FM1        0x02
#define  FMR4_FM0        0x01

#define  FMR5_EIBR       0x40
#define  FMR5_XLD        0x20
#define  FMR5_XLU        0x10
#define  FMR5_XTM        0x04
#define  FMR5_SSC2       0x02


/* LOOP  (Channel Loop Back)

------------------ E1 & T1 ----------------------------*/

#define  LOOP_RTM        0x40
#define  LOOP_ECLB       0x20
#define  LOOP_CLA4       0x10
#define  LOOP_CLA3       0x08
#define  LOOP_CLA2       0x04
#define  LOOP_CLA1       0x02
#define  LOOP_CLA0       0x01


/* LCR1  (Loop Code Register 1)

------------------ E1 & T1 ----------------------------*/

#define  LCR1_EPRM        0x80
#define  LCR1_XPRBS       0x40
#define  LCR1_LDC1        0x20
#define  LCR1_LDC0        0x10
#define  LCR1_LAC1        0x08
#define  LCR1_LAC0        0x04
#define  LCR1_FLLB        0x02
#define  LCR1_LLBP        0x01


/* LCR2  (Loop Code Register 1)

------------------ E1 & T1 ----------------------------*/

#define  LCR2_LDC7        0x80
#define  LCR2_LDC6        0x40
#define  LCR2_LDC5        0x20
#define  LCR2_LDC4        0x10
#define  LCR2_LDC3        0x08
#define  LCR2_LDC2        0x04
#define  LCR2_LDC1        0x02
#define  LCR2_LDC0        0x01


/* LCR3  (Loop Code Register 1)

------------------ E1 & T1 ----------------------------*/

#define  LCR3_LAC7        0x80
#define  LCR3_LAC6        0x40
#define  LCR3_LAC5        0x20
#define  LCR3_LAC4        0x10
#define  LCR3_LAC3        0x08
#define  LCR3_LAC2        0x04
#define  LCR3_LAC1        0x02
#define  LCR3_LAC0        0x01


/* XSW  (Transmit Service UINT Pulseframe)

------------------- E1 ---------------------------*/

#define  XSW_XSIS        0x80
#define  XSW_XTM         0x40
#define  XSW_XRA         0x20
#define  XSW_XY0         0x10
#define  XSW_XY1         0x08
#define  XSW_XY2         0x04
#define  XSW_XY3         0x02
#define  XSW_XY4         0x01


/* XSP  (Transmit Spare Bits)

------------------- E1 ---------------------------*/

#define  XSP_CASEN       0x40
#define  XSP_TT0         0x20
#define  XSP_EBP         0x10
#define  XSP_AXS         0x08
#define  XSP_XSIF        0x04
#define  XSP_XS13        0x02
#define  XSP_XS15        0x01


/* XC0/1  (Transmit Control 0/1)

------------------ E1 & T1 ----------------------------*/

#define  XC0_SA8E        0x80
#define  XC0_SA7E        0x40
#define  XC0_SA6E        0x20
#define  XC0_SA5E        0x10
#define  XC0_SA4E        0x08
#define  XC0_BRM         0x80
#define  XC0_MFBS        0x40
#define  XC0_BRFO        0x08
#define  XC0_XCO10       0x04
#define  XC0_XCO9        0x02
#define  XC0_XCO8        0x01

#define  XC1_XCO7        0x80
#define  XC1_XCO6        0x40
#define  XC1_XCO5        0x20
#define  XC1_XCO4        0x10
#define  XC1_XCO3        0x08
#define  XC1_XCO2        0x04
#define  XC1_XCO1        0x02
#define  XC1_XCO0        0x01


/* RC0/1  (Receive Control 0/1)

------------------ E1 & T1 ----------------------------*/


#define  RC0_SWD         0x80
#define  RC0_SJR         0x80
#define  RC0_ASY4        0x40
#define  RC0_RRAM        0x40
#define  RC0_CRCI        0x20
#define  RC0_XCRCI       0x10
#define  RC0_RDIS        0x08
#define  RC0_RCO10       0x04
#define  RC0_RCO9        0x02
#define  RC0_RCO8        0x01

#define  RC1_RTO7        0x80
#define  RC1_RTO6        0x40
#define  RC1_RTO5        0x20
#define  RC1_RTO4        0x10
#define  RC1_RTO3        0x08
#define  RC1_RTO2        0x04
#define  RC1_RTO1        0x02
#define  RC1_RTO0        0x01

#define  RC1_RCO7        0x80
#define  RC1_RCO6        0x40
#define  RC1_RCO5        0x20
#define  RC1_RCO4        0x10
#define  RC1_RCO3        0x08
#define  RC1_RCO2        0x04
#define  RC1_RCO1        0x02
#define  RC1_RCO0        0x01


/* XPM0-2  (Transmit Pulse Mask 0-2)

--------------------- E1 & T1 -------------------------*/

#define  XPM0_XP12       0x80
#define  XPM0_XP11       0x40
#define  XPM0_XP10       0x20
#define  XPM0_XP04       0x10
#define  XPM0_XP03       0x08
#define  XPM0_XP02       0x04
#define  XPM0_XP01       0x02
#define  XPM0_XP00       0x01

#define  XPM1_XP30       0x80
#define  XPM1_XP24       0x40
#define  XPM1_XP23       0x20
#define  XPM1_XP22       0x10
#define  XPM1_XP21       0x08
#define  XPM1_XP20       0x04
#define  XPM1_XP14       0x02
#define  XPM1_XP13       0x01

#define  XPM2_XLLP       0x80
#define  XPM2_XLT        0x40
#define  XPM2_DAXLT      0x20
#define  XPM2_XP34       0x08
#define  XPM2_XP33       0x04
#define  XPM2_XP32       0x02
#define  XPM2_XP31       0x01


/* TSWM  (Transparent Service UINT Mask)

------------------ E1 ----------------------------*/

#define  TSWM_TSIS       0x80
#define  TSWM_TSIF       0x40
#define  TSWM_TRA        0x20
#define  TSWM_TSA4       0x10
#define  TSWM_TSA5       0x08
#define  TSWM_TSA6       0x04
#define  TSWM_TSA7       0x02
#define  TSWM_TSA8       0x01

/* IDLE  <Idle Channel Code Register>

------------------ E1 & T1 -----------------------*/

#define  IDLE_IDL7       0x80 
#define  IDLE_IDL6       0x40 
#define  IDLE_IDL5       0x20 
#define  IDLE_IDL4       0x10 
#define  IDLE_IDL3       0x08 
#define  IDLE_IDL2       0x04 
#define  IDLE_IDL1       0x02 
#define  IDLE_IDL0       0x01 


/* XSA4-8 <Transmit SA4-8 Register(Read/Write) >

-------------------E1 -----------------------------*/

#define  XSA4_XS47       0x80
#define  XSA4_XS46       0x40
#define  XSA4_XS45       0x20
#define  XSA4_XS44       0x10
#define  XSA4_XS43       0x08
#define  XSA4_XS42       0x04
#define  XSA4_XS41       0x02
#define  XSA4_XS40       0x01

#define  XSA5_XS57       0x80
#define  XSA5_XS56       0x40
#define  XSA5_XS55       0x20
#define  XSA5_XS54       0x10
#define  XSA5_XS53       0x08
#define  XSA5_XS52       0x04
#define  XSA5_XS51       0x02
#define  XSA5_XS50       0x01

#define  XSA6_XS67       0x80
#define  XSA6_XS66       0x40
#define  XSA6_XS65       0x20
#define  XSA6_XS64       0x10
#define  XSA6_XS63       0x08
#define  XSA6_XS62       0x04
#define  XSA6_XS61       0x02
#define  XSA6_XS60       0x01

#define  XSA7_XS77       0x80
#define  XSA7_XS76       0x40
#define  XSA7_XS75       0x20
#define  XSA7_XS74       0x10
#define  XSA7_XS73       0x08
#define  XSA7_XS72       0x04
#define  XSA7_XS71       0x02
#define  XSA7_XS70       0x01

#define  XSA8_XS87       0x80
#define  XSA8_XS86       0x40
#define  XSA8_XS85       0x20
#define  XSA8_XS84       0x10
#define  XSA8_XS83       0x08
#define  XSA8_XS82       0x04
#define  XSA8_XS81       0x02
#define  XSA8_XS80       0x01


/* XDL1-3  (Transmit DL-Bit Register1-3 (read/write))

----------------------- T1 ---------------------*/

#define  XDL1_XDL17      0x80
#define  XDL1_XDL16      0x40
#define  XDL1_XDL15      0x20
#define  XDL1_XDL14      0x10
#define  XDL1_XDL13      0x08
#define  XDL1_XDL12      0x04
#define  XDL1_XDL11      0x02
#define  XDL1_XDL10      0x01

#define  XDL2_XDL27      0x80
#define  XDL2_XDL26      0x40
#define  XDL2_XDL25      0x20
#define  XDL2_XDL24      0x10
#define  XDL2_XDL23      0x08
#define  XDL2_XDL22      0x04
#define  XDL2_XDL21      0x02
#define  XDL2_XDL20      0x01

#define  XDL3_XDL37      0x80
#define  XDL3_XDL36      0x40
#define  XDL3_XDL35      0x20
#define  XDL3_XDL34      0x10
#define  XDL3_XDL33      0x08
#define  XDL3_XDL32      0x04
#define  XDL3_XDL31      0x02
#define  XDL3_XDL30      0x01


/* ICB1-4  (Idle Channel Register 1-4)

------------------ E1 ----------------------------*/

#define  E1_ICB1_IC0        0x80
#define  E1_ICB1_IC1        0x40
#define  E1_ICB1_IC2        0x20
#define  E1_ICB1_IC3        0x10
#define  E1_ICB1_IC4        0x08
#define  E1_ICB1_IC5        0x04
#define  E1_ICB1_IC6        0x02
#define  E1_ICB1_IC7        0x01

#define  E1_ICB2_IC8        0x80
#define  E1_ICB2_IC9        0x40
#define  E1_ICB2_IC10       0x20
#define  E1_ICB2_IC11       0x10
#define  E1_ICB2_IC12       0x08
#define  E1_ICB2_IC13       0x04
#define  E1_ICB2_IC14       0x02
#define  E1_ICB2_IC15       0x01

#define  E1_ICB3_IC16       0x80
#define  E1_ICB3_IC17       0x40
#define  E1_ICB3_IC18       0x20
#define  E1_ICB3_IC19       0x10
#define  E1_ICB3_IC20       0x08
#define  E1_ICB3_IC21       0x04
#define  E1_ICB3_IC22       0x02
#define  E1_ICB3_IC23       0x01

#define  E1_ICB4_IC24       0x80
#define  E1_ICB4_IC25       0x40
#define  E1_ICB4_IC26       0x20
#define  E1_ICB4_IC27       0x10
#define  E1_ICB4_IC28       0x08
#define  E1_ICB4_IC29       0x04
#define  E1_ICB4_IC30       0x02
#define  E1_ICB4_IC31       0x01

/* ICB1-4  (Idle Channel Register 1-4)

------------------ T1 ----------------------------*/

#define  T1_ICB1_IC1        0x80
#define  T1_ICB1_IC2        0x40
#define  T1_ICB1_IC3        0x20
#define  T1_ICB1_IC4        0x10
#define  T1_ICB1_IC5        0x08
#define  T1_ICB1_IC6        0x04
#define  T1_ICB1_IC7        0x02
#define  T1_ICB1_IC8        0x01

#define  T1_ICB2_IC9        0x80
#define  T1_ICB2_IC10       0x40
#define  T1_ICB2_IC11       0x20
#define  T1_ICB2_IC12       0x10
#define  T1_ICB2_IC13       0x08
#define  T1_ICB2_IC14       0x04
#define  T1_ICB2_IC15       0x02
#define  T1_ICB2_IC16       0x01

#define  T1_ICB3_IC17       0x80
#define  T1_ICB3_IC18       0x40
#define  T1_ICB3_IC19       0x20
#define  T1_ICB3_IC20       0x10
#define  T1_ICB3_IC21       0x08
#define  T1_ICB3_IC22       0x04
#define  T1_ICB3_IC23       0x02
#define  T1_ICB3_IC24       0x01

/* FMR3    (Framer Mode Register 3)
--------------------E1------------------------*/

#define  FMR3_XLD        0x20
#define  FMR3_XLU        0x10
#define  FMR3_CMI        0x08
#define  FMR3_SA6SY      0x04
#define  FMR3_EXTIW      0x01



/* CCB1-3  (Clear Channel Register)

------------------- T1 -----------------------*/

#define  CCB1_CH1        0x80
#define  CCB1_CH2        0x40
#define  CCB1_CH3        0x20
#define  CCB1_CH4        0x10
#define  CCB1_CH5        0x08
#define  CCB1_CH6        0x04
#define  CCB1_CH7        0x02
#define  CCB1_CH8        0x01

#define  CCB2_CH9        0x80
#define  CCB2_CH10       0x40
#define  CCB2_CH11       0x20
#define  CCB2_CH12       0x10
#define  CCB2_CH13       0x08
#define  CCB2_CH14       0x04
#define  CCB2_CH15       0x02
#define  CCB2_CH16       0x01

#define  CCB2_CH17       0x80
#define  CCB2_CH18       0x40
#define  CCB2_CH19       0x20
#define  CCB2_CH20       0x10
#define  CCB2_CH21       0x08
#define  CCB2_CH22       0x04
#define  CCB2_CH23       0x02
#define  CCB2_CH24       0x01


/* LIM0/1/2  (Line Interface Mode 0/1/2)

------------------- E1 & T1 ---------------------------*/

#define  LIM0_XFB        0x80
#define  LIM0_XDOS       0x40
#define  LIM0_EQON       0x08
#define  LIM0_RLM        0x04
#define  LIM0_LL         0x02
#define  LIM0_MAS        0x01

#define  LIM1_CLOS       0x80
#define  LIM1_RIL2       0x40
#define  LIM1_RIL1       0x20
#define  LIM1_RIL0       0x10
#define  LIM1_DCOC       0x08
#define  LIM1_JATT       0x04
#define  LIM1_RL         0x02
#define  LIM1_DRS        0x01

#define  LIM2_LBO2       0x80
#define  LIM2_LBO1       0x40
#define  LIM2_SLT1       0x20
#define  LIM2_SLT0       0x10
#define  LIM2_SCF        0x08
#define  LIM2_ELT        0x04
#define  LIM2_LOS1       0x01



/* PCD  (Pulse Count Detection Register(Read/Write))

------------------ E1 & T1 -------------------------*/

#define  PCD_PCD7        0x80
#define  PCD_PCD6        0x40
#define  PCD_PCD5        0x20
#define  PCD_PCD4        0x10
#define  PCD_PCD3        0x08
#define  PCD_PCD2        0x04
#define  PCD_PCD1        0x02
#define  PCD_PCD0        0x01

#define  PCR_PCR7        0x80
#define  PCR_PCR6        0x40
#define  PCR_PCR5        0x20
#define  PCR_PCR4        0x10
#define  PCR_PCR3        0x08
#define  PCR_PCR2        0x04
#define  PCR_PCR1        0x02
#define  PCR_PCR0        0x01


/* DEC  (Disable Error Counter)

------------------ E1 & T1 ----------------------------*/

#define  DEC_DRBD        0x80
#define  DEC_DCEC3       0x20
#define  DEC_DCOEC       0x20
#define  DEC_DCEC2       0x10
#define  DEC_DBEC        0x10
#define  DEC_DCEC1       0x08
#define  DEC_DCEC        0x08
#define  DEC_DEBC        0x04
#define  DEC_DCVC        0x02
#define  DEC_DFEC        0x01


/* FALC Register Bits (Receive Mode)
----------------------------------------------------------------------------*/


/* FRS0/1  (Framer Receive Status Register 0/1)

----------------- E1 & T1 ----------------------------------*/

#define  FRS0_LOS          0x80
#define  FRS0_AIS          0x40
#define  FRS0_LFA          0x20
#define  FRS0_RRA          0x10
#define  FRS0_NMF          0x04
#define  FRS0_LMFA         0x02
#define  FRS0_FSRF         0x01

#define  FRS1_EXZD         0x80
#define  FRS1_TS16RA       0x40
#define  FRS1_PDEN         0x40
#define  FRS1_TS16LOS      0x20
#define  FRS1_TS16AIS      0x10
#define  FRS1_LLBDD        0x10
#define  FRS1_TS16LFA      0x08
#define  FRS1_LLBAD        0x08
#define  FRS1_XLS          0x02
#define  FRS1_XLO          0x01


/* FRS2  (Framer Receive Status Register 2)

----------------- T1 ----------------------------------*/

#define  FRS2_ESC2       0x80
#define  FRS2_ESC1       0x40
#define  FRS2_ESC0       0x20


/* RSW  (Receive Service UINT Pulseframe)

----------------- E1 ------------------------------*/

#define  RSW_RSI         0x80
#define  RSW_RRA         0x20
#define  RSW_RYO         0x10
#define  RSW_RY1         0x08
#define  RSW_RY2         0x04
#define  RSW_RY3         0x02
#define  RSW_RY4         0x01


/* RSP  (Receive Spare Bits / Additional Status)

---------------- E1 -------------------------------*/

#define  RSP_SI1         0x80
#define  RSP_SI2         0x40
#define  RSP_LLBDD       0x10
#define  RSP_LLBAD       0x08
#define  RSP_RSIF        0x04
#define  RSP_RS13        0x02
#define  RSP_RS15        0x01


/* FECL  (Framing Error Counter)

---------------- E1 & T1 --------------------------*/

#define  FECL_FE7        0x80
#define  FECL_FE6        0x40
#define  FECL_FE5        0x20
#define  FECL_FE4        0x10
#define  FECL_FE3        0x08
#define  FECL_FE2        0x04
#define  FECL_FE1        0x02
#define  FECL_FE0        0x01

#define  FECH_FE15       0x80
#define  FECH_FE14       0x40
#define  FECH_FE13       0x20
#define  FECH_FE12       0x10
#define  FECH_FE11       0x08
#define  FECH_FE10       0x04
#define  FECH_FE9        0x02
#define  FECH_FE8        0x01


/* CVCL (Code Violation Counter)

----------------- E1 & T1 ---------------------*/

#define  CVCL_CV7        0x80
#define  CVCL_CV6        0x40
#define  CVCL_CV5        0x20
#define  CVCL_CV4        0x10
#define  CVCL_CV3        0x08
#define  CVCL_CV2        0x04
#define  CVCL_CV1        0x02
#define  CVCL_CV0        0x01

#define  CVCH_CV15       0x80
#define  CVCH_CV14       0x40
#define  CVCH_CV13       0x20
#define  CVCH_CV12       0x10
#define  CVCH_CV11       0x08
#define  CVCH_CV10       0x04
#define  CVCH_CV9        0x02
#define  CVCH_CV8        0x01


/* CEC1-3L  (CRC Error Counter)

------------------ E1 -----------------------------*/

#define  CEC1L_CR7       0x80
#define  CEC1L_CR6       0x40
#define  CEC1L_CR5       0x20
#define  CEC1L_CR4       0x10
#define  CEC1L_CR3       0x08
#define  CEC1L_CR2       0x04
#define  CEC1L_CR1       0x02
#define  CEC1L_CR0       0x01

#define  CEC1H_CR15      0x80
#define  CEC1H_CR14      0x40
#define  CEC1H_CR13      0x20
#define  CEC1H_CR12      0x10
#define  CEC1H_CR11      0x08
#define  CEC1H_CR10      0x04
#define  CEC1H_CR9       0x02
#define  CEC1H_CR8       0x01

#define  CEC2L_CR7       0x80
#define  CEC2L_CR6       0x40
#define  CEC2L_CR5       0x20
#define  CEC2L_CR4       0x10
#define  CEC2L_CR3       0x08
#define  CEC2L_CR2       0x04
#define  CEC2L_CR1       0x02
#define  CEC2L_CR0       0x01

#define  CEC2H_CR15      0x80
#define  CEC2H_CR14      0x40
#define  CEC2H_CR13      0x20
#define  CEC2H_CR12      0x10
#define  CEC2H_CR11      0x08
#define  CEC2H_CR10      0x04
#define  CEC2H_CR9       0x02
#define  CEC2H_CR8       0x01

#define  CEC3L_CR7       0x80
#define  CEC3L_CR6       0x40
#define  CEC3L_CR5       0x20
#define  CEC3L_CR4       0x10
#define  CEC3L_CR3       0x08
#define  CEC3L_CR2       0x04
#define  CEC3L_CR1       0x02
#define  CEC3L_CR0       0x01

#define  CEC3H_CR15      0x80
#define  CEC3H_CR14      0x40
#define  CEC3H_CR13      0x20
#define  CEC3H_CR12      0x10
#define  CEC3H_CR11      0x08
#define  CEC3H_CR10      0x04
#define  CEC3H_CR9       0x02
#define  CEC3H_CR8       0x01


/* CECL  (CRC Error Counter)

------------------ T1 -----------------------------*/

#define  CECL_CR7        0x80
#define  CECL_CR6        0x40
#define  CECL_CR5        0x20
#define  CECL_CR4        0x10
#define  CECL_CR3        0x08
#define  CECL_CR2        0x04
#define  CECL_CR1        0x02
#define  CECL_CR0        0x01

#define  CECH_CR15       0x80
#define  CECH_CR14       0x40
#define  CECH_CR13       0x20
#define  CECH_CR12       0x10
#define  CECH_CR11       0x08
#define  CECH_CR10       0x04
#define  CECH_CR9        0x02
#define  CECH_CR8        0x01

/* EBCL  (E Bit Error Counter)

------------------- E1 & T1 -------------------------*/

#define  EBCL_EB7        0x80
#define  EBCL_EB6        0x40
#define  EBCL_EB5        0x20
#define  EBCL_EB4        0x10
#define  EBCL_EB3        0x08
#define  EBCL_EB2        0x04
#define  EBCL_EB1        0x02
#define  EBCL_EB0        0x01

#define  EBCH_EB15       0x80
#define  EBCH_EB14       0x40
#define  EBCH_EB13       0x20
#define  EBCH_EB12       0x10
#define  EBCH_EB11       0x08
#define  EBCH_EB10       0x04
#define  EBCH_EB9        0x02
#define  EBCH_EB8        0x01


/* BECL  (Bit Error Counter)

--------------------- T1 ---------------------------*/

#define  BECL_BEC7      0x80
#define  BECL_BEC6      0x40
#define  BECL_BEC5      0x20
#define  BECL_BEC4      0x10
#define  BECL_BEC3      0x08
#define  BECL_BEC2      0x04
#define  BECL_BEC1      0x02
#define  BECL_BEC0      0x01

/* BECH  (Bit Error Counter)

--------------------- T1 ---------------------------*/


#define  BECH_BEC15     0x80
#define  BECH_BEC14     0x40
#define  BECH_BEC13     0x20
#define  BECH_BEC12     0x10
#define  BECH_BEC11     0x08
#define  BECH_BEC10     0x04
#define  BECH_BEC9      0x02
#define  BECH_BEC8      0x01

/* COEC  (COFA Event Counter)

--------------------- T1 ---------------------------*/


#define  COEC_COE7      0x80
#define  COEC_COE6      0x40
#define  COEC_COE5      0x20
#define  COEC_COE4      0x10
#define  COEC_COE3      0x08
#define  COEC_COE2      0x04
#define  COEC_COE1      0x02
#define  COEC_COE0      0x01


/* ESM (Errored Second Mask)

-------------------- E1 & T1 -----------------------*/

#define  ESM_LFA        0x80
#define  ESM_FER        0x40
#define  ESM_CER        0x20
#define  ESM_AIS        0x10
#define  ESM_LOS        0x08
#define  ESM_CVE        0x04
#define  ESM_SLIP       0x02
#define  ESM_EBE        0x01


/* RSA4-8 (Receive Sa4-8-Bit Register)

-------------------- E1 ---------------------------*/

#define  RSA4_RS47       0x80
#define  RSA4_RS46       0x40
#define  RSA4_RS45       0x20
#define  RSA4_RS44       0x10
#define  RSA4_RS43       0x08
#define  RSA4_RS42       0x04
#define  RSA4_RS41       0x02
#define  RSA4_RS40       0x01

#define  RSA5_RS57       0x80
#define  RSA5_RS56       0x40
#define  RSA5_RS55       0x20
#define  RSA5_RS54       0x10
#define  RSA5_RS53       0x08
#define  RSA5_RS52       0x04
#define  RSA5_RS51       0x02
#define  RSA5_RS50       0x01

#define  RSA6_RS67       0x80
#define  RSA6_RS66       0x40
#define  RSA6_RS65       0x20
#define  RSA6_RS64       0x10
#define  RSA6_RS63       0x08
#define  RSA6_RS62       0x04
#define  RSA6_RS61       0x02
#define  RSA6_RS60       0x01

#define  RSA7_RS77       0x80
#define  RSA7_RS76       0x40
#define  RSA7_RS75       0x20
#define  RSA7_RS74       0x10
#define  RSA7_RS73       0x08
#define  RSA7_RS72       0x04
#define  RSA7_RS71       0x02
#define  RSA7_RS70       0x01

#define  RSA8_RS87       0x80
#define  RSA8_RS86       0x40
#define  RSA8_RS85       0x20
#define  RSA8_RS84       0x10
#define  RSA8_RS83       0x08
#define  RSA8_RS82       0x04
#define  RSA8_RS81       0x02
#define  RSA8_RS80       0x01


/* RSA6S  (Receive Sa6 Bit Status Register)

------------------------E1 & T1 ----------------------*/

#define  RSA6S_SX        0x20
#define  RSA6S_SF        0x10
#define  RSA6S_SE        0x08
#define  RSA6S_SC        0x04
#define  RSA6S_SA        0x02
#define  RSA6S_S8        0x01

/* RSP1   (Receive Signaling Pointer 1 )

------------------------ E1 & T1 ---------------------*/

#define RSP1_RS8C        0x80
#define RSP1_RS7C        0x40
#define RSP1_RS6C        0x20
#define RSP1_RS5C        0x10
#define RSP1_RS4C        0x08
#define RSP1_RS3C        0x04
#define RSP1_RS2C        0x02
#define RSP1_RS1C        0x01


/* RSP2   (Receive Signaling Pointer 2 )

------------------------ E1 & T1 ---------------------*/

#define RSP2_RS16C       0x80
#define RSP2_RS15C       0x40
#define RSP2_RS14C       0x20
#define RSP2_RS13C       0x10
#define RSP2_RS12C       0x08
#define RSP2_RS11C       0x04
#define RSP2_RS10C       0x02
#define RSP2_RS9C        0x01



/* RDL1-3  Recieve DL-Bit Register1-3)

------------------------ T1 -------------------------*/

#define  RDL1_RDL17      0x80
#define  RDL1_RDL16      0x40
#define  RDL1_RDL15      0x20
#define  RDL1_RDL14      0x10
#define  RDL1_RDL13      0x08
#define  RDL1_RDL12      0x04
#define  RDL1_RDL11      0x02
#define  RDL1_RDL10      0x01

#define  RDL2_RDL27      0x80
#define  RDL2_RDL26      0x40
#define  RDL2_RDL25      0x20
#define  RDL2_RDL24      0x10
#define  RDL2_RDL23      0x08
#define  RDL2_RDL22      0x04
#define  RDL2_RDL21      0x02
#define  RDL2_RDL20      0x01

#define  RDL3_RDL37      0x80
#define  RDL3_RDL36      0x40
#define  RDL3_RDL35      0x20
#define  RDL3_RDL34      0x10
#define  RDL3_RDL33      0x08
#define  RDL3_RDL32      0x04
#define  RDL3_RDL31      0x02
#define  RDL3_RDL30      0x01


/* SIS  (Signaling Status Register)

-------------------- E1 & T1 --------------------------*/

#define  SIS_XDOV        0x80
#define  SIS_XFW         0x40
#define  SIS_XREP        0x20
#define  SIS_IVB         0x10
#define  SIS_RLI         0x08
#define  SIS_CEC         0x04
#define  SIS_SFS         0x02
#define  SIS_BOM         0x01


/* RSIS  (Receive Signaling Status Register)

-------------------- E1 & T1 ---------------------------*/

#define  RSIS_VFR        0x80
#define  RSIS_RDO        0x40
#define  RSIS_CRC16      0x20
#define  RSIS_RAB        0x10
#define  RSIS_HA1        0x08
#define  RSIS_HA0        0x04
#define  RSIS_HFR        0x02
#define  RSIS_LA         0x01


/* RBCL/H  (Receive Byte Count Low/High)

------------------- E1 & T1 -----------------------*/

#define  RBCL_RBC7       0x80
#define  RBCL_RBC6       0x40
#define  RBCL_RBC5       0x20
#define  RBCL_RBC4       0x10
#define  RBCL_RBC3       0x08
#define  RBCL_RBC2       0x04
#define  RBCL_RBC1       0x02
#define  RBCL_RBC0       0x01

#define  RBCH_OV         0x10
#define  RBCH_RBC11      0x08
#define  RBCH_RBC10      0x04
#define  RBCH_RBC9       0x02
#define  RBCH_RBC8       0x01


/* ISR1-4  (Interrupt Status Register 1-4)

------------------ E1 & T1 ------------------------------*/

#define  ISR0_RME        0x80
#define  ISR0_RFS        0x40
#define  ISR0_T8MS       0x20
#define  ISR0_ISF        0x20
#define  ISR0_RMB        0x10
#define  ISR0_CASC       0x08
#define  ISR0_RSC        0x08
#define  ISR0_CRC6       0x04
#define  ISR0_CRC4       0x04   
#define  ISR0_SA6SC      0x02  
#define  ISR0_PDEN       0x02    
#define  ISR0_RPF        0x01

#define  ISR1_CASE       0x80
#define  ISR1_LLBSC      0x80
#define  ISR1_RDO        0x40
#define  ISR1_ALLS       0x20
#define  ISR1_XDU        0x10
#define  ISR1_XMB        0x08
#define  ISR1_SUEX       0x04
#define  ISR1_XLSC       0x02
#define  ISR1_XPR        0x01

#define  ISR2_FAR        0x80
#define  ISR2_LFA        0x40
#define  ISR2_MFAR       0x20
#define  ISR2_T400MS     0x10
#define  ISR2_LMFA       0x10
#define  ISR2_AIS        0x08
#define  ISR2_LOS        0x04
#define  ISR2_RAR        0x02
#define  ISR2_RA         0x01

#define  ISR3_ES         0x80
#define  ISR3_SEC        0x40
#define  ISR3_LMFA16     0x20
#define  ISR3_AIS16      0x10
#define  ISR3_RA16       0x08
#define  ISR3_LLBSC      0x08
#define  ISR3_RSN        0x02
#define  ISR3_RSP        0x01

#define  ISR4_XSP        0x80
#define  ISR4_XSN        0x40


/* GIS  (Global Interrupt Status Register)

--------------------- E1 & T1 ---------------------*/

#define  GIS_ISR4        0x10
#define  GIS_ISR3        0x08
#define  GIS_ISR2        0x04
#define  GIS_ISR1        0x02
#define  GIS_ISR0        0x01

/* CIS  (Channel Interrupt Status Register)

--------------------- E1 & T1 ---------------------*/
#define  CIS_PLLL        0x80
#define  CIS_GIS4        0x08
#define  CIS_GIS3        0x04
#define  CIS_GIS2        0x02
#define  CIS_GIS1        0x01

/* SIC1/2/3  (System Interface Control 1/2/3)

--------------------- E1 & T1 ---------------------*/

#define  SIC1_SSC1       0x80
#define  SIC1_SSD1       0x40
#define  SIC1_RBS1       0x20
#define  SIC1_RBS0       0x10
#define  SIC1_SSC0       0x08
#define  SIC1_BIM        0x04
#define  SIC1_XBS1       0x02
#define  SIC1_XBS0       0x01

#define  SIC2_FFS        0x80
#define  SIC2_SSF        0x40
#define  SIC2_CRB        0x20
#define  SIC2_SSC2       0x10
#define  SIC2_SICS2      0x08
#define  SIC2_SICS1      0x04
#define  SIC2_SICS0      0x02

#define  SIC3_CASMF      0x80
#define  SIC3_CMI        0x80
#define  SIC3_RESX       0x08
#define  SIC3_RESR       0x04
#define  SIC3_TTRF       0x02
#define  SIC3_DAF        0x01


/* CMR1/2  (Clock Mode Register 1/2)

--------------------- E1 & T1 ---------------------*/

#define  CMR1_DRSS1      0x80
#define  CMR1_DRSS0      0x40
#define  CMR1_RS1        0x20
#define  CMR1_RS0        0x10
#define  CMR1_DCS        0x08
#define  CMR1_STF        0x04
#define  CMR1_DXJA       0x02
#define  CMR1_DXSS       0x01

#define  CMR2_DCOXC      0x20
#define  CMR2_DCF        0x10
#define  CMR2_IRSP       0x08
#define  CMR2_IRSC       0x04
#define  CMR2_IXSP       0x02
#define  CMR2_IXSC       0x01


/* PC1/2/3/4  (Port Configuration 1/2/3/4)

--------------------- E1 & T1 ---------------------*/

#define  PC1_RPC12       0x40
#define  PC1_RPC11       0x20
#define  PC1_RPC10       0x10
#define  PC1_XPC13       0x08
#define  PC1_XPC12       0x04
#define  PC1_XPC11       0x02
#define  PC1_XPC10       0x01

#define  PC2_RPC22       PC1_RPC12
#define  PC2_RPC21       PC1_RPC11
#define  PC2_RPC20       PC1_RPC10
#define  PC2_XPC23       PC1_XPC13
#define  PC2_XPC22       PC1_XPC12
#define  PC2_XPC21       PC1_XPC11
#define  PC2_XPC20       PC1_XPC10

#define  PC3_RPC32       PC1_RPC12
#define  PC3_RPC31       PC1_RPC11
#define  PC3_RPC30       PC1_RPC10
#define  PC3_XPC33       PC1_XPC13
#define  PC3_XPC32       PC1_XPC12
#define  PC3_XPC31       PC1_XPC11
#define  PC3_XPC30       PC1_XPC10

#define  PC4_RPC42       PC1_RPC12
#define  PC4_RPC41       PC1_RPC11
#define  PC4_RPC40       PC1_RPC10
#define  PC4_XPC43       PC1_XPC13
#define  PC4_XPC42       PC1_XPC12
#define  PC4_XPC41       PC1_XPC11
#define  PC4_XPC40       PC1_XPC10




/* PC5  (Port Configuration 5)

--------------------- E1 & T1 ---------------------*/

#define  PC5_CXMFS      0x08
#define  PC5_CSXP       0x04
#define  PC5_CSRP       0x02
#define  PC5_CRP        0x01




/* CMDR2 (Command Register 2)

--------------------- E1 & T1 ---------------------*/

#define  CMDR2_RSUC      0x02
#define  CMDR2_XPPR      0x01

/* GIS  (Global Interrupt Status Register)

--------------------- E1 & T1 ---------------------*/

#define  GIS_ISR3        0x08
#define  GIS_ISR2        0x04
#define  GIS_ISR1        0x02
#define  GIS_ISR0        0x01

/* RBD  (Receive Buffer Delay (Read))

--------------------- E1 & T1 ---------------------*/

#define  RBD_RBD5        0x20
#define  RBD_RBD4        0x10
#define  RBD_RBD3        0x08
#define  RBD_RBD2        0x04
#define  RBD_RBD1        0x02
#define  RBD_RBD0        0x01


/* VSTR  (Version Status Register)

--------------------- E1 & T1 ---------------------*/

#define  VSTR_VN7          0x80
#define  VSTR_VN6          0x40
#define  VSTR_VN5          0x20
#define  VSTR_VN4          0x10
#define  VSTR_VN3          0x08
#define  VSTR_VN2          0x04
#define  VSTR_VN1          0x02
#define  VSTR_VN0          0x01


/* RES  (Receive Equalizer Status (Read)

---------------------- E1 & T1 ---------------------*/


#define  RES_EV1           0x80
#define  RES_EV0           0x40
#define  RES_RES4          0x10
#define  RES_RES3          0x08
#define  RES_RES2          0x04
#define  RES_RES1          0x02
#define  RES_RES0          0x01

///-
/*SA bit definition*/
#define SSM_SA4		4
#define SSM_SA5		5
#define SSM_SA6		6
#define SSM_SA7		7
#define SSM_SA8		8


#endif /* _PEF22554_REG_H */

#ifdef __cplusplus
}
#endif

