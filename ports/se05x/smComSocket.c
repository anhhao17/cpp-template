/*
 * smComSocket.c — session-context wrapper over smComSocket_fd.c for SMCOM_JRCP_V1.
 *
 * smComSocket_fd.c (extracted from NXP plug-and-trust commit 6c7bb9e) provides
 * the raw file-descriptor level I/O (RJCT APDU-over-TCP framing).  This file
 * adds the smCom session-context API that sm_connect.c expects.
 *
 * Copyright 2016 NXP — original smComSocket_linux.c.
 * This wrapper: BSD-3-Clause (same as the SE05x port).
 */

#include "smComSocket.h"
#include "smCom.h"
#include "sm_types.h"

#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>

/* The session connection context owns the TCP file descriptor. */
typedef struct {
    int fd;
} SmComSocketCtx_t;

/* Forward-declared FD-level functions from smComSocket_fd.c */
U32 smComSocket_CloseFD(int fd);
U32 smComSocket_GetATRFD(int fd, U8 *pAtr, U16 *atrLen);
U32 smComSocket_TransceiveFD(int fd, apdu_t *pApdu);
U32 smComSocket_TransceiveRawFD(int fd, U8 *pTx, U16 txLen, U8 *pRx, U32 *pRxLen);

static U32 smComSocket_Transceive(void *conn_ctx, apdu_t *pApdu)
{
    return smComSocket_TransceiveFD(((SmComSocketCtx_t *)conn_ctx)->fd, pApdu);
}

static U32 smComSocket_TransceiveRaw(void *conn_ctx, U8 *pTx, U16 txLen, U8 *pRx, U32 *pRxLen)
{
    return smComSocket_TransceiveRawFD(((SmComSocketCtx_t *)conn_ctx)->fd,
                                       pTx, txLen, pRx, pRxLen);
}

U16 smComSocket_Open(void **conn_ctx, U8 *pIpAddrString, U16 portNo, U8 *pAtr, U16 *atrLen)
{
    char portStr[8];
    struct addrinfo hints, *res;
    int fd;

    if (!conn_ctx || !pIpAddrString) return SMCOM_COM_FAILED;

    snprintf(portStr, sizeof(portStr), "%u", portNo);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo((const char *)pIpAddrString, portStr, &hints, &res) != 0)
        return SMCOM_COM_FAILED;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { freeaddrinfo(res); return SMCOM_COM_FAILED; }

    if (connect(fd, res->ai_addr, (socklen_t)res->ai_addrlen) != 0) {
        freeaddrinfo(res); close(fd); return SMCOM_COM_FAILED;
    }
    freeaddrinfo(res);

    if (smComSocket_GetATRFD(fd, pAtr, atrLen) != SMCOM_OK) {
        close(fd); return SMCOM_COM_FAILED;
    }

    SmComSocketCtx_t *ctx = malloc(sizeof(*ctx));
    if (!ctx) { close(fd); return SMCOM_COM_FAILED; }
    ctx->fd  = fd;
    *conn_ctx = ctx;

    return (U16)smCom_Init(&smComSocket_Transceive, &smComSocket_TransceiveRaw);
}

U16 smComSocket_Close(void *conn_ctx)
{
    if (conn_ctx) {
        SmComSocketCtx_t *ctx = (SmComSocketCtx_t *)conn_ctx;
        smComSocket_CloseFD(ctx->fd);
        free(ctx);
    }
    return SMCOM_OK;
}

U32 smComSocket_LockChannel(void *conn_ctx)
{
    (void)conn_ctx;
    return SMCOM_OK;
}

U32 smComSocket_UnlockChannel(void *conn_ctx)
{
    (void)conn_ctx;
    return SMCOM_OK;
}
