/** @file
 * PSP proxy library to interface with the hardware of the PSP - local Linux SEV device provider
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
 * I/O control wrapper for the SEV device.
 *
 * @returns Status code.
 * @param   pThis                   The context instance.
 * @param   idCmd                   The command to execute.
 * @param   pvReq                   The opaque request structure passed.
 * @param   pu32Error               Where to store the PSP status code on success, optional.
 */
static int sevProvCtxIoctl(PPSPPROXYPROVCTXINT pThis, uint32_t idCmd, void *pvArgs, uint32_t *pu32Error)
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
 * @copydoc{PSPPROXYPROV,pfnCtxInit}
 */
int sevProvCtxInit(PSPPROXYPROVCTX hProvCtx, const char *pszDevice)
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
void sevProvCtxDestroy(PSPPROXYPROVCTX hProvCtx)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;

    close(pThis->iFdDev);
    pThis->iFdDev = 0;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxQueryInfo}
 */
static int sevProvCtxQueryInfo(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR *pPspAddrScratchStart, size_t *pcbScratch)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    struct sev_user_data_query_info Req;

    memset(&Req, 0, sizeof(Req));
    Req.ccd_id = idCcd;

    int rc = sevProvCtxIoctl(pThis, SEV_PSP_STUB_QUERY_INFO, &Req, NULL);
    if (!rc)
    {
        *pPspAddrScratchStart = Req.psp_addr_scratch_start;
        *pcbScratch           = Req.scratch_size;
    }

    return rc;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspSmnRead}
 */
int sevProvCtxPspSmnRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    struct sev_user_data_psp_stub_smn_rw Req;

    if (cbVal != 1 && cbVal != 2 && cbVal != 4 && cbVal != 8)
        return -1;

    memset(&Req, 0, sizeof(Req));
    Req.ccd_id     = idCcd;
    Req.ccd_id_tgt = idCcdTgt;
    Req.smn_addr   = uSmnAddr;
    Req.size       = cbVal;
    int rc = sevProvCtxIoctl(pThis, SEV_PSP_STUB_SMN_READ, &Req, NULL);
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


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspSmnWrite}
 */
int sevProvCtxPspSmnWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    struct sev_user_data_psp_stub_smn_rw Req;

    if (cbVal != 1 && cbVal != 2 && cbVal != 4 && cbVal != 8)
        return -1;

    memset(&Req, 0, sizeof(Req));
    Req.ccd_id     = idCcd;
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

    return sevProvCtxIoctl(pThis, SEV_PSP_STUB_SMN_WRITE, &Req, NULL);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMemRead}
 */
int sevProvCtxPspMemRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, void *pvBuf, uint32_t cbRead)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    struct sev_user_data_psp_stub_psp_rw Req;

    memset(&Req, 0, sizeof(Req));
    Req.ccd_id     = idCcd;
    Req.psp_addr   = uPspAddr;
    Req.buf        = (__u64)pvBuf;
    Req.size       = cbRead;

    return sevProvCtxIoctl(pThis, SEV_PSP_STUB_PSP_READ, &Req, NULL);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMemWrite}
 */
int sevProvCtxPspMemWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    struct sev_user_data_psp_stub_psp_rw Req;

    memset(&Req, 0, sizeof(Req));
    Req.ccd_id     = idCcd;
    Req.psp_addr   = uPspAddr;
    Req.buf        = (__u64)pvBuf;
    Req.size       = cbWrite;

    return sevProvCtxIoctl(pThis, SEV_PSP_STUB_PSP_WRITE, &Req, NULL);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MemRead}
 */
int sevProvCtxPspX86MemRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, void *pvBuf, uint32_t cbRead)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    struct sev_user_data_psp_stub_psp_x86_rw Req;

    memset(&Req, 0, sizeof(Req));
    Req.ccd_id     = idCcd;
    Req.x86_phys   = PhysX86Addr;
    Req.buf        = (__u64)pvBuf;
    Req.size       = cbRead;

    return sevProvCtxIoctl(pThis, SEV_PSP_STUB_PSP_X86_READ, &Req, NULL);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MemWrite}
 */
int sevProvCtxPspX86MemWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    struct sev_user_data_psp_stub_psp_x86_rw Req;

    memset(&Req, 0, sizeof(Req));
    Req.ccd_id     = idCcd;
    Req.x86_phys   = PhysX86Addr;
    Req.buf        = (__u64)pvBuf;
    Req.size       = cbWrite;

    return sevProvCtxIoctl(pThis, SEV_PSP_STUB_PSP_X86_WRITE, &Req, NULL);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspSvcCall}
 */
int sevProvCtxPspSvcCall(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, uint32_t idxSyscall, uint32_t u32R0, uint32_t u32R1, uint32_t u32R2, uint32_t u32R3, uint32_t *pu32R0Return)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    struct sev_user_data_psp_stub_svc_call Req;

    memset(&Req, 0, sizeof(Req));
    Req.ccd_id     = idCcd;
    Req.syscall    = idxSyscall;
    Req.r0         = u32R0;
    Req.r1         = u32R1;
    Req.r2         = u32R2;
    Req.r3         = u32R3;
    int rc = sevProvCtxIoctl(pThis, SEV_PSP_STUB_CALL_SVC, &Req, NULL);
    if (!rc)
        *pu32R0Return = Req.r0_return;

    return rc;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxX86SmnRead}
 */
int sevProvCtxX86SmnRead(PSPPROXYPROVCTX hProvCtx, uint16_t idNode, SMNADDR uSmnAddr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    struct sev_user_data_x86_smn_rw Req;

    if (cbVal != 4)
        return -1;

    memset(&Req, 0, sizeof(Req));
    Req.node     = idNode;
    Req.addr     = uSmnAddr;

    int rc = sevProvCtxIoctl(pThis, SEV_X86_SMN_READ, &Req, NULL);
    if (!rc)
        *(uint32_t *)pvVal = Req.value;

    return rc;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxX86SmnWrite}
 */
int sevProvCtxX86SmnWrite(PSPPROXYPROVCTX hProvCtx, uint16_t idNode, SMNADDR uSmnAddr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    struct sev_user_data_x86_smn_rw Req;

    if (cbVal != 4)
        return -1;

    memset(&Req, 0, sizeof(Req));
    Req.node     = idNode;
    Req.addr     = uSmnAddr;
    Req.value    = *(uint32_t *)pvVal;

    return sevProvCtxIoctl(pThis, SEV_X86_SMN_WRITE, &Req, NULL);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxX86MemAlloc}
 */
int sevProvCtxX86MemAlloc(PSPPROXYPROVCTX hProvCtx, uint32_t cbMem, R0PTR *pR0KernVirtual, X86PADDR *pPhysX86Addr)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    struct sev_user_data_x86_mem_alloc Req;

    if (!pR0KernVirtual && !pPhysX86Addr)
        return -1;

    memset(&Req, 0, sizeof(Req));
    Req.size = cbMem;

    int rc = sevProvCtxIoctl(pThis, SEV_X86_MEM_ALLOC, &Req, NULL);
    if (!rc)
    {
        *pR0KernVirtual = Req.addr_virtual;
        *pPhysX86Addr = Req.addr_physical;
    }

    return rc;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxX86MemFree}
 */
int sevProvCtxX86MemFree(PSPPROXYPROVCTX hProvCtx, R0PTR R0KernVirtual)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    struct sev_user_data_x86_mem_free Req;

    memset(&Req, 0, sizeof(Req));
    Req.addr_virtual = R0KernVirtual;

    return sevProvCtxIoctl(pThis, SEV_X86_MEM_FREE, &Req, NULL);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxX86MemRead}
 */
int sevProvCtxX86MemRead(PSPPROXYPROVCTX hProvCtx, void *pvDst, R0PTR R0KernVirtualSrc, uint32_t cbRead)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    struct sev_user_data_x86_mem_rw Req;

    memset(&Req, 0, sizeof(Req));
    Req.kern_buf = R0KernVirtualSrc;
    Req.user_buf = (uint64_t)pvDst;
    Req.size     = cbRead;

    return sevProvCtxIoctl(pThis, SEV_X86_MEM_READ, &Req, NULL);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxX86MemWrite}
 */
int sevProvCtxX86MemWrite(PSPPROXYPROVCTX hProvCtx, R0PTR R0KernVirtualDst, const void *pvSrc, uint32_t cbWrite)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    struct sev_user_data_x86_mem_rw Req;

    memset(&Req, 0, sizeof(Req));
    Req.kern_buf = R0KernVirtualDst;
    Req.user_buf = (uint64_t)pvSrc;
    Req.size     = cbWrite;

    return sevProvCtxIoctl(pThis, SEV_X86_MEM_WRITE, &Req, NULL);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxX86PhysMemRead}
 */
int sevProvCtxX86PhysMemRead(PSPPROXYPROVCTX hProvCtx, void *pvDst, X86PADDR PhysX86AddrSrc, uint32_t cbRead)
{
    (void)hProvCtx; /* Not required here. */

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


/**
 * @copydoc{PSPPROXYPROV,pfnCtxX86PhysMemWrite}
 */
int sevProvCtxX86PhysMemWrite(PSPPROXYPROVCTX hProvCtx, X86PADDR PhysX86AddrDst, const void *pvSrc, uint32_t cbWrite)
{
    (void)hProvCtx; /* Not required here. */

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


/**
 * @copydoc{PSPPROXYPROV,pfnCtxEmuWaitForWork}
 */
static int sevProvCtxEmuWaitForWork(PSPPROXYPROVCTX hProvCtx, uint32_t *pidCmd, X86PADDR *pPhysX86AddrCmdBuf, uint32_t msWait)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    struct sev_user_data_emu_wait_for_work Req;

    memset(&Req, 0, sizeof(Req));
    Req.timeout = msWait;
    int rc = sevProvCtxIoctl(pThis, SEV_EMU_WAIT_FOR_WORK, &Req, NULL);
    if (!rc)
    {
        *pidCmd = Req.cmd;
        *pPhysX86AddrCmdBuf = ((X86PADDR)Req.phys_msb << 32) | (X86PADDR)Req.phys_lsb;
    }

    return rc;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxEmuSetResult}
 */
static int sevProvCtxEmuSetResult(PSPPROXYPROVCTX hProvCtx, uint32_t uResult)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    struct sev_user_data_emu_set_result Req;

    memset(&Req, 0, sizeof(Req));
    Req.result = uResult;

    return sevProvCtxIoctl(pThis, SEV_EMU_SET_RESULT, &Req, NULL);
}


/**
 * Provider registration structure.
 */
const PSPPROXYPROV g_PspProxyProvSev =
{
    /** pszId */
    "sev",
    /** pszDesc */
    "PSP access through the local /dev/sev device using a modified Linux kernel module.",
    /** cbCtx */
    sizeof(PSPPROXYPROVCTXINT),
    /** fFeatures */
    0,
    /** pfnCtxInit */
    sevProvCtxInit,
    /** pfnCtxDestroy */
    sevProvCtxDestroy,
    /** pfnCtxQueryInfo */
    sevProvCtxQueryInfo,
    /** pfnCtxPspSmnRead */
    sevProvCtxPspSmnRead,
    /** pfnCtxPspSmnWrite */
    sevProvCtxPspSmnWrite,
    /** pfnCtxPspMemRead */
    sevProvCtxPspMemRead,
    /** pfnCtxPspMemWrite */
    sevProvCtxPspMemWrite,
    /** pfnCtxPspX86MemRead */
    sevProvCtxPspX86MemRead,
    /** pfnCtxPspX86MemWrite */
    sevProvCtxPspX86MemWrite,
    /** pfnCtxPspSvcCall */
    sevProvCtxPspSvcCall,
    /** pfnCtxX86SmnRead */
    sevProvCtxX86SmnRead,
    /** pfnCtxX86SmnWrite */
    sevProvCtxX86SmnWrite,
    /** pfnCtxX86MemAlloc */
    sevProvCtxX86MemAlloc,
    /** pfnCtxX86MemFree */
    sevProvCtxX86MemFree,
    /** pfnCtxX86MemRead */
    sevProvCtxX86MemRead,
    /** pfnCtxX86MemWrite */
    sevProvCtxX86MemWrite,
    /** pfnCtxX86PhysMemRead */
    sevProvCtxX86PhysMemRead,
    /** pfnCtxX86PhysMemWrite */
    sevProvCtxX86PhysMemWrite,
    /** pfnCtxEmuWaitForWork */
    sevProvCtxEmuWaitForWork,
    /** pfnCtxEmuSetResult */
    sevProvCtxEmuSetResult
};

