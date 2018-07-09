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
 * It calculates the checksum of the obtained packet-data and compares it with
 * the obtained checksum. This function ensures that the received packet-data
 * is valid and uncorrupted.
 * Refer https://sourceware.org/gdb/onlinedocs/gdb/Overview.html#Overview
 *
 * @param packet_buffer    the pointer to the received packet buffer data
 * @param packet_len       the length of th packet buffer
 * @param packet_data_len  the length  of the packet-data
 * @param packet_data      the packet-data received
 *
 */
bool validate_rsp_packet(char *packet_buffer, int packet_len,
                         int *packet_data_len, char *packet_data);

/**
 * Set packet-data into the RSP format: $packet-data#checksum
 * 
 * Refer https://sourceware.org/gdb/onlinedocs/gdb/Overview.html#Overview
 *
 * @param packet_data        the packet-data buffer
 * @param packet_data_len    the length of the packet-data
 * @param packet_buffer      the packet buffer in RSP format
 */
osd_result encode_rsp_packet(char *packet_data, int packet_data_len,
                             char *packet_buffer);

#endif  // OSD_GDBSERVER_PRIVATE_H
