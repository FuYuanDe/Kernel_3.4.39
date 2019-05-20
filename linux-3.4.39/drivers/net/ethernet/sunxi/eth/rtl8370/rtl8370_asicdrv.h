#ifndef _RTL8370_ASICDRV_H_
#define _RTL8370_ASICDRV_H_

#include <linux/types.h>
#include "rtk_types.h"
#include "rtk_error.h"
#include "rtl8370_reg.h"
#include "rtl8370_base.h"

#define _LITTLE_ENDIAN  1

#define DATA_LEN        32

#define MDIO_8370_GROUP0            0

#define RTL8370_REGBITLENGTH               16
#define RTL8370_REGDATAMAX                 0xFFFF

#define RTL8370_VIDMAX                     0xFFF
#define RTL8370_EVIDMAX                    0x1FFF
#define RTL8370_CVLANMCNO                  32
#define RTL8370_CVIDXMAX                   (RTL8370_CVLANMCNO-1)

#define RTL8370_PRIMAX                     7

#define RTL8370_PRIDECMAX                  0xFF

#define RTL8370_PORTNO                     16   
#define RTL8370_PORTIDMAX                  (RTL8370_PORTNO-1)
#define RTL8370_PMSKMAX                    ((1<<(RTL8370_PORTNO))-1) 
#define RTL8370_PORTMASK                   0xFFFF

#define RTL8370_SVIDXNO                    64
#define RTL8370_SVIDXMAX                   (RTL8370_SVIDXNO-1)
#define RTL8370_MSTIMAX                    15  

#define RTL8370_METERNO                    64
#define RTL8370_METERMAX                   (RTL8370_METERNO-1)

#define RTL8370_QUEUENO                    8
#define RTL8370_QIDMAX                     (RTL8370_QUEUENO-1)       


#define RTL8370_PHY_BUSY_CHECK_COUNTER     1000    
#define RTL8370_PHYNO                      8 
#define RTL8370_PHYIDMAX                  (RTL8370_PHYNO-1)

#define RTL8370_QOS_GRANULARTY_MAX         0x1FFFF
#define RTL8370_QOS_GRANULARTY_LSB_MASK    0xFFFF
#define RTL8370_QOS_GRANULARTY_LSB_OFFSET  0
#define RTL8370_QOS_GRANULARTY_MSB_MASK    0x10000
#define RTL8370_QOS_GRANULARTY_MSB_OFFSET  16

#define RTL8370_QOS_GRANULARTY_UNIT_KBPS   8

#define RTL8370_QOS_RATE_INPUT_MAX         (0x1FFFF * 8)
#define RTL8370_QOS_RATE_INPUT_MIN         8

#define RTL8370_QUEUE_MASK                 0xFF

#define RTL8370_EFIDMAX                    0x7
#define RTL8370_FIDMAX                     0xFFF

/* the above macro is generated by genDotH */
#define RTL8370_VALID_REG_NO               3236

/*=======================================================================
 *  Enum
 *========================================================================*/
enum RTL8370_TABLE_ACCESS_OP
{
    TB_OP_READ = 0,
    TB_OP_WRITE
};

enum RTL8370_TABLE_ACCESS_TARGET
{
    TB_TARGET_ACLRULE = 1,
    TB_TARGET_ACLACT,
    TB_TARGET_CVLAN,
    TB_TARGET_L2
};

#define RTL8370_TABLE_ACCESS_REG_DATA(op, target)    ((op << 4) | target)

/*=======================================================================
 *  Structures
 *========================================================================*/


typedef struct   smi_ether_addr_s{
    uint16  mac0:8;
    uint16  mac1:8;
    uint16  mac2:8;
    uint16  mac3:8;
    uint16  mac4:8;
    uint16  mac5:8;

}smi_ether_addr_t;

struct reg_save_t
{
    uint16 reg;
    uint16 val;
};

#define CONST_T const

int simple_phy_read(u16 reg, u32 *ret);
int simple_phy_write(u16 reg, u16 data);

u16 geth_phy_read(int phy_adr, u16 reg);
void geth_phy_write(u8 phy_adr, u8 reg, u16 data);

extern ret_t rtl8370_setAsicRegBit(uint32 reg, uint32 bitNum, uint32 value);
extern ret_t rtl8370_getAsicRegBit(uint32 reg, uint32 bitNum, uint32 *value);

extern ret_t rtl8370_setAsicRegBits(uint32 reg, uint32 bits, uint32 value);
extern ret_t rtl8370_getAsicRegBits(uint32 reg, uint32 bits, uint32 *value);

extern ret_t rtl8370_setAsicReg(uint32 reg, uint32 value);
extern ret_t rtl8370_getAsicReg(uint32 reg, uint32 *value);

#endif /*#ifndef _RTL8370_ASICDRV_H_*/
