/* Copyright 2017 The Open SoC Debug Project
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

#ifndef OSD_HOSTMOD_H
#define OSD_HOSTMOD_H

#include <osd/module.h>
#include <osd/osd.h>
#include <osd/packet.h>

#include <czmq.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup libosd-hostmod Host Module
 * @ingroup libosd
 *
 * @{
 */

/** Flag: fully blocking operation (i.e. wait forever) */
#define OSD_HOSTMOD_BLOCKING 1

/**
 * Opaque context object
 *
 * This object contains all state information. Create and initialize a new
 * object with osd_hostmod_new() and delete it with osd_hostmod_free().
 */
struct osd_hostmod_ctx;

/**
 * Event handler function prototype
 *
 * The ownership of packet is passed to the handler function.
 */
typedef osd_result (*osd_hostmod_event_handler_fn)(
    void * /* arg */, struct osd_packet * /* packet */);

/**
 * Create new osd_hostmod instance
 *
 * @param[out] ctx the osd_hostmod_ctx context to be created
 * @param[in] log_ctx the log context to be used. Set to NULL to disable logging
 * @param[in] host_controller_address ZeroMQ endpoint of the host controller
 * @param[in] event_handler function called when a new event packet is received
 * @param[in] event_handler_arg argument passed to the event handler callback
 * @return OSD_OK on success, any other value indicates an error
 *
 * @see osd_hostmod_free()
 */
osd_result osd_hostmod_new(struct osd_hostmod_ctx **ctx,
                           struct osd_log_ctx *log_ctx,
                           const char *host_controller_address,
                           osd_hostmod_event_handler_fn event_handler,
                           void *event_handler_arg);

/**
 * Free and NULL a communication API context object
 *
 * Call osd_hostmod_disconnect() before calling this function.
 *
 * @param ctx the osd_com context object
 */
void osd_hostmod_free(struct osd_hostmod_ctx **ctx);

osd_result osd_hostmod_get_modules(struct osd_hostmod_ctx *ctx,
                                   struct osd_module_desc **modules,
                                   size_t *modules_len);

/**
 * Connect to the host controller
 *
 * @param ctx the osd_hostmod_ctx context object
 * @return OSD_OK on success, any other value indicates an error
 *
 * @see osd_hostmod_disconnect()
 */
osd_result osd_hostmod_connect(struct osd_hostmod_ctx *ctx);

/**
 * Shut down all communication with the device
 *
 * @param ctx the osd_hostmod context object
 * @return OSD_OK on success, any other value indicates an error
 *
 * @see osd_hostmod_run()
 */
osd_result osd_hostmod_disconnect(struct osd_hostmod_ctx *ctx);

/**
 * Is the connection to the device active?
 *
 * @param ctx the osd_hostmod context object
 * @return 1 if connected, 0 if not connected
 *
 * @see osd_hostmod_connect()
 * @see osd_hostmod_disconnect()
 */
bool osd_hostmod_is_connected(struct osd_hostmod_ctx *ctx);

/**
 * Read a register of a module in the debug system
 *
 * Unless the flag OSD_HOSTMOD_BLOCKING has been set this function times out
 * if the module does not reply within a ZMQ_RCV_TIMEOUT milliseconds.
 *
 * @param ctx the osd_hostmod_ctx context object
 * @param[out] reg_val the result of the register read. Preallocate a variable
 *                     large enough to hold @p reg_size_bit bits.
 * @param diaddr the DI address of the module to read the register from
 * @param reg_addr the address of the register to read
 * @param reg_size_bit size of the register in bit.
 *                     Supported values: 16, 32, 64 and 128.
 * @param flags flags. Set OSD_HOSTMOD_BLOCKING to block indefinitely until the
 *              access succeeds.
 * @return OSD_OK on success, any other value indicates an error
 * @return OSD_ERROR_TIMEDOUT if the register read timed out (only if
 *         OSD_HOSTMOD_BLOCKING is not set)
 *
 * @see osd_hostmod_write()
 */
osd_result osd_hostmod_reg_read(struct osd_hostmod_ctx *ctx, void *reg_val,
                                uint16_t diaddr, uint16_t reg_addr,
                                int reg_size_bit, int flags);

/**
 * Write a register of a module in the debug system
 *
 * @param ctx the osd_hostmod_ctx context object
 * @param reg_val the data to be written. Provide enough data according to
 *                @p reg_size_bit
 * @param diaddr the DI address of the accessed module
 * @param reg_addr the address of the register to read
 * @param reg_size_bit size of the register in bit.
 *                     Supported values: 16, 32, 64 and 128.
 * @param flags flags. Set OSD_HOSTMOD_BLOCKING to block indefinitely until the
 *              access succeeds.
 * @return OSD_OK on success, any other value indicates an error
 * @return OSD_ERROR_TIMEDOUT if the register read timed out (only if
 *         OSD_HOSTMOD_BLOCKING is not set)
 */
osd_result osd_hostmod_reg_write(struct osd_hostmod_ctx *ctx,
                                 const void *reg_val, uint16_t diaddr,
                                 uint16_t reg_addr, int reg_size_bit,
                                 int flags);

/**
 * Get the DI address assigned to this host debug module
 *
 * The address is assigned during the connection, i.e. you need to call
 * osd_hostmod_connect() before calling this function.
 *
 * @param ctx the osd_hostmod_ctx context object
 * @return the address assigned to this debug module
 */
uint16_t osd_hostmod_get_diaddr(struct osd_hostmod_ctx *ctx);

/**
 * Get the description fields of a debug module (type, vendor, version)
 */
osd_result osd_hostmod_describe_module(struct osd_hostmod_ctx *ctx,
                                       uint16_t di_addr,
                                       struct osd_module_desc *desc);

/**
 * Get the maximum number paylaod of words in an event packet
 *
 * @param ctx the osd_hostmod_ctx context object
 * @return the number of payload words in an event packet sent to
 *         @p di_addr_target
 */
unsigned int osd_hostmod_get_max_event_words(struct osd_hostmod_ctx *ctx,
                                             unsigned int di_addr_target);

/**
 * Send an event packet to its destination
 */
osd_result osd_hostmod_event_send(struct osd_hostmod_ctx *ctx,
                                  const struct osd_packet* event_pkg);

/**
 * Receive an event packet (blocking)
 *
 * Block until a new event packet is available, and return it.
 */
osd_result osd_hostmod_event_receive(struct osd_hostmod_ctx *ctx,
                                     struct osd_packet **event_pkg);

/**@}*/ /* end of doxygen group libosd-hostmod */

#ifdef __cplusplus
}
#endif

#endif  // OSD_HOSTMOD_H
