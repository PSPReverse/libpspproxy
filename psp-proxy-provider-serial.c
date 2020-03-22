/** @file
 * PSP proxy library to interface with the hardware of the PSP - remote access over serial
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#include <poll.h>
#include <sys/ioctl.h>

#include <common/cdefs.h>
#include <common/types.h>

#include "psp-proxy-provider.h"
#include "psp-stub-pdu.h"


/**
 * Internal PSP proxy provider context.
 */
typedef struct PSPPROXYPROVCTXINT
{
    /** The file descriptor of the device proxying our calls. */
    int                             iFdDev;
    /** The PDU context. */
    PSPSTUBPDUCTX                   hPduCtx;
    /** Flag whether we are currently in blocking mode. */
    bool                            fBlocking;
} PSPPROXYPROVCTXINT;
/** Pointer to an internal PSP proxy context. */
typedef PSPPROXYPROVCTXINT *PPSPPROXYPROVCTXINT;


/**
 * Baud rate conversion descriptor.
 */
typedef struct PSPPROXYSERIALBAUDRATECONV
{
    /** The baudrate. */
    uint32_t                        u32BaudRate;
    /** The termios API identifier. */
    speed_t                         TermiosSpeed;
} PSPPROXYSERIALBAUDRATECONV;
/** Pointer to a baud rate conversion descriptor. */
typedef PSPPROXYSERIALBAUDRATECONV *PPSPPROXYSERIALBAUDRATECONV;
/** Pointer to a const baud rate conversion descriptor. */
typedef const PSPPROXYSERIALBAUDRATECONV *PCPSPPROXYSERIALBAUDRATECONV;


/** Baud rate conversion table. */
static const PSPPROXYSERIALBAUDRATECONV g_aBaudRateConv[] =
{
    { 9600,   B9600   },
    { 19200,  B19200  },
    { 38400,  B38400  },
    { 57600,  B57600  },
    { 115200, B115200 }
};



/**
 * Ensures that the correct blocking mode is set.
 *
 * @returns Status code.
 * @param   pThis                   The serial providcer context.
 * @param   fBlocking               Flag whether to switch to blocking or non blocking mode.
 */
static int serialProvCtxEnsureBlockingMode(PPSPPROXYPROVCTXINT pThis, bool fBlocking)
{
    if (pThis->fBlocking == fBlocking)
        return 0;

    int rc = 0;
    int fFcntl = fcntl(pThis->iFdDev, F_GETFL, 0);
    if (fFcntl >= 0)
    {
        if (fBlocking)
            fFcntl &= ~O_NONBLOCK;
        else
            fFcntl |= O_NONBLOCK;

        int rcPsx = fcntl(pThis->iFdDev, F_SETFL, fFcntl);
        if (rcPsx != -1)
            pThis->fBlocking = fBlocking;
        else
            rc = -1;
    }
    else
        rc = -1;

    return rc;
}


/**
 * @copydoc{PSPSTUBPDUIOIF,pfnPeek}
 */
static size_t serialProvPduIoIfPeek(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser)
{
    (void)hPspStubPduCtx;

    PPSPPROXYPROVCTXINT pThis = (PPSPPROXYPROVCTXINT)pvUser;
    int cbAvail = 0;
    int rc = ioctl(pThis->iFdDev, FIONREAD, &cbAvail);
    if (rc)
        return 0;

    return cbAvail;
}


/**
 * @copydoc{PSPSTUBPDUIOIF,pfnRead}
 */
static int serialProvPduIoIfRead(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser, void *pvDst, size_t cbRead, size_t *pcbRead)
{
    (void)hPspStubPduCtx;
    PPSPPROXYPROVCTXINT pThis = (PPSPPROXYPROVCTXINT)pvUser;

    if (serialProvCtxEnsureBlockingMode(pThis, false /*fBlocking*/) == -1)
        return -1;

    ssize_t cbRet = read(pThis->iFdDev, pvDst, cbRead);
    if (cbRet > 0)
    {
        *pcbRead = cbRead;
        return 0;
    }

    if (!cbRet)
        return -1;

    if (errno == EAGAIN || errno == EWOULDBLOCK)
        return 0;

    return -1;
}


/**
 * @copydoc{PSPSTUBPDUIOIF,pfnWrite}
 */
static int serialProvPduIoIfWrite(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser, const void *pvPkt, size_t cbPkt)
{
    (void)hPspStubPduCtx;
    PPSPPROXYPROVCTXINT pThis = (PPSPPROXYPROVCTXINT)pvUser;

    if (serialProvCtxEnsureBlockingMode(pThis, true /*fBlocking*/) == -1)
        return -1;

    ssize_t cbRet = write(pThis->iFdDev, pvPkt, cbPkt);
    if (cbRet == cbPkt)
        return 0;

    return -1;
}


/**
 * @copydoc{PSPSTUBPDUIOIF,pfnPoll}
 */
static int serialProvPduIoIfPoll(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser, uint32_t cMillies)
{
    (void)hPspStubPduCtx;
    PPSPPROXYPROVCTXINT pStub = (PPSPPROXYPROVCTXINT)pvUser;
    struct pollfd PollFd;

    PollFd.fd      = pStub->iFdDev;
    PollFd.events  = POLLIN | POLLHUP | POLLERR;
    PollFd.revents = 0;

    int rc = 0;
    for (;;)
    {
        int rcPsx = poll(&PollFd, 1, cMillies);
        if (rcPsx == 1)
            break; /* Stop polling if the single descriptor has events. */
        if (rcPsx == -1)
            rc = -1; /** @todo Better status codes for the individual errors. */
    }

    return rc;
}


/**
 * @copydoc{PSPSTUBPDUIOIF,pfnInterrupt}
 */
static int serialProvPduIoIfInterrupt(PSPSTUBPDUCTX hPspStubPduCtx, void *pvUser)
{
    return -1; /** @todo */
}


/**
 * I/O interface callback table.
 */
static const PSPSTUBPDUIOIF g_PduIoIf =
{
    /** pfnPeek */
    serialProvPduIoIfPeek,
    /** pfnRead */
    serialProvPduIoIfRead,
    /** pfnWrite */
    serialProvPduIoIfWrite,
    /** pfnPoll */
    serialProvPduIoIfPoll,
    /** pfnInterrupt */
    serialProvPduIoIfInterrupt
};


/**
 * Converts the given baud rate to the termios speed.
 *
 * @returns Speed identifier or B0 if no matching one was found.
 * @param   u32Baudrate             The baudrate to convert.
 */
static inline speed_t serialProvCtxBaudrateToTermiosSpeed(uint32_t u32Baudrate)
{
    for (uint32_t i = 0; i < ELEMENTS(g_aBaudRateConv); i++)
    {
        if (g_aBaudRateConv[i].u32BaudRate == u32Baudrate)
            return g_aBaudRateConv[i].TermiosSpeed;
    }

    return B0;
}


/**
 * Parses the given device config and returns the individual parameters.
 *
 * @returns Status code.
 * @param   pszDevice               The device string with the parameters.
 * @param   ppszDevice              Where to store the pointer to the device path to use on success.
 *                                  Must be freed with free().
 * @param   pu32Baudrate            Where to store the baud rate on success.
 * @param   pcDataBits              Where to store the number of data bits.
 * @param   pchParity               Where to store the parity.
 * @param   pcStopBits              Where to store the number of stop bits.
 */
static int serialProvCtxParseDevice(const char *pszDevice, char **ppszDevice, uint32_t *pu32Baudrate,
                                    uint8_t *pcDataBits, char *pchParity, uint8_t *pcStopBits)
{
    char szDevice[256]; /* Should be plenty. */

    memset(&szDevice[0], 0, sizeof(szDevice));
    strncpy(&szDevice[0], pszDevice, sizeof(szDevice));
    if (szDevice[sizeof(szDevice) - 1] != '\0') /* No termination means buffer overflow -> error. */
        return -1;

    int rc = 0;
    char *pszStart = strchr(&szDevice[0], ':');
    if (pszStart)
    {
        *pszStart = '\0';
        pszStart++;

        char *pszSep = strchr(pszStart, ':');
        if (pszSep)
        {
            *pszSep = '\0';
            pszSep++;

            /* Baud rate. */
            errno = 0;
            *pu32Baudrate = strtoul(pszStart, NULL, 10);
            if (!errno)
            {
                pszStart = pszSep;
                pszSep = strchr(pszStart, ':');
                if (pszSep)
                {
                    *pszSep = '\0';
                    pszSep++;

                    /* Data bit count. */
                    uint32_t u32Tmp = strtoul(pszStart, NULL, 10);
                    if (   !errno
                        && u32Tmp >= 5
                        && u32Tmp <= 8)
                    {
                        *pcDataBits = (uint8_t)u32Tmp;

                        pszStart = pszSep;
                        pszSep = strchr(pszStart, ':');
                        if (   pszSep
                            && pszSep - pszStart == 1
                            && (   *pszStart == 'n'
                                || *pszStart == 'o'
                                || *pszStart == 'e'))
                        {
                            *pchParity = *pszStart;
                            pszSep++;
                            pszStart = pszSep;
                            if (   (   *pszStart == '1'
                                    || *pszStart == '2')
                                && pszStart[1] == '\0')
                            {
                                *pcStopBits = *pszStart == '1' ? 1 : 2;
                                *ppszDevice = strdup(&szDevice[0]);
                                if (!*ppszDevice)
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
 * Sets the given serial config.
 *
 * @returns Status code.
 * @param   pThis                   The serial provider context.
 * @param   u32Baudrate             The baudrate to set.
 * @param   cDataBits               Number of databits to set.
 * @param   chParity                Parity to set.
 * @param   cStopBits               Number of stop bits to set.
 */
static int serialProvCtxSetTermiosCfg(PPSPPROXYPROVCTXINT pThis, uint32_t u32Baudrate,
                                      uint8_t cDataBits, char chParity, uint8_t cStopBits)
{
    int rc = 0;
    speed_t Speed = serialProvCtxBaudrateToTermiosSpeed(u32Baudrate);
    if (Speed != B0)
    {
        struct termios TermiosCfg;

        memset(&TermiosCfg, 0, sizeof(TermiosCfg));
        cfsetispeed(&TermiosCfg, Speed);
        cfsetospeed(&TermiosCfg, Speed);
        TermiosCfg.c_cflag |= CREAD | CLOCAL;

        switch (cDataBits)
        {
            case 5:
                TermiosCfg.c_cflag |= CS5;
                break;
            case 6:
                TermiosCfg.c_cflag |= CS6;
                break;
            case 7:
                TermiosCfg.c_cflag |= CS7;
                break;
            case 8:
                TermiosCfg.c_cflag |= CS8;
                break;
            default:
                return -1; /* Should not happen as the input is checked serialProvCtxParseDevice(). */
        }

        switch (chParity)
        {
            case 'n':
                break;
            case 'o':
                TermiosCfg.c_cflag |= PARENB | PARODD;
                break;
            case 'e':
                TermiosCfg.c_cflag |= PARENB;
                break;
            default:
                return -1; /* Should not happen as the input is checked serialProvCtxParseDevice(). */
        }

        if (cStopBits == 2)
            TermiosCfg.c_cflag |= CSTOPB;

        /* Set to raw input mode. */
        TermiosCfg.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ECHOK | ISIG | IEXTEN);
        TermiosCfg.c_cc[VMIN]  = 0;
        TermiosCfg.c_cc[VTIME] = 0;

        /* Flush everything and set new config. */
        int rcPsx = tcflush(pThis->iFdDev, TCIOFLUSH);
        if (!rcPsx)
        {
            rcPsx = tcsetattr(pThis->iFdDev, TCSANOW, &TermiosCfg);
            if (rcPsx)
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
 * @copydoc{PSPPROXYPROV,pfnCtxInit}
 */
int serialProvCtxInit(PSPPROXYPROVCTX hProvCtx, const char *pszDevice)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    char *pszDevPath = NULL;
    uint32_t u32Baudrate = 0;
    uint8_t cDataBits = 0;
    char chParity = '\0';
    uint8_t cStopBits = 0;

    int rc = serialProvCtxParseDevice(pszDevice, &pszDevPath, &u32Baudrate,
                                      &cDataBits, &chParity, &cStopBits);
    if (!rc)
    {
        int iFd = open(pszDevPath, O_RDWR);
        if (iFd > 0)
        {
            pThis->iFdDev    = iFd;
            pThis->fBlocking = true;
            rc = serialProvCtxSetTermiosCfg(pThis, u32Baudrate, cDataBits, chParity, cStopBits);
            if (!rc)
            {
                /* Create the PDU context. */
                rc = pspStubPduCtxCreate(&pThis->hPduCtx, &g_PduIoIf, pThis);
                if (!rc)
                {
                    rc = pspStubPduCtxConnect(pThis->hPduCtx, 10 * 1000);
                    if (rc)
                        pspStubPduCtxDestroy(pThis->hPduCtx);
                }
            }
        }
        else
            rc = -1; /** @todo Error handling. */

        free(pszDevPath);
    }

    return rc;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxDestroy}
 */
void serialProvCtxDestroy(PSPPROXYPROVCTX hProvCtx)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;

    pspStubPduCtxDestroy(pThis->hPduCtx);
    close(pThis->iFdDev);
    pThis->iFdDev = 0;
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxQueryInfo}
 */
static int serialProvCtxQueryInfo(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR *pPspAddrScratchStart, size_t *pcbScratch)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxQueryInfo(pThis->hPduCtx, idCcd, pPspAddrScratchStart, pcbScratch);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspSmnRead}
 */
int serialProvCtxPspSmnRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspSmnRead(pThis->hPduCtx, idCcd, idCcdTgt, uSmnAddr, cbVal, pvVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspSmnWrite}
 */
int serialProvCtxPspSmnWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, uint32_t idCcdTgt, SMNADDR uSmnAddr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspSmnWrite(pThis->hPduCtx, idCcd, idCcdTgt, uSmnAddr, cbVal, pvVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMemRead}
 */
int serialProvCtxPspMemRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, void *pvBuf, uint32_t cbRead)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspMemRead(pThis->hPduCtx, idCcd, uPspAddr, pvBuf, cbRead);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMemWrite}
 */
int serialProvCtxPspMemWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspMemWrite(pThis->hPduCtx, idCcd, uPspAddr, pvBuf, cbWrite);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMmioRead}
 */
int serialProvCtxPspMmioRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspMmioRead(pThis->hPduCtx, idCcd, uPspAddr, pvVal, cbVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspMmioWrite}
 */
int serialProvCtxPspMmioWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, PSPADDR uPspAddr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspMmioWrite(pThis->hPduCtx, idCcd, uPspAddr, pvVal, cbVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MemRead}
 */
int serialProvCtxPspX86MemRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, void *pvBuf, uint32_t cbRead)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspX86MemRead(pThis->hPduCtx, idCcd, PhysX86Addr, pvBuf, cbRead);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MemWrite}
 */
int serialProvCtxPspX86MemWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, const void *pvBuf, uint32_t cbWrite)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspX86MemWrite(pThis->hPduCtx, idCcd, PhysX86Addr, pvBuf, cbWrite);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MmioRead}
 */
int serialProvCtxPspX86MmioRead(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, uint32_t cbVal, void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspX86MmioRead(pThis->hPduCtx, idCcd, PhysX86Addr, pvVal, cbVal);
}


/**
 * @copydoc{PSPPROXYPROV,pfnCtxPspX86MmioWrite}
 */
int serialProvCtxPspX86MmioWrite(PSPPROXYPROVCTX hProvCtx, uint32_t idCcd, X86PADDR PhysX86Addr, uint32_t cbVal, const void *pvVal)
{
    PPSPPROXYPROVCTXINT pThis = hProvCtx;
    return pspStubPduCtxPspX86MmioWrite(pThis->hPduCtx, idCcd, PhysX86Addr, pvVal, cbVal);
}


/**
 * Provider registration structure.
 */
const PSPPROXYPROV g_PspProxyProvSerial =
{
    /** pszId */
    "serial",
    /** pszDesc */
    "PSP access through a serial connection, device schema looks like serial://<device path>:<baudrate>:<data bit count>:<parity [e|n|o]>:<stop bit count>",
    /** cbCtx */
    sizeof(PSPPROXYPROVCTXINT),
    /** fFeatures */
    0,
    /** pfnCtxInit */
    serialProvCtxInit,
    /** pfnCtxDestroy */
    serialProvCtxDestroy,
    /** pfnCtxQueryInfo */
    serialProvCtxQueryInfo,
    /** pfnCtxPspSmnRead */
    serialProvCtxPspSmnRead,
    /** pfnCtxPspSmnWrite */
    serialProvCtxPspSmnWrite,
    /** pfnCtxPspMemRead */
    serialProvCtxPspMemRead,
    /** pfnCtxPspMemWrite */
    serialProvCtxPspMemWrite,
    /** pfnCtxPspMmioRead */
    serialProvCtxPspMmioRead,
    /** pfnCtxPspMmioWrite */
    serialProvCtxPspMmioWrite,
    /** pfnCtxPspX86MemRead */
    serialProvCtxPspX86MemRead,
    /** pfnCtxPspX86MemWrite */
    serialProvCtxPspX86MemWrite,
    /** pfnCtxPspX86MmioRead */
    serialProvCtxPspX86MmioRead,
    /** pfnCtxPspX86MmioWrite */
    serialProvCtxPspX86MmioWrite,
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

