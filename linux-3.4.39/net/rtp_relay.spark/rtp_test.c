
 /******************************************************************************
        (c) COPYRIGHT 2016- by Dinstar technologies Co.,Ltd
                          All rights reserved.
File:relay_core.c
Desc: relay流水线模式处理core

Modification history(no, author, date, desc)
spark 16-11-22create file
******************************************************************************/

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/netfilter.h>
#include <linux/spinlock.h>
#include <linux/netlink.h>
#include <linux/net.h>
#include <linux/netfilter_ipv4.h>
#include <net/sock.h>
#include <net/flow.h>
#include <net/dn.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <net/udp.h>
#include <net/route.h>
#include <linux/if_ether.h>
#include "relay.h"

#define MAX_RTP_SAMPLE  32
#define MAX_RTP_SIZE  512
#define 20_MS 20
#define SERVER_PORT 20020
#define SERVER_IP "10.251.1.2"

enum CODEC_TYPE{
          G711A_20MS = 0,
          G711A_30MS ,
          G711A_60MS ,
          G729_20MS ,
          G729_30MS ,
          G729_60MS, 
          CODEC_TYPE_BUTT
};

	
struct Rtp_sample_location{
	enum CODEC_TYPE  codec_type;
	char filename[256];
};

struct Rtp_sample_location rtp_data[MAX_RTP_SAMPLE]=
{
    	{G711A_20MS,"/tmp/rtp_data/g11a_20ms.dat"},
    	{G711A_30MS,"/tmp/rtp_data/g11a_30ms.dat"},
    	{G711A_60MS,"/tmp/rtp_data/g11a_60ms.dat"},
    	{G729_20MS,"/tmp/rtp_data/g29_20ms.dat"},
    	{G729_30MS,"/tmp/rtp_data/g29_30ms.dat"},
    	{G729_60MS,"/tmp/rtp_data/g29_60ms.dat"}
};

char* rtp_packet[MAX_RTP_SAMPLE];
U32 rtp_send_count[MAX_RTP_SAMPLE];
U32 rtp_receive_count[MAX_RTP_SAMPLE];
U32 rtp_sending_timer[MAX_RTP_SAMPLE];

U8* rtp_g711a = {0x80, 0x08, 0xd0, 0x4f, 0x5f, 0xfd, 0x58, 0xc5, 0x89, 0xdb, 0x9a, 0x5e, 0xd5, 0xd7, 0xd3, 0xd2,
	                    0xdd, 0xdf, 0xd8, 0xd9, 0xd8, 0xd9, 0xd2, 0xd3, 0xd2, 0xd3, 0xd3, 0xdc, 0xdd, 0xdd, 0xd1, 0xd1,
	                    0xd6, 0xd7, 0xd3, 0xd4, 0x54, 0x57, 0x51, 0x57, 0xd4, 0x55, 0xd1, 0xd2, 0xd0, 0xd3, 0xd1, 0xd4, 
	                    0xd4, 0xd4, 0xd2, 0xdd, 0xd1, 0xd7, 0xd4, 0xd4, 0xd1, 0xdc, 0xd3, 0xd6, 0x55, 0x56, 0x56, 0x50,
	                    0x50, 0x51, 0x5d, 0x50, 0x51, 0x50, 0x51, 0x51, 0x50, 0x55, 0xd4, 0xd5, 0xd3, 0xd2, 0xd2, 0xdd,
	                    0xd3. 0xd0, 0xd9, 0xde, 0xdf, 0xdc, 0xd1, 0xd3, 0xd0, 0xd0, 0xd3, 0xd1, 0x57, 0x51, 0x50, 0x56,
	                    0x51, 0x52, 0x5d, 0x5d, 0x5c, 0x5c, 0x5d, 0x5c, 0x55, 0xd4, 0xd6, 0xd2, 0xd1, 0xd1, 0xd4, 0xd3,
	                    0xd7, 0x54, 0x55, 0xd7, 0xd1, 0xd7, 0xd1, 0xd5, 0xd4, 0x55, 0x53, 0x50, 0x55, 0x56, 0x55, 0xd4,
	                    0xd6, 0xd7, 0xd5, 0x54, 0xd7, 0xd1, 0xd3, 0xde, 0xdd, 0xd2, 0xd2, 0xd6, 0x55, 0xd5, 0x57, 0x53,
	                    0x51, 0x57, 0x56, 0x50, 0x52, 0x5d, 0x51, 0x50, 0x5d, 0x51, 0x54, 0x50, 0x57, 0x55, 0x50, 0xd4,
	                    0xd1, 0xd0, 0xdc, 0xd3, 0xd0, 0xd2, 0xdd, 0xd6, 0xd4, 0xd7, 0xd7, 0xd1};

int socket_client;

/*module init*/
void rtp_test_init()
{
    int i;
    struct sockaddr_in servaddr;
	
   /*Init the global data*/
   for(i=0;i<MAX_RTP_SAMPLE;i++){
   	rtp_packet[i]=NULL;
	rtp_send_count[i]=0
	rtp_receive_count[i]=0;
	rtp_sending_timer[i]=20_MS;
   	}
   
   /*Create socket to target*/
   socket_client = socket(AF_INET,SOCK_STREAM,0);
   menset(&servaddr,0,sizeof(servaddr));
   servaddr.sin_family = AF_INET;
   servaddr.sin_port = htons(SERVER_PORT);
   servaddr.sin_addr.s_addr = inet_addr(SERVER_IP);

   if(connet(socket_client, (struct sockaddr*)servaddr,sizeof(servaddr))<0){
   	perror("connet to server error!");
	exit(1);
   	 
   
   
   /*Create sending timer*/
   
}

/* load rtp packet data from file to skbuffer */
bool rtp_test_load(U32 codec_type, U8* rtp_packet)
{
   /*Load rtp data from file*/


}

/*send rtp packet to target system; to fill the 5-tuple etc*/
bool  rtp_test_send(U8* rtp_packet)
{


    /*Fill the rtp header and send the rtp packet, timestamp += 8*sending timer.*/
    

     /*Sent packet count*/


}

/*receive rtp packet from target system; to check if the packet is correct and count the received packets */
bool  rtp_test_receive(U8* rtp_packet)
{

   /*Check the received packet*/


   /*Received packet count*/


}


/*handle the process messages;*/
void rtp_test_main()
{

   /*Process message hander:
      1.Sending timeout to send the rtp packet
      2.Received packet to count
      3.Report the static*/
      

}





