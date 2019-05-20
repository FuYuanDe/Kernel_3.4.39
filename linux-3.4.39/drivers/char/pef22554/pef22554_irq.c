/******************************************************************************
        (c) COPYRIGHT 2002-2003 by Shenzhen Allywll Information Co.,Ltd
                          All rights reserved.
File: pef22554.c
Desc:the source file of user config
Modification history(no, author, date, desc)
1.Holy 2003-04-02 create file
2.luke 2006-04-20 for AOS
******************************************************************************/
//#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>

#include "pef22554_drv.h"
#include "pef22554_reg.h"

extern struct quadFALC_dev QuadFALCDev;
extern ST_E1_RECEIVE_DATA  stE1BufferRec[MAX_PEF22554_NUM*NUM_OF_FALC];
extern u8 casrs[MAX_PEF22554_NUM][NUM_OF_FALC][16];
extern u8 casxs[MAX_PEF22554_NUM][NUM_OF_FALC][16];
extern volatile int sendflag[MAX_PEF22554_NUM*NUM_OF_FALC];
extern QFALC_REG_MAP *ptrreg[MAX_PEF22554_NUM*NUM_OF_FALC];

/*------------------------------------------------------------------------
  ����:  RPF�жϴ���

  �������:
                falc - �Ĵ����ṹ��ָ��
                bufindex - e1�ӿ����

  �������:
                ��

  ����:

  ����ֵ:
                 0:�ɹ�
                 -PACKET_TOOBIG:����

  ����:
-------------------------------------------------------------------------*/
static inline int quadFALC_irq_handle_RPF(QFALC_REG_MAP  *falc, int bufindex)
{
    u32 k;
    char *mem_ptr = NULL;
    int chipno = bufindex/NUM_OF_FALC, e1no = bufindex%NUM_OF_FALC;

    //printk(KERN_WARNING "%d receive e1 data:rpf\n", bufindex);
    //RPF Receive Pool Full
    //RFIFO�н��յ�32 �ֽڣ�����Ϣû�н������
    QuadFALCDev.pef22554info[chipno].rpfcnt[e1no]++;
    /*�������Ϊһ������Ϣ�Ŀ�ʼ�����û�����״̬ΪE1_RX_BUFF_CLEAN*/
    if( stE1BufferRec[bufindex].enBuffSta == E1_RX_BUFF_CLEAN )
    {
        stE1BufferRec[bufindex].ptSend = 0;  //payload type,chan number,slot number��
        stE1BufferRec[bufindex].enBuffSta = E1_RX_BUFF_SETTLE;
        memset(stE1BufferRec[bufindex].ucBuffer, 0, sizeof(stE1BufferRec[bufindex].ucBuffer));
        mem_ptr = stE1BufferRec[bufindex].ucBuffer;
    }
    else
    {
        /*��¼���յ����ֽ���*/
        mem_ptr = (char *)stE1BufferRec[bufindex].ucBuffer;

        if( stE1BufferRec[bufindex].ptSend >= DATA_LEN_LIMIT )
        {
            QuadFALCDev.pef22554info[chipno].recvfrmerr[e1no]++;
            //����̫��ֱ�Ӷ���
            return -PACKET_TOOBIG;
        }
    }

    for( k=0;k<MAX_SEND_LEN;k++)
    {
        MEM_READ8((u8 *)&falc->rd.rfifo, &mem_ptr[stE1BufferRec[bufindex].ptSend++]);
    }
    //mb();
    SET_BIT8(&falc->wr.cmdr,  CMDR_RMC);

    return 0;
}

/*------------------------------------------------------------------------
  ����:  RME�жϴ���

  �������:
                falc - �Ĵ����ṹ��ָ��
                bufindex - e1�ӿ����

  �������:
                ��

  ����:

  ����ֵ:
                 0:�ɹ�
                 -PACKET_TOOBIG:����

  ����:
-------------------------------------------------------------------------*/
static inline int quadFALC_irq_handle_RME(QFALC_REG_MAP  *falc, int bufindex)
{
    u32 k;
    int ret;
    u8 regRbcl,regRbch;
    char *mem_ptr = NULL;
    int chipno = bufindex/NUM_OF_FALC, e1no = bufindex%NUM_OF_FALC;

    //��Ϣ�������
    printk(KERN_ERR "RME\n");
    
    QuadFALCDev.pef22554info[chipno].rmecnt[e1no]++;
    /*�������Ϊһ������Ϣ�Ŀ�ʼ�����û�����״̬ΪE1_RX_BUFF_CLEAN*/
    if( stE1BufferRec[bufindex].enBuffSta == E1_RX_BUFF_CLEAN )
    {
        stE1BufferRec[bufindex].enBuffSta = E1_RX_BUFF_SETTLE;
        stE1BufferRec[bufindex].ptSend = 0;
        memset(stE1BufferRec[bufindex].ucBuffer, 0, sizeof(stE1BufferRec[bufindex].ucBuffer));
        mem_ptr = stE1BufferRec[bufindex].ucBuffer;
    }
    else
    {
        mem_ptr = (char *)stE1BufferRec[bufindex].ucBuffer;

        if( stE1BufferRec[bufindex].ptSend >= DATA_LEN_LIMIT )
        {
            QuadFALCDev.pef22554info[chipno].recvfrmerr[e1no]++;
            //����̫��ֱ�Ӷ���
            return -PACKET_TOOBIG;
        }
    }
    //RME Receive Message End
    //One complete message of length less than 32 bytes, or the last part
    //of a frame at least 32 bytes long is stored in the receive FIFO,
   // including the status byte.

    MEM_READ8((u8 *)&falc->rd.rbc , &regRbcl);
    MEM_READ8((u8 *)&falc->rd.rbc + 1 , &regRbch);
    if( regRbch&0x10 )
    {
        //gulE1OvCnt++;
    }
    //mb();

    //printk(KERN_ERR "RBCL = %x\n", regRbcl);
    //printk(KERN_ERR "RBCH = %x\n", regRbch);

    if(0 == (regRbcl&0x1f) && (0 != (regRbcl&0xe0) || 0 != (regRbch&0x1f)))
    {
        for( k=0;k<MAX_SEND_LEN;k++)
        {
            MEM_READ8((u8 *)&falc->rd.rfifo,&mem_ptr[stE1BufferRec[bufindex].ptSend++]);
        }
    }
    else
    {
        for( k=0;k<(regRbcl&0xff);k++) //rbcl�ĵ�5λ��ʾ��ǰRFIFO�е���Ч���ݳ���
        {
    	    if( k < MAX_SEND_LEN )
    	    {
    		    MEM_READ8((u8 *)&falc->rd.rfifo,&mem_ptr[ stE1BufferRec[bufindex].ptSend++]);
                //printk(KERN_ERR "data = %x\n", mem_ptr[ stE1BufferRec[bufindex].ptSend-1]);
    	    }
        }
    }
    //mb();

    SET_BIT8(&falc->wr.cmdr,  CMDR_RMC);
   /*�����յ�����Ϣ��ӵ���ͨ���Ľ��ջ�������*/
    if( FDUSED == QuadFALCDev.pef22554info[chipno].e1_use_state[e1no] )
    {
        if( (stE1BufferRec[bufindex].ptSend - 1 > 0) &&(stE1BufferRec[bufindex].ptSend - 1 <= DATA_LEN_LIMIT) )
        {
           ret = quadFALC_buffer_add(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no], mem_ptr, stE1BufferRec[bufindex].ptSend - 1, 0);//���һ���ֽ�RSIS������
           if( ret < 0 )
           {
                QuadFALCDev.pef22554info[chipno].recvfrmfail[e1no]++;
		        //debug("receive data error\n");
		        printk(KERN_WARNING "receive data error\n");
		        wake_up_interruptible(&QuadFALCDev.pef22554info[chipno].rq[e1no]);
           }
           else
           {
                //QuadFALCDev.recvmsg++;
                wake_up_interruptible(&QuadFALCDev.pef22554info[chipno].rq[e1no]);
           }
        }
    }

    QuadFALCDev.infocntall++;
    QuadFALCDev.pef22554info[chipno].infocnt[e1no]++;
    stE1BufferRec[bufindex].enBuffSta = E1_RX_BUFF_CLEAN;
    stE1BufferRec[bufindex].ptSend= 0;
    memset(stE1BufferRec[bufindex].ucBuffer, 0, sizeof(stE1BufferRec[bufindex].ucBuffer));

    return 0;
}

/*------------------------------------------------------------------------
  ����:  CASC�жϴ���

  �������:
                falc - �Ĵ����ṹ��ָ��

  �������:
                ��

  ����:

  ����ֵ:
                 0:�ɹ�

  ����:
-------------------------------------------------------------------------*/
static inline int quadFALC_irq_handle_CASC(QFALC_REG_MAP  *falc, int bufindex)
{
    u32 k;
    u8 rsp1, rsp2;
    R2_MSG_ST *p_r2_msg = NULL, r2_msg_tmp;
    struct st_data *pnode = NULL;
    //char *mem_ptr = NULL;
    //int ret;
    u8 freeze;
    int chipno = bufindex/NUM_OF_FALC, e1no = bufindex%NUM_OF_FALC;

    //printk(KERN_WARNING "%d receive e1 data:CASC\n", bufindex);
    //RPF Receive Pool Full
    //RFIFO�н��յ�32 �ֽڣ�����Ϣû�н������
    QuadFALCDev.pef22554info[chipno].casccnt[e1no]++;
    /*�������Ϊһ������Ϣ�Ŀ�ʼ�����û�����״̬ΪE1_RX_BUFF_CLEAN*/
    /*��ȡ����������2ms����ɣ��������rsp1��rsp2���������ݶ�Ϊ0,
      �����ݶ�ȡ����*/
    /*�жϵ�ָʾ�Ƿ���ȷ����ȡ�������Ƿ���ȷ���뷢�͵������й�ϵ*/
    MEM_READ8(&falc->rd.rsp1, &rsp1);
    MEM_READ8(&falc->rd.rsp2, &rsp2);
    //mb();
///-
//printk(KERN_WARNING "rsp1=0x%02x, rsp2=0x%02x\n", rsp1, rsp2);
    /*��sis.sfs��1��˵�����������д�����*/
    MEM_READ8(&falc->rd.sis, &freeze);
    if(freeze & SIS_SFS)
    {
        QuadFALCDev.pef22554info[chipno].freezecnt[e1no]++;
        return 0;
    }
    //mb();

#if 1
    if( FDUSED == QuadFALCDev.pef22554info[chipno].e1_use_state[e1no] )
    {
        for(k=0; k<MAX_R2_E1_CASREG_NUM; k++)
        {
            if(((k < 8) && (rsp1 & (1 << k))) || ((k >= 8) && (rsp2 & (1 << (k - 8)))))
            {
                if(NULL != (pnode = quadFALC_buffer_get_empty_node(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no])))
                {
                    p_r2_msg = (R2_MSG_ST *)pnode->buf;
                    memset(p_r2_msg, 0, sizeof(R2_MSG_ST));
                    MEM_READ8(&falc->rd.rs[k], &p_r2_msg->cas);

                    /* ���ͨ��0�����˱仯������ */
                    if(0 == k)
                    {
                        casrs[chipno][e1no][k] = p_r2_msg->cas;
                        continue;
                    }

                    if((p_r2_msg->cas & 0xf) != (casrs[chipno][e1no][k] & 0xf)
                        && (p_r2_msg->cas & 0xf0) != (casrs[chipno][e1no][k] & 0xf0))
                    {
                        /* ����ͨ�������ݶ������˱仯�����뽫��ȡ���������ݷֳ�������Ϣ */
                        casrs[chipno][e1no][k] = p_r2_msg->cas;
                        memset(&r2_msg_tmp, 0, sizeof(r2_msg_tmp));
                        r2_msg_tmp.cas = p_r2_msg->cas & 0xf;
                        r2_msg_tmp.channel = k + (MAX_R2_E1_CASREG_NUM - 1);
                        p_r2_msg->channel = k;
                        p_r2_msg->cas = (p_r2_msg->cas & 0xf0) >> 4;
                        pnode->len = sizeof(R2_MSG_ST);
                        quadFALC_buffer_get_next_node(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no]);

                        if(NULL != (pnode = quadFALC_buffer_get_empty_node(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no])))
                        {
                            p_r2_msg = (R2_MSG_ST *)pnode->buf;

                            *p_r2_msg = r2_msg_tmp;
                            pnode->len = sizeof(R2_MSG_ST);
                            quadFALC_buffer_get_next_node(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no]);
                        }
                        else
                        {
                            QuadFALCDev.pef22554info[chipno].recvfrmfail[e1no]++;
                        }
                    }
                    else if((p_r2_msg->cas & 0xf) != (casrs[chipno][e1no][k] & 0xf))
                    {
                        /* ��ͨ�������ݷ����˱仯 */
                        casrs[chipno][e1no][k] &= 0xf0;
                        casrs[chipno][e1no][k] |= p_r2_msg->cas & 0xf;
                        p_r2_msg->channel = k + (MAX_R2_E1_CASREG_NUM - 1);
                        p_r2_msg->cas &= 0xf;
                        pnode->len = sizeof(R2_MSG_ST);
                        quadFALC_buffer_get_next_node(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no]);
                    }
                    else if((p_r2_msg->cas & 0xf0) != (casrs[chipno][e1no][k] & 0xf0))
                    {
                        /* ��ͨ�������ݷ����˱仯 */
                        //printk(KERN_WARNING "casrs[0][%d][%d] = 0x%02x\n", bufindex, k, casrs[0][bufindex][k]);
                        casrs[chipno][e1no][k] &= 0xf;
                        casrs[chipno][e1no][k] |= p_r2_msg->cas & 0xf0;
                        p_r2_msg->channel = k;
                        p_r2_msg->cas = (p_r2_msg->cas & 0xf0) >> 4;
                        pnode->len = sizeof(R2_MSG_ST);
                        quadFALC_buffer_get_next_node(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no]);
                    }
                }
                else
                {
                    QuadFALCDev.pef22554info[chipno].recvfrmfail[e1no]++;
                }
            }
        }

        wake_up_interruptible(&QuadFALCDev.pef22554info[chipno].rq[e1no]);
    }

    QuadFALCDev.infocntall++;
    QuadFALCDev.pef22554info[chipno].infocnt[e1no]++;
#else

    //��Ϣ�������
    //printk(KERN_WARNING "%d receive e1 data:CASC\n", bufindex);
    QuadFALCDev.rmecnt++;
    /*�������Ϊһ������Ϣ�Ŀ�ʼ�����û�����״̬ΪE1_RX_BUFF_CLEAN*/
    if( stE1BufferRec[bufindex].enBuffSta == E1_RX_BUFF_CLEAN )
    {
        stE1BufferRec[bufindex].enBuffSta = E1_RX_BUFF_SETTLE;
        stE1BufferRec[bufindex].ptSend = 0;
        memset(stE1BufferRec[bufindex].ucBuffer, 0, sizeof(stE1BufferRec[bufindex].ucBuffer));
        mem_ptr = stE1BufferRec[bufindex].ucBuffer;
    }
    else
    {
        mem_ptr = (char *)stE1BufferRec[bufindex].ucBuffer;

        if( stE1BufferRec[bufindex].ptSend >= DATA_LEN_LIMIT )
        {
            QuadFALCDev.recvfrmerr[bufindex]++;
            //����̫��ֱ�Ӷ���
            return -PACKET_TOOBIG;
        }
    }

    for(k=0; k<16; k++)
    {
        MEM_READ8(&falc->rd.rs[k], &mem_ptr[stE1BufferRec[bufindex].ptSend++]);
    }

    if( (stE1BufferRec[bufindex].ptSend - 1 > 0) &&(stE1BufferRec[bufindex].ptSend - 1 <= DATA_LEN_LIMIT) )
    {
       ret = add_to_buffer(&QuadFALCDev.aos_buf_receive[bufindex], mem_ptr, stE1BufferRec[bufindex].ptSend, 0);//���һ���ֽ�RSIS������
       if( ret < 0 )
       {
            QuadFALCDev.recvfrmfail[bufindex]++;
	        //debug("receive data error\n");
	        printk(KERN_WARNING "receive data error\n");
	        wake_up_interruptible(&QuadFALCDev.rq[bufindex]);
       }
       else
       {
            //QuadFALCDev.recvmsg++;
            wake_up_interruptible(&QuadFALCDev.rq[bufindex]);
       }
    }

    stE1BufferRec[bufindex].enBuffSta = E1_RX_BUFF_CLEAN;
    stE1BufferRec[bufindex].ptSend= 0;
    memset(stE1BufferRec[bufindex].ucBuffer, 0, sizeof(stE1BufferRec[bufindex].ucBuffer));
#endif

    return 0;
}

/*------------------------------------------------------------------------
  ����:  RSC�жϴ���

  �������:
                falc - �Ĵ����ṹ��ָ��

  �������:
                ��

  ����:

  ����ֵ:
                 0:�ɹ�

  ����:
-------------------------------------------------------------------------*/
static inline int quadFALC_irq_handle_RSC(QFALC_REG_MAP  *falc, int bufindex)
{
    u32 k;
    u8 rsp1, rsp2;
    R2_MSG_ST *p_r2_msg = NULL, r2_msg_tmp;
    struct st_data *pnode = NULL;
    //char *mem_ptr = NULL;
    //int ret;
    u8 freeze;
    int chipno = bufindex/NUM_OF_FALC, e1no = bufindex%NUM_OF_FALC;

    //printk(KERN_WARNING "%d receive e1 data:CASC\n", bufindex);
    //RPF Receive Pool Full
    //RFIFO�н��յ�32 �ֽڣ�����Ϣû�н������
    //QuadFALCDev.rpfcnt++;
    /*�������Ϊһ������Ϣ�Ŀ�ʼ�����û�����״̬ΪE1_RX_BUFF_CLEAN*/
    /*��ȡ����������2ms����ɣ��������rsp1��rsp2���������ݶ�Ϊ0,
      �����ݶ�ȡ����*/
    /*�жϵ�ָʾ�Ƿ���ȷ����ȡ�������Ƿ���ȷ���뷢�͵������й�ϵ*/
    MEM_READ8(&falc->rd.rsp1, &rsp1);
    MEM_READ8(&falc->rd.rsp2, &rsp2);
    mb();
///-
//printk(KERN_WARNING "rsp1=0x%02x, rsp2=0x%02x\n", rsp1, rsp2);
    /*��sis.sfs��1��˵�����������д�����*/
    MEM_READ8(&falc->rd.sis, &freeze);
    if(freeze & SIS_SFS)
    {
        QuadFALCDev.pef22554info[chipno].freezecnt[e1no]++;
        return 0;
    }
    //mb();

    if( FDUSED == QuadFALCDev.pef22554info[chipno].e1_use_state[e1no] )
    {
        for(k=0; k<MAX_R2_T1_CASREG_NUM; k++)
        {
            if(((k < 8) && (rsp1 & (1 << k))) || ((k >= 8) && (rsp2 & (1 << (k - 8)))))
            {
                if(NULL != (pnode = quadFALC_buffer_get_empty_node(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no])))
                {
                    p_r2_msg = (R2_MSG_ST *)pnode->buf;
                    memset(p_r2_msg, 0, sizeof(R2_MSG_ST));
                    MEM_READ8(&falc->rd.rs[k], &p_r2_msg->cas);

                    if((p_r2_msg->cas & 0xf) != (casrs[chipno][e1no][k] & 0xf)
                        && (p_r2_msg->cas & 0xf0) != (casrs[chipno][e1no][k] & 0xf0))
                    {
                        /*����ͨ�������ݶ������˱仯�����뽫
                          ��ȡ���������ݷֳ�������Ϣ*/
                        casrs[chipno][e1no][k] = p_r2_msg->cas;
                        memset(&r2_msg_tmp, 0, sizeof(r2_msg_tmp));
                        r2_msg_tmp.cas = p_r2_msg->cas & 0xf;
                        r2_msg_tmp.channel = (k + 1) * 2;
                        p_r2_msg->channel = (k + 1) * 2 - 1;
                        p_r2_msg->cas = (p_r2_msg->cas & 0xf0) >> 4;
                        pnode->len = sizeof(R2_MSG_ST);
                        quadFALC_buffer_get_next_node(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no]);

                        if(NULL != (pnode = quadFALC_buffer_get_empty_node(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no])))
                        {
                            p_r2_msg = (R2_MSG_ST *)pnode->buf;

                            *p_r2_msg = r2_msg_tmp;
                            pnode->len = sizeof(R2_MSG_ST);
                            quadFALC_buffer_get_next_node(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no]);
                        }
                        else
                        {
                            QuadFALCDev.pef22554info[chipno].recvfrmfail[e1no]++;
                        }
                    }
                    else if((p_r2_msg->cas & 0xf) != (casrs[chipno][e1no][k] & 0xf))
                    {
                        /*��ͨ�������ݷ����˱仯*/
                        casrs[chipno][e1no][k] &= 0xf0;
                        casrs[chipno][e1no][k] |= p_r2_msg->cas & 0xf;
                        p_r2_msg->channel = (k + 1) * 2;
                        p_r2_msg->cas &= 0xf;
                        pnode->len = sizeof(R2_MSG_ST);
                        quadFALC_buffer_get_next_node(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no]);
                    }
                    else if((p_r2_msg->cas & 0xf0) != (casrs[chipno][e1no][k] & 0xf0))
                    {
                        /*��ͨ�������ݷ����˱仯*/
                        //printk(KERN_WARNING "casrs[0][%d][%d] = 0x%02x\n", bufindex, k, casrs[0][bufindex][k]);
                        casrs[chipno][e1no][k] &= 0xf;
                        casrs[chipno][e1no][k] |= p_r2_msg->cas & 0xf0;
                        p_r2_msg->channel = (k + 1) * 2 - 1;
                        p_r2_msg->cas = (p_r2_msg->cas & 0xf0) >> 4;
                        pnode->len = sizeof(R2_MSG_ST);
                        quadFALC_buffer_get_next_node(&QuadFALCDev.pef22554info[chipno].aos_buf_receive[e1no]);
                    }
                }
                else
                {
                    QuadFALCDev.pef22554info[chipno].recvfrmfail[e1no]++;
                }
            }
        }

        wake_up_interruptible(&QuadFALCDev.pef22554info[chipno].rq[e1no]);
    }

    QuadFALCDev.infocntall++;
    QuadFALCDev.pef22554info[chipno].infocnt[e1no]++;

    return 0;
}

static inline int quadFALC_irq_handle_XMB(QFALC_REG_MAP  *falc, int bufindex)
{
    int len;
    struct st_data *buf_head = NULL;
    R2_MSG_ST *p_r2_msg = NULL;
    int channel = 0;
    u8 writecas;
    int chipno = bufindex/NUM_OF_FALC, e1no = bufindex%NUM_OF_FALC;

///-
    //printk(KERN_WARNING "%s %d\n", __FUNCTION__, __LINE__);
    //printk(KERN_WARNING "irq senddata\n");
    QuadFALCDev.pef22554info[chipno].xmbcnt[e1no]++;
    buf_head = QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].head;

    if((((unsigned long)(buf_head)) < ((unsigned long)(QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].databuf))) ||
        (((unsigned long)(buf_head)) > ((unsigned long)(QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].databuf + QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].size))) )
    {
        QuadFALCDev.pef22554info[chipno].sendfrmerr[e1no]++;
        printk(KERN_WARNING "pointer error\n");
        return -PACKET_TOOBIG;
    }

    if( !quadFALC_buffer_is_empty(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no]) )
    {
        if(buf_head->len < 0 || buf_head->len > sizeof(R2_MSG_ST))
        {
            QuadFALCDev.pef22554info[chipno].sendfrmerr[e1no]++;
            printk(KERN_WARNING "buf_head->len = %d\n", buf_head->len);
        }
    }

    /*�ж��豸��Ӧͨ���ķ��ͻ������Ƿ�Ϊ��*/
    if( !quadFALC_buffer_is_empty(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no]) )
    {
        /*��ʼ���ͣ����ȴ����͵��ֽ�������32������32�ֽڣ���
           �ȴ����͵��ֽ���������32�����������ֽ�������֪ͨ
           ���ն˱�����Ϣ�������*/
        len = buf_head->len - buf_head->cur;
        if(len < 0 || len > sizeof(R2_MSG_ST))
        {
            QuadFALCDev.pef22554info[chipno].sendfrmerr[e1no]++;
            printk(KERN_WARNING "#################%s %d\n",
                    __FUNCTION__, __LINE__);
            return 0;
        }
        //debug("write send data len = %d\n",len);
        p_r2_msg = (R2_MSG_ST *)buf_head->buf;

        if(p_r2_msg->channel > MAX_R2_E1_CHANNEL)
        {
            QuadFALCDev.pef22554info[chipno].sendfrmerr[e1no]++;
            printk(KERN_WARNING "channel = %d\n", p_r2_msg->channel);
            return 0;
        }

        if(p_r2_msg->channel >= MAX_R2_E1_CASREG_NUM)
        {
            channel = p_r2_msg->channel - (MAX_R2_E1_CASREG_NUM - 1);
        }
        else
        {
            channel = p_r2_msg->channel;
        }

        if(p_r2_msg->channel >= MAX_R2_E1_CASREG_NUM)
        {
            /*����Ҫ���͵�CAS�������һ�η��͵�CAS���浽ȫ������*/
            writecas = (p_r2_msg->cas & 0xf) | (casxs[chipno][e1no][channel] & 0xf0);
            casxs[chipno][e1no][channel] &= 0xf0;
            casxs[chipno][e1no][channel] |= p_r2_msg->cas & 0xf;
        }
        else
        {
            writecas = ((p_r2_msg->cas << 4) & 0xf0) | (casxs[chipno][e1no][channel] & 0xf);
            casxs[chipno][e1no][channel] &= 0xf;
            casxs[chipno][e1no][channel] |= (p_r2_msg->cas << 4) & 0xf0;
        }

        if( buf_head == (QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].databuf + QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].size) )
        {
            QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].head = QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].databuf;
        }
        else
        {
            QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].head++;
        }
#if 0
        channel = p_r2_msg->channel;
        writecas = p_r2_msg->cas;
        //printk(KERN_WARNING "channel=%d, writecas = 0x%02x\n", channel, writecas);
#endif
        /*�����п�������һ��XMB�жϲ���ʱд�룬��������д��ʧ�ܣ�
          д���μĴ����Ա����������*/
        MEM_WRITE8(&falc->wr.xs[channel], writecas);
#if MOD_R2_DEBUG0
        MEM_WRITE8(&falc->wr.xs[channel], writecas);
#endif

        wake_up_interruptible(&QuadFALCDev.pef22554info[chipno].wq[e1no]);
    }

    return 0;
}

/*------------------------------------------------------------------------
  ����:  CASE�жϴ���

  �������:
                falc - �Ĵ����ṹ��ָ��

  �������:
                ��

  ����:

  ����ֵ:
                 0:�ɹ�

  ����:
-------------------------------------------------------------------------*/
static inline int quadFALC_irq_handle_CASE(QFALC_REG_MAP  *falc, int bufindex)
{
    int len;
    struct st_data *buf_head = NULL;
    R2_MSG_ST *p_r2_msg = NULL;
    int channel = 0;
    u8 writecas;
    int chipno = bufindex/NUM_OF_FALC, e1no = bufindex%NUM_OF_FALC;

///-
    //printk(KERN_WARNING "%s %d\n", __FUNCTION__, __LINE__);
    //printk(KERN_WARNING "irq senddata\n");
    buf_head = QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].head;

    if((((unsigned long)(buf_head)) < ((unsigned long)(QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].databuf))) ||
        (((unsigned long)(buf_head)) > ((unsigned long)(QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].databuf + QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].size))) )
    {
        QuadFALCDev.pef22554info[chipno].sendfrmerr[e1no]++;
        printk(KERN_WARNING "pointer error\n");
        return -PACKET_TOOBIG;
    }

    if( !quadFALC_buffer_is_empty(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no]) )
    {
        if(buf_head->len < 0 || buf_head->len > sizeof(R2_MSG_ST))
        {
            QuadFALCDev.pef22554info[chipno].sendfrmerr[e1no]++;
            printk(KERN_WARNING "buf_head->len = %d\n", buf_head->len);
        }
    }

    /*�ж��豸��Ӧͨ���ķ��ͻ������Ƿ�Ϊ��*/
    if( !quadFALC_buffer_is_empty(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no]) )
    {
        /*��ʼ���ͣ����ȴ����͵��ֽ�������32������32�ֽڣ���
           �ȴ����͵��ֽ���������32�����������ֽ�������֪ͨ
           ���ն˱�����Ϣ�������*/
        len = buf_head->len - buf_head->cur;
        if(len < 0 || len > sizeof(R2_MSG_ST))
        {
            QuadFALCDev.pef22554info[chipno].sendfrmerr[e1no]++;
            printk(KERN_WARNING "#################%s %d\n",
                    __FUNCTION__, __LINE__);
            return 0;
        }
        //debug("write send data len = %d\n",len);
        p_r2_msg = (R2_MSG_ST *)buf_head->buf;

        if(p_r2_msg->channel > MAX_R2_T1_CHANNEL)
        {
            QuadFALCDev.pef22554info[chipno].sendfrmerr[e1no]++;
            printk(KERN_WARNING "channel = %d\n", p_r2_msg->channel);
            return 0;
        }

        channel = (p_r2_msg->channel - 1)/2;

        if(0 == p_r2_msg->channel%2)
        {
            /*����Ҫ���͵�CAS�������һ�η��͵�CAS���浽ȫ������*/
            writecas = (p_r2_msg->cas & 0xf) | (casxs[chipno][e1no][channel] & 0xf0);
            casxs[chipno][e1no][channel] &= 0xf0;
            casxs[chipno][e1no][channel] |= p_r2_msg->cas & 0xf;
        }
        else
        {
            writecas = ((p_r2_msg->cas << 4) & 0xf0) | (casxs[chipno][e1no][channel] & 0xf);
            casxs[chipno][e1no][channel] &= 0xf;
            casxs[chipno][e1no][channel] |= (p_r2_msg->cas << 4) & 0xf0;
        }

        if( buf_head == (QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].databuf + QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].size) )
        {
            QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].head = QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].databuf;
        }
        else
        {
            QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].head++;
        }
#if 0
        channel = p_r2_msg->channel;
        writecas = p_r2_msg->cas;
        //printk(KERN_WARNING "channel=%d, writecas = 0x%02x\n", channel, writecas);
#endif
        /*�����п�������һ��XMB�жϲ���ʱд�룬��������д��ʧ�ܣ�
          д���μĴ����Ա����������*/
        MEM_WRITE8(&falc->wr.xs[channel], writecas);
#if MOD_R2_DEBUG0
        MEM_WRITE8(&falc->wr.xs[channel], writecas);
#endif

        wake_up_interruptible(&QuadFALCDev.pef22554info[chipno].wq[e1no]);
    }

    return 0;
}


/*------------------------------------------------------------------------
  ����:  XPR�жϴ���

  �������:
                falc - �Ĵ����ṹ��ָ��
                bufindex - e1�ӿ����

  �������:
                ��

  ����:

  ����ֵ:
                 0:�ɹ�
                 -PACKET_TOOBIG:����

  ����:
-------------------------------------------------------------------------*/
static int quadFALC_irq_handle_XPR(QFALC_REG_MAP  *falc, int bufindex)
{
    u32 k;
    int len;
    struct st_data *buf_head = NULL, *buf_tail = NULL;
    int chipno = bufindex/NUM_OF_FALC, e1no = bufindex%NUM_OF_FALC;

    //printk(KERN_WARNING "irq senddata\n");
    buf_head = QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].head;
    buf_tail = QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].tail;

    if((((unsigned long)(buf_head)) < ((unsigned long)(QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].databuf))) ||
        (((unsigned long)(buf_head)) > ((unsigned long)(QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].databuf + QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].size))) )
    {
        QuadFALCDev.pef22554info[chipno].sendfrmerr[e1no]++;
        printk(KERN_WARNING "pointer error\n");
        return -PACKET_TOOBIG;
    }

    if( !quadFALC_buffer_is_empty(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no]) )
    {
        if(buf_head->cur > DATA_LEN_LIMIT - MAX_SEND_LEN)
        {
            QuadFALCDev.pef22554info[chipno].sendfrmerr[e1no]++;
            printk(KERN_WARNING "buf_head->cur = %d\n", buf_head->cur);
            return -PACKET_TOOBIG;
        }

        if(buf_head->len < 0 || buf_head->len > DATA_LEN_LIMIT)
        {
            QuadFALCDev.pef22554info[chipno].sendfrmerr[e1no]++;
            printk(KERN_WARNING "buf_head->len = %d\n", buf_head->len);
        }
    }

    sendflag[bufindex] = 0;

    /*�ж��豸��Ӧͨ���ķ��ͻ������Ƿ�Ϊ��*/
    if( !quadFALC_buffer_is_empty(&QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no]) )
    {
        /*��ʼ���ͣ����ȴ����͵��ֽ�������32������32�ֽڣ���
           �ȴ����͵��ֽ���������32�����������ֽ�������֪ͨ
           ���ն˱�����Ϣ�������*/
        sendflag[bufindex] = 1;

        if( (buf_head->len - buf_head->cur ) > MAX_SEND_LEN )
        {
            for(k=0; k<MAX_SEND_LEN; k++)
            {
                MEM_WRITE8((u8 *)&falc->wr.xfifo,buf_head->buf[buf_head->cur++]);
            }
            //mb();
            SET_BIT8(&falc->wr.cmdr, CMDR_XHF);
        }
        else
        {
            len = buf_head->len - buf_head->cur;
            if(len <0 || len > MAX_SEND_LEN)
            {
                QuadFALCDev.pef22554info[chipno].sendfrmerr[e1no]++;
                printk(KERN_WARNING "send date length error\n");
                return -PACKET_TOOBIG;
            }
            for(k=0; k<len; k++)
            {
                MEM_WRITE8((u8 *)&falc->wr.xfifo,buf_head->buf[buf_head->cur++]);
            }
            //mb();
            if( buf_head == (QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].databuf + QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].size) )
            {
                QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].head = QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].databuf;
            }
            else
            {
                QuadFALCDev.pef22554info[chipno].aos_buf_send[e1no].head++;
            }

            SET_BIT8(&falc->wr.cmdr, CMDR_XHF | CMDR_XME);
            wake_up_interruptible(&QuadFALCDev.pef22554info[chipno].wq[e1no]);
        }

    }

    return 0;
}


/*------------------------------------------------------------------------
  ����:  22554�жϴ���

  �������:
                ��

  �������:
                ��

  ����:

  ����ֵ:
                 ��

  ����:
-------------------------------------------------------------------------*/
static inline void quadFALC_irq_proc(void)
{
    u32 i,j, chipno;
    int err;
    QFALC_REG_MAP  *falc = NULL;
    u8 regCis[MAX_PEF22554_NUM],regIsr[5],regGis;

    
	QuadFALCDev.intcnt++;

    for(i=0; i<MAX_PEF22554_NUM; i++)
    {
        /* ��ȡCIS �Ĵ������ж��жϷ�����ͨ�� */
        falc = ptrreg[NUM_OF_FALC * i];
        MEM_READ8(&falc->rd.cis,&regCis[i]);
    }
    //mb();
    
    for(chipno=0; chipno<MAX_PEF22554_NUM; chipno++)
    {
        for(i=0; i<NUM_OF_FALC; i++)  // ÿ��ͨ�����жϷֱ���
        {
            falc = ptrreg[i + NUM_OF_FALC * chipno];


            //mb();
            if((regCis[chipno] & (1<<i)) )
            {
                
                
                QuadFALCDev.pef22554info[chipno].intdeadcnt[i]++;

                /* ��ȡGIS �Ĵ������ж��жϷ�����ISR ��� */
                MEM_READ8(&falc->rd.gis, &regGis);

                //printk(KERN_WARNING "e1 irq:%x\n", regGis);
                

                /* ͨ�����ж��¼���������ѯ��ʲô�����жϣ�����������ж��ڵ�1,2 isr״̬�Ĵ���������ֻ��ѯǰ�����ж�״̬�Ĵ��� */
                for( j=0; j< 2;j++ )
                {
                	regIsr[j] = 0;
                    if( regGis& (1<<j) )
                    {
                        MEM_READ8(((u8 *)&falc->rd.isr0) +j, &regIsr[j]);

                        //printk(KERN_WARNING "e1:%d isr%d : %x\n", i, j, regIsr[j]);
                    }
                }
                //mb();

                /* ���� RPF �жϣ�RFIFO ��������������Ϣû�н��� */
                if( regIsr[0] & ISR0_RPF )
                {
                    err = quadFALC_irq_handle_RPF(falc, i + NUM_OF_FALC * chipno);
                    if( err )
                    {
                        goto err_proc;
                    }
                }

                /* ���� RME �жϣ�������Ϣ���ս��� */
                if( regIsr[0] & ISR0_RME )
                {
                    err = quadFALC_irq_handle_RME(falc, i + NUM_OF_FALC * chipno);
                    if( err )
                    {
                        goto err_proc;
                    }
                }

                /* ����XPR �жϣ� ���ͻ�����λ�գ���������XFIFO��������ݲ����� */
                if( regIsr[1] & ISR1_XPR )
                {
                    quadFALC_irq_handle_XPR(falc, i + NUM_OF_FALC * chipno);
                }

                if(SS1 == QuadFALCDev.pef22554info[chipno].mode[i])
                {
                    if(MODE_E1 == QuadFALCDev.pef22554info[chipno].chipmode)
                    {
                        /* ���� CASC �жϣ��Է� CAS ������仯 */
                        if( regIsr[0] & ISR0_CASC )
                        {
                            quadFALC_irq_handle_CASC(falc, i + NUM_OF_FALC * chipno);
                        }

                        if( regIsr[1] & ISR1_XMB)
                        {
                            quadFALC_irq_handle_XMB(falc, i + NUM_OF_FALC * chipno);
                        }
                    }
                    else if(MODE_T1 == QuadFALCDev.pef22554info[chipno].chipmode)
                    {
                        if( regIsr[0] & ISR0_RSC )
                        {
                            quadFALC_irq_handle_RSC(falc, i + NUM_OF_FALC * chipno);
                        }

                        if( regIsr[1] & ISR1_CASE )
                        {
                            quadFALC_irq_handle_CASE(falc, i + NUM_OF_FALC * chipno);
                        }
                    }
                }
            }
        }
    }

    return;

err_proc:
    stE1BufferRec[i + NUM_OF_FALC * chipno].ptSend = 0;
    stE1BufferRec[i + NUM_OF_FALC * chipno].enBuffSta = E1_RX_BUFF_CLEAN;
    memset(stE1BufferRec[i + NUM_OF_FALC * chipno].ucBuffer, 0, sizeof(stE1BufferRec[i + NUM_OF_FALC * chipno].ucBuffer));
    debug("some error happen\n");
    SET_BIT8(&falc->wr.cmdr,  CMDR_RMC);
    
    return;
}

/*------------------------------------------------------------------------
  ����:  �豸���жϴ�����

  �������:
                irq - �����жϵ��жϺ�
                dev_id - �豸��

  �������:
                ��

  ����:

  ����ֵ:
                 �жϴ�����ɱ�־

  ����:
-------------------------------------------------------------------------*/
irqreturn_t quadFALC_irq_handler(int irq, void *dev_id)
{
    //printk(KERN_WARNING "gpio irq:%d\n", irq);

    quadFALC_irq_proc();
    
    return IRQ_HANDLED;
}


/*------------------------------------------------------------------------
  ����:  �����豸��IRQ

  �������:
                chipno - PEF22554оƬ��

  �������:
                ��

  ����:
                �����豸��RPF��RME��CASC��XPR�ж�
                ����TS16 Ϊ����ͨ��

  ����ֵ:
                 ��

  ����:
                 quadFALC_irq_cfg(0)
                 ����0��pef22554��IRQ����
-------------------------------------------------------------------------*/
void quadFALC_irq_cfg(int chipno)
{
	QFALC_REG_MAP   *falc;
	int i;
	u8 tmp;

    
	/*����4 ��ͨ����IRQ*/
	for(i=0; i<NUM_OF_FALC; i++)
	{
		falc = ptrreg[i + NUM_OF_FALC * chipno];

		/*���ù���ģʽΪ͸��ģʽ��ʹ��RFIFO���չ���*/
		MEM_WRITE8(&falc->rdWr.mode, MODE_MDS2|MODE_HRAC);
        MEM_WRITE8(&falc->rdWr.ccr1, CCR1_EITS|CCR1_ITF);
///-
        SET_BIT8(&falc->wr.dec, DEC_DFEC);
        //SET_BIT8(&falc->rdWr.xsp_fmr5, XSP_CASEN);

		/*����RPF��RME��CASC��XPR�ж�*/
		MEM_READ8(&falc->rd.isr0, &tmp);
		CLEAR_BIT8(&falc->rdWr.imr0,IMR0_RPF);
		CLEAR_BIT8(&falc->rdWr.imr0,IMR0_RME);
		//CLEAR_BIT8(&falc->rdWr.imr0,IMR0_CASC);
		MEM_READ8(&falc->rd.isr1,&tmp);
		CLEAR_BIT8(&falc->rdWr.imr1,IMR1_XPR);
		//CLEAR_BIT8(&falc->rdWr.imr1,IMR1_XMB);
		switch(QuadFALCDev.pef22554info[chipno].chipmode)
		{
		    case MODE_E1:
    		    MEM_WRITE8(&falc->rdWr.rtr3, RTR3_TS16);
                MEM_WRITE8(&falc->rdWr.ttr3, TTR3_TS16);
                break;
            case MODE_T1:
                if(PRI == QuadFALCDev.pef22554info[chipno].mode[i]
                    || SS7 == QuadFALCDev.pef22554info[chipno].mode[i])
                {
                    MEM_WRITE8(&falc->rdWr.rtr4, RTR4_TS24);
                    MEM_WRITE8(&falc->rdWr.ttr4, TTR4_TS24);
                }
                break;
        }
        MEM_WRITE8(&falc->rdWr.tsbs1, 0xff);
	}
}


/*------------------------------------------------------------------------
  ����:  ��ֹ22554 �ж�©������̺߳���

  �������:
                data - ��������

  �������:
                ��

  ����:

  ����ֵ:
                 0 - �߳������˳�

  ����:
-------------------------------------------------------------------------*/
int quadFALC_irq_thread(void *data)
{
    unsigned long flags;

	//ST_GPIO *gpiop = (ST_GPIO *)QuadFALCDev.virtual_gpio_base_addr;

	while(1)
	{

		/*�����߳�״̬Ϊ�ɴ��ģʽ*/
		set_current_state(TASK_INTERRUPTIBLE);

		/*����߳��Ƿ��յ��˳��źţ����յ��˳��źţ�
		�߳��˳�*/
		if( kthread_should_stop() )
		{
			break;
		}

		/*��ʱ�����жϣ�����22554 �жϴ���*/
		//*(volatile u32 *)&gpiop->gpio_int_mask &= ~(1 << 8);
		spin_lock_irqsave(&QuadFALCDev.lock, flags);
		quadFALC_irq_proc();
		//*(volatile u32 *)&gpiop->gpio_int_mask |= (1 << 8);
		spin_unlock_irqrestore(&QuadFALCDev.lock, flags);

		schedule_timeout(10);
	}

    QuadFALCDev.quadFALC_task = NULL;

	return 0;
}


/*------------------------------------------------------------------------
  ����:  ������ʱ�����豸�жϵ������ں��߳�

  �������:
                ��

  �������:
                ��

  ����:

  ����ֵ:
                 0:����
                 С��0:����

  ����:
-------------------------------------------------------------------------*/
int quadFALC_irq_thread_create(void)
{
    int err;

	QuadFALCDev.quadFALC_task= kthread_create(quadFALC_irq_thread, NULL, "quadFALC");
	if( IS_ERR(QuadFALCDev.quadFALC_task) )
	{
		printk(KERN_WARNING "Unable to start quadFALC thread\n");
		err = PTR_ERR(QuadFALCDev.quadFALC_task);
		QuadFALCDev.quadFALC_task = NULL;
		return err;
	}

    return 0;
}

/*------------------------------------------------------------------------
  ����:  ɱ����ʱ�����豸�жϵ������ں��߳�

  �������:
                ��

  �������:
                ��

  ����:

  ����ֵ:
                ��

  ����:
-------------------------------------------------------------------------*/
void quadFALC_irq_thread_destroy(void)
{
	if( QuadFALCDev.quadFALC_task )
	{
		kthread_stop(QuadFALCDev.quadFALC_task);
		QuadFALCDev.quadFALC_task = NULL;
	}
}

