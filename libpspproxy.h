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
#ifndef __libpspproxy_h
#define __libpspproxy_h

#include <common/cdefs.h>
#include <common/types.h>

/** Opaque PSP proxy context handle. */
typedef struct PSPPROXYCTXINT *PSPPROXYCTX;
/** Pointer to a PSP proxy context handle. */
typedef PSPPROXYCTX *PPSPPROXYCTX;


/**
 * PSP proxy address space type.
 */
typedef enum PSPPROXYADDRSPACE
{
    /** Invalid address space. */
    PSPPROXYADDRSPACE_INVALID = 0,
    /** PSP SRAM. */
    PSPPROXYADDRSPACE_PSP_MEM,
    /** PSP MMIO. */
    PSPPROXYADDRSPACE_PSP_MMIO,
    /** SMN. */
    PSPPROXYADDRSPACE_SMN,
    /** x86 standard memory. */
    PSPPROXYADDRSPACE_X86_MEM,
    /** x86 MMIO. */
    PSPPROXYADDRSPACE_X86_MMIO,
    /** 32bit hack. */
    PSPPROXYADDRSPACE_32BIT_HACK = 0x7fffffff
} PSPPROXYADDRSPACE;


/**
 * PSP proxy address.
 */
typedef struct PSPPROXYADDR
{
    /** The address space type. */
    PSPPROXYADDRSPACE           enmAddrSpace;
    /** Type dependent data. */
    union
    {
        /** PSP address. */
        PSPADDR                 PspAddr;
        /** SMN address. */
        SMNADDR                 SmnAddr;
        /** x86 address dependent data. */
        struct
        {
            /** Physical x86 address. */
            X86PADDR            PhysX86Addr;
            /** Caching information associated with that address. */
            uint32_t            fCaching;
        } X86;
    } u;
} PSPPROXYADDR;
/** Pointer to a PSP proxy address. */
typedef PSPPROXYADDR *PPSPPROXYADDR;
/** Pointer to a const PSP proxy address. */
typedef const PSPPROXYADDR *PCPSPPROXYADDR;


/**
 * I/O interface callback table.
 */
typedef struct PSPPROXYIOIF
{
    /**
     * Log message received callback.
     *
     * @returns nothing.
     * @param   hCtx                    The PSP proxy context handle.
     * @param   pvUser                  Opaque user data passed during creation.
     * @param   pszMsg                  The received message.
     */
    void (*pfnLogMsg) (PSPPROXYCTX hCtx, void *pvUser, const char *pszMsg);

    /**
     * Output buffer write callback.
     *
     * @returns Status code.
     * @param   hCtx                    The PSP proxy context handle.
     * @param   pvUser                  Opaque user data passed during creation.
     * @param   idOutBuf                Output buffer ID written to.
     * @param   pvBuf                   The data being written.
     * @param   cbBuf                   Amount of bytes written.
     */
    int (*pfnOutBufWrite) (PSPPROXYCTX hCtx, void *pvUser, uint32_t idOutBuf, const void *pvBuf, size_t cbBuf);

    /**
     * Peeks how much is available for reading from the given input buffer.
     *
     * @returns Number of bytes available for reading from the input buffer.
     * @param   hCtx                    The PSP proxy context handle.
     * @param   pvUser                  Opaque user data passed during creation.
     * @param   idInBuf                 The input buffer ID.
     */
    size_t (*pfnInBufPeek) (PSPPROXYCTX hCtx, void *pvUser, uint32_t idInBuf);

    /**
     * Reads data from the given input buffer.
     *
     * @returns Status code.
     * @param   hCtx                    The PSP proxy context handle.
     * @param   pvUser                  Opaque user data passed during creation.
     * @param   idInBuf                 The input buffer ID.
     * @param   pvBuf                   Where to store the read data.
     * @param   cbRead                  Number of bytes to read.
     * @param   pcbRead                 Where to store the number of bytes actually read, optional.
     */
    int (*pfnInBufRead) (PSPPROXYCTX hCtx, void *pvUser, uint32_t idInBuf, void *pvBuf, size_t cbRead, size_t *pcbRead);

} PSPPROXYIOIF;
/** Pointer to an I/O interface callback table. */
typedef PSPPROXYIOIF *PPSPPROXYIOIF;
/** Pointer a const I/O interface callback table. */
typedef const PSPPROXYIOIF *PCPSPPROXYIOIF;


/** Request is a read. */
#define PSPPROXY_CTX_ADDR_XFER_F_READ          (1 << 0)
/** Request is a write. */
#define PSPPROXY_CTX_ADDR_XFER_F_WRITE         (1 << 1)
/** Request is a memset() like operation containing only a single value. */
#define PSPPROXY_CTX_ADDR_XFER_F_MEMSET        (1 << 2)
/** Increment the PSP address after each access by the given stride,
 * if not given the transfer will write to/read from the same address for each request
 * (for optimized accesses to data ports). */
#define PSPPROXY_CTX_ADDR_XFER_F_INCR_ADDR     (1 << 3)
/** Mask ov valid operation bits. */
#define PSPPROXY_CTX_ADDR_XFER_F_OP_MASK_VALID (0x7)


/**
 * Creates a new PSP proxy context for the given device.
 *
 * @returns Status code.
 * @param   phCtx                   Where to store the handle to the PSP proxy context on success.
 * @param   pszDevice               The device to use, usually /dev/sev.
 * @param   pIoIf                   Pointer to the I/O interface callbacks.
 * @param   pvUser                  Opaque user data to pass to the callback.
 */
int PSPProxyCtxCreate(PPSPPROXYCTX phCtx, const char *pszDevice, PCPSPPROXYIOIF pIoIf,
                      void *pvUser);

/**
 * Destroys a given PSP proxy context.
 *
 * @returns nothing.
 * @param   hCtx                    The PSP proxy context handle to destroy.
 */
void PSPProxyCtxDestroy(PSPPROXYCTX hCtx);

/**
 * Sets the CCD ID used as the operating environment (memory read/written target this particular CCD PSP address space).
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   idCcd                   The CCD ID to set.
 *
 * @note This doesn't work well together with the scratch space allocator as each PSP has its own scratch space.
 *       If you are intending to use the scratch space don't use this after the first call to PSPProxyCtxScratchSpaceAlloc().
 *       Instead create a dedicated proxy context for each PSP and set the CCD ID once at the beginning.
 */
int PSPProxyCtxPspCcdSet(PSPPROXYCTX hCtx, uint32_t idCcd);

/**
 * Reads the register at the given SMN address.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   idCcdTgt                The target CCD ID to access the register on.
 * @param   uSmnAddr                The SMN address/offset to access.
 * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
 * @param   pvVal                   Where to store the value on success.
 */
int PSPProxyCtxPspSmnRead(PSPPROXYCTX hCtx, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, void *pvVal);

/**
 * Writes to the register at the given SMN address.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   idCcdTgt                The target CCD ID to access the register on.
 * @param   uSmnAddr                The SMN address/offset to access.
 * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
 * @param   pvVal                   The value to write.
 */
int PSPProxyCtxPspSmnWrite(PSPPROXYCTX hCtx, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, const void *pvVal);

/**
 * Reads from the PSP address space at the given address.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   uPspAddr                The PSP address to start reading from.
 * @param   pvBuf                   Where to store the read data.
 * @param   cbRead                  Amount of data to read.
 */
int PSPProxyCtxPspMemRead(PSPPROXYCTX hCtx, PSPADDR uPspAddr, void *pvBuf, uint32_t cbRead);

/**
 * Writes to the PSP address space at the given address.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   uPspAddr                The PSP address to start writing to.
 * @param   pvBuf                   The data to write.
 * @param   cbWrite                 Amount of data to write.
 */
int PSPProxyCtxPspMemWrite(PSPPROXYCTX hCtx, PSPADDR uPspAddr, const void *pvBuf, uint32_t cbWrite);

/**
 * Reads the register at the given PSP MMIO address.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   uPspAddr                The PSP address to start reading from.
 * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
 * @param   pvVal                   Where to store the value on success.
 */
int PSPProxyCtxPspMmioRead(PSPPROXYCTX hCtx, PSPADDR uPspAddr, uint32_t cbVal, void *pvVal);

/**
 * Writes to the register at the given PSP MMIO address.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   uPspAddr                The PSP address to start writing to.
 * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
 * @param   pvVal                   The value to write.
 */
int PSPProxyCtxPspMmioWrite(PSPPROXYCTX hCtx, PSPADDR uPspAddr, uint32_t cbVal, const void *pvVal);

/**
 * Reads from the x86 address space using the PSP (to circumvent protection mechanisms
 * on the x86 core).
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   PhysX86Addr             The x86 address to start reading from.
 * @param   pvBuf                   Where to store the read data.
 * @param   cbRead                  Amount of data to read.
 */
int PSPProxyCtxPspX86MemRead(PSPPROXYCTX hCtx, X86PADDR PhysX86Addr, void *pvBuf, uint32_t cbRead);

/**
 * Writes to the x86 address space using the PSP (to circumvent protection mechanisms
 * on the x86 core).
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   PhysX86Addr             The x86 address to start writing to.
 * @param   pvBuf                   The data to write.
 * @param   cbWrite                 Amount of data to write.
 */
int PSPProxyCtxPspX86MemWrite(PSPPROXYCTX hCtx, X86PADDR PhysX86Addr, const void *pvBuf, uint32_t cbWrite);

/**
 * Reads from the x86 MMIO address space using the PSP (to circumvent protection mechanisms
 * on the x86 core).
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   PhysX86Addr             The x86 address to start reading from.
 * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
 * @param   pvVal                   Where to store the value on success.
 */
int PSPProxyCtxPspX86MmioRead(PSPPROXYCTX hCtx, X86PADDR PhysX86Addr, uint32_t cbVal, void *pvVal);

/**
 * Writes to the x86 MMIO address space using the PSP (to circumvent protection mechanisms
 * on the x86 core).
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   PhysX86Addr             The x86 address to start writing to.
 * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
 * @param   pvVal                   The value to write.
 */
int PSPProxyCtxPspX86MmioWrite(PSPPROXYCTX hCtx, X86PADDR PhysX86Addr, uint32_t cbVal, const void *pvVal);

/**
 * Generic data transfer method, more capable than the other methods but also more cmoplicated to use.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   pPspAddr                The PSP address information for this transfer.
 * @param   fFlags                  Flags for this transfer, see PSPPROXY_CTX_ADDR_XFER_F_XXX.
 * @param   cbStride                Stride for an individual access (1, 2 or 4 bytes).
 * @param   cbXfer                  Overall number of bytes to transfer, must be multiple of stride.
 * @param   pvLocal                 The local data buffer to write to/read from.
 */
int PSPProxyCtxPspAddrXfer(PSPPROXYCTX hCtx, PCPSPPROXYADDR pPspAddr, uint32_t fFlags, size_t cbStride, size_t cbXfer, void *pvLocal);

/**
 * Writes to the given co processor register.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   idCoProc                Co-Processor identifier to access.
 * @param   idCrn                   The CRn value.
 * @param   idCrm                   The CRm value.
 * @param   idOpc1                  The opc1 value.
 * @param   idOpc2                  The opc2 value.
 * @param   u32Val                  The value to write.
 */
int PSPProxyCtxPspCoProcWrite(PSPPROXYCTX hCtx, uint8_t idCoProc, uint8_t idCrn, uint8_t idCrm, uint8_t idOpc1, uint8_t idOpc2,
                              uint32_t u32Val);

/**
 * Reads from the given co processor register.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   idCoProc                Co-Processor identifier to access.
 * @param   idCrn                   The CRn value.
 * @param   idCrm                   The CRm value.
 * @param   idOpc1                  The opc1 value.
 * @param   idOpc2                  The opc2 value.
 * @param   pu32Val                 Where to store the value read on success.
 */
int PSPProxyCtxPspCoProcRead(PSPPROXYCTX hCtx, uint8_t idCoProc, uint8_t idCrn, uint8_t idCrm, uint8_t idOpc1, uint8_t idOpc2,
                             uint32_t *pu32Val);

/**
 * Execute a syscall on the PSP.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   idxSyscall              The syscall to execute.
 * @param   u32R0                   R0 argument.
 * @param   u32R1                   R1 argument.
 * @param   u32R2                   R2 argument.
 * @param   u32R3                   R3 argument.
 * @param   pu32R0Return            Where to store the R0 return value of the syscall.
 */
int PSPProxyCtxPspSvcCall(PSPPROXYCTX hCtx, uint32_t idxSyscall, uint32_t u32R0, uint32_t u32R1, uint32_t u32R2, uint32_t u32R3, uint32_t *pu32R0Return);

/**
 * Wait for interrupt to happen on one of the PSPs.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   pidCcd                  Where to store the CCD ID where the interrupt occured on success.
 * @param   pfIrq                   Where to return whether an IRQ is pending on success.
 * @param   pfFirq                  Where to return whether an FIRQ is pending on success.
 * @param   cWaitMs                 Number if milliseconds to wait before returning a timeout.
 */
int PSPProxyCtxPspWaitForIrq(PSPPROXYCTX hCtx, uint32_t *pidCcd, bool *pfIrq, bool *pfFirq, uint32_t cWaitMs);

/**
 * Reads the register at the given SMN address, the access is initiated from the x86 core and not the PSP.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   idNode                  The node ID to target the read from.
 * @param   uSmnAddr                The SMN address/offset to access.
 * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
 * @param   pvVal                   Where to store the value on success.
 */
int PSPProxyCtxX86SmnRead(PSPPROXYCTX hCtx, uint16_t idNode, SMNADDR uSmnAddr, uint32_t cbVal, void *pvVal);

/**
 * Writes the register at the given SMN address, the access is initiated from the x86 core and not the PSP.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   idNode                  The node ID to target the write from.
 * @param   uSmnAddr                The SMN address/offset to access.
 * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
 * @param   pvVal                   Where to store the value on success.
 */
int PSPProxyCtxX86SmnWrite(PSPPROXYCTX hCtx, uint16_t idNode, SMNADDR uSmnAddr, uint32_t cbVal, const void *pvVal);

/**
 * Allocates a contiguous region of memory accessible from R0.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   cbMem                   Number of bytes to allocate.
 * @param   pR0KernVirtual          Where to store the R0 virtual address of the allocated region on success.
 * @param   pPhysX86Addr            Where to store the X86 physical address of the allocated region on success.
 */
int PSPProxyCtxX86MemAlloc(PSPPROXYCTX hCtx, uint32_t cbMem, R0PTR *pR0KernVirtual, X86PADDR *pPhysX86Addr);

/**
 * Frees a previous allocated R0 memory region.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   R0KernVirtual           The R0 virtual address to free.
 */
int PSPProxyCtxX86MemFree(PSPPROXYCTX hCtx, R0PTR R0KernVirtual);

/**
 * Copies memory from a given R0 virtual address to a supplied userspace buffer.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   pvDst                   Where to store the read bits.
 * @param   R0KernVirtualSrc        The virtual R0 address to read from.
 * @param   cbRead                  How much to read.
 */
int PSPProxyCtxX86MemRead(PSPPROXYCTX hCtx, void *pvDst, R0PTR R0KernVirtualSrc, uint32_t cbRead);

/**
 * Copies memory from a supplied userspace buffer tp the given R0 virtual address.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   R0KernVirtualDst        The virtual R0 address to write to.
 * @param   pvSrc                   The source buffer.
 * @param   cbWrite                 How much to write.
 */
int PSPProxyCtxX86MemWrite(PSPPROXYCTX hCtx, R0PTR R0KernVirtualDst, const void *pvSrc, uint32_t cbWrite);

/**
 * Copies memory from a given x86 physical address to a supplied userspace buffer.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   pvDst                   Where to store the read bits.
 * @param   PhysX86AddrSrc          The physical x86 address to read from.
 * @param   cbRead                  How much to read.
 */
int PSPProxyCtxX86PhysMemRead(PSPPROXYCTX hCtx, void *pvDst, X86PADDR PhysX86AddrSrc, uint32_t cbRead);

/**
 * Copies memory from a supplied userspace buffer tp the given x86 physical address.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   PhysX86AddrDst          The physical x86 address to write to.
 * @param   pvSrc                   The source buffer.
 * @param   cbWrite                 How much to write.
 */
int PSPProxyCtxX86PhysMemWrite(PSPPROXYCTX hCtx, X86PADDR PhysX86AddrDst, const void *pvSrc, uint32_t cbWrite);

/**
 * Waits for a command when the PSP emulation is used.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   pidCmd                  Where to store the command id on success.
 * @param   pPhysX86AddrCmdBuf      Where to store the physical address of the command buffer on success.
 * @param   msWait                  The amount of milliseconds to wait before timing out.
 */
int PSPProxyCtxEmuWaitForWork(PSPPROXYCTX hCtx, uint32_t *pidCmd, X86PADDR *pPhysX86AddrCmdBuf, uint32_t msWait);

/**
 * Sets the result of a previous received command.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   uResult                 The result to set.
 */
int PSPProxyCtxEmuSetResult(PSPPROXYCTX hCtx, uint32_t uResult);

/**
 * Allocates a region of scratch space on the PSP.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   cbAlloc                 How much to allocate.
 * @param   pPspAddr                Where to store the PSP address of the start of the allocated scratch space on success.
 */
int PSPProxyCtxScratchSpaceAlloc(PSPPROXYCTX hCtx, size_t cbAlloc, PSPADDR *pPspAddr);

/**
 * Frees a previously allocated scratch space region.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   PspAddr                 The previously allocated scratch space PSP address.
 * @param   cb                      Size of the scratch space area to free (must be same as used during allocation).
 */
int PSPProxyCtxScratchSpaceFree(PSPPROXYCTX hCtx, PSPADDR PspAddr, size_t cb);

/**
 * Loads the given code module into the PSP.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   pvCm                    The code module binary data.
 * @param   cbCm                    Size of the code module in bytes.
 */
int PSPProxyCtxCodeModLoad(PSPPROXYCTX hCtx, const void *pvCm, size_t cbCm);

/**
 * Executes the currently loaded code module using the provided arguments.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   u32Arg0                 Argument 0.
 * @param   u32Arg1                 Argument 1.
 * @param   u32Arg2                 Argument 2.
 * @param   u32Arg3                 Argument 3.
 * @param   pu32CmdRet              Where to store the return value of the code module when it returns.
 * @param   cMillies                How long to wait for the code module to finish exeucting until a timeout
 *                                  error is returned.
 */
int PSPProxyCtxCodeModExec(PSPPROXYCTX hCtx, uint32_t u32Arg0, uint32_t u32Arg1, uint32_t u32Arg2, uint32_t u32Arg3,
                           uint32_t *pu32CmRet, uint32_t cMillies);

#endif /* __libpspproxy_h */
