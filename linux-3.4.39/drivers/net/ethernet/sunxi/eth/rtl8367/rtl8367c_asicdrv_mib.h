#ifndef _RTL8367C_ASICDRV_MIB_H_
#define _RTL8367C_ASICDRV_MIB_H_

#include "rtl8367c_asicdrv.h"

#define RTL8367C_MIB_PORT_OFFSET                (0x7C)
#define RTL8367C_MIB_LEARNENTRYDISCARD_OFFSET   (0x420)

#define RTL8367C_MAX_LOG_CNT_NUM                (32)
#define RTL8367C_MIB_MAX_LOG_CNT_IDX            (RTL8367C_MAX_LOG_CNT_NUM - 1)
#define RTL8367C_MIB_LOG_CNT_OFFSET             (0x3E0)
#define RTL8367C_MIB_MAX_LOG_MODE_IDX           (16-1)

typedef enum RTL8367C_MIBCOUNTER_E{

    /* RX */
	ifInOctets = 0,

	dot3StatsFCSErrors, // 1
	dot3StatsSymbolErrors, // 2
	dot3InPauseFrames, // 3
	dot3ControlInUnknownOpcodes, // 4

	etherStatsFragments, // 5
	etherStatsJabbers, // 6
	ifInUcastPkts, // 7
	etherStatsDropEvents, // 8

    ifInMulticastPkts, // 9
    ifInBroadcastPkts, // 10
    inMldChecksumError, // 11
    inIgmpChecksumError, // 12
    inMldSpecificQuery, // 13
    inMldGeneralQuery, // 14
    inIgmpSpecificQuery, // 15
    inIgmpGeneralQuery, // 16
    inMldLeaves, // 17
    inIgmpLeaves, // 18

    /* TX/RX */
	etherStatsOctets, // 19

	etherStatsUnderSizePkts, // 20
	etherOversizeStats, // 21
	etherStatsPkts64Octets, // 22
	etherStatsPkts65to127Octets, // 23
	etherStatsPkts128to255Octets, // 24
	etherStatsPkts256to511Octets, // 25
	etherStatsPkts512to1023Octets, // 26
	etherStatsPkts1024to1518Octets, // 27

    /* TX */
	ifOutOctets, // 28

	dot3StatsSingleCollisionFrames, // 29
	dot3StatMultipleCollisionFrames, // 30
	dot3sDeferredTransmissions, // 31
	dot3StatsLateCollisions, // 32
	etherStatsCollisions, // 33
	dot3StatsExcessiveCollisions, // 34
	dot3OutPauseFrames, // 35
    ifOutDiscards, // 36

    /* ALE */
	dot1dTpPortInDiscards, // 37
	ifOutUcastPkts, // 38
	ifOutMulticastPkts, // 39
	ifOutBroadcastPkts, // 40
	outOampduPkts, // 41
	inOampduPkts, // 42

    inIgmpJoinsSuccess, // 43
    inIgmpJoinsFail, // 44
    inMldJoinsSuccess, // 45
    inMldJoinsFail, // 46
    inReportSuppressionDrop, // 47
    inLeaveSuppressionDrop, // 48
    outIgmpReports, // 49
    outIgmpLeaves, // 50
    outIgmpGeneralQuery, // 51
    outIgmpSpecificQuery, // 52
    outMldReports, // 53
    outMldLeaves, // 54
    outMldGeneralQuery, // 55
    outMldSpecificQuery, // 56
    inKnownMulticastPkts, // 57

	/*Device only */
	dot1dTpLearnedEntryDiscards, // 58
	RTL8367C_MIBS_NUMBER, // 59

}RTL8367C_MIBCOUNTER;


extern ret_t rtl8367c_setAsicMIBsCounterReset(rtk_uint32 greset, rtk_uint32 qmreset, rtk_uint32 pmask);
extern ret_t rtl8367c_getAsicMIBsCounter(rtk_uint32 port,RTL8367C_MIBCOUNTER mibIdx, rtk_uint64* pCounter);
extern ret_t rtl8367c_getAsicMIBsLogCounter(rtk_uint32 index, rtk_uint32 *pCounter);
extern ret_t rtl8367c_getAsicMIBsControl(rtk_uint32* pMask);

extern ret_t rtl8367c_setAsicMIBsResetValue(rtk_uint32 value);
extern ret_t rtl8367c_getAsicMIBsResetValue(rtk_uint32* value);

extern ret_t rtl8367c_setAsicMIBsUsageMode(rtk_uint32 mode);
extern ret_t rtl8367c_getAsicMIBsUsageMode(rtk_uint32* pMode);
extern ret_t rtl8367c_setAsicMIBsTimer(rtk_uint32 timer);
extern ret_t rtl8367c_getAsicMIBsTimer(rtk_uint32* pTimer);
extern ret_t rtl8367c_setAsicMIBsLoggingMode(rtk_uint32 index, rtk_uint32 mode);
extern ret_t rtl8367c_getAsicMIBsLoggingMode(rtk_uint32 index, rtk_uint32* pMode);
extern ret_t rtl8367c_setAsicMIBsLoggingType(rtk_uint32 index, rtk_uint32 type);
extern ret_t rtl8367c_getAsicMIBsLoggingType(rtk_uint32 index, rtk_uint32* pType);
extern ret_t rtl8367c_setAsicMIBsResetLoggingCounter(rtk_uint32 index);
extern ret_t rtl8367c_setAsicMIBsLength(rtk_uint32 txLengthMode, rtk_uint32 rxLengthMode);
extern ret_t rtl8367c_getAsicMIBsLength(rtk_uint32 *pTxLengthMode, rtk_uint32 *pRxLengthMode);

#endif /*#ifndef _RTL8367C_ASICDRV_MIB_H_*/

