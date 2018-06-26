/* Copyright 2018 The Open SoC Debug Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <osd/gdbserver.h>
#include <osd/module.h>
#include <osd/osd.h>
#include <osd/reg.h>
#include "osd-private.h"

#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <gelf.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * OSD-GDB server context
 */
struct osd_gdbserver_ctx {
    struct osd_hostmod_ctx *hostmod_ctx;
    struct osd_log_ctx *log_ctx;
    int fd;
    char *name;
    char *port;
    struct sockaddr_in sin;
    char buffer[OSD_GDBSERVER_BUFF_SIZE];
    int buf_cnt;
    char *buf_p;
    int closed;
    int client_fd;
};

API_EXPORT
osd_result osd_gdbserver_new(struct osd_gdbserver_ctx **ctx,
                             struct osd_log_ctx *log_ctx,
                             const char *host_controller_address)
{
    osd_result rv;

    struct osd_gdbserver_ctx *c = calloc(1, sizeof(struct osd_gdbserver_ctx));
    assert(c);

    c->log_ctx = log_ctx;

    struct osd_hostmod_ctx *hostmod_ctx;
    rv = osd_hostmod_new(&hostmod_ctx, log_ctx, host_controller_address, NULL,
                         NULL);
    assert(OSD_SUCCEEDED(rv));
    c->hostmod_ctx = hostmod_ctx;

    *ctx = c;

    return OSD_OK;
}

static int dectohex(int packet_char)
{
    if (packet_char < 10) {
        return packet_char + '0';
    } else {
        return packet_char - 10 + 'a';
    }
}

static void free_service(struct osd_gdbserver_ctx *ctx)
{
    free(ctx->name);
    free(ctx->port);
    free(ctx);
}

API_EXPORT
osd_result osd_gdbserver_connect(struct osd_gdbserver_ctx *ctx, char *name,
                                 char *port)
{
    osd_result rv = osd_hostmod_connect(ctx->hostmod_ctx);
    if (OSD_FAILED(rv)) {
        return rv;
    }

    int sockoptval = 1;

    ctx->name = strdup(name);
    ctx->port = strdup(port);
    ctx->fd = socket(AF_INET, SOCK_STREAM, 0);

    if (OSD_FAILED(ctx->fd)) {
        free_service(ctx);
        return OSD_ERROR_CONNECTION_FAILED;
    }

    setsockopt(ctx->fd, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof(int));

    memset(&ctx->sin, 0, sizeof(ctx->sin));
    ctx->sin.sin_family = AF_INET;
    ctx->sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ctx->sin.sin_port = htons(OSD_GDBSERVER_PORT);

    if (OSD_FAILED(
            bind(ctx->fd, (struct sockaddr *)&ctx->sin, sizeof(ctx->sin)))) {
        close(ctx->fd);
        free_service(ctx);
        return OSD_ERROR_CONNECTION_FAILED;
    }

    if (OSD_FAILED(listen(ctx->fd, 1))) {
        close(ctx->fd);
        free_service(ctx);
        return OSD_ERROR_CONNECTION_FAILED;
    }

    struct sockaddr_in addr_in;
    addr_in.sin_port = 0;
    socklen_t addr_in_size = sizeof(addr_in);

    getsockname(ctx->fd, (struct sockaddr *)&addr_in, &addr_in_size);
    printf("server started on %s, listening on port %d\n", name,
           ntohs(addr_in.sin_port));

    while (1) {
        ctx->client_fd =
            accept(ctx->fd, (struct sockaddr *)&addr_in, &addr_in_size);
        if (OSD_FAILED(ctx->client_fd)) {
            rv = close(ctx->client_fd);
            if (OSD_SUCCEEDED(rv)) {
                break;
            }
        }
        printf("Server got connection from client %s\n",
               inet_ntoa(addr_in.sin_addr));

        if (OSD_SUCCEEDED(close(ctx->client_fd))) {
            break;
        }
    }

    close(ctx->fd);

    return OSD_OK;
}

API_EXPORT
osd_result osd_gdbserver_disconnect(struct osd_gdbserver_ctx *ctx)
{
    return osd_hostmod_disconnect(ctx->hostmod_ctx);
}

API_EXPORT
bool osd_gdbserver_is_connected(struct osd_gdbserver_ctx *ctx)
{
    return osd_hostmod_is_connected(ctx->hostmod_ctx);
}

API_EXPORT
void osd_gdbserver_free(struct osd_gdbserver_ctx **ctx_p)
{
    assert(ctx_p);
    struct osd_gdbserver_ctx *ctx = *ctx_p;
    if (!ctx) {
        return;
    }

    osd_hostmod_free(&ctx->hostmod_ctx);

    free(ctx);
    *ctx_p = NULL;
}

API_EXPORT
osd_result osd_gdbserver_read_data(struct osd_gdbserver_ctx *ctx)
{
    memset(ctx->buffer, 0, sizeof ctx->buffer);
    ctx->buf_cnt = read(ctx->client_fd, ctx->buffer, OSD_GDBSERVER_BUFF_SIZE);

    if (OSD_FAILED(ctx->buf_cnt)) {
        return OSD_ERROR_CONNECTION_FAILED;
    } else {
        if (ctx->buf_cnt > 0) {
            printf("Server:Packet Received %s\n", ctx->buffer);
            printf("Size of Packet:%d\n", ctx->buf_cnt);
            return OSD_OK;
        }
        if (ctx->buf_cnt == 0) {
            ctx->closed = 1;
            return OSD_ERROR_FAILURE;
        }
    }

    return OSD_OK;
}

API_EXPORT
osd_result osd_gdbserver_write_data(struct osd_gdbserver_ctx *ctx, char *data,
                                    int len)
{
    if (ctx->closed == 1) {
        return OSD_ERROR_NOT_CONNECTED;
    }
    int wlen = write(ctx->client_fd, data, len);
    if (wlen == len) {
        return OSD_OK;
    }

    return OSD_ERROR_NOT_CONNECTED;
}

static osd_result get_char(struct osd_gdbserver_ctx *ctx, int *ch)
{
    osd_result rv;

    ctx->buf_p = ctx->buffer;
    ctx->buf_cnt--;
    if (OSD_FAILED(ctx->buf_cnt)) {
        return OSD_ERROR_FAILURE;
    }
    *ch = *(ctx->buf_p++);

    return OSD_OK;
}

static osd_result validate_rsp_packet(struct osd_gdbserver_ctx *ctx,
                                      bool *ver_checksum, int *len,
                                      char *buffer)
{
    unsigned char val_checksum = 0;
    char packet_checksum[3];
    int packet_char;
    int cnt = 0;
    osd_result rv;

    char *buf_p = ctx->buf_p;
    int buf_cnt = ctx->buf_cnt;

    // packet-format: $packet-data#checksum
    int i = 0;
    char *buf = buf_p;
    int done = 0;
    // traversing through the obtained packet till we obtained '#'
    while (1) {
        packet_char = *buf++;
        i++;

        if (packet_char == '#') {
            done = 1;
            break;
        }
        /*Any escaped byte (here, '}') is transmitted as the escape
        * character followed by the original character XORed with 0x20.
        */
        if (packet_char == '}') {
            val_checksum += packet_char & 0xff;
            packet_char = *buf++;
            i++;
            val_checksum += packet_char & 0xff;
            buffer[cnt++] = (packet_char ^ 0x20) & 0xff;
        } else {
            val_checksum += packet_char & 0xff;
            buffer[cnt++] = packet_char & 0xff;
        }
    }

    *len = cnt;
    packet_char = *buf++;
    packet_checksum[0] = packet_char;
    packet_char = *buf;
    packet_checksum[1] = packet_char;
    packet_checksum[2] = 0;
    *ver_checksum = (val_checksum == strtoul(packet_checksum, NULL, 16));

    return OSD_OK;
}

static osd_result receive_rsp_packet(struct osd_gdbserver_ctx *ctx,
                                     char *buffer, int *len)
{
    int packet_char;
    osd_result rv;

    do {
        rv = get_char(ctx, &packet_char);
        if (OSD_FAILED(rv)) {
            return rv;
        }
    } while (packet_char != '$');

    bool ver_checksum = 0;
    rv = validate_rsp_packet(ctx, &ver_checksum, len, buffer);

    if (OSD_FAILED(rv)) {
        return rv;
    } else {
        if (ver_checksum == 1) {
            rv = osd_gdbserver_write_data(ctx, "+", 1);
        } else {
            rv = osd_gdbserver_write_data(ctx, "-", 1);
        }
        if (OSD_FAILED(rv)) {
            return rv;
        }
    }
    return OSD_OK;
}

static osd_result send_rsp_packet(struct osd_gdbserver_ctx *ctx, char *buffer,
                                  int len)
{
    char packet_buffer[len + 3];
    int packet_checksum = 0;
    osd_result rv;

    while (1) {
        packet_buffer[0] = '$';
        memcpy(packet_buffer + 1, buffer, len);
        int j = len + 1;
        packet_buffer[j++] = '#';
        for (int i = 0; i < len; i++) {
            packet_checksum += buffer[i];
        }
        packet_buffer[j++] = dectohex((packet_checksum >> 4) & 0xf);
        packet_buffer[j] = dectohex(packet_checksum & 0xf);

        rv = osd_gdbserver_write_data(ctx, packet_buffer, len + 4);
        if (OSD_FAILED(rv)) {
            return OSD_ERROR_FAILURE;
        }

        rv = osd_gdbserver_read_data(ctx);
        if (OSD_FAILED(rv)) {
            return OSD_ERROR_FAILURE;
        }

        char reply = ctx->buffer[0];
        if (reply == '+') {
            break;
        } else {
            return OSD_ERROR_FAILURE;
        }
    }

    return OSD_OK;
}
