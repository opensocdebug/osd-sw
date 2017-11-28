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

#include <assert.h>
#include <errno.h>
#include <osd/osd.h>
#include <osd/packet.h>
#include <stdio.h>
#include <string.h>
#include "osd-private.h"

#define MACROSTR(k) #k

API_EXPORT
const uint16_t osd_packet_get_data_size_words_from_payload(
    const unsigned int size_payload)
{
    unsigned int s = size_payload + 3 /* dest, src, flags */;
    assert(s <= UINT16_MAX);
    return s;
}

API_EXPORT
osd_result osd_packet_new(struct osd_packet **packet, size_t data_size_words)
{
    ssize_t size = sizeof(uint16_t) * 1  // osd_packet.data_size_words
                   + sizeof(uint16_t) * data_size_words;  // osd_packet.data
    struct osd_packet *pkg = calloc(1, size);
    assert(pkg);

    pkg->data_size_words = data_size_words;

    *packet = pkg;

    return OSD_OK;
}

API_EXPORT
osd_result osd_packet_new_from_zframe(struct osd_packet **packet,
                                      const zframe_t *frame)
{
    assert(frame);
    uint16_t *data = (uint16_t *)zframe_data((zframe_t *)frame);
    size_t data_size_bytes = zframe_size((zframe_t *)frame);
    size_t data_size_words = data_size_bytes / sizeof(uint16_t);
    assert(data);
    assert(data_size_words >= 3);  // 3 header words

    osd_result rv = osd_packet_new(packet, data_size_words);
    assert(OSD_SUCCEEDED(rv));
    memcpy((*packet)->data_raw, data, data_size_bytes);

    return OSD_OK;
}

API_EXPORT
void osd_packet_free(struct osd_packet **packet_p)
{
    assert(packet_p);
    struct osd_packet *packet = *packet_p;

    free(packet);
    *packet_p = NULL;
}

API_EXPORT
unsigned int osd_packet_get_dest(const struct osd_packet *packet)
{
    assert((packet->data_size_words >= 3) &&
           "The packet must be large enough for 3 header words.");

    return (packet->data.dest >> DP_HEADER_DEST_SHIFT) & DP_HEADER_DEST_MASK;
}

API_EXPORT
unsigned int osd_packet_get_src(const struct osd_packet *packet)
{
    assert((packet->data_size_words >= 3) &&
           "The packet must be large enough for 3 header words.");

    return (packet->data.src >> DP_HEADER_SRC_SHIFT) & DP_HEADER_SRC_MASK;
}

API_EXPORT
unsigned int osd_packet_get_type(const struct osd_packet *packet)
{
    assert((packet->data_size_words >= 3) &&
           "The packet must be large enough for 3 header words.");

    return (packet->data.flags >> DP_HEADER_TYPE_SHIFT) & DP_HEADER_TYPE_MASK;
}

API_EXPORT
unsigned int osd_packet_get_type_sub(const struct osd_packet *packet)
{
    assert((packet->data_size_words >= 3) &&
           "The packet must be large enough for 3 header words.");

    return (packet->data.flags >> DP_HEADER_TYPE_SUB_SHIFT) &
           DP_HEADER_TYPE_SUB_MASK;
}

API_EXPORT
osd_result osd_packet_set_header(struct osd_packet *packet,
                                 const unsigned int dest,
                                 const unsigned int src,
                                 const enum osd_packet_type type,
                                 const unsigned int type_sub)
{
    assert((packet->data_size_words >= 3) &&
           "The packet must be large enough for 3 header words.");

    packet->data.dest = 0x0000;
    packet->data.src = 0x0000;
    packet->data.flags = 0x0000;

    // DEST
    assert((dest & DP_HEADER_DEST_MASK) == dest);  // overflow detection
    packet->data.dest |= (dest & DP_HEADER_DEST_MASK) << DP_HEADER_DEST_SHIFT;

    // SRC
    assert((src & DP_HEADER_SRC_MASK) == src);
    packet->data.src |= (src & DP_HEADER_SRC_MASK) << DP_HEADER_SRC_SHIFT;

    // FLAGS.TYPE
    assert((type & DP_HEADER_TYPE_MASK) == type);
    packet->data.flags |= (type & DP_HEADER_TYPE_MASK) << DP_HEADER_TYPE_SHIFT;

    // FLAGS.TYPE_SUB
    assert((type_sub & DP_HEADER_TYPE_SUB_MASK) == type_sub);
    packet->data.flags |= (type_sub & DP_HEADER_TYPE_SUB_MASK)
                          << DP_HEADER_TYPE_SUB_SHIFT;

    return OSD_OK;
}

API_EXPORT
size_t osd_packet_sizeof(const struct osd_packet *packet)
{
    return packet->data_size_words * sizeof(uint16_t);
}

static void sprintf_append(char **strp, const char *fmt, ...)
{
    va_list ap;
    char *append_str;

    va_start(ap, fmt);
    int append_size = vasprintf(&append_str, fmt, ap);
    assert(append_size != 0);
    va_end(ap);

    if (*strp == NULL) {
        *strp = append_str;
    } else {
        size_t cur_size = strlen(*strp);
        *strp = realloc(*strp, cur_size + append_size + 1 /* \0 */);
        strncat(*strp, append_str, append_size);
        free(append_str);
    }
}

API_EXPORT
void osd_packet_to_string(const struct osd_packet *packet, char **str)
{
    static char *osd_packet_type_name[] = {
        MACROSTR(OSD_PACKET_TYPE_REG), MACROSTR(OSD_PACKET_TYPE_PLAIN),
        MACROSTR(OSD_PACKET_TYPE_EVENT), MACROSTR(OSD_PACKET_TYPE_RES),
    };

    sprintf_append(str, "Packet of %u data words:\n", packet->data_size_words);
    if (packet->data_size_words >= 3) {
        sprintf_append(str,
                       "DEST = %u, SRC = %u, TYPE = %u (%s), TYPE_SUB = %u\n",
                       osd_packet_get_dest(packet), osd_packet_get_src(packet),
                       osd_packet_get_type(packet),
                       osd_packet_type_name[osd_packet_get_type(packet)],
                       osd_packet_get_type_sub(packet));
    }
    sprintf_append(str, "Packet data (including header):\n");
    for (uint16_t i = 0; i < packet->data_size_words; i++) {
        sprintf_append(str, "  0x%04x\n", packet->data_raw[i]);
    }
}

API_EXPORT
void osd_packet_dump(const struct osd_packet *packet, FILE *fd)
{
    char *str = NULL;
    osd_packet_to_string(packet, &str);
    fprintf(fd, "%s", str);
    free(str);
}

/**
 * Log a debug message with the packet in human-readable form
 *
 * @param packet packet to log
 * @param log_ctx the log context to write to
 */
API_EXPORT
void osd_packet_log(const struct osd_packet *packet,
                    struct osd_log_ctx *log_ctx)
{
    char *str = NULL;
    osd_packet_to_string(packet, &str);
    dbg(log_ctx, "%s", str);
    free(str);
}
