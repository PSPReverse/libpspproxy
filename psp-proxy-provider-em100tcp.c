/** @file
 * PSP proxy library to interface with the hardware of the PSP - remote access over a SPI channel using the EM100 flash emulator
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
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <poll.h>
#include <sys/ioctl.h>

#include <common/cdefs.h>

#include "psp-proxy-provider.h"
#include "psp-stub-pdu.h"


/**
 * Request header sent over the network.
 */
typedef struct REQHDR
{
    /** Magic for the header. */
    uint32_t                u32Magic;
    /** Command ID 0 = read, 1 = write. */
    uint32_t                u32Cmd;
    /** Start address to access. */
    uint32_t                u32AddrStart;
    /** Number of bytes for the transfer. */
    uint32_t                cbXfer;
} REQHDR;

/** Magic identifying the request. */
#define REQHDR_MAGIC 0xebadc0de


/**
 * A ring buffer header.
 */
typedef struct SPIRINGBUF
{
    /** Size of the ring buffer. */
    uint32_t                        cbRingBuf;
    /** The head counter (producer). */
    uint32_t                        offHead;
    /** The tail counter (consumer). */
    uint32_t                        offTail;
} SPIRINGBUF;
/** Pointer to a ring buffer header. */
typedef struct SPIRINGBUF *PSPIRINGBUF;


/**
 * The message channel header.
 */
typedef struct SPIMSGCHANHDR
{
    /** Offset where the EXT -> PSP ring buffer is located (from the beginning of the message channel). */
    uint32_t                        offExt2PspBuf;
    /** Offset where the PSP -> EXT ring buffer is located (from the beginning of the message channel). */
    uint32_t                        offPsp2ExtBuf;
    /** The EXT -> PSP ring buffer header. */
    SPIRINGBUF                      Ext2PspRingBuf;
    /** The PSP -> EXT ring buffer header. */
    SPIRINGBUF                      Psp2ExtRingBuf;
    /** The message channel header magic, must be last. */
    uint32_t                        u32Magic;
} SPIMSGCHANHDR;

/** Where in the flash the message channel is located. */
#define SPI_MSG_CHAN_HDR_OFF   0xaab000
/** THe magic vaue to identify the message channel header (J. R. R. Tolkien). */
#define SPI_MSG_CHAN_HDR_MAGIC 0x18920103


/**
 * Internal PSP proxy provider context.
 */
typedef struct PSPPROXYPROVCTXINT
{
    /** The socket descriptor for the connection to the em100 network server. */
    int                             iFdCon;
    /** The PDU context. */
    PSPSTUBPDUCTX                   hPduCtx;
    /** The message channel header constantly updated. */
    SPIMSGCHANHDR                   MsgChanHdr;
} PSPPROXYPROVCTXINT;
/** Pointer to an internal PSP proxy context. */
typedef PSPPROXYPROVCTXINT *PPSPPROXYPROVCTXINT;



/**
 * Reads from the given SPI flash address.
 *
 * @returns Status code.
 * @param   pThis                   The EM100 provider instance.
 * @param   u32SpiAddrStart         The flash address to start reading from.
 * @param   pvBuf                   Where to store the read data.
 * @param   cbRead                  How many bytes to read.
 */
static int em100TcpSpiFlashRead(PPSPPROXYPROVCTXINT pThis, uint32_t u32AddrStart, void *pvBuf, size_t cbRead)
{
    int rc = 0;
    REQHDR Req;

    Req.u32Magic     = 0xebadc0de;
    Req.u32Cmd       = 0;
    Req.u32AddrStart = u32AddrStart;
    Req.cbXfer       = cbRead;
    ssize_t cbXfer = send(pThis->iFdCon, &Req, sizeof(Req), 0);
    if (cbXfer == sizeof(Req))
    {
        /* Wait for the status code. */
        int32_t rcReq = 0;
        cbXfer = recv(pThis->iFdCon, &rcReq, sizeof(rcReq), 0);
        if (   cbXfer == sizeof(rcReq)
            && rcReq == 0)
        {
            cbXfer = recv(pThis->iFdCon, pvBuf, cbRead, 0);
            if (cbXfer != cbRead)
                rc = -1;
        }
        else
            rc = -1;
    }
    else
        rc = -1;

    return rc;
}


/**
 * Writes to the given SPI flash address.
 *
 * @returns Status code.
 * @param   pThis                   The EM100 provider instance.
 * @param   u32SpiAddrStart         The flash address to start writing to.
 * @param   pvBuf                   The data to write.
 * @param   cbWrite                 How many bytes to write.
 */
static int em100TcpSpiFlashWrite(PPSPPROXYPROVCTXINT pThis, uint32_t u32AddrStart, const void *pvBuf, size_t cbWrite)
{
    int rc = 0;
    REQHDR Req;

    Req.u32Magic     = 0xebadc0de;
    Req.u32Cmd       = 1;
    Req.u32AddrStart = u32AddrStart;
    Req.cbXfer       = cbWrite;
    ssize_t cbXfer = send(pThis->iFdCon, &Req, sizeof(Req), 0);
    if (cbXfer == sizeof(Req))
    {
        cbXfer = send(pThis->iFdCon, pvBuf, cbWrite, 0);
        if (cbXfer == cbWrite)
        {
            /* Wait for the status code. */
            int32_t rcReq = 0;
            cbXfer = recv(pThis->iFdCon, &rcReq, sizeof(rcReq), 0);
            if (   cbXfer != sizeof(rcReq)
                || rcReq != 0)
                rc = -1;
        }
        else
            rc = -1;
    }
    else
        rc = -1;

    return rc;
}


/**
 * Init the SPI message buffer structures.
 *
 * @returns Status code.
 * @param   pThis                   The EM100 provider instance.
 */
static int em100TcpSpiMsgBufferInit(PPSPPROXYPROVCTXINT pThis)
{
    pThis->MsgChanHdr.u32Magic      = SPI_MSG_CHAN_HDR_MAGIC;
    pThis->MsgChanHdr.offExt2PspBuf = sizeof(SPIMSGCHANHDR);
    pThis->MsgChanHdr.offPsp2ExtBuf = sizeof(SPIMSGCHANHDR) + _4K;
    pThis->MsgChanHdr.Ext2PspRingBuf.cbRingBuf = _4K;
    pThis->MsgChanHdr.Ext2PspRingBuf.offHead   = 0;
    pThis->MsgChanHdr.Ext2PspRingBuf.offTail   = 0;
    pThis->MsgChanHdr.Psp2ExtRingBuf.cbRingBuf = _4K;
    pThis->MsgChanHdr.Psp2ExtRingBuf.offHead   = 0;
    pThis->MsgChanHdr.Psp2ExtRingBuf.offTail   = 0;

    return em100TcpSpiFlashWrite(pThis, SPI_MSG_CHAN_HDR_OFF, &pThis->MsgChanHdr, sizeof(pThis->MsgChanHdr));
}


/**
 * Updates our copy of the SPI message buffer header.
 *
 * @returns Status code.
 * @param   pThis                   The EM100 provider instance.
 */
static int em100TcpSpiMsgBufferHdrFetch(PPSPPROXYPROVCTXINT pThis)
{
    return em100TcpSpiFlashRead(pThis, SPI_MSG_CHAN_HDR_OFF, &pThis->MsgChanHdr, sizeof(pThis->MsgChanHdr));
}


/**
 * Returns the amount of free bytes in the ring buffer.
 *
 * @returns Number of bytes free in the ring buffer.
 * @param   pRingBuf                The ring buffer.
 */
static size_t em100TcpSpiMsgBufferGetFree(PSPIRINGBUF pRingBuf)
{
    if (pRingBuf->offHead >= pRingBuf->offTail)
        return pRingBuf->cbRingBuf - (pRingBuf->offHead - pRingBuf->offTail);

    /* Wrap around. */
    return pRingBuf->offTail - pRingBuf->offHead;
}


/**
 * Returns the amount of used bytes in the ring buffer.
 *
 * @returns Number of bytes used in the ring buffer.
 * @param   pRingBuf                The ring buffer.
 */
static size_t em100TcpSpiMsgBufferGetUsed(PSPIRINGBUF pRingBuf)
{
    return pRingBuf->cbRingBuf - em100TcpSpiMsgBufferGetFree(pRingBuf);
}


/**
 * Returns the amount of bytes which can be written in one go, i.e. until the
 * buffer is full or a head pointer wraparound occurs.
 *
 * @returns Number of bytes which can be written in one go.
 * @param   pRingBuf                The ring buffer.
 */
static size_t em100TcpSpiMsgBufferGetWrite(PSPIRINGBUF pRingBuf)
{
    size_t cbFree = em100TcpSpiMsgBufferGetFree(pRingBuf);
    return MIN(cbFree, pRingBuf->cbRingBuf - pRingBuf->offHead);
}


/**
 * Returns the amount of bytes which can be read in one go, i.e. until the
 * buffer is full or a tail pointer wraparound occurs.
 *
 * @returns Number of bytes which can be read in one go.
 * @param   pRingBuf                The ring buffer.
 */
static size_t em100TcpSpiMsgBufferGetRead(PSPIRINGBUF pRingBuf)
{
    size_t cbUsed = em100TcpSpiMsgBufferGetUsed(pRingBuf);
    return MIN(cbUsed, pRingBuf->cbRingBuf - pRingBuf->offTail);
}


/**
 * Advances the write pointer of the given ring buffer.
 *
 * @returns nothing.
 * @param   pRingBuf                The ring buffer.
 * @param   cbWrite                 Amount of bytes to advance the head pointer.
 */
static void em100TcpSpiMsgBufferWriteAdv(PSPIRINGBUF pRingBuf, size_t cbWrite)
{
    pRingBuf->offHead += cbWrite;
    pRingBuf->offHead %= pRingBuf->cbRingBuf;
}


/**
 * Advances the read pointer of the given ring buffer.
 *
 * @returns nothing.
 * @param   pRingBuf                The ring buffer.
 * @param   cbRead                  Amount of bytes to advance the tail pointer.
 */
static void em100TcpSpiMsgBufferReadAdv(PSPIRINGBUF pRingBuf, size_t cbRead)
{
    pRingBuf->offTail += cbRead;
    pRingBuf->offTail %= pRingBuf->cbRingBuf;
}


/**
 * Writes into the SPI message buffer.
 *
 * @returns Status code.
 * @param   pThis                   The EM100 provider instance.
 * @param   pvBuf                   The data to write.
 * @param   cbWrite                 Number of bytes to write.
 */
static int em100TcpSpiMsgBufferWrite(PPSPPROXYPROVCTXINT pThis, const void *pvBuf, size_t cbWrite)
{
    int rc = 0;
    uint8_t *pbBuf = (uint8_t *)pvBuf;
    size_t cbWriteLeft = cbWrite;

    do
    {
        rc = em100TcpSpiMsgBufferHdrFetch(pThis);
        if (!rc)
        {
            /* Check whether we have room to write the data into the ring buffer. */
            size_t cbThisWrite = MIN(cbWriteLeft,
                                     em100TcpSpiMsgBufferGetWrite(&pThis->MsgChanHdr.Ext2PspRingBuf));
            if (cbThisWrite)
            {
                /* Write the data. */
                size_t offSpiFlashWrite =   SPI_MSG_CHAN_HDR_OFF                       /* Static offset. */
                                          + pThis->MsgChanHdr.offExt2PspBuf            /* Relative offset where the data area of the ring buffer starts. */
                                          + pThis->MsgChanHdr.Ext2PspRingBuf.offHead;  /* Offset into the data area to write. */
                rc = em100TcpSpiFlashWrite(pThis, offSpiFlashWrite, pbBuf, cbThisWrite);
                if (!rc)
                {
                    pbBuf       += cbThisWrite;
                    cbWriteLeft -= cbThisWrite;
                    em100TcpSpiMsgBufferWriteAdv(&pThis->MsgChanHdr.Ext2PspRingBuf, cbThisWrite);
                    /* Update the pointer in flash. */
                    rc = em100TcpSpiFlashWrite(pThis, SPI_MSG_CHAN_HDR_OFF + offsetof(SPIMSGCHANHDR, Ext2PspRingBuf.offHead),
                                               &pThis->MsgChanHdr.Ext2PspRingBuf.offHead,
                                               sizeof(pThis->MsgChanHdr.Ext2PspRingBuf.offHead));
                }
            }
        }
    } while (   !rc
             && cbWriteLeft);

    return rc;
}


/**
 * Reads from the SPI message buffer.
 *
 * @returns Status code.
 * @param   pThis                   The EM100 provider instance.
 * @param   pvBuf                   Where to store the read data.
 * @param   cbRead                  Number of bytes to read.
 */
static int em100TcpSpiMsgBufferRead(PPSPPROXYPROVCTXINT pThis, void *pvBuf, size_t cbRead)
{
    int rc = 0;
    uint8_t *pbBuf = (uint8_t *)pvBuf;
    size_t cbReadLeft = cbRead;

    do
    {
        rc = em100TcpSpiMsgBufferHdrFetch(pThis);
        if (!rc)
        {
            /* Check whether we have room to write the data into the ring buffer. */
            size_t cbThisRead = MIN(cbReadLeft,
                                    em100TcpSpiMsgBufferGetRead(&pThis->MsgChanHdr.Psp2ExtRingBuf));
            if (cbThisRead)
            {
                /* Write the data. */
                size_t offSpiFlashRead =   SPI_MSG_CHAN_HDR_OFF                       /* Static offset. */
                                         + pThis->MsgChanHdr.offPsp2ExtBuf            /* Relative offset where the data area of the ring buffer starts. */
                                         + pThis->MsgChanHdr.Psp2ExtRingBuf.offTail;  /* Offset into the data area to read. */
                rc = em100TcpSpiFlashRead(pThis, offSpiFlashRead, pbBuf, cbThisRead);
                if (!rc)
                {
                    pbBuf      += cbThisRead;
                    cbReadLeft -= cbThisRead;
                    em100TcpSpiMsgBufferReadAdv(&pThis->MsgChanHdr.Psp2ExtRingBuf, cbThisRead);
                    /* Update the pointer in flash. */
                    rc = em100TcpSpiFlashWrite(pThis, SPI_MSG_CHAN_HDR_OFF + offsetof(SPIMSGCHANHDR, Psp2ExtRingBuf.offTail),
                                               &pThis->MsgChanHdr.Psp2ExtRingBuf.offTail,
                                               sizeof(pThis->MsgChanHdr.Psp2ExtRingBuf.offTail));
                }
            }
        }
    } while (   !rc
             && cbReadLeft);

    return rc;
}


/**
 * Checks how many bytes are available for reading in the message buffer.
 *
 * @returns Status code.
 * @param   pThis                   The EM100 provider instance.
 * @param   pcbAvail                Where to store the number of bytes available for reading on success.
 */
static int em100TcpSpiMsgBufferPeek(PPSPPROXYPROVCTXINT pThis, size_t *pcbAvail)
{
    *pcbAvail = 0;

    int rc = em100TcpSpiMsgBufferHdrFetch(pThis);
    if (!rc)
        *pcbAvail = em100TcpSpiMsgBufferGetUsed(&pThis->MsgChanHdr.Psp2ExtRingBuf);

    return rc;
}


/**
 * @copydoc{PSPSTUBPDUIOIF,pfnPeek}
 */
static size_t em100TcpProvPduIoIfPeek(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser)
{
    (void)hPspStubPduCtx;

    PPSPPROXYPROVCTXINT pThis = (PPSPPROXYPROVCTXINT)pvUser;
    size_t cbAvail = 0;
    int rc = em100TcpSpiMsgBufferPeek(pThis, &cbAvail);
    if (!rc)
        return cbAvail;

    return 0;
}


/**
 * @copydoc{PSPSTUBPDUIOIF,pfnRead}
 */
static int em100TcpProvPduIoIfRead(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser, void *pvDst, size_t cbRead, size_t *pcbRead)
{
    (void)hPspStubPduCtx;

    PPSPPROXYPROVCTXINT pThis = (PPSPPROXYPROVCTXINT)pvUser;
    int rc = em100TcpSpiMsgBufferRead(pThis, pvDst, cbRead);
    if (!rc)
        *pcbRead = cbRead;

    return rc;
}


/**
 * @copydoc{PSPSTUBPDUIOIF,pfnWrite}
 */
static int em100TcpProvPduIoIfWrite(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser, const void *pvPkt, size_t cbPkt)
{
    (void)hPspStubPduCtx;

    PPSPPROXYPROVCTXINT pThis = (PPSPPROXYPROVCTXINT)pvUser;
    return em100TcpSpiMsgBufferWrite(pThis, pvPkt, cbPkt);
}


/**
 * @copydoc{PSPSTUBPDUIOIF,pfnPoll}
 */
static int em100TcpProvPduIoIfPoll(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser, uint32_t cMillies)
{
    (void)hPspStubPduCtx;
    PPSPPROXYPROVCTXINT pThis = (PPSPPROXYPROVCTXINT)pvUser;

    /* Do a peek until there is something available, inefficient but what choice do we have... */
    int rc = 0;
    size_t cbAvail = 0;
    do
    {
        /** @todo Maybe sleep for a short period if this turns out to be super inefficient. */
        rc = em100TcpSpiMsgBufferPeek(pThis, &cbAvail);
    } while (   !rc
             && !cbAvail);

    return rc;
}


/**
 * @copydoc{PSPSTUBPDUIOIF,pfnInterrupt}
 */
static int em100TcpProvPduIoIfInterrupt(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser)
{
    return -1; /** @todo */
}


/**
 * I/O interface callback table.
 */
static const PSPSTUBPDUIOIF g_PduIoIf =
{
    /** pfnPeek */
    em100TcpProvPduIoIfPeek,
    /** pfnRead */
    em100TcpProvPduIoIfRead,
    /** pfnWrite */
    em100TcpProvPduIoIfWrite,
    /** pfnPoll */
    em100TcpProvPduIoIfPoll,
    /** pfnInterrupt */
    em100TcpProvPduIoIfInterrupt
};


/**
 * @copydoc{PSPPROXYPROV,pfnCtxInit}
 */
static int em100TcpProvCtxInit(PSPPROXYPROVCTX hProvCtx, const char *pszDevice)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    int rc = 0;
    char szDev[256]; /* Should be plenty. */

    memset(&szDev[0], 0, sizeof(szDev));
    strncpy(&szDev[0], pszDevice, sizeof(szDev));
    if (szDev[sizeof(szDev) - 1] == '\0')
    {
        char *pszSep = strchr(&szDev[0], ':');
        if (pszSep)
        {
            *pszSep = '\0';
            pszSep++;
            struct hostent *pSrv = gethostbyname(&szDev[0]);
            if (pSrv)
            {
                struct sockaddr_in SrvAddr;
                memset(&SrvAddr, 0, sizeof(SrvAddr));
                SrvAddr.sin_family = AF_INET;
                memcpy(&SrvAddr.sin_addr.s_addr, pSrv->h_addr, pSrv->h_length);
                SrvAddr.sin_port = htons(atoi(pszSep));

                pThis->iFdCon = socket(AF_INET, SOCK_STREAM, 0);
                if (pThis->iFdCon > -1)
                {
                    int rcPsx = connect(pThis->iFdCon,(struct sockaddr *)&SrvAddr,sizeof(SrvAddr));
                    if (!rcPsx)
                    {
                        rc = em100TcpSpiMsgBufferInit(pThis);
                        if (!rc)
                        {
                            /* Create the PDU context. */
                            rc = pspStubPduCtxCreate(&pThis->hPduCtx, &g_PduIoIf, pThis);
                            if (!rc)
                            {
                                rc = pspStubPduCtxConnect(pThis->hPduCtx, 10 * 1000);
                                if (!rc)
                                    return 0;

                                pspStubPduCtxDestroy(pThis->hPduCtx);
                            }
                        }
                    }
                    else
                        rc = -1;

                    close(pThis->iFdCon);
                }
                else
                    rc = -1;
            }
            else
                rc = -1;
        }
        else
            rc = -1;
    }
    else
        rc = -1;

    return rc;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxDestroy}
 */
static void em100TcpProvCtxDestroy(PSPPROXYPROVCTX hProvCtx)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;

    pspStubPduCtxDestroy(pThis->hPduCtx);
    shutdown(pThis->iFdCon, SHUT_RDWR);
    close(pThis->iFdCon);
    pThis->iFdCon = 0;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxQueryInfo}
 */
static int em100TcpProvCtxQueryInfo(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR *pPspAddrScratchStart, size_t *pcbScratch)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxQueryInfo(pThis->hPduCtx, idCcd, pPspAddrScratchStart, pcbScratch);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspSmnRead}
 */
static int em100TcpProvCtxPspSmnRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspSmnRead(pThis->hPduCtx, idCcd, idCcdTgt, uSmnAddr, cbVal, pvVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspSmnWrite}
 */
static int em100TcpProvCtxPspSmnWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspSmnWrite(pThis->hPduCtx, idCcd, idCcdTgt, uSmnAddr, cbVal, pvVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMemRead}
 */
static int em100TcpProvCtxPspMemRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, void *pvBuf, uint32_t cbRead)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspMemRead(pThis->hPduCtx, idCcd, uPspAddr, pvBuf, cbRead);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMemWrite}
 */
static int em100TcpProvCtxPspMemWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspMemWrite(pThis->hPduCtx, idCcd, uPspAddr, pvBuf, cbWrite);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMmioRead}
 */
static int em100TcpProvCtxPspMmioRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspMmioRead(pThis->hPduCtx, idCcd, uPspAddr, pvVal, cbVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMmioWrite}
 */
static int em100TcpProvCtxPspMmioWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspMmioWrite(pThis->hPduCtx, idCcd, uPspAddr, pvVal, cbVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MemRead}
 */
static int em100TcpProvCtxPspX86MemRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, void *pvBuf, uint32_t cbRead)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspX86MemRead(pThis->hPduCtx, idCcd, PhysX86Addr, pvBuf, cbRead);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MemWrite}
 */
static int em100TcpProvCtxPspX86MemWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspX86MemWrite(pThis->hPduCtx, idCcd, PhysX86Addr, pvBuf, cbWrite);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MmioRead}
 */
static int em100TcpProvCtxPspX86MmioRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspX86MmioRead(pThis->hPduCtx, idCcd, PhysX86Addr, pvVal, cbVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MmioWrite}
 */
static int em100TcpProvCtxPspX86MmioWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspX86MmioWrite(pThis->hPduCtx, idCcd, PhysX86Addr, pvVal, cbVal);
}


/**
 * Provider registration structure.
 */
const PSPPROXYPROV g_PspProxyProvEm100Tcp =
{
    /** pszId */
    "em100tcp",
    /** pszDesc */
    "PSP access through a SPI connection using a modified em100 tool device, schema looks like tcp://<hostname>:<port>",
    /** cbCtx */
    sizeof(PSPPROXYPROVCTXINT),
    /** fFeatures */
    0,
    /** pfnCtxInit */
    em100TcpProvCtxInit,
    /** pfnCtxDestroy */
    em100TcpProvCtxDestroy,
    /** pfnCtxQueryInfo */
    em100TcpProvCtxQueryInfo,
    /** pfnCtxPspSmnRead */
    em100TcpProvCtxPspSmnRead,
    /** pfnCtxPspSmnWrite */
    em100TcpProvCtxPspSmnWrite,
    /** pfnCtxPspMemRead */
    em100TcpProvCtxPspMemRead,
    /** pfnCtxPspMemWrite */
    em100TcpProvCtxPspMemWrite,
    /** pfnCtxPspMmioRead */
    em100TcpProvCtxPspMmioRead,
    /** pfnCtxPspMmioWrite */
    em100TcpProvCtxPspMmioWrite,
    /** pfnCtxPspX86MemRead */
    em100TcpProvCtxPspX86MemRead,
    /** pfnCtxPspX86MemWrite */
    em100TcpProvCtxPspX86MemWrite,
    /** pfnCtxPspX86MmioRead */
    em100TcpProvCtxPspX86MmioRead,
    /** pfnCtxPspX86MmioWrite */
    em100TcpProvCtxPspX86MmioWrite,
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

