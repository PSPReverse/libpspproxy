/** @file
 * PSP proxy library to interface with the hardware of the PSP
 */

/*
 * Copyright (C) 2019-2020 Alexander Eichner <alexander.eichner@campus.tu-berlin.de>
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "psp-proxy-provider.h"
#include "psp-stub-pdu.h"


/**
 * A free chunk of scratch space memory.
 */
typedef struct PSPSCRATCHCHUNKFREE
{
    /** Pointer to the next free chunk or NULL if end of list. */
    struct PSPSCRATCHCHUNKFREE     *pNext;
    /** Pointer to the previous free chunk or NULL if head of list. */
    struct PSPSCRATCHCHUNKFREE     *pPrev;
    /** Start address of the free chunk. */
    PSPADDR                        PspAddrStart;
    /** Size of the chunk. */
    size_t                         cbChunk;
} PSPSCRATCHCHUNKFREE;
/** Pointer to a free scratch space chunk. */
typedef PSPSCRATCHCHUNKFREE *PPSPSCRATCHCHUNKFREE;


/**
 * Internal PSP proxy context.
 */
typedef struct PSPPROXYCTXINT
{
    /** The current CCD ID set. */
    uint32_t                        idCcd;
    /** I/O interface. */
    PCPSPPROXYIOIF                  pIoIf;
    /** Opaque user data to pass to the I/O interface callbacks. */
    void                            *pvUser;
    /** Flag whether the scratch space manager was initialized. */
    int                             fScratchSpaceMgrInit;
    /** List of free scratch space blocks, sorted by PSP address (lowest is head). */
    PPSPSCRATCHCHUNKFREE            pScratchFreeHead;
    /** The provider used. */
    PCPSPPROXYPROV                  pProv;
    /** The stub PDU context. */
    PSPSTUBPDUCTX                   hPduCtx;
    /** The provider specific context data, variable in size. */
    uint8_t                         abProvCtx[1];
} PSPPROXYCTXINT;
/** Pointer to an internal PSP proxy context. */
typedef PSPPROXYCTXINT *PPSPPROXYCTXINT;


//extern const PSPPROXYPROV g_PspProxyProvSev;
extern const PSPPROXYPROV g_PspProxyProvSerial;
extern const PSPPROXYPROV g_PspProxyProvTcp;
//extern const PSPPROXYPROV g_PspProxyProvEm100Tcp;

/**
 * Array of known PSP proxy providers.
 */
static PCPSPPROXYPROV g_apPspProxyProv[] =
{
//    &g_PspProxyProvSev,
    &g_PspProxyProvSerial,
    &g_PspProxyProvTcp,
//    &g_PspProxyProvEm100Tcp,
    NULL
};


/**
 * Initializes the scratch space manager.
 *
 * @returns Status code.
 * @param   pThis                   The context instance.
 */
static int pspProxyCtxScratchSpaceMgrInit(PPSPPROXYCTXINT pThis)
{
    PSPADDR PspAddrScratchStart = 0;
    size_t cbScratch = 0;

    int rc = pspStubPduCtxQueryInfo(pThis->hPduCtx, pThis->idCcd, &PspAddrScratchStart, &cbScratch);
    if (!rc)
    {
        /* Set up the first chunk covering the whole scratch space area. */
        PPSPSCRATCHCHUNKFREE pFree = (PPSPSCRATCHCHUNKFREE)malloc(sizeof(*pFree));
        if (pFree)
        {
            pFree->pNext        = NULL;
            pFree->pPrev        = NULL;
            pFree->PspAddrStart = PspAddrScratchStart;
            pFree->cbChunk      = cbScratch;

            pThis->pScratchFreeHead = pFree;
            pThis->fScratchSpaceMgrInit = 1;
        }
        else
            rc = -1;
    }

    return rc;
}


/**
 * Finds the appropriate proxy provider from the given device URI.
 *
 * @returns Pointer to the matching provider or NULL if none was found.
 * @param   pszDevice               The device URI to match.
 * @param   ppszDevRem              Where to store the pointer to remainder of the device string passed to the provider
 *                                  during initialization.
 */
static PCPSPPROXYPROV pspProxyCtxProvFind(const char *pszDevice, const char **ppszDevRem)
{
    size_t cchDevice = strlen(pszDevice);
    const char *pszSep = strchr(pszDevice, ':');

    if (   pszSep
        && cchDevice - (pszSep - pszDevice) >= 3
        && pszSep[1] == '/'
        && pszSep[2] == '/')
    {
        size_t cchProv = pszSep - pszDevice;
        PCPSPPROXYPROV *ppProv = &g_apPspProxyProv[0];

        while (*ppProv)
        {
            if (!strncmp(pszDevice, (*ppProv)->pszId, cchProv))
            {
                *ppszDevRem = pszSep + 3;
                return *ppProv; /* Found */
            }

            ppProv++;
        }
    }

    return NULL;
}


int PSPProxyCtxCreate(PPSPPROXYCTX phCtx, const char *pszDevice, PCPSPPROXYIOIF pIoIf,
                      void *pvUser)
{
    int rc = 0;

    const char *pszDevRem = NULL;
    PCPSPPROXYPROV pProv = pspProxyCtxProvFind(pszDevice, &pszDevRem);
    if (pProv)
    {
        PPSPPROXYCTXINT pThis = (PPSPPROXYCTXINT)calloc(1, sizeof(*pThis) + pProv->cbCtx);
        if (pThis != NULL)
        {
            pThis->idCcd                = 0;
            pThis->pIoIf                = pIoIf;
            pThis->pvUser               = pvUser;
            pThis->fScratchSpaceMgrInit = 0;
            pThis->pProv                = pProv;
            rc = pProv->pfnCtxInit((PSPPROXYPROVCTX)&pThis->abProvCtx[0], pszDevRem);
            if (!rc)
            {
                /* Create the PDU context. */
                rc = pspStubPduCtxCreate(&pThis->hPduCtx, pProv, (PSPPROXYPROVCTX)&pThis->abProvCtx[0],
                                         pIoIf, pThis, pvUser);
                if (!rc)
                {
                    rc = pspStubPduCtxConnect(pThis->hPduCtx, 10 * 1000);
                    if (!rc)
                    {
                        *phCtx = pThis;
                        return 0;
                    }

                    pspStubPduCtxDestroy(pThis->hPduCtx);
                }

                pThis->pProv->pfnCtxDestroy((PSPPROXYPROVCTX)&pThis->abProvCtx[0]);
            }

            free(pThis);
        }
        else
            rc = -1;
    }
    else
        rc = -1;

    return rc;
}


void PSPProxyCtxDestroy(PSPPROXYCTX hCtx)
{
    PPSPPROXYCTXINT pThis = hCtx;

    pspStubPduCtxDestroy(pThis->hPduCtx);
    pThis->pProv->pfnCtxDestroy((PSPPROXYPROVCTX)&pThis->abProvCtx[0]);
    free(pThis);
}


int PSPProxyCtxPspCcdSet(PSPPROXYCTX hCtx, uint32_t idCcd)
{
    PPSPPROXYCTXINT pThis = hCtx;

    /** @todo Check that the ID is in range. */
    pThis->idCcd = idCcd;
    return 0;
}

int PSPProxyCtxQueryLastReqRc(PSPPROXYCTX hCtx, PSPSTS *pReqRcLast)
{
    PPSPPROXYCTXINT pThis = hCtx;

    return pspStubPduCtxQueryLastReqRc(pThis->hPduCtx, pReqRcLast);
}

int PSPProxyCtxPspSmnRead(PSPPROXYCTX hCtx, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYCTXINT pThis = hCtx;

    return pspStubPduCtxPspSmnRead(pThis->hPduCtx, pThis->idCcd, idCcdTgt, uSmnAddr, cbVal, pvVal);
}


int PSPProxyCtxPspSmnWrite(PSPPROXYCTX hCtx, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYCTXINT pThis = hCtx;

    return pspStubPduCtxPspSmnWrite(pThis->hPduCtx, pThis->idCcd, idCcdTgt, uSmnAddr, cbVal, pvVal);
}


int PSPProxyCtxPspMemRead(PSPPROXYCTX hCtx, PSPADDR uPspAddr, void *pvBuf, uint32_t cbRead)
{
    PPSPPROXYCTXINT pThis = hCtx;

    return pspStubPduCtxPspMemRead(pThis->hPduCtx, pThis->idCcd, uPspAddr, pvBuf, cbRead);
}


int PSPProxyCtxPspMemWrite(PSPPROXYCTX hCtx, PSPADDR uPspAddr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPPROXYCTXINT pThis = hCtx;

    return pspStubPduCtxPspMemWrite(pThis->hPduCtx, pThis->idCcd, uPspAddr, pvBuf, cbWrite);
}


int PSPProxyCtxPspMmioRead(PSPPROXYCTX hCtx, PSPADDR uPspAddr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYCTXINT pThis = hCtx;

    return pspStubPduCtxPspMmioRead(pThis->hPduCtx, pThis->idCcd, uPspAddr, pvVal, cbVal);
}


int PSPProxyCtxPspMmioWrite(PSPPROXYCTX hCtx, PSPADDR uPspAddr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYCTXINT pThis = hCtx;

    return pspStubPduCtxPspMmioWrite(pThis->hPduCtx, pThis->idCcd, uPspAddr, pvVal, cbVal);
}


int PSPProxyCtxPspX86MemRead(PSPPROXYCTX hCtx, X86PADDR PhysX86Addr, void *pvBuf, uint32_t cbRead)
{
    PPSPPROXYCTXINT pThis = hCtx;

    return pspStubPduCtxPspX86MemRead(pThis->hPduCtx, pThis->idCcd, PhysX86Addr, pvBuf, cbRead);
}


int PSPProxyCtxPspX86MemWrite(PSPPROXYCTX hCtx, X86PADDR PhysX86Addr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPPROXYCTXINT pThis = hCtx;

    return pspStubPduCtxPspX86MemWrite(pThis->hPduCtx, pThis->idCcd, PhysX86Addr, pvBuf, cbWrite);
}


int PSPProxyCtxPspX86MmioRead(PSPPROXYCTX hCtx, X86PADDR PhysX86Addr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYCTXINT pThis = hCtx;

    return pspStubPduCtxPspX86MmioRead(pThis->hPduCtx, pThis->idCcd, PhysX86Addr, pvVal, cbVal);
}


int PSPProxyCtxPspX86MmioWrite(PSPPROXYCTX hCtx, X86PADDR PhysX86Addr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYCTXINT pThis = hCtx;

    return pspStubPduCtxPspX86MmioWrite(pThis->hPduCtx, pThis->idCcd, PhysX86Addr, pvVal, cbVal);
}


int PSPProxyCtxPspSvcCall(PSPPROXYCTX hCtx, uint32_t idxSyscall, uint32_t u32R0, uint32_t u32R1, uint32_t u32R2, uint32_t u32R3, uint32_t *pu32R0Return)
{
    PPSPPROXYCTXINT pThis = hCtx;

#if 0 /** @todo Reintroduce again. */
    if (pThis->pProv->pfnCtxPspSvcCall)
        return pThis->pProv->pfnCtxPspSvcCall((PSPPROXYPROVCTX)&pThis->abProvCtx[0], pThis->idCcd,
                                              idxSyscall, u32R0, u32R1, u32R2, u32R3, pu32R0Return);
#endif

    return -1;
}


int PSPProxyCtxPspAddrXfer(PSPPROXYCTX hCtx, PCPSPPROXYADDR pPspAddr, uint32_t fFlags, size_t cbStride, size_t cbXfer, void *pvLocal)
{
    PPSPPROXYCTXINT pThis = hCtx;

    if (   cbStride != 1
        && cbStride != 2
        && cbStride != 4)
        return -1;
    if (cbXfer % cbStride != 0)
        return -1;
    uint32_t fOp = fFlags & PSPPROXY_CTX_ADDR_XFER_F_OP_MASK_VALID; /* Only set flag is allowed. */
    if (   (fOp & PSPPROXY_CTX_ADDR_XFER_F_READ) != PSPPROXY_CTX_ADDR_XFER_F_READ
        && (fOp & PSPPROXY_CTX_ADDR_XFER_F_WRITE) != PSPPROXY_CTX_ADDR_XFER_F_WRITE
        && (fOp & PSPPROXY_CTX_ADDR_XFER_F_MEMSET) != PSPPROXY_CTX_ADDR_XFER_F_MEMSET)
        return -1;

    return pspStubPduCtxPspAddrXfer(pThis->hPduCtx, pThis->idCcd, pPspAddr, fFlags, cbStride, cbXfer, pvLocal);
}


int PSPProxyCtxPspCoProcWrite(PSPPROXYCTX hCtx, uint8_t idCoProc, uint8_t idCrn, uint8_t idCrm, uint8_t idOpc1, uint8_t idOpc2,
                              uint32_t u32Val)
{
    PPSPPROXYCTXINT pThis = hCtx;

    return pspStubPduCtxPspCoProcWrite(pThis->hPduCtx, pThis->idCcd, idCoProc, idCrn, idCrm, idOpc1, idOpc2, u32Val);
}


int PSPProxyCtxPspCoProcRead(PSPPROXYCTX hCtx, uint8_t idCoProc, uint8_t idCrn, uint8_t idCrm, uint8_t idOpc1, uint8_t idOpc2,
                             uint32_t *pu32Val)
{
    PPSPPROXYCTXINT pThis = hCtx;

    return pspStubPduCtxPspCoProcRead(pThis->hPduCtx, pThis->idCcd, idCoProc, idCrn, idCrm, idOpc1, idOpc2, pu32Val);
}


int PSPProxyCtxPspWaitForIrq(PSPPROXYCTX hCtx, uint32_t *pidCcd, bool *pfIrq, bool *pfFirq, uint32_t cWaitMs)
{
    PPSPPROXYCTXINT pThis = hCtx;

    return pspStubPduCtxPspWaitForIrq(pThis->hPduCtx, pidCcd, pfIrq, pfFirq, cWaitMs);
}


int PSPProxyCtxX86SmnRead(PSPPROXYCTX hCtx, uint16_t idNode, SMNADDR uSmnAddr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYCTXINT pThis = hCtx;

    if (cbVal != 4)
        return -1;

    if (pThis->pProv->pfnCtxX86SmnRead)
        return pThis->pProv->pfnCtxX86SmnRead((PSPPROXYPROVCTX)&pThis->abProvCtx[0], idNode, uSmnAddr, cbVal, pvVal);

    return -1;
}


int PSPProxyCtxX86SmnWrite(PSPPROXYCTX hCtx, uint16_t idNode, SMNADDR uSmnAddr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYCTXINT pThis = hCtx;

    if (cbVal != 4)
        return -1;

    if (pThis->pProv->pfnCtxX86SmnWrite)
        return pThis->pProv->pfnCtxX86SmnWrite((PSPPROXYPROVCTX)&pThis->abProvCtx[0], idNode, uSmnAddr, cbVal, pvVal);

    return -1;
}

int PSPProxyCtxX86MemAlloc(PSPPROXYCTX hCtx, uint32_t cbMem, R0PTR *pR0KernVirtual, X86PADDR *pPhysX86Addr)
{
    PPSPPROXYCTXINT pThis = hCtx;

    if (!pR0KernVirtual && !pPhysX86Addr)
        return -1;

    if (pThis->pProv->pfnCtxX86MemAlloc)
        return pThis->pProv->pfnCtxX86MemAlloc((PSPPROXYPROVCTX)&pThis->abProvCtx[0], cbMem, pR0KernVirtual, pPhysX86Addr);

    return -1;
}

int PSPProxyCtxX86MemFree(PSPPROXYCTX hCtx, R0PTR R0KernVirtual)
{
    PPSPPROXYCTXINT pThis = hCtx;

    if (pThis->pProv->pfnCtxX86MemFree)
        return pThis->pProv->pfnCtxX86MemFree((PSPPROXYPROVCTX)&pThis->abProvCtx[0], R0KernVirtual);

    return -1;
}

int PSPProxyCtxX86MemRead(PSPPROXYCTX hCtx, void *pvDst, R0PTR R0KernVirtualSrc, uint32_t cbRead)
{
    PPSPPROXYCTXINT pThis = hCtx;

    if (pThis->pProv->pfnCtxX86MemRead)
        return pThis->pProv->pfnCtxX86MemRead((PSPPROXYPROVCTX)&pThis->abProvCtx[0], pvDst, R0KernVirtualSrc, cbRead);

    return -1;
}

int PSPProxyCtxX86MemWrite(PSPPROXYCTX hCtx, R0PTR R0KernVirtualDst, const void *pvSrc, uint32_t cbWrite)
{
    PPSPPROXYCTXINT pThis = hCtx;

#if 0 /** @todo Reintroduce again. */
    if (pThis->pProv->pfnCtxX86MemWrite)
        return pThis->pProv->pfnCtxX86MemWrite((PSPPROXYPROVCTX)&pThis->abProvCtx[0], R0KernVirtualDst, pvSrc, cbWrite);
#endif

    return -1;
}

int PSPProxyCtxX86PhysMemRead(PSPPROXYCTX hCtx, void *pvDst, X86PADDR PhysX86AddrSrc, uint32_t cbRead)
{
    PPSPPROXYCTXINT pThis = hCtx;

    if (pThis->pProv->pfnCtxX86PhysMemRead)
        return pThis->pProv->pfnCtxX86PhysMemRead((PSPPROXYPROVCTX)&pThis->abProvCtx[0], pvDst, PhysX86AddrSrc, cbRead);

    return -1;
}

int PSPProxyCtxX86PhysMemWrite(PSPPROXYCTX hCtx, X86PADDR PhysX86AddrDst, const void *pvSrc, uint32_t cbWrite)
{
    PPSPPROXYCTXINT pThis = hCtx;

    if (pThis->pProv->pfnCtxX86PhysMemWrite)
        return pThis->pProv->pfnCtxX86PhysMemWrite((PSPPROXYPROVCTX)&pThis->abProvCtx[0], PhysX86AddrDst, pvSrc, cbWrite);

    return -1;
}

int PSPProxyCtxEmuWaitForWork(PSPPROXYCTX hCtx, uint32_t *pidCmd, X86PADDR *pPhysX86AddrCmdBuf, uint32_t msWait)
{
    PPSPPROXYCTXINT pThis = hCtx;

    if (pThis->pProv->pfnCtxEmuWaitForWork)
        return pThis->pProv->pfnCtxEmuWaitForWork((PSPPROXYPROVCTX)&pThis->abProvCtx[0], pidCmd, pPhysX86AddrCmdBuf, msWait);

    return -1;
}

int PSPProxyCtxEmuSetResult(PSPPROXYCTX hCtx, uint32_t uResult)
{
    PPSPPROXYCTXINT pThis = hCtx;

    if (pThis->pProv->pfnCtxEmuSetResult)
        return pThis->pProv->pfnCtxEmuSetResult((PSPPROXYPROVCTX)&pThis->abProvCtx[0], uResult);

    return -1;
}

int PSPProxyCtxScratchSpaceAlloc(PSPPROXYCTX hCtx, size_t cbAlloc, PSPADDR *pPspAddr)
{
    PPSPPROXYCTXINT pThis = hCtx;

    if (!pThis->fScratchSpaceMgrInit)
    {
        int rc = pspProxyCtxScratchSpaceMgrInit(pThis);
        if (rc)
            return rc;
    }

    /** @todo Align size on 8 byte boundary maybe.
     * This is a very very simple "heap" manager (doesn't really deserve the name),
     * enough for our purpose but don't have to high expectations on it...
     */

    /* Find the most optimal chunk first (best match). */
    PPSPSCRATCHCHUNKFREE pChunkBest = NULL;
    PPSPSCRATCHCHUNKFREE pChunkCur = pThis->pScratchFreeHead;

    while (pChunkCur)
    {
        if (   pChunkCur->cbChunk >= cbAlloc
            && (   !pChunkBest
                || pChunkBest->cbChunk > pChunkCur->cbChunk))
        {
            pChunkBest = pChunkCur;

            /* No point in going further in case of an exact match. */
            if (pChunkBest->cbChunk == cbAlloc)
                break;
        }

        pChunkCur = pChunkCur->pNext;
    }

    if (pChunkBest)
    {
        if (pChunkBest->cbChunk == cbAlloc)
        {
            /* Remove free chunk from list. */
            if (pChunkBest->pPrev)
                pChunkBest->pPrev->pNext = pChunkBest->pNext;
            else
                pThis->pScratchFreeHead = pChunkBest->pNext;

            if (pChunkBest->pNext)
                pChunkBest->pNext->pPrev = pChunkBest->pPrev;

            *pPspAddr = pChunkBest->PspAddrStart;
            free(pChunkBest);
        }
        else
        {
            /* Resize chunk and leave everything else in place. */
            size_t cbLeft = pChunkBest->cbChunk - cbAlloc;
            *pPspAddr = pChunkBest->PspAddrStart + cbLeft;
            pChunkBest->cbChunk = cbLeft;
        }

        return 0;
    }

    return -1;
}

int PSPProxyCtxScratchSpaceFree(PSPPROXYCTX hCtx, PSPADDR PspAddr, size_t cb)
{
    PPSPPROXYCTXINT pThis = hCtx;

    /** @todo Align size on 8 byte boundary when done in the alloc method too. */

    if (!pThis->pScratchFreeHead)
    {
        /* No free chunk left, create the first one. */
        PPSPSCRATCHCHUNKFREE pChunk = (PPSPSCRATCHCHUNKFREE)malloc(sizeof(*pChunk));
        if (!pChunk)
            return -1;

        pChunk->pNext        = NULL;
        pChunk->pPrev        = NULL;
        pChunk->PspAddrStart = PspAddr;
        pChunk->cbChunk      = cb;
        pThis->pScratchFreeHead = pChunk;
    }
    else
    {
        /* Find the right chunk to append to. */
        PPSPSCRATCHCHUNKFREE pChunkCur = pThis->pScratchFreeHead;

        while (pChunkCur)
        {
            /* Check whether we can append or prepend the memory to the chunk. */
            if (PspAddr + cb == pChunkCur->PspAddrStart)
            {
                /* Prepend and check whether we can merge the previous and current chunk. */
                pChunkCur->PspAddrStart = PspAddr;
                pChunkCur->cbChunk     += cb;

                if (   pChunkCur->pPrev
                    && pChunkCur->pPrev->PspAddrStart + pChunkCur->pPrev->cbChunk == pChunkCur->PspAddrStart)
                {
                    /* Merge */
                    PPSPSCRATCHCHUNKFREE pPrev = pChunkCur->pPrev;

                    pPrev->cbChunk += pChunkCur->cbChunk;
                    pPrev->pNext = pChunkCur->pNext;
                    if (pChunkCur->pNext)
                        pChunkCur->pNext->pPrev = pPrev;

                    free(pChunkCur);
                }
                break;
            }
            else if (pChunkCur->PspAddrStart + pChunkCur->cbChunk == PspAddr)
            {
                /* Append and check whether we can merge the next and current chunk. */
                pChunkCur->cbChunk += cb;

                if (   pChunkCur->pNext
                    && pChunkCur->PspAddrStart + pChunkCur->cbChunk == pChunkCur->pNext->PspAddrStart)
                {
                    /* Merge */
                    PPSPSCRATCHCHUNKFREE pNext = pChunkCur->pNext;

                    pNext->cbChunk += pChunkCur->cbChunk;
                    pNext->pPrev = pChunkCur->pPrev;
                    if (pChunkCur->pPrev)
                        pChunkCur->pPrev->pNext = pNext;
                    else
                        pThis->pScratchFreeHead = pNext;

                    free(pChunkCur);
                }
                break;
            }
            else if (   !pChunkCur->pNext
                     || (   pChunkCur->PspAddrStart + pChunkCur->cbChunk < PspAddr
                         && pChunkCur->pNext->PspAddrStart > PspAddr))
            {
                /* Insert/Append a new chunk (correct ordering). */
                PPSPSCRATCHCHUNKFREE pChunk = (PPSPSCRATCHCHUNKFREE)malloc(sizeof(*pChunk));
                if (!pChunk)
                    return -1;

                pChunk->pNext        = NULL;
                pChunk->pPrev        = NULL;
                pChunk->PspAddrStart = PspAddr;
                pChunk->cbChunk      = cb;

                PPSPSCRATCHCHUNKFREE pNext = pChunkCur->pNext;
                pChunkCur->pNext = pChunk;
                pChunk->pPrev = pChunkCur;
                if (pNext)
                {
                    pNext->pPrev = pChunk;
                    pChunk->pNext = pNext;
                }
                break;
            }

            pChunkCur = pChunkCur->pNext;
        }
    }

    return 0;
}

int PSPProxyCtxCodeModLoad(PSPPROXYCTX hCtx, const void *pvCm, size_t cbCm)
{
    PPSPPROXYCTXINT pThis = hCtx;

    return pspStubPduCtxPspCodeModLoad(pThis->hPduCtx, pThis->idCcd, pvCm, cbCm);
}

int PSPProxyCtxCodeModExec(PSPPROXYCTX hCtx, uint32_t u32Arg0, uint32_t u32Arg1, uint32_t u32Arg2, uint32_t u32Arg3,
                           uint32_t *pu32CmRet, uint32_t cMillies)
{
    PPSPPROXYCTXINT pThis = hCtx;

    return pspStubPduCtxPspCodeModExec(pThis->hPduCtx, pThis->idCcd, u32Arg0, u32Arg1, u32Arg2, u32Arg3,
                                       pu32CmRet, cMillies);
}

int PSPProxyCtxBranchTo(PSPPROXYCTX hCtx, PSPPADDR PspAddrPc, bool fThumb, uint32_t *pau32Gprs)
{
    PPSPPROXYCTXINT pThis = hCtx;

    return pspStubPduCtxBranchTo(pThis->hPduCtx, pThis->idCcd, PspAddrPc, fThumb, pau32Gprs);
}

