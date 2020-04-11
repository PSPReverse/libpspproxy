/** @file
 * Internal PSP proxy provider definitions.
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
#ifndef __psp_proxy_provider_h
#define __psp_proxy_provider_h

#include "libpspproxy.h"


/** Opaque proxy provider context. */
typedef struct PSPPROXYPROVCTXINT *PSPPROXYPROVCTX;

/**
 * The proxy provider struct.
 */
typedef struct PSPPROXYPROV
{
    /** Provider ID. */
    const char                  *pszId;
    /** Provider description. */
    const char                  *pszDesc;
    /** Size of the provider context structure passed around in bytes. */
    size_t                      cbCtx;
    /** Feature flags. */
    uint32_t                    fFeatures;

    /**
     * Initializes the provider context and the given device.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   pszDevice               The device to use, structure is provider specific.
     * @param   pfnLogMsg               Callback handler for received log messages from the proxy.
     * @param   pvUser                  Opaque user data to pass to the callback.
     */
    int (*pfnCtxInit) (PSPPROXYPROVCTX hProvCtx, const char *pszDevice, PFNPSPPROXYLOGMSGRECV pfnLogMsg,
                       void *pvUser);

    /**
     * Destroys the given provider context, freeing all allocated resources.
     *
     * @returns nothing.
     * @param   hProvCtx                Provider context instance data.
     */
    void (*pfnCtxDestroy) (PSPPROXYPROVCTX hProvCtx);

    /**
     * Queries information about the given PSP.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   idCcd                   The CCD to query the data for.
     * @param   pPspAddrScratchStart    Where to store the the start address of the scratch space area on success.
     * @param   pcbScratch              Where to store the size of the scratch space area in bytes on success.
     */
    int (*pfnCtxQueryInfo) (PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR *pPspAddrScratchStart, size_t *pcbScratch);

    /**
     * Reads the register at the given SMN address.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   idCcd                   The source CCD ID for the read.
     * @param   idCcdTgt                The target CCD ID to access the register on.
     * @param   uSmnAddr                The SMN address/offset to access.
     * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
     * @param   pvVal                   Where to store the value on success.
     */
    int (*pfnCtxPspSmnRead) (PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, void *pvVal);

    /**
     * Writes to the register at the given SMN address.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   idCcd                   The source CCD ID for the write.
     * @param   idCcdTgt                The target CCD ID to access the register on.
     * @param   uSmnAddr                The SMN address/offset to access.
     * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
     * @param   pvVal                   The value to write.
     */
    int (*pfnCtxPspSmnWrite) (PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, const void *pvVal);

    /**
     * Reads from the PSP address space at the given address.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   idCcd                   The CCD ID for the read.
     * @param   uPspAddr                The PSP address to start reading from.
     * @param   pvBuf                   Where to store the read data.
     * @param   cbRead                  Amount of data to read.
     */
    int (*pfnCtxPspMemRead) (PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, void *pvBuf, uint32_t cbRead);

    /**
     * Writes to the PSP address space at the given address.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   idCcd                   The CCD ID for the read.
     * @param   uPspAddr                The PSP address to start writing to.
     * @param   pvBuf                   The data to write.
     * @param   cbWrite                 Amount of data to write.
     */
    int (*pfnCtxPspMemWrite) (PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, const void *pvBuf, uint32_t cbWrite);

    /**
     * Reads the register at the given PSP MMIO address.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   idCcd                   The source CCD ID for the read.
     * @param   uPspAddr                The MMIO address/offset to access.
     * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
     * @param   pvVal                   Where to store the value on success.
     */
    int (*pfnCtxPspMmioRead) (PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, uint32_t cbVal, void *pvVal);

    /**
     * Writes to the register at the given PSP MMIO address.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   idCcd                   The source CCD ID for the write.
     * @param   uPspAddr                The MMIO address/offset to access.
     * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
     * @param   pvVal                   The value to write.
     */
    int (*pfnCtxPspMmioWrite) (PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, uint32_t cbVal, const void *pvVal);

    /**
     * Reads from the x86 address space using the PSP (to circumvent protection mechanisms
     * on the x86 core).
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   idCcd                   The CCD ID for the read.
     * @param   PhysX86Addr             The x86 address to start reading from.
     * @param   pvBuf                   Where to store the read data.
     * @param   cbRead                  Amount of data to read.
     */
    int (*pfnCtxPspX86MemRead) (PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, void *pvBuf, uint32_t cbRead);

    /**
     * Writes to the x86 address space using the PSP (to circumvent protection mechanisms
     * on the x86 core).
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   idCcd                   The CCD ID for the read.
     * @param   PhysX86Addr             The x86 address to start writing to.
     * @param   pvBuf                   The data to write.
     * @param   cbWrite                 Amount of data to write.
     */
    int (*pfnCtxPspX86MemWrite) (PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, const void *pvBuf, uint32_t cbWrite);

    /**
     * Reads the register at the given x86 MMIO address.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   idCcd                   The source CCD ID for the read.
     * @param   PhysX86Addr             The MMIO address/offset to access.
     * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
     * @param   pvVal                   Where to store the value on success.
     */
    int (*pfnCtxPspX86MmioRead) (PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, uint32_t cbVal, void *pvVal);

    /**
     * Writes to the register at the given x86 MMIO address.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   idCcd                   The source CCD ID for the write.
     * @param   PhysX86Addr             The MMIO address/offset to access.
     * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
     * @param   pvVal                   The value to write.
     */
    int (*pfnCtxPspX86MmioWrite) (PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, uint32_t cbVal, const void *pvVal);

    /**
     * Execute a syscall on the PSP - optional.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   idCcd                   The CCD ID for the read.
     * @param   idxSyscall              The syscall to execute.
     * @param   u32R0                   R0 argument.
     * @param   u32R1                   R1 argument.
     * @param   u32R2                   R2 argument.
     * @param   u32R3                   R3 argument.
     * @param   pu32R0Return            Where to store the R0 return value of the syscall.
     */
    int (*pfnCtxPspSvcCall) (PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, uint32_t idxSyscall, uint32_t u32R0, uint32_t u32R1, uint32_t u32R2, uint32_t u32R3, uint32_t *pu32R0Return);

    /**
     * Reads the register at the given SMN address, the access is initiated from the x86 core and not the PSP - optional.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   idNode                  The node ID to target the read from.
     * @param   uSmnAddr                The SMN address/offset to access.
     * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
     * @param   pvVal                   Where to store the value on success.
     */
    int (*pfnCtxX86SmnRead) (PSPPROXYPROVCTX hProvCtx, uint16_t idNode, SMNADDR uSmnAddr, uint32_t cbVal, void *pvVal);

    /**
     * Writes the register at the given SMN address, the access is initiated from the x86 core and not the PSP - optional.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   idNode                  The node ID to target the write from.
     * @param   uSmnAddr                The SMN address/offset to access.
     * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
     * @param   pvVal                   Where to store the value on success.
     */
    int (*pfnCtxX86SmnWrite) (PSPPROXYPROVCTX hProvCtx, uint16_t idNode, SMNADDR uSmnAddr, uint32_t cbVal, const void *pvVal);

    /**
     * Allocates a contiguous region of memory accessible from R0 - optional.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   cbMem                   Number of bytes to allocate.
     * @param   pR0KernVirtual          Where to store the R0 virtual address of the allocated region on success.
     * @param   pPhysX86Addr            Where to store the X86 physical address of the allocated region on success.
     */
    int (*pfnCtxX86MemAlloc) (PSPPROXYPROVCTX hProvCtx, uint32_t cbMem,  R0PTR *pR0KernVirtual, X86PADDR *pPhysX86Addr);

    /**
     * Frees a previous allocated R0 memory region - optional.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   R0KernVirtual           The R0 virtual address to free.
     */
    int (*pfnCtxX86MemFree) (PSPPROXYPROVCTX hProvCtx, R0PTR R0KernVirtual);

    /**
     * Copies memory from a given R0 virtual address to a supplied userspace buffer - optional.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   pvDst                   Where to store the read bits.
     * @param   R0KernVirtualSrc        The virtual R0 address to read from.
     * @param   cbRead                  How much to read.
     */
    int (*pfnCtxX86MemRead) (PSPPROXYPROVCTX hProvCtx, void *pvDst, R0PTR R0KernVirtualSrc, uint32_t cbRead);

    /**
     * Copies memory from a supplied userspace buffer tp the given R0 virtual address - optional.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   R0KernVirtualDst        The virtual R0 address to write to.
     * @param   pvSrc                   The source buffer.
     * @param   cbWrite                 How much to write.
     */
    int (*pfnCtxX86MemWrite) (PSPPROXYPROVCTX hProvCtx, R0PTR R0KernVirtualDst, const void *pvSrc, uint32_t cbWrite);

    /**
     * Copies memory from a given x86 physical address to a supplied userspace buffer - optional.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   pvDst                   Where to store the read bits.
     * @param   PhysX86AddrSrc          The physical x86 address to read from.
     * @param   cbRead                  How much to read.
     */
    int (*pfnCtxX86PhysMemRead) (PSPPROXYPROVCTX hProvCtx, void *pvDst, X86PADDR PhysX86AddrSrc, uint32_t cbRead);

    /**
     * Copies memory from a supplied userspace buffer tp the given x86 physical address - optional.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   PhysX86AddrDst          The physical x86 address to write to.
     * @param   pvSrc                   The source buffer.
     * @param   cbWrite                 How much to write.
     */
    int (*pfnCtxX86PhysMemWrite) (PSPPROXYPROVCTX hProvCtx, X86PADDR PhysX86AddrDst, const void *pvSrc, uint32_t cbWrite);

    /**
     * Waits for a command when the PSP emulation is used - optional.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   pidCmd                  Where to store the command id on success.
     * @param   pPhysX86AddrCmdBuf      Where to store the physical address of the command buffer on success.
     * @param   msWait                  The amount of milliseconds to wait before timing out.
     */
    int (*pfnCtxEmuWaitForWork) (PSPPROXYPROVCTX hProvCtx, uint32_t *pidCmd, X86PADDR *pPhysX86AddrCmdBuf, uint32_t msWait);

    /**
     * Sets the result of a previous received command - optional.
     *
     * @returns Status code.
     * @param   hProvCtx                Provider context instance data.
     * @param   uResult                 The result to set.
     */
    int (*pfnCtxEmuSetResult) (PSPPROXYPROVCTX hProvCtx, uint32_t uResult);

} PSPPROXYPROV;
/** Pointer to a proxy provider. */
typedef PSPPROXYPROV *PPSPPROXYPROV;
/** Pointer to a const proxy provider. */
typedef const PSPPROXYPROV *PCPSPPROXYPROV;


#endif /* !__psp_proxy_provider_h */
