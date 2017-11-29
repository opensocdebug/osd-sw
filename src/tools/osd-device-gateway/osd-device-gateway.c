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

/**
 * Open SoC Debug Device Gateway
 */

#define CLI_TOOL_PROGNAME "osd-device-gateway"
#define CLI_TOOL_SHORTDESC "Open SoC Debug device gateway"

#include <osd/gateway.h>
#include "../cli-util.h"

#include <byteswap.h>
#include <libglip.h>

/**
 * Default GLIP backend to be used when connecting to a device
 */
#define GLIP_DEFAULT_BACKEND "tcp"

/**
 * Subnet address of the device. Currently static and must be 0.
 */
#define DEVICE_SUBNET_ADDRESS 0

/**
 * GLIP library context
 */
struct glip_ctx *glip_ctx;

// command line arguments
struct arg_str *a_glip_backend;
struct arg_str *a_glip_backend_options;
struct arg_str *a_hostctrl_ep;

/**
 * Log handler for GLIP
 */
static void glip_log_handler(struct glip_ctx *ctx, int priority,
                             const char *file, int line, const char *fn,
                             const char *format, va_list args)
{
    cli_vlog(priority, "libglip", format, args);
}

/**
 * Read data from the device
 *
 * @param buf a preallocated buffer for the read data
 * @param size_words number of uint16_t words to read from the device
 * @param flags currently unused, set to 0
 *
 * @return the number of uint16_t words read
 * @return -ENOTCONN if the connection was closed during the read
 * @return any other negative value indicates an error
 */
static ssize_t device_read(uint16_t *buf, size_t size_words, int flags)
{
    int rv;
    size_t words_read;
    size_t bytes_read;

    uint16_t *buf_be;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    buf_be = malloc(size_words * sizeof(uint16_t));
    if (!buf_be) {
        return -1;
    }
#else
    buf_be = buf;
#endif

    rv = glip_read_b(glip_ctx, 0, size_words * sizeof(uint16_t),
                     (uint8_t *)buf_be, &bytes_read,
                     0 /* timeout [ms]; 0 == never */);
    if (rv == -ENOTCONN) {
        return rv;
    } else if (rv != 0) {
        return -1;
    }
    words_read = bytes_read / sizeof(uint16_t);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    for (size_t w = 0; w < words_read; w++) {
        buf[w] = bswap_16(buf_be[w]);
    }
    free(buf_be);
#endif

    return words_read;
}

/**
 * Write to the device
 *
 * @param buf data to write
 * @param size_words size of @p buf in uint16_t words
 * @param flags currently unused, set to 0
 *
 * @return the number of uint16_t words written, if successful
 * @return -ENOTCONN if the device is not connected
 * @return any other negative value indicates an error
 */
static ssize_t device_write(const uint16_t *buf, size_t size_words, int flags)
{
    size_t bytes_written;
    int rv;

    // GLIP and OSD are big endian, |buf| is in native endianness
    uint16_t *buf_be;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    buf_be = malloc(size_words * sizeof(uint16_t));
    if (!buf_be) {
        return -1;
    }

    for (size_t w = 0; w < size_words; w++) {
        buf_be[w] = bswap_16(buf[w]);
    }
#else
    buf_be = buf;
#endif

    rv = glip_write_b(glip_ctx, 0, size_words * sizeof(uint16_t),
                      (uint8_t *)buf_be, &bytes_written,
                      0 /* timeout [ms]; 0 == never */);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    free(buf_be);
#endif

    if (rv == -ENOTCONN) {
        return rv;
    } else if (rv != 0) {
        return -1;
    }

    size_t words_written = bytes_written / sizeof(uint16_t);
    return words_written;
}

/**
 * Initialize GLIP for device communication
 */
static void init_glip(void)
{
    int rv;

    struct glip_option *glip_backend_options;
    size_t glip_backend_options_len;
    rv = glip_parse_option_string(a_glip_backend_options->sval[0],
                                  &glip_backend_options,
                                  &glip_backend_options_len);
    if (rv != 0) {
        fatal("Unable to parse GLIP backend options.\n");
    }

    dbg("Creating GLIP device context for backend %s\n",
        a_glip_backend->sval[0]);
    rv = glip_new(&glip_ctx, a_glip_backend->sval[0], glip_backend_options,
                  glip_backend_options_len, &glip_log_handler);
    if (rv < 0) {
        fatal("Unable to create new GLIP context (rv=%d).\n", rv);
    }

    if (glip_get_fifo_width(glip_ctx) != 2) {
        fatal("FIFO width of GLIP channel must be 16 bit, not %d bit.\n",
              glip_get_fifo_width(glip_ctx) * 8);
    }

    // route log messages to our log handler
    glip_set_log_priority(glip_ctx, cfg.log_level);
}

osd_result setup(void)
{
    a_hostctrl_ep = arg_str0(
        "e", "hostctrl", "<URL>",
        "ZeroMQ endpoint of the host controller "
        "(default: " DEFAULT_HOSTCTRL_EP ")");
    a_hostctrl_ep->sval[0] = DEFAULT_HOSTCTRL_EP;
    osd_tool_add_arg(a_hostctrl_ep);

    a_glip_backend =
        arg_str0("b", "glip-backend", "<name>", "GLIP backend name");
    a_glip_backend->sval[0] = GLIP_DEFAULT_BACKEND;
    osd_tool_add_arg(a_glip_backend);

    a_glip_backend_options =
        arg_str0("o", "glip-backend-options",
                 "<option1=value1,option2=value2,...>", "GLIP backend options");
    osd_tool_add_arg(a_glip_backend_options);

    return OSD_OK;
}

static osd_result packet_read_from_device(struct osd_packet **pkg)
{
    osd_result rv;
    ssize_t s_rv;

    // read packet size, which is transmitted as first word in a DTD
    uint16_t pkg_size_words;
    s_rv = device_read(&pkg_size_words, 1, 0);
    if (s_rv == -ENOTCONN) {
        return OSD_ERROR_NOT_CONNECTED;
    } else if (s_rv != 1) {
        err("Unable to read packet length from device (%zd).", s_rv);
        return OSD_ERROR_FAILURE;
    }

    rv = osd_packet_new(pkg, pkg_size_words);
    assert(OSD_SUCCEEDED(rv));

    // read packet data
    s_rv = device_read((*pkg)->data_raw, pkg_size_words, 0);
    if (s_rv == -ENOTCONN) {
        return OSD_ERROR_NOT_CONNECTED;
    } else if (s_rv == pkg_size_words) {
        err("Unable to read packet data from device (%zd).", s_rv);
        return OSD_ERROR_FAILURE;
    }

    return OSD_OK;
}

static osd_result packet_write_to_device(const struct osd_packet *pkg)
{
    ssize_t s_rv;

    uint16_t *pkg_dtd = (uint16_t *)pkg;
    size_t pkg_dtd_size_words = 1 /* len */ + pkg->data_size_words;

    s_rv = device_write(pkg_dtd, pkg_dtd_size_words, 0);
    if (s_rv == -ENOTCONN) {
        return OSD_ERROR_NOT_CONNECTED;
    } else if (s_rv != pkg->data_size_words) {
        return OSD_ERROR_FAILURE;
    }
    return OSD_OK;
}

int run(void)
{
    osd_result rv;
    int glip_rv;
    int exitcode;

    zsys_init();

    // initialize GLIP for device communication
    init_glip();

    // initialize OSD gateway
    struct osd_log_ctx *osd_log_ctx;
    rv = osd_log_new(&osd_log_ctx, cfg.log_level, &osd_log_handler);
    assert(OSD_SUCCEEDED(rv));

    // connect to device
    dbg("Connecting to device");
    glip_rv = glip_open(glip_ctx, 1);
    if (glip_rv < 0) {
        fatal("Unable to open connection to device (%d)", glip_rv);
        exitcode = 1;
        goto free_return;
    }
    dbg("Connected to device.");

    // create gateway
    struct osd_gateway_ctx *gateway_ctx;
    rv = osd_gateway_new(&gateway_ctx, osd_log_ctx, a_hostctrl_ep->sval[0],
                         DEVICE_SUBNET_ADDRESS, packet_read_from_device,
                         packet_write_to_device);
    assert(OSD_SUCCEEDED(rv));

    // connect to host controller
    dbg("Connecting to host controller");
    rv = osd_gateway_connect(gateway_ctx);
    if (OSD_FAILED(rv)) {
        fatal("Unable to connect to host controller at %s (rv=%d).\n",
              a_hostctrl_ep->sval[0], rv);
        exitcode = 1;
        goto free_return;
    }
    dbg("Connected to host controller at %s.", a_hostctrl_ep->sval[0]);

    while (!zsys_interrupted) {
        pause();
    }
    info("Shutdown signal received, cleaning up.");

    // disconnect from device
    glip_rv = glip_close(glip_ctx);
    if (glip_rv != 0) {
        err("Unable to close device connection. (%d)", rv);
    }

    // disconnect gateway from host controller and from device
    rv = osd_gateway_disconnect(gateway_ctx);
    if (OSD_FAILED(rv)) {
        fatal("Unable to disconnect from host controller (%d)", rv);
        exitcode = 1;
        goto free_return;
    }

    exitcode = 0;
free_return:
    osd_gateway_free(&gateway_ctx);
    glip_free(glip_ctx);
    osd_log_free(&osd_log_ctx);
    return exitcode;
}
