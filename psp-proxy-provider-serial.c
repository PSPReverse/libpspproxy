/** @file
 * PSP proxy library to interface with the hardware of the PSP - remote access over serial
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <poll.h>
#include <sys/ioctl.h>

#include "psp-proxy-provider.h"
#include "psp-stub-pdu.h"


/**
 * Internal PSP proxy provider context.
 */
typedef struct PSPPROXYPROVCTXINT
{
    /** The file descriptor of the device proxying our calls. */
    int                             iFdDev;
    /** The PDU context. */
    PSPSTUBPDUCTX                   hPduCtx;
} PSPPROXYPROVCTXINT;
/** Pointer to an internal PSP proxy context. */
typedef PSPPROXYPROVCTXINT *PPSPPROXYPROVCTXINT;



/**
 * @copydoc{PSPSTUBPDUIOIF,pfnPeek}
 */
static size_t serialProvPduIoIfPeek(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser)
{
    (void)hPspStubPduCtx;

    PPSPPROXYPROVCTXINT pThis = (PPSPPROXYPROVCTXINT)pvUser;
    int cbAvail = 0;
    int rc = ioctl(pThis->iFdDev, FIONREAD, &cbAvail);
    if (rc)
        return 0;

    return cbAvail;
}


/**
 * @copydoc{PSPSTUBPDUIOIF,pfnRead}
 */
static int serialProvPduIoIfRead(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser, void *pvDst, size_t cbRead, size_t *pcbRead)
{
    (void)hPspStubPduCtx;

    PPSPPROXYPROVCTXINT pThis = (PPSPPROXYPROVCTXINT)pvUser;
    ssize_t cbRet = read(pThis->iFdDev, pvDst, cbRead);
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
static int serialProvPduIoIfWrite(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser, const void *pvPkt, size_t cbPkt)
{
    (void)hPspStubPduCtx;

    PPSPPROXYPROVCTXINT pThis = (PPSPPROXYPROVCTXINT)pvUser;
    ssize_t cbRet = write(pThis->iFdDev, pvPkt, cbPkt);
    if (cbRet == cbPkt)
        return 0;

    return -1;
}


/**
 * @copydoc{PSPSTUBPDUIOIF,pfnPoll}
 */
static int serialProvPduIoIfPoll(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser, uint32_t cMillies)
{
    (void)hPspStubPduCtx;
    PPSPPROXYPROVCTXINT pStub = (PPSPPROXYPROVCTXINT)pvUser;
    struct pollfd PollFd;

    PollFd.fd      = pStub->iFdDev;
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
static int serialProvPduIoIfInterrupt(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser)
{
    return -1; /** @todo */
}


/**
 * I/O interface callback table.
 */
static const PSPSTUBPDUIOIF g_PduIoIf =
{
    /** pfnPeek */
    serialProvPduIoIfPeek,
    /** pfnRead */
    serialProvPduIoIfRead,
    /** pfnWrite */
    serialProvPduIoIfWrite,
    /** pfnPoll */
    serialProvPduIoIfPoll,
    /** pfnInterrupt */
    serialProvPduIoIfInterrupt
};


/**
 * @copydoc{PSPPROXYPROV,pfnCtxInit}
 */
int serialProvCtxInit(PSPPROXYPROVCTX hProvCtx, const char *pszDevice)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    int rc = 0;

    int iFd = open(pszDevice, O_RDWR);
    if (iFd > 0)
        pThis->iFdDev = iFd;
    else
        rc = -1; /** @todo Error handling. */

    return rc;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxDestroy}
 */
void serialProvCtxDestroy(PSPPROXYPROVCTX hProvCtx)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;

    pspStubPduCtxDestroy(pThis->hPduCtx);
    close(pThis->iFdDev);
    pThis->iFdDev = 0;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxQueryInfo}
 */
static int serialProvCtxQueryInfo(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR *pPspAddrScratchStart, size_t *pcbScratch)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxQueryInfo(pThis->hPduCtx, idCcd, pPspAddrScratchStart, pcbScratch);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspSmnRead}
 */
int serialProvCtxPspSmnRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspSmnRead(pThis->hPduCtx, idCcd, idCcdTgt, uSmnAddr, cbVal, pvVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspSmnWrite}
 */
int serialProvCtxPspSmnWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspSmnWrite(pThis->hPduCtx, idCcd, idCcdTgt, uSmnAddr, cbVal, pvVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMemRead}
 */
int serialProvCtxPspMemRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, void *pvBuf, uint32_t cbRead)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspMemRead(pThis->hPduCtx, idCcd, uPspAddr, pvBuf, cbRead);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMemWrite}
 */
int serialProvCtxPspMemWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspMemWrite(pThis->hPduCtx, idCcd, uPspAddr, pvBuf, cbWrite);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMmioRead}
 */
int serialProvCtxPspMmioRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspMmioRead(pThis->hPduCtx, idCcd, uPspAddr, pvVal, cbVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMmioWrite}
 */
int serialProvCtxPspMmioWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspMmioWrite(pThis->hPduCtx, idCcd, uPspAddr, pvVal, cbVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MemRead}
 */
int serialProvCtxPspX86MemRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, void *pvBuf, uint32_t cbRead)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspX86MemRead(pThis->hPduCtx, idCcd, PhysX86Addr, pvBuf, cbRead);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MemWrite}
 */
int serialProvCtxPspX86MemWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspX86MemWrite(pThis->hPduCtx, idCcd, PhysX86Addr, pvBuf, cbWrite);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MmioRead}
 */
int serialProvCtxPspX86MmioRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspX86MmioRead(pThis->hPduCtx, idCcd, PhysX86Addr, pvVal, cbVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MmioWrite}
 */
int serialProvCtxPspX86MmioWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspX86MmioWrite(pThis->hPduCtx, idCcd, PhysX86Addr, pvVal, cbVal);
}


/**
 * Provider registration structure.
 */
const PSPPROXYPROV g_PspProxyProvSerial =
{
    /** pszId */
    "serial",
    /** pszDesc */
    "PSP access through a serial connection, device schema looks like serial://<device path>:<baudrate>:<parity [e|n|o]>:<data bit count>",
    /** cbCtx */
    sizeof(PSPPROXYPROVCTXINT),
    /** fFeatures */
    0,
    /** pfnCtxInit */
    serialProvCtxInit,
    /** pfnCtxDestroy */
    serialProvCtxDestroy,
    /** pfnCtxQueryInfo */
    serialProvCtxQueryInfo,
    /** pfnCtxPspSmnRead */
    serialProvCtxPspSmnRead,
    /** pfnCtxPspSmnWrite */
    serialProvCtxPspSmnWrite,
    /** pfnCtxPspMemRead */
    serialProvCtxPspMemRead,
    /** pfnCtxPspMemWrite */
    serialProvCtxPspMemWrite,
    /** pfnCtxPspMmioRead */
    serialProvCtxPspMmioRead,
    /** pfnCtxPspMmioWrite */
    serialProvCtxPspMmioWrite,
    /** pfnCtxPspX86MemRead */
    serialProvCtxPspX86MemRead,
    /** pfnCtxPspX86MemWrite */
    serialProvCtxPspX86MemWrite,
    /** pfnCtxPspX86MmioRead */
    serialProvCtxPspX86MmioRead,
    /** pfnCtxPspX86MmioWrite */
    serialProvCtxPspX86MmioWrite,
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

