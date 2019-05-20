#ifndef _RTL8367_H_
#define _RTL8367_H_

#define RTK_MAX_SLOT            6

#define RTK_MAX_PROT            17
#define RTK_MIN_PROT            0

#define MIRROR_DIREXT_RX     0
#define MIRROR_DIREXT_TX     1
#define MIRROR_DIREXT_BOTH     2
#define MIRROR_DIREXT_NONE 3
typedef struct rtk_mirror_port
{
    int mirror_src_port;
    int mirror_dst_port; 
    int mirror_direct;
    
}rtk_mirror_port_t;

//int rtk8367_mirror_add2cpu(rtk_mirror_port_t *user_mirrorport);
//int rtk8367_mirror_del2cpu(rtk_mirror_port_t *user_mirrorport);
#endif
