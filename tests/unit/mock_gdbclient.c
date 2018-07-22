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

#include "mock_gdbclient.h"
#include <osd/module.h>
#include <osd/osd.h>
#include <osd/reg.h>

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

osd_result mock_gdbclient_new(struct mock_gdbclient_ctx **ctx)
{
    struct mock_gdbclient_ctx *c = calloc(1, sizeof(struct mock_gdbclient_ctx));
    assert(c);

    c->fd = 0;
    c->closed = 0;
    c->buf_cnt = 0;
    *ctx = c;

    return OSD_OK;
}

osd_result mock_gdbclient_connect(struct mock_gdbclient_ctx *ctx)
{
    osd_result rv;

    struct sockaddr_in gdb_serv_addr;

    if (OSD_FAILED(ctx->fd = socket(AF_INET, SOCK_STREAM, 0))) {
        return OSD_ERROR_CONNECTION_FAILED;
    }

    memset(&gdb_serv_addr, '0', sizeof(gdb_serv_addr));

    gdb_serv_addr.sin_family = AF_INET;
    gdb_serv_addr.sin_port = htons(MOCK_GDBCLIENT_PORT);

    if (OSD_FAILED(inet_pton(AF_INET, "127.0.0.1", &gdb_serv_addr.sin_addr))) {
        return OSD_ERROR_CONNECTION_FAILED;
    }

    if (OSD_FAILED(connect(ctx->fd, (struct sockaddr *)&gdb_serv_addr,
                           sizeof(gdb_serv_addr)))) {
        return OSD_ERROR_CONNECTION_FAILED;
    }

    mock_gdbclient_write_data(ctx, "OK", 2);

    return OSD_OK;
}

osd_result mock_gdbclient_read_data(struct mock_gdbclient_ctx *ctx)
{
    // memset(ctx->buffer, 0, sizeof ctx->buffer);
    ctx->buf_cnt = read(ctx->fd, ctx->buffer, MOCK_GDBCLIENT_BUFF_SIZE);

    if (OSD_FAILED(ctx->buf_cnt)) {
        return OSD_ERROR_CONNECTION_FAILED;
    } else {
        if (ctx->buf_cnt > 0) {
            printf("Client:Packet Received %s\n", ctx->buffer);
            printf("Client of Packet:%d\n", ctx->buf_cnt);
            return OSD_OK;
        }
        if (ctx->buf_cnt == 0) {
            ctx->closed = 1;
            return OSD_ERROR_FAILURE;
        }
    }
    return OSD_OK;
}

osd_result mock_gdbclient_write_data(struct mock_gdbclient_ctx *ctx, char *data,
                                     int len)
{
    if (ctx->closed == 1) {
        return OSD_ERROR_NOT_CONNECTED;
    }
    int wlen = write(ctx->fd, data, len);
    if (wlen == len) {
        return OSD_OK;
    }

    return OSD_ERROR_NOT_CONNECTED;
}

void mock_gdbclient_free(struct mock_gdbclient_ctx **ctx_p)
{
    assert(ctx_p);
    struct mock_gdbclient_ctx *ctx = *ctx_p;
    if (!ctx) {
        return;
    }

    free(ctx);
    *ctx_p = NULL;
}
