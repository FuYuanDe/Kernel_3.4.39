#ifndef _RTK_TYPES_H_
#define _RTK_TYPES_H_

typedef unsigned long long    uint64;
typedef long long				      int64;
typedef unsigned int			    uint32;
typedef int                   int32;
typedef unsigned short        uint16;
typedef short                 int16;
typedef unsigned char         uint8;
typedef char                  int8;

typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
typedef unsigned int U32;
typedef unsigned short U16;
typedef unsigned char U8;

#define CONST_T               const

typedef uint32               ipaddr_t;
typedef uint32				 memaddr;	

#ifndef ETHER_ADDR_LEN
#define ETHER_ADDR_LEN		6
#endif

typedef struct ether_addr_s {
	uint8 octet[ETHER_ADDR_LEN];
} ether_addr_t;

#define swapl32(x)\
        ((((x) & 0xff000000U) >> 24) | \
         (((x) & 0x00ff0000U) >>  8) | \
         (((x) & 0x0000ff00U) <<  8) | \
         (((x) & 0x000000ffU) << 24))
#define swaps16(x)        \
        ((((x) & 0xff00) >> 8) | \
         (((x) & 0x00ff) << 8))  

#define MEM16(x)        (swaps16(x)) 

typedef int32                   rtk_api_ret_t;
typedef int32                   ret_t;
typedef uint64                  rtk_u_long_t;

#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef SUCCESS
#define SUCCESS 0
#endif

#ifndef FAILED
#define FAILED -1
#endif

#endif
