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

#ifndef MOCK_GDBCLIENT_H
#define MOCK_GDBCLIENT_H

#include <check.h>

#include <osd/hostmod.h>
#include <osd/osd.h>

#define MOCK_GDBCLIENT_PORT_DEFAULT 5555
#define MOCK_GDBCLIENT_BUFF_SIZE 1024

struct mock_gdbclient_ctx {
    int fd;
    int closed;
    int buf_cnt;
    int port;
    char *addr;
    char buffer[MOCK_GDBCLIENT_BUFF_SIZE];
};

osd_result mock_gdbclient_new(struct mock_gdbclient_ctx **ctx);
osd_result mock_gdbclient_start(struct mock_gdbclient_ctx *ctx);
void mock_gdbclient_set_port(struct mock_gdbclient_ctx *ctx, int port);
void mock_gdbclient_set_addr(struct mock_gdbclient_ctx *ctx, char *address);
osd_result mock_gdbclient_handle_resp(struct mock_gdbclient_ctx *ctx);
osd_result mock_gdbclient_connect(struct mock_gdbclient_ctx *ctx);
osd_result mock_gdbclient_write_data(struct mock_gdbclient_ctx *ctx, char *data,
                                     int len);
osd_result mock_gdbclient_read_data(struct mock_gdbclient_ctx *ctx);
void mock_gdbclient_free(struct mock_gdbclient_ctx **ctx_p);

#endif  // MOCK_GDBCLIENT_H
