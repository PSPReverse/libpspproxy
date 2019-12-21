/** @file
 * PSP proxy library to interface with the hardware of the PSP
 */

/*
 * Copyright (C) 2019 Alexander Eichner <alexander.eichner@campus.tu-berlin.de>
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

#include <stdint.h>

/** A SMN (System Management Network) address. */
typedef uint32_t SMNADDR;
/** A virtual PSP memory address. */
typedef uint32_t PSPADDR;
/** A x86 physical address. */
typedef uint64_t X86PADDR;
/** R0 pointer. */
typedef uint64_t R0PTR;

/** Opaque PSP proxy context handle. */
typedef struct PSPPROXYCTXINT *PSPPROXYCTX;
/** Pointer to a PSP proxy context handle. */
typedef PSPPROXYCTX *PPSPPROXYCTX;


/**
 * Creates a new PSP proxy context for the given device.
 *
 * @returns Status code.
 * @param   phCtx                   Where to store the handle to the PSP proxy context on success.
 * @param   pszDevice               The device to use, usually /dev/sev.
 */
int PSPProxyCtxCreate(PPSPPROXYCTX phCtx, const char *pszDevice);

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

#endif /* __libpspproxy_h */
