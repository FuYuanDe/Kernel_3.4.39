
#ifndef __SMI_H__
#define __SMI_H__

#include "rtk_types.h"
#include "rtk_error.h"

#define OUTPUT          0
#define INPUT           1

#define DELAY                        100

#define MDC_MDIO_DUMMY_ID           0
#define MDC_MDIO_CTRL0_REG          31
#define MDC_MDIO_START_REG          29
#define MDC_MDIO_CTRL1_REG          21
#define MDC_MDIO_ADDRESS_REG        23
#define MDC_MDIO_DATA_WRITE_REG     24
#define MDC_MDIO_DATA_READ_REG      25
#define MDC_MDIO_PREAMBLE_LEN       32

#define MDC_MDIO_START_OP          0xFFFF
#define MDC_MDIO_ADDR_OP           0x000E
#define MDC_MDIO_READ_OP           0x0001
#define MDC_MDIO_WRITE_OP          0x0003

#define ack_timer                   500

#define CLK_DURATION(clk)            { int i; for(i=0; i<clk; i++); }

#define smi_SCK         22
#define smi_SDA         23

#endif /* __SMI_H__ */


