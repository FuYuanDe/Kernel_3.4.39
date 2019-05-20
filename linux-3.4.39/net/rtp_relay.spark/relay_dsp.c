 /******************************************************************************
        (c) COPYRIGHT 2016- by Dinstar technologies Co.,Ltd
                          All rights reserved.
File:relay_qos.c
Desc: qos 处理考虑网络抖动延时，暂定qos处理时间为1s

Modification history(no, author, date, desc)
spark 16-11-29create file
******************************************************************************/
#include <linux/sysctl.h>
#include <linux/errno.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/net.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include "relay.h"



int __init relay_dsp_init()
{
	return 1;
}

void __exit relay_dsp_exit()
{
}

