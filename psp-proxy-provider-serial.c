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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <psp-stub/psp-serial-stub.h>

#include "psp-proxy-provider.h"


/**
 * Internal PSP proxy provider context.
 */
typedef struct PSPPROXYPROVCTXINT
{
    /** The file descriptor of the device proxying our calls. */
    int                             iFdDev;
} PSPPROXYPROVCTXINT;
/** Pointer to an internal PSP proxy context. */
typedef PSPPROXYPROVCTXINT *PPSPPROXYPROVCTXINT;



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

    close(pThis->iFdDev);
    pThis->iFdDev = 0;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxQueryInfo}
 */
static int serialProvCtxQueryInfo(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR *pPspAddrScratchStart, size_t *pcbScratch)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return -1;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspSmnRead}
 */
int serialProvCtxPspSmnRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return -1;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspSmnWrite}
 */
int serialProvCtxPspSmnWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return -1;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMemRead}
 */
int serialProvCtxPspMemRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, void *pvBuf, uint32_t cbRead)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return -1;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMemWrite}
 */
int serialProvCtxPspMemWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return -1;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MemRead}
 */
int serialProvCtxPspX86MemRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, void *pvBuf, uint32_t cbRead)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return -1;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MemWrite}
 */
int serialProvCtxPspX86MemWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return -1;
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
    /** pfnCtxPspX86MemRead */
    serialProvCtxPspX86MemRead,
    /** pfnCtxPspX86MemWrite */
    serialProvCtxPspX86MemWrite,
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

