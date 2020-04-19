/** @file
 * cm-tool - Code module execution helper
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "libpspproxy.h"

/**
 * Loads the given file into memory.
 *
 * @returns Status code.
 * @param   pszFilename         The filename to load.
 * @param   ppv                 Where to store the memory buffer pointer on success.
 * @param   pcb                 Where to store the size of the loaded file on success.
 */
int cmToolLoadFromFile(const char *pszFilename, void **ppv, size_t *pcb)
{
    int rc = 0;
    FILE *pFwFile = fopen(pszFilename, "rb");
    if (pFwFile)
    {
        /* Determine file size. */
        rc = fseek(pFwFile, 0, SEEK_END);
        if (!rc)
        {
            long cbFw = ftell(pFwFile);
            if (cbFw != -1)
            {
                rewind(pFwFile);

                void *pvFw = malloc(cbFw);
                if (pvFw)
                {
                    size_t cbRead = fread(pvFw, cbFw, 1, pFwFile);
                    if (cbRead == 1)
                    {
                        *ppv = pvFw;
                        *pcb = cbFw;
                        return 0;
                    }

                    free(pvFw);
                    rc = -1;
                }
                else
                    rc = -1;
            }
            else
                rc = errno;
        }
        else
            rc = errno;

        fclose(pFwFile);
    }
    else
        rc = errno;

    return rc;
}


/**
 * @copydoc{PSPPROXYIOIF,pfnLogMsg}
 */
static void cmToolProxyIoIfLogMsg(PSPPROXYCTX hCtx, void *pvUser, const char *pszMsg)
{
    printf("%s", pszMsg);
}


/**
 * @copydoc{PSPPROXYIOIF,pfnOutBufWrite}
 */
static int cmToolProxyIoIfOutBufWrite(PSPPROXYCTX hCtx, void *pvUser, uint32_t idOutBuf, const void *pvBuf, size_t cbBuf)
{
    if (idOutBuf == 0)
        printf("%.*s", cbBuf, (const char *)pvBuf);

    return 0;
}


/**
 * @copydoc{PSPPROXYIOIF,pfnInBufPeek}
 */
static size_t cmToolProxyIoIfInBufPeek(PSPPROXYCTX hCtx, void *pvUser, uint32_t idInBuf)
{
    size_t cbAvail = 0;

    if (idInBuf == 0)
    {
        int iAvail = 0;
        ioctl(0, FIONREAD, &iAvail);

        cbAvail = iAvail;
    }

    return cbAvail;
}


/**
 * @copydoc{PSPPROXYIOIF,pfnInBufRead}
 */
static int cmToolProxyIoIfInBufRead(PSPPROXYCTX hCtx, void *pvUser, uint32_t idInBuf, void *pvBuf, size_t cbRead, size_t *pcbRead)
{
    if (idInBuf == 0)
    {
        uint8_t *pbBuf = (uint8_t *)pvBuf;
        while (cbRead)
        {
            int iChr = getchar();
            if (iChr == EOF)
                return -1;

            *pbBuf++ = (char)iChr;
            cbRead--;
        }
    }

    return 0;
}


/**
 * Proxy I/O interface callbacks.
 */
static PSPPROXYIOIF g_ProxyIoIf =
{
    /** pfnLogMsg */
    cmToolProxyIoIfLogMsg,
    /** pfnOutBufWrite */
    cmToolProxyIoIfOutBufWrite,
    /** pfnInBufPeek */
    cmToolProxyIoIfInBufPeek,
    /** pfnInBufRead */
    cmToolProxyIoIfInBufRead,
};


int main(int argc, char *argv[])
{
    void *pv = NULL;
    size_t cb = 0;
    int rc = cmToolLoadFromFile(argv[2], &pv, &cb);
    if (!rc)
    {
        PSPPROXYCTX hCtx;

        rc = PSPProxyCtxCreate(&hCtx, argv[1], &g_ProxyIoIf, NULL);
        if (!rc)
        {
            rc = PSPProxyCtxCodeModLoad(hCtx, pv, cb);
            if (!rc)
            {
                uint32_t u32CmRet = 0;
                rc = PSPProxyCtxCodeModExec(hCtx, 0 /*u32Arg0*/, 0 /*u32Arg1*/, 0 /*u32Arg2*/, 0 /*u32Arg3*/, &u32CmRet, UINT32_MAX);
                if (!rc)
                    printf("Code module executed successfully and returned %#x\n", u32CmRet);
                else
                    printf("Code module execution failed with %d\n", rc);
            }
            else
                printf("Loading the code module failed with %d\n", rc);

            PSPProxyCtxDestroy(hCtx);
        }

        free(pv);
    }
    else
        printf("Loading the file \"%s\" failed with %d\n", argv[2]);

    return rc;
}

