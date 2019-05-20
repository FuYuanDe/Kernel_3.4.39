/*******************************************************************************
 * Copyright ? 2016-2018, spark
 *		Author: sparkr 
 *
 * This file is provided under a dual BSD/GPL license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 ********************************************************************************/

#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/crypto.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/types.h>

#include <net/ip.h>
#include <linux/kthread.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/udp.h>
#include <net/route.h>

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)

///-
//#define DISPATCH_UDP_PORT_MAX      5000
#define DISPATCH_UDP_PORT_MAX       65536

/// 分发信息
struct dispatch_info {
	int default_cpu;
/// 8192	
	unsigned long udp_bitmap[DISPATCH_UDP_PORT_MAX/sizeof(unsigned long)];
	__u16 udp_offset;				// udp ?????? 
};
/// 分发信息组
/// 静态全局变量
static struct dispatch_info g_dispatch_info;
static __u8 g_port_cpu[DISPATCH_UDP_PORT_MAX];		// udp port 对应 cpu

//??????????cpu??
int dispatch_match(struct sk_buff *skb)
{
	struct dispatch_info * info = &g_dispatch_info;
	struct iphdr *iphdr;
	struct udphdr *uh;
	__u16 udp_dest;

	// ip v4
	if(eth_hdr(skb)->h_proto == htons(ETH_P_IP))
	{
		iphdr = (struct iphdr *)((sk_buff_data_t)eth_hdr(skb)+sizeof(struct ethhdr));
		// udp
		if(iphdr->protocol == IPPROTO_UDP)
		{
			// port match
			uh = (struct udphdr *)((sk_buff_data_t)iphdr+sizeof(struct iphdr));
			udp_dest = ntohs(uh->dest);
			if(udp_dest >= info->udp_offset)
			{
				udp_dest = udp_dest - info->udp_offset;
///-
#if 1
				if((udp_dest < DISPATCH_UDP_PORT_MAX)
					&& test_bit(udp_dest%sizeof(unsigned long),&info->udp_bitmap[udp_dest/sizeof(unsigned long)]))
#else
                if((udp_dest < DISPATCH_UDP_PORT_MAX))
#endif
				{
					//????????Χ???cpu
					//printk(KERN_ERR"dinstar dispatch matched %d\n",udp_dest);
					return g_port_cpu[udp_dest];
				}
				else
				{
					return info->default_cpu;
				}
			}
			else
			{
				return info->default_cpu;
			}	
		}
		else
		{
			return info->default_cpu;
		}
	}
	else
	{
		return info->default_cpu;
	}
	
}

///  u16=65536
/// 注意返回值，
/// 
int dinstar_dispatch_register(__u16 dstport,__u8 cpu)
{
	__u16 tmp;

	if((dstport >= g_dispatch_info.udp_offset) && (dstport < (g_dispatch_info.udp_offset +DISPATCH_UDP_PORT_MAX)))
	{
		tmp = dstport - g_dispatch_info.udp_offset;
		g_port_cpu[tmp] = cpu;
/// 内核函数，set_bit，将一个指针的第n位置一		
		set_bit((tmp %sizeof(unsigned long)), &g_dispatch_info.udp_bitmap[tmp/sizeof(unsigned long)]);	
		printk(KERN_ERR"dinstar_dispatch_register dstport %d\n",dstport);
		return 1;
	}
	else
	{
		printk(KERN_ERR"dinstar_dispatch_register dstport %d error\n",dstport);
		return -1;
	}
}
EXPORT_SYMBOL(dinstar_dispatch_register);

/// 分发注销函数，作用和注册函数相反
int dinstar_dispatch_unregister(__u16 dstport)
{
	__u16 tmp;
	
	if((dstport >= g_dispatch_info.udp_offset) && (dstport < (g_dispatch_info.udp_offset + DISPATCH_UDP_PORT_MAX)))
	{
		tmp = dstport - g_dispatch_info.udp_offset;
		clear_bit((tmp %sizeof(unsigned long)), &g_dispatch_info.udp_bitmap[tmp/sizeof(unsigned long)]);
		g_port_cpu[tmp] = g_dispatch_info.default_cpu;
		return 1;
	}
	else
	{
		return -1;
	}
}
EXPORT_SYMBOL(dinstar_dispatch_unregister);

/// 设置基类型
/// 应该和注册和注销函数一起使用
int dinstar_dispatch_setbase(__u16 dstport)
{	
	g_dispatch_info.udp_offset = dstport;
}
EXPORT_SYMBOL(dinstar_dispatch_setbase);



int  dispatch_init(void)
{
	int ret = 1;
	int i, j;

	memset(&g_dispatch_info,0,sizeof(struct dispatch_info));
///-
    /*Ĭ�����ж˿ڶ����д���*/
    memset(g_dispatch_info.udp_bitmap, 0xff, sizeof(g_dispatch_info.udp_bitmap));
	g_dispatch_info.udp_offset = 0;
	g_dispatch_info.default_cpu = 1;
///-
#if 0
	for(i=0;i<DISPATCH_UDP_PORT_MAX;i++)
	{
		g_port_cpu[i] = g_dispatch_info.default_cpu;
	}
#else
#if 0
    /*CPU0����ѹ�����󣬷���1/4�Ľ��յ�cpu0�ϣ�cpu1�е�3/4�Ľ���*/
    for(i=0;i<DISPATCH_UDP_PORT_MAX;i+=8)
    {
        for(j=i; j<(i+6); j++)
        {
            g_port_cpu[j] = g_dispatch_info.default_cpu;
        }
        g_port_cpu[i+6] = 0;
        g_port_cpu[i+7] = 0;
    }
#else
    for(i=0;i<DISPATCH_UDP_PORT_MAX;i+=4)
    {
#if defined(PRODUCT_SBC1000USER) || defined(PRODUCT_SBCUSER)
        g_port_cpu[i] = 0;
        g_port_cpu[i+1] = 0;
        g_port_cpu[i+2] = 1;
        g_port_cpu[i+3] = 1;
#else
        g_port_cpu[i] = 1;
        g_port_cpu[i+1] = 1;
        g_port_cpu[i+2] = 2;
        g_port_cpu[i+3] = 2;
#endif
    }
#endif
#endif
	
	return ret;
}

void  dispatch_exit(void)
{
	return;
}


#endif
