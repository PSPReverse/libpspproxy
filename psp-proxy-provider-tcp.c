/** @file
 * PSP proxy library to interface with the hardware of the PSP - remote access over a TCP socket
 */

/*
 * Copyright (C) 2020 Alexander Eichner <alexander.eichner@campus.tu-berlin.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define _DEFAULT_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include <poll.h>
#include <sys/ioctl.h>

#include "psp-proxy-provider.h"


/**
 * Internal PSP proxy provider context.
 */
typedef struct PSPPROXYPROVCTXINT
{
    /** The socket descriptor for the connection. */
    int                             iFdCon;
} PSPPROXYPROVCTXINT;
/** Pointer to an internal PSP proxy context. */
typedef PSPPROXYPROVCTXINT *PPSPPROXYPROVCTXINT;


/**
 * @copydoc{PSPPROXYPROV,pfnCtxInit}
 */
static int tcpProvCtxInit(PSPPROXYPROVCTX hProvCtx, const char *pszDevice)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    int rc = 0;
    char szDev[256]; /* Should be plenty. */

    memset(&szDev[0], 0, sizeof(szDev));
    strncpy(&szDev[0], pszDevice, sizeof(szDev));
    if (szDev[sizeof(szDev) - 1] == '\0')
    {
        char *pszSep = strchr(&szDev[0], ':');
        if (pszSep)
        {
            *pszSep = '\0';
            pszSep++;
            struct hostent *pSrv = gethostbyname(&szDev[0]);
            if (pSrv)
            {
                struct sockaddr_in SrvAddr;
                memset(&SrvAddr, 0, sizeof(SrvAddr));
                SrvAddr.sin_family = AF_INET;
                memcpy(&SrvAddr.sin_addr.s_addr, pSrv->h_addr, pSrv->h_length);
                SrvAddr.sin_port = htons(atoi(pszSep));

                pThis->iFdCon = socket(AF_INET, SOCK_STREAM, 0);
                if (pThis->iFdCon > -1)
                {
                    /* Disable nagle. */
                    int fNoDelay = 1;
                    int rcPsx = setsockopt(pThis->iFdCon, IPPROTO_TCP, TCP_NODELAY, &fNoDelay, sizeof(int));
                    if (!rcPsx)
                    {
                        int rcPsx = connect(pThis->iFdCon,(struct sockaddr *)&SrvAddr,sizeof(SrvAddr));
                        if (!rcPsx)
                            return 0;
                        else
                            rc = -1;
                    }
                    else
                        rc = -1;

                    close(pThis->iFdCon);
                }
                else
                    rc = -1;
            }
            else
                rc = -1;
        }
        else
            rc = -1;
    }
    else
        rc = -1;

    return rc;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxDestroy}
 */
static void tcpProvCtxDestroy(PSPPROXYPROVCTX hProvCtx)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;

    shutdown(pThis->iFdCon, SHUT_RDWR);
    close(pThis->iFdCon);
    pThis->iFdCon = 0;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPeek}
 */
static size_t tcpProvCtxPeek(PSPPROXYPROVCTX hProvCtx)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;

    int cbAvail = 0;
    int rc = ioctl(pThis->iFdCon, FIONREAD, &cbAvail);
    if (rc)
        return 0;

    return cbAvail;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxRead}
 */
static int tcpProvCtxRead(PSPPROXYPROVCTX hProvCtx, void *pvDst, size_t cbRead, size_t *pcbRead)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;

    ssize_t cbRet = recv(pThis->iFdCon, pvDst, cbRead, MSG_DONTWAIT);
    if (cbRet > 0)
    {
        *pcbRead = cbRead;
        return 0;
    }

    if (!cbRet)
        return -1;

    if (errno == EAGAIN || errno == EWOULDBLOCK)
        return 0;

    return -1;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxWrite}
 */
static int tcpProvCtxWrite(PSPPROXYPROVCTX hProvCtx, const void *pvPkt, size_t cbPkt)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;

    ssize_t cbRet = send(pThis->iFdCon, pvPkt, cbPkt, 0);
    if (cbRet == cbPkt)
        return 0;

    return -1;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPoll}
 */
static int tcpProvCtxPoll(PSPPROXYPROVCTX hProvCtx, uint32_t cMillies)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    struct pollfd PollFd;

    PollFd.fd      = pThis->iFdCon;
    PollFd.events  = POLLIN | POLLHUP | POLLERR;
    PollFd.revents = 0;

    int rc = 0;
    int rcPsx = poll(&PollFd, 1, cMillies);
    if (rcPsx == 0)
        rc = STS_ERR_PSP_PROXY_TIMEOUT;
    else if (rcPsx == -1)
        rc = -1; /** @todo Better status codes for the individual errors. */

    return rc;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxInterrupt}
 */
static int tcpProvCtxInterrupt(PSPPROXYPROVCTX hProvCtx)
{
    return -1; /** @todo */
}


/**
 * Provider registration structure.
 */
const PSPPROXYPROV g_PspProxyProvTcp =
{
    /** pszId */
    "tcp",
    /** pszDesc */
    "PSP access through a TCP connection, device schema looks like tcp://<hostname>:<port>",
    /** cbCtx */
    sizeof(PSPPROXYPROVCTXINT),
    /** fFeatures */
    0,
    /** pfnCtxInit */
    tcpProvCtxInit,
    /** pfnCtxDestroy */
    tcpProvCtxDestroy,
    /** pfnCtxPeek */
    tcpProvCtxPeek,
    /** pfnCtxRead */
    tcpProvCtxRead,
    /** pfnCtxWrite */
    tcpProvCtxWrite,
    /** pfnCtxPoll */
    tcpProvCtxPoll,
    /** pfnCtxInterrupt */
    tcpProvCtxInterrupt,
    /** pfnCtxX86SmnRead */
    NULL,
    /** pfnCtxX86SmnWrite */
    NULL,
    /** pfnCtxX86MemAlloc */
    NULL,
    /** pfnCtxX86MemFree */
    NULL,
    /** pfnCtxX86MemRead */
    NULL,
    /** pfnCtxX86MemWrite */
    NULL,
    /** pfnCtxX86PhysMemRead */
    NULL,
    /** pfnCtxX86PhysMemWrite */
    NULL,
    /** pfnCtxEmuWaitForWork */
    NULL,
    /** pfnCtxEmuSetResult */
    NULL
};

