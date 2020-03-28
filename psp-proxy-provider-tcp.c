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
#include <netdb.h>

#include <poll.h>
#include <sys/ioctl.h>

#include "psp-proxy-provider.h"
#include "psp-stub-pdu.h"


/**
 * Internal PSP proxy provider context.
 */
typedef struct PSPPROXYPROVCTXINT
{
    /** The socket descriptor for the connection. */
    int                             iFdCon;
    /** The PDU context. */
    PSPSTUBPDUCTX                   hPduCtx;
} PSPPROXYPROVCTXINT;
/** Pointer to an internal PSP proxy context. */
typedef PSPPROXYPROVCTXINT *PPSPPROXYPROVCTXINT;



/**
 * @copydoc{PSPSTUBPDUIOIF,pfnPeek}
 */
static size_t tcpProvPduIoIfPeek(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser)
{
    (void)hPspStubPduCtx;

    PPSPPROXYPROVCTXINT pThis = (PPSPPROXYPROVCTXINT)pvUser;
    int cbAvail = 0;
    int rc = ioctl(pThis->iFdCon, FIONREAD, &cbAvail);
    if (rc)
        return 0;

    return cbAvail;
}


/**
 * @copydoc{PSPSTUBPDUIOIF,pfnRead}
 */
static int tcpProvPduIoIfRead(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser, void *pvDst, size_t cbRead, size_t *pcbRead)
{
    (void)hPspStubPduCtx;

    PPSPPROXYPROVCTXINT pThis = (PPSPPROXYPROVCTXINT)pvUser;
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
 * @copydoc{PSPSTUBPDUIOIF,pfnWrite}
 */
static int tcpProvPduIoIfWrite(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser, const void *pvPkt, size_t cbPkt)
{
    (void)hPspStubPduCtx;

    PPSPPROXYPROVCTXINT pThis = (PPSPPROXYPROVCTXINT)pvUser;
    ssize_t cbRet = send(pThis->iFdCon, pvPkt, cbPkt, 0);
    if (cbRet == cbPkt)
        return 0;

    return -1;
}


/**
 * @copydoc{PSPSTUBPDUIOIF,pfnPoll}
 */
static int tcpProvPduIoIfPoll(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser, uint32_t cMillies)
{
    (void)hPspStubPduCtx;
    PPSPPROXYPROVCTXINT pStub = (PPSPPROXYPROVCTXINT)pvUser;
    struct pollfd PollFd;

    PollFd.fd      = pStub->iFdCon;
    PollFd.events  = POLLIN | POLLHUP | POLLERR;
    PollFd.revents = 0;

    int rc = 0;
    for (;;)
    {
        int rcPsx = poll(&PollFd, 1, cMillies);
        if (rcPsx == 1)
            break; /* Stop polling if the single descriptor has events. */
        if (rcPsx == -1)
            rc = -1; /** @todo Better status codes for the individual errors. */
    }

    return rc;
}


/**
 * @copydoc{PSPSTUBPDUIOIF,pfnInterrupt}
 */
static int tcpProvPduIoIfInterrupt(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser)
{
    return -1; /** @todo */
}


/**
 * I/O interface callback table.
 */
static const PSPSTUBPDUIOIF g_PduIoIf =
{
    /** pfnPeek */
    tcpProvPduIoIfPeek,
    /** pfnRead */
    tcpProvPduIoIfRead,
    /** pfnWrite */
    tcpProvPduIoIfWrite,
    /** pfnPoll */
    tcpProvPduIoIfPoll,
    /** pfnInterrupt */
    tcpProvPduIoIfInterrupt
};


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
                    int rcPsx = connect(pThis->iFdCon,(struct sockaddr *)&SrvAddr,sizeof(SrvAddr));
                    if (!rcPsx)
                    {
                        /* Create the PDU context. */
                        rc = pspStubPduCtxCreate(&pThis->hPduCtx, &g_PduIoIf, pThis);
                        if (!rc)
                        {
                            rc = pspStubPduCtxConnect(pThis->hPduCtx, 10 * 1000);
                            if (!rc)
                                return 0;

                            pspStubPduCtxDestroy(pThis->hPduCtx);
                        }
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

    pspStubPduCtxDestroy(pThis->hPduCtx);
    shutdown(pThis->iFdCon, SHUT_RDWR);
    close(pThis->iFdCon);
    pThis->iFdCon = 0;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxQueryInfo}
 */
static int tcpProvCtxQueryInfo(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR *pPspAddrScratchStart, size_t *pcbScratch)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxQueryInfo(pThis->hPduCtx, idCcd, pPspAddrScratchStart, pcbScratch);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspSmnRead}
 */
static int tcpProvCtxPspSmnRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspSmnRead(pThis->hPduCtx, idCcd, idCcdTgt, uSmnAddr, cbVal, pvVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspSmnWrite}
 */
static int tcpProvCtxPspSmnWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspSmnWrite(pThis->hPduCtx, idCcd, idCcdTgt, uSmnAddr, cbVal, pvVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMemRead}
 */
static int tcpProvCtxPspMemRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, void *pvBuf, uint32_t cbRead)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspMemRead(pThis->hPduCtx, idCcd, uPspAddr, pvBuf, cbRead);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMemWrite}
 */
static int tcpProvCtxPspMemWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspMemWrite(pThis->hPduCtx, idCcd, uPspAddr, pvBuf, cbWrite);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMmioRead}
 */
static int tcpProvCtxPspMmioRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspMmioRead(pThis->hPduCtx, idCcd, uPspAddr, pvVal, cbVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMmioWrite}
 */
static int tcpProvCtxPspMmioWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspMmioWrite(pThis->hPduCtx, idCcd, uPspAddr, pvVal, cbVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MemRead}
 */
static int tcpProvCtxPspX86MemRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, void *pvBuf, uint32_t cbRead)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspX86MemRead(pThis->hPduCtx, idCcd, PhysX86Addr, pvBuf, cbRead);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MemWrite}
 */
static int tcpProvCtxPspX86MemWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspX86MemWrite(pThis->hPduCtx, idCcd, PhysX86Addr, pvBuf, cbWrite);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MmioRead}
 */
static int tcpProvCtxPspX86MmioRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspX86MmioRead(pThis->hPduCtx, idCcd, PhysX86Addr, pvVal, cbVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MmioWrite}
 */
static int tcpProvCtxPspX86MmioWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspX86MmioWrite(pThis->hPduCtx, idCcd, PhysX86Addr, pvVal, cbVal);
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
    /** pfnCtxQueryInfo */
    tcpProvCtxQueryInfo,
    /** pfnCtxPspSmnRead */
    tcpProvCtxPspSmnRead,
    /** pfnCtxPspSmnWrite */
    tcpProvCtxPspSmnWrite,
    /** pfnCtxPspMemRead */
    tcpProvCtxPspMemRead,
    /** pfnCtxPspMemWrite */
    tcpProvCtxPspMemWrite,
    /** pfnCtxPspMmioRead */
    tcpProvCtxPspMmioRead,
    /** pfnCtxPspMmioWrite */
    tcpProvCtxPspMmioWrite,
    /** pfnCtxPspX86MemRead */
    tcpProvCtxPspX86MemRead,
    /** pfnCtxPspX86MemWrite */
    tcpProvCtxPspX86MemWrite,
    /** pfnCtxPspX86MmioRead */
    tcpProvCtxPspX86MmioRead,
    /** pfnCtxPspX86MmioWrite */
    tcpProvCtxPspX86MmioWrite,
    /** pfnCtxPspSvcCall */
    NULL,
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

