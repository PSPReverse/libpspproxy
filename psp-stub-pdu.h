/** @file
 * PSP PDU library for the several providers.
 */

/*
 * Copyright (C) 2020 Alexander Eichner <alexander.eichner@campus.tu-berlin.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef __psp_stub_pdu_h
#define __psp_stub_pdu_h

#include <stdint.h>
#include <stddef.h>

#include "psp-proxy-provider.h"


/** Opaque PSP Stub PDU context handle. */
typedef struct PSPSTUBPDUCTXINT *PSPSTUBPDUCTX;
/** Pointer to an opaque PSP Stub PDU context handle. */
typedef PSPSTUBPDUCTX *PPSPSTUBPDUCTX;


/**
 * Creates a new PSP Stub PDU context.
 *
 * @returns Status code.
 * @param   phPduCtx                Where to store the handle to the context on success.
 * @param   pProvIf                 The provider interface.
 * @param   hProvCtx                The provider context handle to use.
 * @param   pProxyIoIf              Proxy I/O interface callback table.
 * @param   hProxyCtx               The proxy context handle to pass to callbacks in pProxyIoIf.
 * @param   pvUser                  Opaque user data to pass to the callbacks in pProxyIoIf.
 */
int pspStubPduCtxCreate(PPSPSTUBPDUCTX phPduCtx, PCPSPPROXYPROV pProvIf, PSPPROXYPROVCTX hProvCtx,
                        PCPSPPROXYIOIF pProxyIoIf, PSPPROXYCTX hProxyCtx, void *pvUser);


/**
 * Destroys a given PSP Stub PDU context.
 *
 * @returns nothing.
 * @param   hPduCtx                 The PDU context handle to destroy.
 */
void pspStubPduCtxDestroy(PSPSTUBPDUCTX hPduCtx);


/**
 * Tries to connect to the remote end.
 *
 * @returns nothing.
 * @param   hPduCtx                 The PDU context handle.
 * @param   cMillies                Number of milliseconds to try to connect.
 */
int pspStubPduCtxConnect(PSPSTUBPDUCTX hPduCtx, uint32_t cMillies);


/**
 * Queries information about the given PSP.
 *
 * @returns Status code.
 * @param   hPduCtx                 The PDU context handle.
 * @param   idCcd                   The CCD to query the data for.
 * @param   pPspAddrScratchStart    Where to store the the start address of the scratch space area on success.
 * @param   pcbScratch              Where to store the size of the scratch space area in bytes on success.
 */
int pspStubPduCtxQueryInfo(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, PSPADDR *pPspAddrScratchStart, size_t *pcbScratch);


/**
 * Query the returned status code of the last request.
 *
 * @returns Status code of this call.
 * @param   hPduCtx                 The PDU context handle.
 * @param   pReqRcLast              Where to store the status code of the last issued request.
 */
int pspStubPduCtxQueryLastReqRc(PSPSTUBPDUCTX hPduCtx, PSPSTS *pReqRcLast);


/**
 * Reads the register at the given SMN address.
 *
 * @returns Status code.
 * @param   hPduCtx                 The PDU context handle.
 * @param   idCcd                   The source CCD ID for the read.
 * @param   idCcdTgt                The target CCD ID to access the register on.
 * @param   uSmnAddr                The SMN address/offset to access.
 * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
 * @param   pvVal                   Where to store the value on success.
 */
int pspStubPduCtxPspSmnRead(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, void *pvVal);


/**
 * Writes to the register at the given SMN address.
 *
 * @returns Status code.
 * @param   hPduCtx                 The PDU context handle.
 * @param   idCcd                   The source CCD ID for the write.
 * @param   idCcdTgt                The target CCD ID to access the register on.
 * @param   uSmnAddr                The SMN address/offset to access.
 * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
 * @param   pvVal                   The value to write.
 */
int pspStubPduCtxPspSmnWrite(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, const void *pvVal);


/**
 * Reads from the PSP address space at the given address.
 *
 * @returns Status code.
 * @param   hPduCtx                 The PDU context handle.
 * @param   idCcd                   The CCD ID for the read.
 * @param   uPspAddr                The PSP address to start reading from.
 * @param   pvBuf                   Where to store the read data.
 * @param   cbRead                  Amount of data to read.
 */
int pspStubPduCtxPspMemRead(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, PSPADDR uPspAddr, void *pvBuf, uint32_t cbRead);


/**
 * Writes to the PSP address space at the given address.
 *
 * @returns Status code.
 * @param   hPduCtx                 The PDU context handle.
 * @param   idCcd                   The CCD ID for the read.
 * @param   uPspAddr                The PSP address to start writing to.
 * @param   pvBuf                   The data to write.
 * @param   cbWrite                 Amount of data to write.
 */
int pspStubPduCtxPspMemWrite(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, PSPADDR uPspAddr, const void *pvBuf, uint32_t cbWrite);


/**
 * Reads from the PSP MMIO address space at the given address.
 *
 * @returns Status code.
 * @param   hPduCtx                 The PDU context handle.
 * @param   idCcd                   The CCD ID for the read.
 * @param   uPspAddr                The PSP address to start reading from.
 * @param   pvVal                   The value to write.
 * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
 */
int pspStubPduCtxPspMmioRead(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, PSPADDR uPspAddr, void *pvVal, uint32_t cbVal);


/**
 * Writes to the PSP MMIO address space at the given address.
 *
 * @returns Status code.
 * @param   hPduCtx                 The PDU context handle.
 * @param   idCcd                   The CCD ID for the read.
 * @param   uPspAddr                The PSP address to start writing to.
 * @param   pvVal                   The value to write.
 * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
 */
int pspStubPduCtxPspMmioWrite(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, PSPADDR uPspAddr, const void *pvVal, uint32_t cbVal);


/**
 * Reads from the x86 address space using the PSP (to circumvent protection mechanisms
 * on the x86 core).
 *
 * @returns Status code.
 * @param   hPduCtx                 The PDU context handle.
 * @param   idCcd                   The CCD ID for the read.
 * @param   PhysX86Addr             The x86 address to start reading from.
 * @param   pvBuf                   Where to store the read data.
 * @param   cbRead                  Amount of data to read.
 */
int pspStubPduCtxPspX86MemRead(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, X86PADDR PhysX86Addr, void *pvBuf, uint32_t cbRead);


/**
 * Writes to the x86 address space using the PSP (to circumvent protection mechanisms
 * on the x86 core).
 *
 * @returns Status code.
 * @param   hPduCtx                 The PDU context handle.
 * @param   idCcd                   The CCD ID for the read.
 * @param   PhysX86Addr             The x86 address to start writing to.
 * @param   pvBuf                   The data to write.
 * @param   cbWrite                 Amount of data to write.
 */
int pspStubPduCtxPspX86MemWrite(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, X86PADDR PhysX86Addr, const void *pvBuf, uint32_t cbWrite);


/**
 * Reads from the x86 MMIO address space at the given address.
 *
 * @returns Status code.
 * @param   hPduCtx                 The PDU context handle.
 * @param   idCcd                   The CCD ID for the read.
 * @param   PhysX86Addr             The x86 address to start reading from.
 * @param   pvVal                   The value to write.
 * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
 */
int pspStubPduCtxPspX86MmioRead(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, X86PADDR PhysX86Addr, void *pvVal, uint32_t cbVal);


/**
 * Writes to the x86 MMIO address space at the given address.
 *
 * @returns Status code.
 * @param   hPduCtx                 The PDU context handle.
 * @param   idCcd                   The CCD ID for the read.
 * @param   uPspAddr                The x86 address to start writing to.
 * @param   pvVal                   The value to write.
 * @param   cbVal                   Size of the register, vaid are 1, 2, 4 or 8 byte.
 */
int pspStubPduCtxPspX86MmioWrite(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, X86PADDR PhysX86Addr, const void *pvVal, uint32_t cbVal);


/**
 * Generic data transfer method, more capable than the other methods but also more cmoplicated to use.
 *
 * @returns Status code.
 * @param   hPduCtx                 The PDU context handle.
 * @param   idCcd                   The CCD ID for the transfer.
 * @param   pPspAddr                The PSP address information for this transfer.
 * @param   fFlags                  Flags for this transfer, see PSPPROXY_CTX_ADDR_XFER_F_XXX.
 * @param   cbStride                Stride for an individual access (1, 2 or 4 bytes).
 * @param   cbXfer                  Overall number of bytes to transfer, must be multiple of stride.
 * @param   pvLocal                 The local data buffer to write to/read from.
 */
int pspStubPduCtxPspAddrXfer(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, PCPSPPROXYADDR pPspAddr, uint32_t fFlags, size_t cbStride,
                             size_t cbXfer, void *pvLocal);


/**
 * Writes to the given co processor register.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   idCcd                   The CCD ID for the transfer.
 * @param   idCoProc                Co-Processor identifier to access.
 * @param   idCrn                   The CRn value.
 * @param   idCrm                   The CRm value.
 * @param   idOpc1                  The opc1 value.
 * @param   idOpc2                  The opc2 value.
 * @param   u32Val                  The value to write.
 */
int pspStubPduCtxPspCoProcWrite(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, uint8_t idCoProc, uint8_t idCrn, uint8_t idCrm,
                                uint8_t idOpc1, uint8_t idOpc2, uint32_t u32Val);


/**
 * Reads from the given co processor register.
 *
 * @returns Status code.
 * @param   hCtx                    The PSP proxy context handle.
 * @param   idCcd                   The CCD ID for the transfer.
 * @param   idCoProc                Co-Processor identifier to access.
 * @param   idCrn                   The CRn value.
 * @param   idCrm                   The CRm value.
 * @param   idOpc1                  The opc1 value.
 * @param   idOpc2                  The opc2 value.
 * @param   pu32Val                 Where to store the value read on success.
 */
int pspStubPduCtxPspCoProcRead(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, uint8_t idCoProc, uint8_t idCrn, uint8_t idCrm,
                               uint8_t idOpc1, uint8_t idOpc2, uint32_t *pu32Val);


/**
 * Wait for interrupt to happen on one of the PSPs.
 *
 * @returns Status code.
 * @param   hPduCtx                 The PDU context handle.
 * @param   pidCcd                  Where to store the CCD ID where the interrupt occured on success.
 * @param   pfIrq                   Where to return whether an IRQ is pending on success.
 * @param   pfFirq                  Where to return whether an FIRQ is pending on success.
 * @param   cWaitMs                 Number if milliseconds to wait before returning a timeout.
 */
int pspStubPduCtxPspWaitForIrq(PSPSTUBPDUCTX hPduCtx, uint32_t *pidCcd, bool *pfIrq, bool *pfFirq, uint32_t cWaitMs);


/**
 * Loads a code module on the given PSP.
 *
 * @returns Status code.
 * @param   hPduCtx                 The PDU context handle.
 * @param   idCcd                   The CCD ID for the request.
 * @param   pvCm                    The code module bits.
 * @param   cbCm                    Size of the code module in bytes.
 */
int pspStubPduCtxPspCodeModLoad(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, const void *pvCm, size_t cbCm);


/**
 * Executes a previously loaded code module on the given PSP.
 *
 * @returns Status code.
 * @param   hPduCtx                 The PDU context handle.
 * @param   idCcd                   The CCD ID for the request.
 * @param   u32Arg0                 Argument 0.
 * @param   u32Arg1                 Argument 1.
 * @param   u32Arg2                 Argument 2.
 * @param   u32Arg3                 Argument 3.
 * @param   pu32CmRet               Where to store the return value of the code module upon return.
 * @param   cMillies                How long to wait for the code module to finish exeucting until a timeout
 *                                  error is returned.
 */
int pspStubPduCtxPspCodeModExec(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, uint32_t u32Arg0, uint32_t u32Arg1,
                                uint32_t u32Arg2, uint32_t u32Arg3, uint32_t *pu32CmRet, uint32_t cMillies);


/**
 * Lets the stub branch to the given destination (probably killing the stub).
 *
 * @returns Status code.
 * @param   hPduCtx                 The PDU context handle.
 * @param   idCcd                   The CCD ID for the request.
 * @param   PspAddrPc               The address to branch to.
 * @param   fThumb                  Flag whether to switch to thumb mode.
 * @param   pau32Gprs               Pointer to the general purpose register values to set up (r0-r12).
 */
int pspStubPduCtxBranchTo(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, PSPPADDR PspAddrPc, bool fThumb, uint32_t *pau32Gprs);

#endif /* !__psp_stub_pdu_h */
