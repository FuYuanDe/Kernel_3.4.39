/*****************************************************************************
* Function: fpga_drv.h
* Copyright: Copyright @2014 Dinstar, Inc, All right reserved.
* Author: Tom
*
* Desc:  逻辑程序模块驱动程序头文件，主要实现向上提供接口
******************************************************************************/
#ifndef _FPGA_DRV_H__
#define _FPGA_DRV_H__

/* 逻辑程序文件大小 */
#define CONFIGURATION_SIZE  (512*1024)
/* FPGA 数目 */
#define FPGA_DEV_NUM   1

/* 返回状态 */
#define FAILED        -1
#define SUCCESS        0

/* 高低电平 */
#define Bit_SET        1
#define Bit_RESET      0

struct fpga_mpp
{
	u8 NRE_WE;      //0
    u8 NCS;

    u8 ADDR0;
    u8 ADDR1;    
    u8 ADDR2;       //4
    u8 ADDR3;
    u8 DATA0;
    u8 DATA1;    
    u8 DATA2;       //8
    u8 DATA3;
    u8 DATA4;
    
    u8 SPI_CLK;
    u8 SPI_MOSI;    //12
    u8 SPI_CS;
    u8 SPI_CRESET;
    u8 SPI_CDONE;
};

/* 加载逻辑程序参数 */
struct load_param_s
{
	int len; //文件长度
	u8 *addr; //应用层内存地址
};

/* local bus 读写参数 */
struct local_bus_param_s
{
	u32 addr; //读写地址
	u32 data; // 写入参数或者返回值
};

/* ioctl 接口相关的定义 */
#define FPGA_IOC_MAGIC  'F'
#define FPGA_PROGRAM_LOAD     _IOWR(FPGA_IOC_MAGIC,  0, struct load_param_s)
#define FPGA_LOCAL_BUS_WRITE  _IOWR(FPGA_IOC_MAGIC,  1, struct local_bus_param_s)
#define FPGA_LOCAL_BUS_READ   _IOWR(FPGA_IOC_MAGIC,  2, struct local_bus_param_s)
//#define FPGA_GPIO_WRITE       _IOWR(FPGA_IOC_MAGIC,  3, struct local_bus_param_s) 不用废弃

/* 读写CPU 寄存器位操作 */
#define CPU_GPIO_READ         _IOWR(FPGA_IOC_MAGIC,  4, struct local_bus_param_s)
#define CPU_GPIO_WRITE        _IOWR(FPGA_IOC_MAGIC,  5, struct local_bus_param_s)

/* 读写CPU 寄存器操作 */
#define MV_READ_REGISTER      _IOWR(FPGA_IOC_MAGIC,  6, struct local_bus_param_s)
#define MV_WRITE_REGISTER     _IOWR(FPGA_IOC_MAGIC,  7, struct local_bus_param_s)

/**读写心跳gpio*/
#define HEAT_GPIO_START       _IOWR(FPGA_IOC_MAGIC,  8, unsigned int)
#define HEAT_GPIO_STOP       _IOWR(FPGA_IOC_MAGIC,  9, unsigned int)
#define HEAT_GPIO_SET       _IOWR(FPGA_IOC_MAGIC,  10, unsigned int)

typedef enum
{
    LOW = 0,
    HIGH,
} LOGIC_STATE_E;

int  FPGA_PreLoad(char *addr, unsigned int ulFileLen);
void FPGA_Test(void);

#endif
