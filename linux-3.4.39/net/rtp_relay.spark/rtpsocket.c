
int rtp_socket_create_one(int iSession)
{
    U16 usListenPort = U16_BUTT;
    rtpsession *rtps;

    rtps = rtp_eia_get_rtp_session(iSession/2,iSession&1);
    if( rtps == NULL )
    {
        AOS_ASSERT( 0 );
        goto proc_end;
    }

    if(g_lRtpRevSocket[iSession].iSocket > 0)
    {
        close(g_lRtpRevSocket[iSession].iSocket);
        g_lRtpRevSocket[iSession].iSocket = -1;
    }

    if(g_lRtpRevSocket[iSession].iSocket < 0)
    {
        
        {
            SOCKADDRIN_S stAddr;

            g_lRtpRevSocket[iSession].iSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if(g_lRtpRevSocket[iSession].iSocket < 0)
            {
                AOS_ASSERT(0);
                goto proc_end;
            }

            usListenPort = rtps->rtps_loclport;
            
            aos_memzero(&stAddr, sizeof(stAddr));
            stAddr.sin_usFamily = AF_INET;
            stAddr.sin_usPort = htons(usListenPort);
            aos_inet_pton(if_get_local_voc_ipaddr(), &(stAddr.sin_stAddr.s_ulAddr));

             //绑定当前端口
            if (bind(g_lRtpRevSocket[iSession].iSocket, (VOID *)&stAddr, sizeof(stAddr)) < 0)
            {
                AOS_LOG(usListenPort);
                close(g_lRtpRevSocket[iSession].iSocket);
                g_lRtpRevSocket[iSession].iSocket = -1;
            }
            else
            {
                g_lRtpRevSocket[iSession].usBindPort = usListenPort;
            }
        }
    }

proc_end:
    if( rtps == NULL )
    {
        return -1;
    }

    return g_lRtpRevSocket[iSession].iSocket;
}



void rtp_socket_send(rtpsession *rtps,rtpencaphdr *rtp,U32 ulLen)
{
    SOCKADDRIN_S stAddr;
    SOCKADDRIN6_S stAddr6;
    int iRet,iSession;


    if( g_lRtpRevSocket[iSession].iSocket < 0 )
    {
        return;
    }


    {
        stAddr.sin_usFamily = AF_INET;
        stAddr.sin_usPort = htons(rtps->rtps_peerport);
        stAddr.sin_stAddr.s_ulAddr = rtps->rtps_peeraddr[0];

        iRet = sendto(g_lRtpRevSocket[iSession].iSocket, (S8 *)rtp, ulLen, 0, (VOID *)&stAddr, sizeof(stAddr));
    }

    if( iRet < ulLen )
    {
        AOS_ASSERT( 0 );
    }
}



void rtp_socket_receive(S32 lMaxSocket)
{
    SOCKADDRIN_S stAddr;
    SOCKADDRIN6_S stAddr6;
    FD_SET_S fd;
    TIME_VAL_S waittime;
    S32 lRet, lLen;
    U32 ulLen,pktLen;
    S8 * pszBuf, *pszData;
    int iSession;
    rtpsession *rtps;
    ipencaphdr *ip;
    ethencaphdr *eth;
	U32 hms;
	U32 ulProto;

    ulProto = aos_get_run_ip_protocol();
    if(lMaxSocket < 0)
    {
        AOS_ASSERT(0);
        return;
    }

    FD_ZERO(&fd);

    for( iSession = 0;iSession < RTP_EIA_SESSION_REAL_MAX;iSession++ )
    {
        rtps = rtp_eia_get_rtp_session( iSession/2,iSession&1);
        if(rtps== NULL || g_lRtpRevSocket[iSession ].iSocket < 0)
        {
            continue;
        }

        if( rtps->rtps_valid == RTP_INVALID )
        {
            if( g_lRtpRevSocket[ iSession ].ulState == RTP_SOCKET_STAUS_WORK )
            {
                g_lRtpRevSocket[ iSession ].ulState = RTP_SOCKET_STAUS_IDLE ;
            }
        }
        else
        {
            if( g_lRtpRevSocket[ iSession ].ulState == RTP_SOCKET_STAUS_IDLE )
            {
                g_lRtpRevSocket[ iSession ].ulState = RTP_SOCKET_STAUS_WORK;
                g_lRtpRevSocket[ iSession ].ulRevCnt = 0;
                g_lRtpRevSocket[ iSession ].ulTransCnt = 0;
            }
        }

        FD_SET(g_lRtpRevSocket[iSession ].iSocket , &fd);

    }

    waittime.tv_lSec = 1;
    waittime.tv_lUsec = 0;
    lRet = select(lMaxSocket  +1, &fd, NULL, NULL, (struct timeval *)&waittime);
    if(lRet <= 0)
    {
        return;
    }

    for( iSession = 0;iSession< RTP_EIA_SESSION_REAL_MAX;iSession++)
    {
        if(g_lRtpRevSocket[iSession ].iSocket < 0)
        {
            continue;
        }
        ulLen  = 0;
        if( FD_ISSET( g_lRtpRevSocket[iSession].iSocket, &fd ) )
        {
            lRet = ioctl(g_lRtpRevSocket[iSession].iSocket , FIOREAD, (S8*)&ulLen);
            if(lRet < 0)
            {
                continue;
            }
            if(ulLen <= 0)
            {
                continue;
            }
            pszBuf = NULL;
            pszBuf = aos_dmem_alloc(MPE_RTPEIA,88, ulLen + RESERV_PKT_HEAD_LEN);
            if(AOS_ADDR_INVALID(pszBuf))
            {
                AOS_ASSERT(0);
                return;
            }

            pszData = pszBuf + RESERV_PKT_HEAD_LEN;

            {
                aos_memzero(&stAddr, sizeof(stAddr));
                lLen = sizeof(stAddr);
                lRet = recvfrom(g_lRtpRevSocket[iSession].iSocket , pszData,
                                                        ulLen, 0, (VOID *)&stAddr, (socklen_t *)&lLen);
                if(lRet <= 0)
                {
                    goto err_proc0;
                }
            }

            #endif

            if (NULL != pszBuf)
            {
                aos_dmem_free( pszBuf );
            }
        }
    }

    return;
err_proc0:
    if (NULL != pszBuf)
    {
        aos_dmem_free(pszBuf);
    }
}


void rtp_receive_socket_task(void * param)
{
    rtpsession *rtps;
    int iSession;
    U32 ulOldIP[4], ulLocalIP[4];
    //U32 ulError = 0;
    S8 *pszLocalIP = NULL;
    U32 ulProto;

    ulProto = aos_get_run_ip_protocol();
    aos_printf( 0,"Rtp_receive_socket_task create......");
    for( iSession = 0; iSession < RTP_EIA_SESSION_REAL_MAX ; iSession++ )
    {
        g_lRtpRevSocket[iSession].iSocket = -1;
        g_lRtpRevSocket[iSession].ulRevCnt = 0;
        g_lRtpRevSocket[iSession].ulTransCnt = 0;
    }

    for (; ;)
    {
        pszLocalIP = if_get_local_voc_ipaddr();
        aos_inet_pton(pszLocalIP, ulLocalIP);
        if ('\0' == pszLocalIP[0])
        {
            aos_memzero(ulOldIP, sizeof(ulOldIP));

            for( iSession = 0; iSession < RTP_EIA_SESSION_REAL_MAX ; iSession++ )
            {
            	rtps = rtp_eia_get_rtp_session(iSession/2,iSession&1);
                if( rtps == NULL )
                {
                    AOS_ASSERT( 0 );
                    break;
                }


                if(g_lRtpRevSocket[iSession].iSocket > 0)
                {
                    close(g_lRtpRevSocket[iSession].iSocket);
                    g_lRtpRevSocket[iSession].iSocket = -1;
                }
            }

            aos_task_delay(1000);
            continue;

        }
        else if (0 != aos_memcmp(ulOldIP, ulLocalIP, sizeof(ulOldIP)))
        {
            iMaxSession = rtp_socket_create();
            aos_memcpy(ulOldIP, ulLocalIP, sizeof(ulOldIP));
            aos_printf(0, "ip up, ip:%s", if_get_local_voc_ipaddr());
        }
        else
        {
            rtp_socket_receive(iMaxSession );
        }

        aos_task_delay( 10 );
    }
}
