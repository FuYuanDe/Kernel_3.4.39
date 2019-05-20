#ifndef _RA_SW_H__
#define _RA_SW_H__

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
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <mach/hardware.h>
#include <mach/sun8i/platform-sun8iw6p1.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <linux/swconfig.h>
//#include <linux/switch.h>



#include "rtk_types.h"
#include "vlan.h"

#include "rtk_error.h"
#include "rtl8367c_reg.h"
#include "rtl8367c_asicdrv.h"
#include "rtl8367c_asicdrv_phy.h"
#include "rtl8367c_asicdrv_port.h"
#include "rtl8367c_asicdrv_led.h"
#include "port.h"
#include "rtk_switch.h"
#include "cpu.h"
#include "led.h"

#include "smi.h"
#include "stat.h"
#include "acl.h"
#include "rtk_acl.h"

#define max_sw_ports 5
#define cpu_sw_ports 5

#define MAX_SW_VLANS 16
/* port num */
#define PORT0                                       0
#define PORT1                                       1
#define PORT2                                       2
#define PORT3                                       3
#define PORT4                                       4


/*表示一个端口*/
typedef struct {
    u16 pvid;			//所属VLAN号
    u16 tagged;
	u16 change;			//用户是否对port进行定义

	struct switch_port_link link;
} ra_sw_port_entry;
/*表示一个VLAN*/
typedef struct {
    u16 ports;				//该VLAN所包含的端口
    u16 untaggeds;			//该VLAN所包含的端口
    u16 vid;				//VLAN ID
    u16 valid;
} ra_sw_vlan_entry;

typedef struct ra_sw_reg {
	u16 p;  // phy
	u16 m;  // mii
} reg;


/*全局表示一个switch设备*/
struct ra_sw_state {					
	struct switch_dev dev;
	ra_sw_port_entry ports[max_sw_ports];
	ra_sw_vlan_entry vlans[MAX_SW_VLANS];

	reg  proc_mii;
 	
	char buf[80];
};

int ra_sw_probe(void);




#endif










































