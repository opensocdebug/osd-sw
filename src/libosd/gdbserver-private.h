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

#ifndef OSD_GDBSERVER_PRIVATE_H
#define OSD_GDBSERVER_PRIVATE_H

#include <osd/hostmod.h>
#include <osd/osd.h>

#include <stdlib.h>

/**
 * Return packet-data from the received data buffer by validating the checksum
 *
 * @param buf_p        the pointer to the received packet buffer data
 * @param ver_checksum '1' indicates valid checksum
 * @param len          the length  of the packet-data
 * @param buffer       the packet-data received
 *
 */
osd_result validate_rsp_packet(char *buf_p, bool *ver_checksum, int *len, 
                               char *buffer);
                               

/**
 * Set packet-data into the RSP format: $packet-data#checksum
 *
 * @param buffer        the packet-data buffer
 * @param len           the length of the packet-data
 * @param packet_buffer the packet buffer in RSP format
 */
osd_result configure_rsp_packet(char *buffer, int len, char *packet_buffer);
   
#endif  // OSD_GDBSERVER_PRIVATE_H
