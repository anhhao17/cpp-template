/*
 * i2c_sim.c — TCP PAL for the wolfSSL SE050 software simulator.
 *
 * Replaces hostlib/hostLib/platform/linux/i2c_a7.c so the T1oI2C protocol
 * stack sends/receives T=1 frames over a TCP socket instead of a real I2C bus.
 * Connect target is resolved in priority order:
 *   1. EX_SSS_BOOT_SSS_PORT=<host>:<port>   (our unified env var)
 *   2. SE050_SIM_HOST + SE050_SIM_PORT       (wolfSSL compatibility)
 *   3. 127.0.0.1:8050                        (built-in default)
 *
 * Based on wolfSSL/simulators SE050Sim/wolfcrypt-test/i2c_a7.c
 * (GPL-3.0-or-later; same licence as this port overlay).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>

#include "i2c_a7.h"

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT  8050

typedef struct { int sockfd; } SimConn_t;

static int read_exact(int fd, unsigned char *buf, int len)
{
    int total = 0;
    while (total < len) {
        int n = read(fd, buf + total, len - total);
        if (n <= 0) return -1;
        total += n;
    }
    return 0;
}

static int write_all(int fd, const unsigned char *buf, int len)
{
    int total = 0;
    while (total < len) {
        int n = write(fd, buf + total, len - total);
        if (n <= 0) return -1;
        total += n;
    }
    return 0;
}

/* Parse "host:port" string. Returns 1 on success, 0 if no ':' found. */
static int parse_hostport(const char *s, char *host_out, size_t host_sz, int *port_out)
{
    const char *colon = strrchr(s, ':');
    if (!colon) return 0;
    size_t hlen = (size_t)(colon - s);
    if (hlen == 0 || hlen >= host_sz) return 0;
    memcpy(host_out, s, hlen);
    host_out[hlen] = '\0';
    *port_out = atoi(colon + 1);
    return (*port_out > 0 && *port_out < 65536);
}

i2c_error_t axI2CInit(void **conn_ctx, const char *pDevName)
{
    char host[128] = DEFAULT_HOST;
    int  port      = DEFAULT_PORT;
    struct sockaddr_in addr;
    SimConn_t *ctx;
    int flag = 1;

    /* Priority 1: EX_SSS_BOOT_SSS_PORT="host:port" */
    const char *ep = getenv("EX_SSS_BOOT_SSS_PORT");
    if (ep && *ep) {
        if (!parse_hostport(ep, host, sizeof(host), &port)) {
            fprintf(stderr, "[i2c_sim] warning: EX_SSS_BOOT_SSS_PORT='%s' "
                    "not a valid host:port, ignoring\n", ep);
        }
    }
    /* Priority 2: pDevName from ex_sss_boot_open (same format) */
    else if (pDevName && *pDevName) {
        parse_hostport(pDevName, host, sizeof(host), &port);
    }
    /* Priority 3: SE050_SIM_HOST / SE050_SIM_PORT (wolfSSL compat) */
    else {
        const char *h = getenv("SE050_SIM_HOST");
        if (h && *h) {
            snprintf(host, sizeof(host), "%s", h);
        }
        const char *p = getenv("SE050_SIM_PORT");
        if (p && *p) port = atoi(p);
    }

    ctx = (SimConn_t *)calloc(1, sizeof(SimConn_t));
    if (!ctx) return I2C_FAILED;

    ctx->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->sockfd < 0) {
        fprintf(stderr, "[i2c_sim] socket() failed: %s\n", strerror(errno));
        free(ctx);
        return I2C_FAILED;
    }
    setsockopt(ctx->sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        fprintf(stderr, "[i2c_sim] invalid host: %s\n", host);
        close(ctx->sockfd);
        free(ctx);
        return I2C_FAILED;
    }
    if (connect(ctx->sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[i2c_sim] connect(%s:%d) failed: %s\n",
                host, port, strerror(errno));
        close(ctx->sockfd);
        free(ctx);
        return I2C_FAILED;
    }

    fprintf(stderr, "[i2c_sim] connected to SE050 simulator at %s:%d\n",
            host, port);
    fflush(stderr);
    *conn_ctx = ctx;
    return I2C_OK;
}

void axI2CTerm(void *conn_ctx, int mode)
{
    SimConn_t *ctx = (SimConn_t *)conn_ctx;
    (void)mode;
    if (ctx) { close(ctx->sockfd); free(ctx); }
}

i2c_error_t axI2CWrite(void *conn_ctx, unsigned char bus,
                       unsigned char addr, unsigned char *pTx,
                       unsigned short txLen)
{
    SimConn_t *ctx = (SimConn_t *)conn_ctx;
    (void)bus; (void)addr;
    if (!ctx || ctx->sockfd < 0) return I2C_FAILED;
    return write_all(ctx->sockfd, pTx, txLen) < 0 ? I2C_FAILED : I2C_OK;
}

i2c_error_t axI2CRead(void *conn_ctx, unsigned char bus,
                      unsigned char addr, unsigned char *pRx,
                      unsigned short rxLen)
{
    SimConn_t *ctx = (SimConn_t *)conn_ctx;
    (void)bus; (void)addr;
    if (!ctx || ctx->sockfd < 0) return I2C_FAILED;

    /*
     * Emulate I2C NACK/retry behaviour: if no data is available yet,
     * return I2C_FAILED so the SDK's polling loop retries with a delay.
     */
    struct pollfd pfd = { ctx->sockfd, POLLIN, 0 };
    if (poll(&pfd, 1, 100) <= 0 || !(pfd.revents & POLLIN))
        return I2C_FAILED;

    if (rxLen >= 260) {
        /* Initial ATR scan: read what's there, zero-fill the rest. */
        memset(pRx, 0, rxLen);
        ssize_t n = read(ctx->sockfd, pRx, rxLen);
        return (n <= 0) ? I2C_FAILED : I2C_OK;
    }
    return read_exact(ctx->sockfd, pRx, rxLen) < 0 ? I2C_FAILED : I2C_OK;
}

i2c_error_t axI2CWriteByte(void *conn_ctx, unsigned char bus,
                           unsigned char addr, unsigned char *pTx)
{
    return axI2CWrite(conn_ctx, bus, addr, pTx, 1);
}

i2c_error_t axI2CWriteRead(void *conn_ctx, unsigned char bus,
                           unsigned char addr, unsigned char *pTx,
                           unsigned short txLen, unsigned char *pRx,
                           unsigned short *pRxLen)
{
    i2c_error_t rc = axI2CWrite(conn_ctx, bus, addr, pTx, txLen);
    if (rc != I2C_OK) return rc;
    return axI2CRead(conn_ctx, bus, addr, pRx, *pRxLen);
}

void axI2CResetBackoffDelay(void) {}
