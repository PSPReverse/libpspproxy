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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "include/psp-sev.h"
#include "include/ptedit_header.h" /* For the x86 physical mem read/write API */
#include "libpspproxy.h"


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
    /** The file descriptor of the device proxying our calls. */
    int                             iFdDev;
    /** The current CCD ID set. */
    uint32_t                        idCcd;
    /** Flag whether the scratch space manager was initialized. */
    int                             fScratchSpaceMgrInit;
    /** List of free scratch space blocks, sorted by PSP address (lowest is head). */
    PPSPSCRATCHCHUNKFREE            pScratchFreeHead;
} PSPPROXYCTXINT;
/** Pointer to an internal PSP proxy context. */
typedef PSPPROXYCTXINT *PPSPPROXYCTXINT;



/**
 * I/O control wrapper for the SEV device.
 *
 * @returns Status code.
 * @param   pThis                   The context instance.
 * @param   idCmd                   The command to execute.
 * @param   pvReq                   The opaque request structure passed.
 * @param   pu32Error               Where to store the PSP status code on success, optional.
 */
static int pspProxyCtxIoctl(PPSPPROXYCTXINT pThis, uint32_t idCmd, void *pvArgs, uint32_t *pu32Error)
{
    struct sev_issue_cmd Cmd;

    memset(&Cmd, 0, sizeof(Cmd));
    Cmd.cmd  = idCmd;
    Cmd.data = (__u64)pvArgs;

    int rc = ioctl(pThis->iFdDev, SEV_ISSUE_CMD, &Cmd);
    if (rc != -1)
    {
        if (pu32Error)
            *pu32Error = Cmd.error;
    }

    return rc;
}


/**
 * Initializes the scratch space manager.
 *
 * @returns Status code.
 * @param   pThis                   The context instance.
 */
static int pspProxyCtxScratchSpaceMgrInit(PPSPPROXYCTXINT pThis)
{
    struct sev_user_data_query_info Req;

    memset(&Req, 0, sizeof(Req));
    Req.ccd_id = pThis->idCcd;

    int rc = pspProxyCtxIoctl(pThis, SEV_PSP_STUB_QUERY_INFO, &Req, NULL);
    if (!rc)
    {
        /* Set up the first chunk covering the whole scratch space area. */
        PPSPSCRATCHCHUNKFREE pFree = (PPSPSCRATCHCHUNKFREE)malloc(sizeof(*pFree));
        if (pFree)
        {
            pFree->pNext        = NULL;
            pFree->pPrev        = NULL;
            pFree->PspAddrStart = Req.psp_addr_scratch_start;
            pFree->cbChunk      = Req.scratch_size;

            pThis->pScratchFreeHead = pFree;
            pThis->fScratchSpaceMgrInit = 1;
        }
        else
            rc = -1;
    }

    return rc;
}

int PSPProxyCtxCreate(PPSPPROXYCTX phCtx, const char *pszDevice)
{
    int rc = 0;

    int iFd = open(pszDevice, O_RDWR);
    if (iFd > 0)
    {
        PPSPPROXYCTXINT pThis = (PPSPPROXYCTXINT)calloc(1, sizeof(*pThis));
        if (pThis != NULL)
        {
            pThis->iFdDev = iFd;
            pThis->idCcd  = 0;
            pThis->fScratchSpaceMgrInit = 0;
            *phCtx = pThis;
            return 0;
        }
        else
            rc = -1;

        close(iFd);
    }
    else
        rc = -1; /** @todo Error handling. */

    return rc;
}


void PSPProxyCtxDestroy(PSPPROXYCTX hCtx)
{
    PPSPPROXYCTXINT pThis = hCtx;

    close(pThis->iFdDev);
    pThis->iFdDev = 0;
    free(pThis);
}


int PSPProxyCtxPspCcdSet(PSPPROXYCTX hCtx, uint32_t idCcd)
{
    PPSPPROXYCTXINT pThis = hCtx;

    /** @todo Check that the ID is in range. */
    pThis->idCcd = idCcd;
    return 0;
}


int PSPProxyCtxPspSmnRead(PSPPROXYCTX hCtx, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYCTXINT pThis = hCtx;
    struct sev_user_data_psp_stub_smn_rw Req;

    if (cbVal != 1 && cbVal != 2 && cbVal != 4 && cbVal != 8)
        return -1;

    memset(&Req, 0, sizeof(Req));
    Req.ccd_id     = pThis->idCcd;
    Req.ccd_id_tgt = idCcdTgt;
    Req.smn_addr   = uSmnAddr;
    Req.size       = cbVal;
    int rc = pspProxyCtxIoctl(pThis, SEV_PSP_STUB_SMN_READ, &Req, NULL);
    if (!rc)
    {
        switch (cbVal)
        {
            case 1:
                *(uint8_t *)pvVal = (uint8_t)Req.value;
                break;
            case 2:
                *(uint16_t *)pvVal = (uint16_t)Req.value;
                break;
            case 4:
                *(uint32_t *)pvVal = (uint32_t)Req.value;
                break;
            case 8:
                *(uint64_t *)pvVal = (uint64_t)Req.value;
                break;
            default:
                /* Impossible. */
                assert(0);
        }
    }

    return rc;
}


int PSPProxyCtxPspSmnWrite(PSPPROXYCTX hCtx, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYCTXINT pThis = hCtx;
    struct sev_user_data_psp_stub_smn_rw Req;

    if (cbVal != 1 && cbVal != 2 && cbVal != 4 && cbVal != 8)
        return -1;

    memset(&Req, 0, sizeof(Req));
    Req.ccd_id     = pThis->idCcd;
    Req.ccd_id_tgt = idCcdTgt;
    Req.smn_addr   = uSmnAddr;
    Req.size       = cbVal;

    switch (cbVal)
    {
        case 1:
            Req.value = (uint64_t)*(uint8_t *)pvVal;
            break;
        case 2:
            Req.value = (uint64_t)*(uint16_t *)pvVal;
            break;
        case 4:
            Req.value = (uint64_t)*(uint32_t *)pvVal;
            break;
        case 8:
            Req.value = (uint64_t)*(uint64_t *)pvVal;
            break;
        default:
            /* Impossible. */
            assert(0);
    }

    return pspProxyCtxIoctl(pThis, SEV_PSP_STUB_SMN_WRITE, &Req, NULL);
}


int PSPProxyCtxPspMemRead(PSPPROXYCTX hCtx, PSPADDR uPspAddr, void *pvBuf, uint32_t cbRead)
{
    PPSPPROXYCTXINT pThis = hCtx;
    struct sev_user_data_psp_stub_psp_rw Req;

    memset(&Req, 0, sizeof(Req));
    Req.ccd_id     = pThis->idCcd;
    Req.psp_addr   = uPspAddr;
    Req.buf        = (__u64)pvBuf;
    Req.size       = cbRead;

    return pspProxyCtxIoctl(pThis, SEV_PSP_STUB_PSP_READ, &Req, NULL);
}


int PSPProxyCtxPspMemWrite(PSPPROXYCTX hCtx, PSPADDR uPspAddr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPPROXYCTXINT pThis = hCtx;
    struct sev_user_data_psp_stub_psp_rw Req;

    memset(&Req, 0, sizeof(Req));
    Req.ccd_id     = pThis->idCcd;
    Req.psp_addr   = uPspAddr;
    Req.buf        = (__u64)pvBuf;
    Req.size       = cbWrite;

    return pspProxyCtxIoctl(pThis, SEV_PSP_STUB_PSP_WRITE, &Req, NULL);
}


int PSPProxyCtxPspX86MemRead(PSPPROXYCTX hCtx, X86PADDR PhysX86Addr, void *pvBuf, uint32_t cbRead)
{
    PPSPPROXYCTXINT pThis = hCtx;
    struct sev_user_data_psp_stub_psp_x86_rw Req;

    memset(&Req, 0, sizeof(Req));
    Req.ccd_id     = pThis->idCcd;
    Req.x86_phys   = PhysX86Addr;
    Req.buf        = (__u64)pvBuf;
    Req.size       = cbRead;

    return pspProxyCtxIoctl(pThis, SEV_PSP_STUB_PSP_X86_READ, &Req, NULL);
}


int PSPProxyCtxPspX86MemWrite(PSPPROXYCTX hCtx, X86PADDR PhysX86Addr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPPROXYCTXINT pThis = hCtx;
    struct sev_user_data_psp_stub_psp_x86_rw Req;

    memset(&Req, 0, sizeof(Req));
    Req.ccd_id     = pThis->idCcd;
    Req.x86_phys   = PhysX86Addr;
    Req.buf        = (__u64)pvBuf;
    Req.size       = cbWrite;

    return pspProxyCtxIoctl(pThis, SEV_PSP_STUB_PSP_X86_WRITE, &Req, NULL);
}


int PSPProxyCtxPspSvcCall(PSPPROXYCTX hCtx, uint32_t idxSyscall, uint32_t u32R0, uint32_t u32R1, uint32_t u32R2, uint32_t u32R3, uint32_t *pu32R0Return)
{
    PPSPPROXYCTXINT pThis = hCtx;
    struct sev_user_data_psp_stub_svc_call Req;

    memset(&Req, 0, sizeof(Req));
    Req.ccd_id     = pThis->idCcd;
    Req.syscall    = idxSyscall;
    Req.r0         = u32R0;
    Req.r1         = u32R1;
    Req.r2         = u32R2;
    Req.r3         = u32R3;
    int rc = pspProxyCtxIoctl(pThis, SEV_PSP_STUB_CALL_SVC, &Req, NULL);
    if (!rc)
        *pu32R0Return = Req.r0_return;

    return rc;
}


int PSPProxyCtxX86SmnRead(PSPPROXYCTX hCtx, uint16_t idNode, SMNADDR uSmnAddr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYCTXINT pThis = hCtx;
    struct sev_user_data_x86_smn_rw Req;

    if (cbVal != 4)
        return -1;

    memset(&Req, 0, sizeof(Req));
    Req.node     = idNode;
    Req.addr     = uSmnAddr;

    int rc = pspProxyCtxIoctl(pThis, SEV_X86_SMN_READ, &Req, NULL);
    if (!rc)
        *(uint32_t *)pvVal = Req.value;

    return rc;
}


int PSPProxyCtxX86SmnWrite(PSPPROXYCTX hCtx, uint16_t idNode, SMNADDR uSmnAddr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYCTXINT pThis = hCtx;
    struct sev_user_data_x86_smn_rw Req;

    if (cbVal != 4)
        return -1;

    memset(&Req, 0, sizeof(Req));
    Req.node     = idNode;
    Req.addr     = uSmnAddr;
    Req.value    = *(uint32_t *)pvVal;

    return pspProxyCtxIoctl(pThis, SEV_X86_SMN_WRITE, &Req, NULL);
}

int PSPProxyCtxX86MemAlloc(PSPPROXYCTX hCtx, uint32_t cbMem, R0PTR *pR0KernVirtual, X86PADDR *pPhysX86Addr)
{
    PPSPPROXYCTXINT pThis = hCtx;
    struct sev_user_data_x86_mem_alloc Req;

    if (!pR0KernVirtual && !pPhysX86Addr)
        return -1;

    memset(&Req, 0, sizeof(Req));
    Req.size = cbMem;

    int rc = pspProxyCtxIoctl(pThis, SEV_X86_MEM_ALLOC, &Req, NULL);
    if (!rc)
    {
        *pR0KernVirtual = Req.addr_virtual;
        *pPhysX86Addr = Req.addr_physical;
    }

    return rc;
}

int PSPProxyCtxX86MemFree(PSPPROXYCTX hCtx, R0PTR R0KernVirtual)
{
    PPSPPROXYCTXINT pThis = hCtx;
    struct sev_user_data_x86_mem_free Req;

    memset(&Req, 0, sizeof(Req));
    Req.addr_virtual = R0KernVirtual;

    return pspProxyCtxIoctl(pThis, SEV_X86_MEM_FREE, &Req, NULL);
}

int PSPProxyCtxX86MemRead(PSPPROXYCTX hCtx, void *pvDst, R0PTR R0KernVirtualSrc, uint32_t cbRead)
{
    PPSPPROXYCTXINT pThis = hCtx;
    struct sev_user_data_x86_mem_rw Req;

    memset(&Req, 0, sizeof(Req));
    Req.kern_buf = R0KernVirtualSrc;
    Req.user_buf = (uint64_t)pvDst;
    Req.size     = cbRead;

    return pspProxyCtxIoctl(pThis, SEV_X86_MEM_READ, &Req, NULL);
}

int PSPProxyCtxX86MemWrite(PSPPROXYCTX hCtx, R0PTR R0KernVirtualDst, const void *pvSrc, uint32_t cbWrite)
{
    PPSPPROXYCTXINT pThis = hCtx;
    struct sev_user_data_x86_mem_rw Req;

    memset(&Req, 0, sizeof(Req));
    Req.kern_buf = R0KernVirtualDst;
    Req.user_buf = (uint64_t)pvSrc;
    Req.size     = cbWrite;

    return pspProxyCtxIoctl(pThis, SEV_X86_MEM_WRITE, &Req, NULL);
}

int PSPProxyCtxX86PhysMemRead(PSPPROXYCTX hCtx, void *pvDst, X86PADDR PhysX86AddrSrc, uint32_t cbRead)
{
    (void)hCtx; /* Not required here. */

    int rc = ptedit_init();
    if (rc)
        return -1;

    /* Map a page aligned anonymous region of memory we use as virtual address range for the physical address later on. */
    void *pvVirt = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pvVirt != MAP_FAILED)
    {
        memset(pvVirt, '0', 4096); /* Make sure the range is backed by memory. */

        int uUcMt = ptedit_find_first_mt(PTEDIT_MT_UC);
        if (uUcMt != -1)
        {
            /* Get the page table entry. */
            ptedit_entry_t VmEntry = ptedit_resolve(pvVirt, 0);
            if(VmEntry.pgd != 0)
            {
                uint8_t *pbDst = (uint8_t *)pvDst;
                size_t uPhysPfn  = (size_t)(PhysX86AddrSrc >> 12);
                uint32_t offPage = (uint32_t)(PhysX86AddrSrc & 0xfff);

                size_t uPhysPfnOrig = ptedit_get_pfn(VmEntry.pte);

                while (cbRead)
                {
                    size_t cbThisRead = cbRead <= 4096 ? cbRead : 4096;
                    VmEntry.pte = ptedit_set_pfn(VmEntry.pte, uPhysPfn);
                    VmEntry.pte = ptedit_apply_mt(VmEntry.pte, uUcMt);

                    /* Update only the PTE of the entry. */
                    VmEntry.valid = PTEDIT_VALID_MASK_PTE;
                    ptedit_update(pvVirt, 0, &VmEntry);

                    ptedit_full_serializing_barrier();

                    memcpy(pbDst, (uint8_t *)pvVirt + offPage, cbThisRead);

                    offPage = 0; /* Page aligned after the first page. */
                    pbDst  += cbThisRead;
                    cbRead -= cbThisRead;
                }

                /* Restore original pfn. */
                ptedit_pte_set_pfn(pvVirt, 0, uPhysPfnOrig);
            }
            else
                rc = -1;
        }
        else
            rc = -1;

        munmap(pvVirt, 4096);
    }
    else
        rc = -1;

    ptedit_cleanup();
    return rc;
}

int PSPProxyCtxX86PhysMemWrite(PSPPROXYCTX hCtx, X86PADDR PhysX86AddrDst, const void *pvSrc, uint32_t cbWrite)
{
    (void)hCtx; /* Not required here. */

    int rc = ptedit_init();
    if (rc)
        return -1;

    /* Map a page aligned anonymous region of memory we use as virtual address range for the physical address later on. */
    void *pvVirt = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pvVirt != MAP_FAILED)
    {
        memset(pvVirt, '0', 4096); /* Make sure the range is backed by memory. */

        int uUcMt = ptedit_find_first_mt(PTEDIT_MT_UC);
        if (uUcMt != -1)
        {
            /* Get the page table entry. */
            ptedit_entry_t VmEntry = ptedit_resolve(pvVirt, 0);
            if(VmEntry.pgd != 0)
            {
                uint8_t *pbSrc = (uint8_t *)pvSrc;
                size_t uPhysPfn  = (size_t)(PhysX86AddrDst >> 12);
                uint32_t offPage = (uint32_t)(PhysX86AddrDst & 0xfff);

                size_t uPhysPfnOrig = ptedit_get_pfn(VmEntry.pte);

                while (cbWrite)
                {
                    size_t cbThisWrite = cbWrite <= 4096 ? cbWrite : 4096;
                    VmEntry.pte = ptedit_set_pfn(VmEntry.pte, uPhysPfn);
                    VmEntry.pte = ptedit_apply_mt(VmEntry.pte, uUcMt);

                    /* Update only the PTE of the entry. */
                    VmEntry.valid = PTEDIT_VALID_MASK_PTE;
                    ptedit_update(pvVirt, 0, &VmEntry);

                    ptedit_full_serializing_barrier();

                    memcpy((uint8_t *)pvVirt + offPage, pbSrc, cbThisWrite);

                    offPage = 0; /* Page aligned after the first page. */
                    pbSrc   += cbThisWrite;
                    cbWrite -= cbThisWrite;
                }

                /* Restore original pfn. */
                ptedit_pte_set_pfn(pvVirt, 0, uPhysPfnOrig);
            }
            else
                rc = -1;
        }
        else
            rc = -1;

        munmap(pvVirt, 4096);
    }
    else
        rc = -1;

    ptedit_cleanup();
    return rc;
}

int PSPProxyCtxEmuWaitForWork(PSPPROXYCTX hCtx, uint32_t *pidCmd, X86PADDR *pPhysX86AddrCmdBuf, uint32_t msWait)
{
    PPSPPROXYCTXINT pThis = hCtx;
    struct sev_user_data_emu_wait_for_work Req;

    memset(&Req, 0, sizeof(Req));
    Req.timeout = msWait;
    int rc = pspProxyCtxIoctl(pThis, SEV_EMU_WAIT_FOR_WORK, &Req, NULL);
    if (!rc)
    {
        *pidCmd = Req.cmd;
        *pPhysX86AddrCmdBuf = ((X86PADDR)Req.phys_msb << 32) | (X86PADDR)Req.phys_lsb;
    }

    return rc;
}

int PSPProxyCtxEmuSetResult(PSPPROXYCTX hCtx, uint32_t uResult)
{
    PPSPPROXYCTXINT pThis = hCtx;
    struct sev_user_data_emu_set_result Req;

    memset(&Req, 0, sizeof(Req));
    Req.result = uResult;

    return pspProxyCtxIoctl(pThis, SEV_EMU_SET_RESULT, &Req, NULL);
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

