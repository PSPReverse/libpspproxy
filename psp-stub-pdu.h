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


/** Opaque PSP Stub PDU context handle. */
typedef struct PSPSTUBPDUCTXINT *PSPSTUBPDUCTX;
/** Pointer to an opaque PSP Stub PDU context handle. */
typedef PSPSTUBPDUCTX *PPSPSTUBPDUCTX;


/**
 * Stub PDU I/O interface callback table.
 */
typedef struct PSPSTUBPDUIOIF
{
    /** 
     * Returns amount of data available for reading (for optimized buffer allocations).
     *
     * @returns Amount of bytes available for reading.
     * @param   hPspStubPduCtx      The PSP stub PDU context handle invoking the callback.
     * @param   pvUser              Opaque user data passed during creation of the stub context.
     */
    size_t (*pfnPeek) (PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser);

    /**
     * Read data from the underlying transport layer - non blocking.
     *
     * @returns Status code.
     * @param   hPspStubPduCtx      The PSP stub PDU context handle invoking the callback.
     * @param   pvUser              Opaque user data passed during creation of the stub context.
     * @param   pvDst               Where to store the read data.
     * @param   cbRead              Maximum number of bytes to read.
     * @param   pcbRead             Where to store the number of bytes actually read.
     */
    int    (*pfnRead) (PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser, void *pvDst, size_t cbRead, size_t *pcbRead);

    /** 
     * Write a packet to the underlying transport layer.
     *
     * @returns Status code.
     * @param   hPspStubPduCtx      The PSP stub PDU context handle invoking the callback.
     * @param   pvUser              Opaque user data passed during creation of the stub context.
     * @param   pvPkt               The packet data to write.
     * @param   cbPkt               The number of bytes to write.
     *
     * @note Unlike the read callback this should only return when the whole packet has been written
     *       or an unrecoverable error occurred.
     */
    int    (*pfnWrite) (PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser, const void *pvPkt, size_t cbPkt);

    /**
     * Blocks until data is available for reading.
     *
     * @returns Status code.
     * @param   hPspStubPduCtx      The PSP stub PDU context handle invoking the callback.
     * @param   pvUser              Opaque user data passed during creation of the stub context.
     * @param   cMillies            Number of milliseconds to wait before returning a timeout error.
     */
    int    (*pfnPoll) (PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser, uint32_t cMillies);

    /**
     * Interrupt any polling.
     *
     * @returns Status code.
     * @param   hPspStubPduCtx      The PSP stub PDU context handle invoking the callback.
     * @param   pvUser              Opaque user data passed during creation of the stub context.
     */
    int    (*pfnInterrupt) (PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser);

} PSPSTUBPDUIOIF;
/** Pointer to a I/O interface callback table. */
typedef PSPSTUBPDUIOIF *PPSPSTUBPDUIOIF;
/** Pointer to a const I/O interface callback table. */
typedef const PSPSTUBPDUIOIF *PCPSPSTUBPDUIOIF;


/**
 * Creates a new PSP Stub PDU context.
 *
 * @returns Status code.
 * @param   phPduCtx                Where to store the handle to the context on success.
 * @param   pIoIf                   The I/O interface callback table to use.
 * @param   pvUser                  Opaque user data to pass to the I/O interface callbacks.
 */
int pspStubPduCtxCreate(PPSPSTUBPDUCTX phPduCtx, PCPSPSTUBPDUIOIF pIoIf, void *pvUser);


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

#endif /* !__psp_stub_pdu_h */
