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

#ifndef OSD_GDBSERVER_H
#define OSD_GDBSERVER_H

#include <osd/cl_cdm.h>
#include <osd/cl_mam.h>
#include <osd/hostmod.h>
#include <osd/osd.h>

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup libosd-gdbserver OSD-GDB server utility
 * @ingroup libosd
 *
 * @{
 */

struct osd_gdbserver_ctx;

/**
 * Indicates the port for connecting to GDB
 */
#define OSD_GDBSERVER_PORT 5555

/**
 * Indicates the size of the buffer
 */
#define OSD_GDBSERVER_BUFF_SIZE 1024

/**
 * Create a new context object
 */
osd_result osd_gdbserver_new(struct osd_gdbserver_ctx **ctx,
                             struct osd_log_ctx *log_ctx,
                             const char *host_controller_address,
                             uint16_t cdm_di_addr, uint16_t mam_di_addr);

/**
 * Connect GDB server to the GDB client
 *
 * @return OSD_OK on success, any other value indicates an error
 */
osd_result osd_gdbserver_connect_GDB(struct osd_gdbserver_ctx *ctx);

/**
 * Connect GDB server to the host controller
 *
 * @return OSD_OK on success, any other value indicates an error
 */
osd_result osd_gdbserver_connect_hostmod(struct osd_gdbserver_ctx *ctx);

/**
 * @copydoc osd_hostmod_disconnect()
 */
osd_result osd_gdbserver_disconnect_hostmod(struct osd_gdbserver_ctx *ctx);

/**
 * @copydoc osd_hostmod_is_connected()
 */
bool osd_gdbserver_is_connected_hostmod(struct osd_gdbserver_ctx *ctx);

/**
 * Free the context object
 */
void osd_gdbserver_free(struct osd_gdbserver_ctx **ctx_p);

/**
 * Start the connection with GDB client
 */
osd_result osd_gdbserver_start(struct osd_gdbserver_ctx *ctx);

/**
 * Stop the connection with GDB client
 */
void osd_gdbserver_stop(struct osd_gdbserver_ctx *ctx);
/**
 * Read data from the GDB client
 *
 * @return OSD_OK on success, any other value indicates an error
 *
 * @see osd_gdbserver_write_data()
 */
osd_result osd_gdbserver_read_data(struct osd_gdbserver_ctx *ctx);

/**
 * Write data to the GDB client
 *
 * @param data the data to be written to the connected client
 * @param len the length of the data to be written
 * @return OSD_OK on success, any other value indicates an error
 *
 * @see osd_gdbserver_read_data()
 */
osd_result osd_gdbserver_write_data(struct osd_gdbserver_ctx *ctx, char *data,
                                    int len);

/**@}*/ /* end of doxygen group libosd-gdbserver */

#ifdef __cplusplus
}
#endif

#endif  // OSD_GDBSERVER_H
