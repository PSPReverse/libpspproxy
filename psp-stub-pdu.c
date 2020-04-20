/** @file
 * PSP proxy library to interface with the hardware of the PSP - PDU protocol handling.
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <psp-stub/psp-serial-stub.h>

#include "psp-stub-pdu.h"


/** Returns the minimum of two numbers. */
#define MIN(a, b) ((a) < (b) ? (a) : (b))


/**
 * PDU receive states.
 */
typedef enum PSPSERIALPDURECVSTATE
{
    /** Invalid receive state. */
    PSPSERIALPDURECVSTATE_INVALID = 0,
    /** Currently receiveing the header. */
    PSPSERIALPDURECVSTATE_HDR,
    /** Currently receiveing the payload. */
    PSPSERIALPDURECVSTATE_PAYLOAD,
    /** Currently receiving the footer. */
    PSPSERIALPDURECVSTATE_FOOTER,
    /** 32bit hack. */
    PSPSERIALPDURECVSTATE_32BIT_HACK = 0x7fffffff
} PSPSERIALPDURECVSTATE;


/**
 * Internal PSP PDU context.
 */
typedef struct PSPSTUBPDUCTXINT
{
    /** Proxy provider interface table. */
    PCPSPPROXYPROV              pProvIf;
    /** Proxy provider handle. */
    PSPPROXYPROVCTX             hProvCtx;
    /** Proxy I/O interface callbacks. */
    PCPSPPROXYIOIF              pProxyIoIf;
    /** Proxy context handle. */
    PSPPROXYCTX                 hProxyCtx;
    /** Opaque user data to pass to the pProxyIoIf callbacks. */
    void                        *pvProxyIoUser;
    /** Number of PDUs sent so far. */
    uint32_t                    cPdusSent;
    /** Next PDU counter value expected for a received PDU. */
    uint32_t                    cPduRecvNext;
    /** Beacons seen. */
    uint32_t                    cBeaconsSeen;
    /** The PDU receive state. */
    PSPSERIALPDURECVSTATE       enmPduRecvState;
    /** Number of bytes to receive remaining in the current state. */
    size_t                      cbPduRecvLeft;
    /** Current offset into the PDU buffer. */
    uint32_t                    offPduRecv;
    /** The PDU receive buffer. */
    uint8_t                     abPdu[4096];
    /** Flag whether a connection was established. */
    bool                        fConnect;
    /** Maximum PDU length supported. */
    uint32_t                    cbPduMax;
    /** Size of the scratch space area in bytes. */
    uint32_t                    cbScratch;
    /** Start address of the scratch space area. */
    PSPADDR                     PspAddrScratch;
    /** Number of sockets in the system. */
    uint32_t                    cSysSockets;
    /** Number of CCDs in the systems */
    uint32_t                    cCcdsPerSocket;
    /** Total number of CCDs in the remote system. */
    uint32_t                    cCcds;
    /** Log message buffer. */
    char                        achLogMsg[1024];
    /** Number of bytes valid in the log message buffer. */
    size_t                      cchLogMsgAvail;
} PSPSTUBPDUCTXINT;
/** Pointer to an internal PSP proxy context. */
typedef PSPSTUBPDUCTXINT *PPSPSTUBPDUCTXINT;



/**
 * Resets the PDU receive state machine.
 *
 * @returns nothing.
 * @param   pThis                   The serial stub instance data.
 */
static void pspStubPduCtxRecvReset(PPSPSTUBPDUCTXINT pThis)
{
    pThis->enmPduRecvState = PSPSERIALPDURECVSTATE_HDR;
    pThis->cbPduRecvLeft   = sizeof(PSPSERIALPDUHDR);
    pThis->offPduRecv      = 0;
}


/**
 * Validates the given PDU header.
 *
 * @returns Status code.
 * @param   pThis                   The serial stub instance data.
 * @param   pHdr                    PDU header to validate.
 */
static int pspStubPduCtxHdrValidate(PPSPSTUBPDUCTXINT pThis, PCPSPSERIALPDUHDR pHdr)
{
    if (pHdr->u32Magic != PSP_SERIAL_PSP_2_EXT_PDU_START_MAGIC)
        return -1;
    if (pHdr->u.Fields.cbPdu > sizeof(pThis->abPdu) - sizeof(PSPSERIALPDUHDR) - sizeof(PSPSERIALPDUFOOTER))
        return -1;
    if (!(   (   pHdr->u.Fields.enmRrnId >= PSPSERIALPDURRNID_NOTIFICATION_FIRST
              && pHdr->u.Fields.enmRrnId < PSPSERIALPDURRNID_NOTIFICATION_INVALID_FIRST)
          || (   pHdr->u.Fields.enmRrnId >= PSPSERIALPDURRNID_RESPONSE_FIRST
              && pHdr->u.Fields.enmRrnId < PSPSERIALPDURRNID_RESPONSE_INVALID_FIRST)))
        return -1;
    if (   pHdr->u.Fields.cPdus != pThis->cPduRecvNext + 1
        && pThis->fConnect)
        return -1;
    if (pHdr->u.Fields.idCcd >= pThis->cCcds)
        return -1;

    return 0;
}


/**
 * Validates the complete PDU, the header was validated mostly at an earlier stage already.
 *
 * @returns Status code.
 * @param   pThis                   The serial stub instance data.
 * @param   pHdr                    The header of the PDU to validate.
 */
static int pspStubPduCtxValidate(PPSPSTUBPDUCTXINT pThis, PCPSPSERIALPDUHDR pHdr)
{
    uint32_t uChkSum = 0;

    for (uint32_t i = 0; i < sizeof(pHdr->u.ab); i++)
        uChkSum += pHdr->u.ab[i];

    uint8_t *pbPayload = (uint8_t *)(pHdr + 1);
    for (uint32_t i = 0; i < pHdr->u.Fields.cbPdu; i++)
        uChkSum += *pbPayload++;

    /* Check whether the footer magic and checksum are valid. */
    PCPSPSERIALPDUFOOTER pFooter = (PCPSPSERIALPDUFOOTER)pbPayload;
    if (   uChkSum + pFooter->u32ChkSum != 0
        || pFooter->u32Magic != PSP_SERIAL_PSP_2_EXT_PDU_END_MAGIC)
        return -1;

    return 0;
}


/**
 * Processes the current state and advances to the next one.
 *
 * @returns Status code.
 * @param   pThis                   The serial stub instance data.
 * @param   ppPduRcvd               Where to store the pointer to the received complete PDU on success.
 */
static int pspStubPduCtxRecvAdvance(PPSPSTUBPDUCTXINT pThis, PCPSPSERIALPDUHDR *ppPduRcvd)
{
    int rc = 0;

    *ppPduRcvd = NULL;

    switch (pThis->enmPduRecvState)
    {
        case PSPSERIALPDURECVSTATE_HDR:
        {
            /* Validate header. */
            PCPSPSERIALPDUHDR pHdr = (PCPSPSERIALPDUHDR)&pThis->abPdu[0];

            int rc2 = pspStubPduCtxHdrValidate(pThis, pHdr);
            if (!rc2)
            {
                /* No payload means going directly to the footer. */
                if (pHdr->u.Fields.cbPdu)
                {
                    pThis->enmPduRecvState = PSPSERIALPDURECVSTATE_PAYLOAD;
                    pThis->cbPduRecvLeft   = pHdr->u.Fields.cbPdu;
                }
                else
                {
                    pThis->enmPduRecvState = PSPSERIALPDURECVSTATE_FOOTER;
                    pThis->cbPduRecvLeft   = sizeof(PSPSERIALPDUFOOTER);
                }
            }
            else
            {
                /** @todo Send out of band error. */
                pspStubPduCtxRecvReset(pThis);
            }
            break;
        }
        case PSPSERIALPDURECVSTATE_PAYLOAD:
        {
            /* Just advance to the next state. */
            pThis->enmPduRecvState = PSPSERIALPDURECVSTATE_FOOTER;
            pThis->cbPduRecvLeft   = sizeof(PSPSERIALPDUFOOTER);
            break;
        }
        case PSPSERIALPDURECVSTATE_FOOTER:
        {
            /* Validate the footer and complete PDU. */
            PCPSPSERIALPDUHDR pHdr = (PCPSPSERIALPDUHDR)&pThis->abPdu[0];

            rc = pspStubPduCtxValidate(pThis, pHdr);
            if (!rc)
            {
                pThis->cPduRecvNext++;
                *ppPduRcvd = pHdr;
            }
            /** @todo Send out of band error. */
            /* Start receiving a new PDU in any case. */
            pspStubPduCtxRecvReset(pThis);
            break;
        }
        default:
            rc = -1;
    }

    return rc;
}


/**
 * Waits for a PDU to be received or until the given timeout elapsed.
 *
 * @returns Status code.
 * @param   pThis                   The serial stub instance data.
 * @param   ppPduRcvd               Where to store the pointer to the received complete PDU on success.
 * @param   cMillies                Amount of milliseconds to wait until a timeout is returned.
 */
static int pspStubPduCtxRecv(PPSPSTUBPDUCTXINT pThis, PCPSPSERIALPDUHDR *ppPduRcvd, uint32_t cMillies)
{
    int rc = 0;

    /** @todo Timeout handling. */
    do
    {
        rc = pThis->pProvIf->pfnCtxPoll(pThis->hProvCtx, cMillies);
        if (rc == -2)
            break;
        if (!rc)
        {
            size_t cbAvail = pThis->pProvIf->pfnCtxPeek(pThis->hProvCtx);
            if (cbAvail)
            {
                /* Only read what is required for the current state. */
                /** @todo If the connection turns out to be unreliable we have to do a marker search first. */
                size_t cbThisRecv = MIN(cbAvail, pThis->cbPduRecvLeft);

                rc = pThis->pProvIf->pfnCtxRead(pThis->hProvCtx, &pThis->abPdu[pThis->offPduRecv], cbThisRecv, &cbThisRecv);
                if (!rc)
                {
                    pThis->offPduRecv    += cbThisRecv;
                    pThis->cbPduRecvLeft -= cbThisRecv;

                    /* Advance state machine and process the data if this state is complete. */
                    if (!pThis->cbPduRecvLeft)
                    {
                        rc = pspStubPduCtxRecvAdvance(pThis, ppPduRcvd);
                        if (   !rc
                            && *ppPduRcvd != NULL)
                            break; /* We received a complete and valid PDU. */
                    }
                }
            }
        }
    } while (!rc);

    return rc;
}


/**
 * Waits for a PDU with the specific ID to be received.
 *
 * @returns nothing.
 * @param   pThis                   The serial stub instance data.
 * @param   pPdu                    The log message PDU.
 */
static void pspStubPduCtxLogMsgHandle(PPSPSTUBPDUCTXINT pThis, PCPSPSERIALPDUHDR pPdu)
{
    size_t cchMsg = pPdu->u.Fields.cbPdu;
    const char *pbMsg = (const char *)(pPdu + 1);

    /* Drop any log message PDU which is too big to fit into the message buffer. */
    if (sizeof(pThis->achLogMsg) - pThis->cchLogMsgAvail >= cchMsg)
    {
        memcpy(&pThis->achLogMsg[pThis->cchLogMsgAvail], pbMsg, cchMsg);
        pThis->cchLogMsgAvail += cchMsg;

        for (;;)
        {
            /* Parse the buffer for newlines and hand them over to the callback. */
            char *pszNewLine = strchr(&pThis->achLogMsg[0], '\n');
            if (pszNewLine)
            {
                /* Terminate the string after the newline. */
                pszNewLine++;
                char chOld = *pszNewLine;
                *pszNewLine = '\0';

                pThis->pProxyIoIf->pfnLogMsg(pThis->hProxyCtx, pThis->pvProxyIoUser, &pThis->achLogMsg[0]);
                /* Restore old value and move everything remaining to the front, clearing out the remainder. */
                *pszNewLine = chOld;
                size_t cchMsg = pszNewLine - &pThis->achLogMsg[0];
                /** @todo assert(cchMsg <= pThis->cchLogMsgAvail) */
                memmove(&pThis->achLogMsg[0], pszNewLine, pThis->cchLogMsgAvail - cchMsg);
                pThis->cchLogMsgAvail -= cchMsg;
                memset(&pThis->achLogMsg[pThis->cchLogMsgAvail], 0, sizeof(pThis->achLogMsg) - pThis->cchLogMsgAvail);
                if (!pThis->cchLogMsgAvail)
                    break;
            }
            else
                break;
        }
    }
}


/**
 * handles an output buffer write.
 *
 * @returns nothing.
 * @param   pThis                   The serial stub instance data.
 * @param   pPdu                    The log message PDU.
 */
static void pspStubPduCtxOutBufWriteHandle(PPSPSTUBPDUCTXINT pThis, PCPSPSERIALPDUHDR pPdu)
{
    PCPSPSERIALOUTBUFNOT pNot = (PCPSPSERIALOUTBUFNOT)(pPdu + 1);
    const uint8_t *pbData = (const uint8_t *)(pNot + 1);
    size_t cbData = pPdu->u.Fields.cbPdu - sizeof(*pNot);

    pThis->pProxyIoIf->pfnOutBufWrite(pThis->hProxyCtx, pThis->pvProxyIoUser, pNot->idOutBuf,
                                      pbData, cbData);
}


/**
 * Waits for a PDU with the specific ID to be received.
 *
 * @returns Status code.
 * @param   pThis                   The serial stub instance data.
 * @param   enmRrnId                The PDU ID to wait for.
 * @param   ppPduRcvd               Where to store the pointer to the received complete PDU on success.
 * @param   ppvPayload              Where to store the pointer to the payload data on success.
 * @param   pcbPayload              Where to store the size of the payload in bytes on success.
 * @param   cMillies                Amount of milliseconds to wait until a timeout is returned.
 */
static int pspStubPduCtxRecvId(PPSPSTUBPDUCTXINT pThis, PSPSERIALPDURRNID enmRrnId, PCPSPSERIALPDUHDR *ppPduRcvd,
                               void **ppvPayload, size_t *pcbPayload, uint32_t cMillies)
{
    int rc = 0;

    while (!rc)
    {
        PCPSPSERIALPDUHDR pPdu = NULL;
        rc = pspStubPduCtxRecv(pThis, &pPdu, cMillies);
        if (rc == -2)
            break;
        if (!rc)
        {
            if (pPdu->u.Fields.enmRrnId != enmRrnId)
            {
                if (pPdu->u.Fields.enmRrnId == PSPSERIALPDURRNID_NOTIFICATION_LOG_MSG)
                {
                    if (   pThis->pProxyIoIf
                        && pThis->pProxyIoIf->pfnLogMsg)
                        pspStubPduCtxLogMsgHandle(pThis, pPdu);
                    continue;
                }

                if (pPdu->u.Fields.enmRrnId == PSPSERIALPDURRNID_NOTIFICATION_OUT_BUF)
                {
                    if (   pThis->pProxyIoIf
                        && pThis->pProxyIoIf->pfnOutBufWrite)
                        pspStubPduCtxOutBufWriteHandle(pThis, pPdu);
                    continue;
                }

                /*
                 * Beacons are only ignored if not in connected mode or when
                 * the counter matches what we've seen so far.
                 *
                 * A reset counter means that the target reset.
                 */
                if (pPdu->u.Fields.enmRrnId == PSPSERIALPDURRNID_NOTIFICATION_BEACON)
                {
                    PCPSPSERIALBEACONNOT pBeacon = (PCPSPSERIALBEACONNOT)(pPdu + 1);
                    if (   !pThis->fConnect
                        || pBeacon->cBeaconsSent == pThis->cBeaconsSeen + 1)
                    {
                        pThis->cBeaconsSeen++;
                        continue;
                    }
                }

                rc = -1; /* Unexpected PDU received or system resetted. */
                break;
            }
            else
            {
                /* Return the PDU. */
                *ppPduRcvd = pPdu;
                if (ppvPayload)
                    *ppvPayload = (void *)(pPdu + 1);
                if (pcbPayload)
                    *pcbPayload = pPdu->u.Fields.cbPdu;
                break;
            }
        }
    }

    return rc;
}


/**
 * Sends the given PDU.
 *
 * @returns Status code.
 * @param   pThis                   The serial stub instance data.
 * @param   idCcd                   The CCD ID the PDU is designated for.
 * @param   enmPduRrnId             The Request/Response/Notification ID.
 * @param   pvPayload               Pointer to the PDU payload to send, optional.
 * @param   cbPayload               Size of the PDU payload in bytes.
 */
static int pspStubPduCtxSend(PPSPSTUBPDUCTXINT pThis, uint32_t idCcd, PSPSERIALPDURRNID enmPduRrnId, const void *pvPayload, size_t cbPayload)
{
    PSPSERIALPDUHDR PduHdr;
    PSPSERIALPDUFOOTER PduFooter;

    /* Initialize header and footer. */
    PduHdr.u32Magic           = PSP_SERIAL_EXT_2_PSP_PDU_START_MAGIC;
    PduHdr.u.Fields.cbPdu     = cbPayload;
    PduHdr.u.Fields.cPdus     = ++pThis->cPdusSent;
    PduHdr.u.Fields.enmRrnId  = enmPduRrnId;
    PduHdr.u.Fields.idCcd     = idCcd;
    PduHdr.u.Fields.tsMillies = 0;

    uint32_t uChkSum = 0;
    for (uint32_t i = 0; i < sizeof(PduHdr.u.ab); i++)
        uChkSum += PduHdr.u.ab[i];

    const uint8_t *pbPayload = (const uint8_t *)pvPayload;
    for (size_t i = 0; i < cbPayload; i++)
        uChkSum += pbPayload[i];

    PduFooter.u32ChkSum = (0xffffffff - uChkSum) + 1;
    PduFooter.u32Magic  = PSP_SERIAL_EXT_2_PSP_PDU_END_MAGIC;

    /* Send everything, header first, then payload and footer last. */
    int rc = pThis->pProvIf->pfnCtxWrite(pThis->hProvCtx, &PduHdr, sizeof(PduHdr));
    if (!rc && pvPayload && cbPayload)
        rc = pThis->pProvIf->pfnCtxWrite(pThis->hProvCtx, pvPayload, cbPayload);
    if (!rc)
        rc = pThis->pProvIf->pfnCtxWrite(pThis->hProvCtx, &PduFooter, sizeof(PduFooter));

    return rc;
}


/**
 * Sends the given request with payload and waits for the appropriate response returning the
 * payload data in the given buffer.
 *
 * @returns Status code.
 * @param   pThis                   The serial stub instance data.
 * @param   idCcd                   The CCD ID the PDU is designated for.
 * @param   enmReq                  The request to issue.
 * @param   enmResp                 The expected response.
 * @param   pvReqPayload            The request payload data.
 * @param   cbReqPayload            Size of the request payload data in bytes.
 * @param   pvResp                  Where to store the response data on success.
 * @param   cbResp                  Size of the response buffer.
 * @param   cMillies                Timeout in milliseconds.
 */
static int pspStubPduCtxReqResp(PPSPSTUBPDUCTXINT pThis, uint32_t idCcd, PSPSERIALPDURRNID enmReq,
                                PSPSERIALPDURRNID enmResp,
                                const void *pvReqPayload, size_t cbReqPayload, void *pvResp, size_t cbResp,
                                uint32_t cMillies)
{
    int rc = pspStubPduCtxSend(pThis, idCcd, enmReq, pvReqPayload, cbReqPayload);
    if (!rc)
    {
        PCPSPSERIALPDUHDR pPdu = NULL;
        void *pvPduResp = NULL;
        size_t cbPduResp = 0;
        rc = pspStubPduCtxRecvId(pThis, enmResp, &pPdu, &pvPduResp, &cbPduResp, cMillies);
        if (!rc)
        {
            if (   pPdu->u.Fields.rcReq == PSP_SERIAL_STS_INF_SUCCESS
                && cbPduResp == cbResp)
            {
                if (cbPduResp)
                    memcpy(pvResp, pvPduResp, cbPduResp);
            }
            else
                rc = -1;
        }
    }

    return rc;
}


/**
 * Wrapper to send a write request consisting of two consecutive payload parts but
 * doesn't has any payload data in the response.
 *
 * @returns Status code.
 * @param   pThis                   The serial stub instance data.
 * @param   idCcd                   The CCD ID the PDU is designated for.
 * @param   enmReq                  The request to issue.
 * @param   enmResp                 The expected response.
 * @param   pvReqPayload1           Stage 1 request payload data.
 * @param   cbReqPayload1           Size of the stage 1 request payload data in bytes.
 * @param   pvReqPayload2           Stage 2 request payload data.
 * @param   cbReqPayload2           Size of the stage 2 request payload data in bytes.
 * @param   cMillies                Timeout in milliseconds.
 */
static int pspStubPduCtxReqRespWr(PPSPSTUBPDUCTXINT pThis, uint32_t idCcd, PSPSERIALPDURRNID enmReq,
                                  PSPSERIALPDURRNID enmResp,
                                  const void *pvReqPayload1, size_t cbReqPayload1,
                                  const void *pvReqPayload2, size_t cbReqPayload2,
                                  uint32_t cMillies)
{
    int rc = 0;

    /* Quick and dirty way, allocate buffer and merge the payload buffers. */
    void *pvTmp = malloc(cbReqPayload1 + cbReqPayload2);
    if (pvTmp)
    {
        memcpy(pvTmp, pvReqPayload1, cbReqPayload1);
        memcpy((uint8_t *)pvTmp + cbReqPayload1, pvReqPayload2, cbReqPayload2);
        rc = pspStubPduCtxReqResp(pThis, idCcd, enmReq, enmResp, pvTmp, cbReqPayload1 + cbReqPayload2,
                                  NULL /*pvRespPayload*/, 0 /*cbRespPayload*/, cMillies);
        free(pvTmp);
    }
    else
        rc = -1;

    return rc;
}


int pspStubPduCtxCreate(PPSPSTUBPDUCTX phPduCtx, PCPSPPROXYPROV pProvIf, PSPPROXYPROVCTX hProvCtx,
                        PCPSPPROXYIOIF pProxyIoIf, PSPPROXYCTX hProxyCtx, void *pvUser)
{
    int rc = 0;
    PPSPSTUBPDUCTXINT pThis = (PPSPSTUBPDUCTXINT)calloc(1, sizeof(*pThis));
    if (pThis)
    {
        pThis->pProvIf       = pProvIf;
        pThis->hProvCtx      = hProvCtx;
        pThis->pProxyIoIf    = pProxyIoIf;
        pThis->hProxyCtx     = hProxyCtx;
        pThis->pvProxyIoUser = pvUser;
        pThis->cBeaconsSeen  = 0;
        pThis->cCcds         = 1; /* To make validation succeed during the initial connect phase. */
        pThis->fConnect      = false;
        pspStubPduCtxRecvReset(pThis);
        *phPduCtx = pThis;
    }
    else
        rc = -1;

    return rc;
}


void pspStubPduCtxDestroy(PSPSTUBPDUCTX hPduCtx)
{
    PPSPSTUBPDUCTXINT pThis = hPduCtx;

    free(pThis);
}


int pspStubPduCtxConnect(PSPSTUBPDUCTX hPduCtx, uint32_t cMillies)
{
    PPSPSTUBPDUCTXINT pThis = hPduCtx;

    pThis->cchLogMsgAvail = 0;
    memset(&pThis->achLogMsg[0], 0, sizeof(pThis->achLogMsg));

    /* Wait for a beacon PDU. */
    /** @todo Timeout handling. */
    PCPSPSERIALPDUHDR pPdu = NULL;
    PCPSPSERIALBEACONNOT pBeacon = NULL;
    size_t cbBeacon = 0;
    int rc = pspStubPduCtxRecvId(pThis, PSPSERIALPDURRNID_NOTIFICATION_BEACON, &pPdu,
                                 (void **)&pBeacon, &cbBeacon, cMillies);
    if (!rc)
    {
        if (cbBeacon == sizeof(PSPSERIALBEACONNOT))
        {
            /* Remember the beacon count for later when we successfully connected. */
            uint32_t cBeaconsSeen = pBeacon->cBeaconsSent;

            /* Send connect request. */
            rc = pspStubPduCtxSend(pThis, 0 /*idCcd*/, PSPSERIALPDURRNID_REQUEST_CONNECT, NULL /*pvPayload*/, 0 /*cbPayload*/);
            if (!rc)
            {
                PCPSPSERIALCONNECTRESP pConResp = NULL;
                size_t cbConResp = 0;
                rc = pspStubPduCtxRecvId(pThis, PSPSERIALPDURRNID_RESPONSE_CONNECT, &pPdu,
                                         (void **)&pConResp, &cbConResp, cMillies);
                if (!rc)
                {
                    pThis->cbPduMax       = pConResp->cbPduMax;
                    pThis->cbScratch      = pConResp->cbScratch;
                    pThis->PspAddrScratch = pConResp->PspAddrScratch;
                    pThis->cSysSockets    = pConResp->cSysSockets;
                    pThis->cCcdsPerSocket = pConResp->cCcdsPerSocket;
                    pThis->cCcds          = pThis->cSysSockets * pThis->cCcdsPerSocket;
                    pThis->fConnect       = true;
                    pThis->cBeaconsSeen   = cBeaconsSeen;
                    pThis->cPduRecvNext   = 1;
                }
            }
        }
        else
            rc = -1;
    }

    return rc;
}


int pspStubPduCtxQueryInfo(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, PSPADDR *pPspAddrScratchStart, size_t *pcbScratch)
{
    PPSPSTUBPDUCTXINT pThis = hPduCtx;

    *pPspAddrScratchStart = pThis->PspAddrScratch;
    *pcbScratch           = pThis->cbScratch;
    return 0;
}


int pspStubPduCtxPspSmnRead(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, void *pvVal)
{
    PPSPSTUBPDUCTXINT pThis = hPduCtx;

    PSPSERIALSMNMEMXFERREQ Req;
    Req.SmnAddrStart = uSmnAddr;
    Req.cbXfer       = cbVal;
    return pspStubPduCtxReqResp(pThis, idCcd, PSPSERIALPDURRNID_REQUEST_PSP_SMN_READ,
                                PSPSERIALPDURRNID_RESPONSE_PSP_SMN_READ,
                                &Req, sizeof(Req), pvVal, cbVal, 10000);
}


int pspStubPduCtxPspSmnWrite(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, const void *pvVal)
{
    PPSPSTUBPDUCTXINT pThis = hPduCtx;

    PSPSERIALSMNMEMXFERREQ Req;
    Req.SmnAddrStart = uSmnAddr;
    Req.cbXfer       = cbVal;
    return pspStubPduCtxReqRespWr(pThis, idCcd, PSPSERIALPDURRNID_REQUEST_PSP_SMN_WRITE,
                                  PSPSERIALPDURRNID_RESPONSE_PSP_SMN_WRITE,
                                  &Req, sizeof(Req), pvVal, cbVal, 10000);
}


int pspStubPduCtxPspMemRead(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, PSPADDR uPspAddr, void *pvBuf, uint32_t cbRead)
{
    PPSPSTUBPDUCTXINT pThis = hPduCtx;

    PSPSERIALPSPMEMXFERREQ Req;
    Req.PspAddrStart = uPspAddr;
    Req.cbXfer       = cbRead;
    return pspStubPduCtxReqResp(pThis, idCcd, PSPSERIALPDURRNID_REQUEST_PSP_MEM_READ,
                                PSPSERIALPDURRNID_RESPONSE_PSP_MEM_READ,
                                &Req, sizeof(Req), pvBuf, cbRead, 10000);
}


int pspStubPduCtxPspMemWrite(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, PSPADDR uPspAddr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPSTUBPDUCTXINT pThis = hPduCtx;

    PSPSERIALPSPMEMXFERREQ Req;
    Req.PspAddrStart = uPspAddr;
    Req.cbXfer       = cbWrite;
    return pspStubPduCtxReqRespWr(pThis, idCcd, PSPSERIALPDURRNID_REQUEST_PSP_MEM_WRITE,
                                  PSPSERIALPDURRNID_RESPONSE_PSP_MEM_WRITE,
                                  &Req, sizeof(Req), pvBuf, cbWrite, 10000);
}


int pspStubPduCtxPspMmioRead(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, PSPADDR uPspAddr, void *pvVal, uint32_t cbVal)
{
    PPSPSTUBPDUCTXINT pThis = hPduCtx;

    PSPSERIALPSPMEMXFERREQ Req;
    Req.PspAddrStart = uPspAddr;
    Req.cbXfer       = cbVal;
    return pspStubPduCtxReqResp(pThis, idCcd, PSPSERIALPDURRNID_REQUEST_PSP_MMIO_READ,
                                PSPSERIALPDURRNID_RESPONSE_PSP_MMIO_READ,
                                &Req, sizeof(Req), pvVal, cbVal, 10000);
}


int pspStubPduCtxPspMmioWrite(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, PSPADDR uPspAddr, const void *pvVal, uint32_t cbVal)
{
    PPSPSTUBPDUCTXINT pThis = hPduCtx;

    PSPSERIALPSPMEMXFERREQ Req;
    Req.PspAddrStart = uPspAddr;
    Req.cbXfer       = cbVal;
    return pspStubPduCtxReqRespWr(pThis, idCcd, PSPSERIALPDURRNID_REQUEST_PSP_MMIO_WRITE,
                                  PSPSERIALPDURRNID_RESPONSE_PSP_MMIO_WRITE,
                                  &Req, sizeof(Req), pvVal, cbVal, 10000);
}


int pspStubPduCtxPspX86MemRead(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, X86PADDR PhysX86Addr, void *pvBuf, uint32_t cbRead)
{
    PPSPSTUBPDUCTXINT pThis = hPduCtx;

    PSPSERIALX86MEMXFERREQ Req;
    Req.PhysX86Start = PhysX86Addr;
    Req.cbXfer       = cbRead;
    Req.u32Pad0      = 0;
    return pspStubPduCtxReqResp(pThis, idCcd, PSPSERIALPDURRNID_REQUEST_PSP_X86_MEM_READ,
                                PSPSERIALPDURRNID_RESPONSE_PSP_X86_MEM_READ,
                                &Req, sizeof(Req), pvBuf, cbRead, 10000);
}


int pspStubPduCtxPspX86MemWrite(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, X86PADDR PhysX86Addr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPSTUBPDUCTXINT pThis = hPduCtx;

    PSPSERIALX86MEMXFERREQ Req;
    Req.PhysX86Start = PhysX86Addr;
    Req.cbXfer       = cbWrite;
    Req.u32Pad0      = 0;
    return pspStubPduCtxReqRespWr(pThis, idCcd, PSPSERIALPDURRNID_REQUEST_PSP_X86_MEM_WRITE,
                                  PSPSERIALPDURRNID_RESPONSE_PSP_X86_MEM_WRITE,
                                  &Req, sizeof(Req), pvBuf, cbWrite, 10000);
}


int pspStubPduCtxPspX86MmioRead(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, X86PADDR PhysX86Addr, void *pvVal, uint32_t cbVal)
{
    PPSPSTUBPDUCTXINT pThis = hPduCtx;

    PSPSERIALX86MEMXFERREQ Req;
    Req.PhysX86Start = PhysX86Addr;
    Req.cbXfer       = cbVal;
    Req.u32Pad0      = 0;
    return pspStubPduCtxReqResp(pThis, idCcd, PSPSERIALPDURRNID_REQUEST_PSP_X86_MMIO_READ,
                                PSPSERIALPDURRNID_RESPONSE_PSP_X86_MMIO_READ,
                                &Req, sizeof(Req), pvVal, cbVal, 10000);
}


int pspStubPduCtxPspX86MmioWrite(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, X86PADDR PhysX86Addr, const void *pvVal, uint32_t cbVal)
{
    PPSPSTUBPDUCTXINT pThis = hPduCtx;

    PSPSERIALX86MEMXFERREQ Req;
    Req.PhysX86Start = PhysX86Addr;
    Req.cbXfer       = cbVal;
    Req.u32Pad0      = 0;
    return pspStubPduCtxReqRespWr(pThis, idCcd, PSPSERIALPDURRNID_REQUEST_PSP_X86_MMIO_WRITE,
                                  PSPSERIALPDURRNID_RESPONSE_PSP_X86_MMIO_WRITE,
                                  &Req, sizeof(Req), pvVal, cbVal, 10000);
}


int pspStubPduCtxPspCodeModLoad(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, const void *pvCm, size_t cbCm)
{
    PPSPSTUBPDUCTXINT pThis = hPduCtx;

    PSPSERIALLOADCODEMODREQ Req;
    Req.enmCmType = PSPSERIALCMTYPE_FLAT_BINARY;
    Req.u32Pad0   = 0; /* idInBuf */
    int rc = pspStubPduCtxReqResp(pThis, idCcd, PSPSERIALPDURRNID_REQUEST_LOAD_CODE_MOD,
                                  PSPSERIALPDURRNID_RESPONSE_LOAD_CODE_MOD,
                                  &Req, sizeof(Req), NULL /*pvResp*/, 0 /*cbResp*/, 10000);
    if (!rc)
    {
        /* Load the code module in chunks so we don't exceed the maximum PDU size. */
        PSPSERIALINBUFWRREQ InBufWrReq;
        const uint8_t *pbCm = (const uint8_t *)pvCm;
        size_t cbPduPayloadMax =   pThis->cbPduMax
                                 - sizeof(InBufWrReq)
                                 - sizeof(PSPSERIALPDUHDR)
                                 - sizeof(PSPSERIALPDUFOOTER);

        InBufWrReq.idInBuf = 0;
        InBufWrReq.u32Pad0 = 0;

        while (   cbCm
               && !rc)
        {
            size_t cbThisSend = MIN(cbPduPayloadMax, cbCm);

            rc = pspStubPduCtxReqRespWr(pThis, idCcd, PSPSERIALPDURRNID_REQUEST_INPUT_BUF_WRITE,
                                        PSPSERIALPDURRNID_RESPONSE_INPUT_BUF_WRITE,
                                        &InBufWrReq, sizeof(InBufWrReq), pbCm, cbThisSend, 10000);

            cbCm -= cbThisSend;
            pbCm += cbThisSend;
        }
    }

    return rc;
}


int pspStubPduCtxPspCodeModExec(PSPSTUBPDUCTX hPduCtx, uint32_t idCcd, uint32_t u32Arg0, uint32_t u32Arg1,
                                uint32_t u32Arg2, uint32_t u32Arg3, uint32_t *pu32CmRet, uint32_t cMillies)
{
    PPSPSTUBPDUCTXINT pThis = hPduCtx;

    PSPSERIALEXECCODEMODREQ Req;
    Req.u32Arg0 = u32Arg0;
    Req.u32Arg1 = u32Arg1;
    Req.u32Arg2 = u32Arg2;
    Req.u32Arg3 = u32Arg3;
    int rc = pspStubPduCtxReqResp(pThis, idCcd, PSPSERIALPDURRNID_REQUEST_EXEC_CODE_MOD,
                                  PSPSERIALPDURRNID_RESPONSE_EXEC_CODE_MOD,
                                  &Req, sizeof(Req), NULL /*pvResp*/, 0 /*cbResp*/, 10000);
    if (!rc)
    {
        /*
         * Code is running now, excercise the runloop until we receive a code module execution finished notification,
         * The runloop will handle all the I/O transfers.
         */
        do
        {
            PCPSPSERIALPDUHDR pPdu = NULL;
            PCPSPSERIALEXECCMFINISHEDNOT pExecNot = NULL;
            size_t cbExecNot = 0;
            rc = pspStubPduCtxRecvId(pThis, PSPSERIALPDURRNID_NOTIFICATION_CODE_MOD_EXEC_FINISHED, &pPdu,
                                     (void **)&pExecNot, &cbExecNot, 1);
            if (!rc)
            {
                if (pExecNot)
                {
                    *pu32CmRet = pExecNot->u32CmRet;
                    break;
                }
            }
            else if (rc == -2)
            {
                /* Nothing received for now, check input. */
                rc = 0;
                if (   pThis->pProxyIoIf
                    && pThis->pProxyIoIf->pfnInBufPeek)
                {
                    size_t cbAvail = pThis->pProxyIoIf->pfnInBufPeek(pThis->hProxyCtx, pThis->pvProxyIoUser, 0);
                    if (cbAvail)
                    {
                        uint8_t abBuf[512];
                        size_t cbThisRead = MIN(cbAvail, sizeof(abBuf));
                        rc = pThis->pProxyIoIf->pfnInBufRead(pThis->hProxyCtx, pThis->pvProxyIoUser,
                                                             0 /*idInBuf*/, &abBuf[0], cbThisRead, NULL /*pcbRead*/);
                        if (!rc)
                        {
                            PSPSERIALINBUFWRREQ InBufWrReq;

                            InBufWrReq.idInBuf = 0;
                            InBufWrReq.u32Pad0 = 0;
                            rc = pspStubPduCtxReqRespWr(pThis, idCcd, PSPSERIALPDURRNID_REQUEST_INPUT_BUF_WRITE,
                                                        PSPSERIALPDURRNID_RESPONSE_INPUT_BUF_WRITE,
                                                        &InBufWrReq, sizeof(InBufWrReq), &abBuf[0], cbThisRead, 10000);
                        }
                    }
                }
            }
        } while (!rc);
    }

    return rc;
}

