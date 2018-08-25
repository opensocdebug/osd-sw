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

/**Convert the read memory data into hexadecimal format
 *
 * Each byte of the memory is transmitted as a two-digit hexadecimal number.
 * It is converted into the this format to transfer these values to the GDB.
 *
 * @param mem_val       the read memory content
 * @param mem_len       the length of the memory content
 * @param mem_hex       the memory content in hexadecimal format
 */
void mem2hex(uint8_t *mem_val, size_t mem_len, uint8_t *mem_hex);

/**Convert the hexadecimal data into unsigned int format to be placed in memory
 *
 * Each byte of the memory is transmitted as a two-digit hexadecimal number.
 * It is converted into the unsigned int format to write the actual memory value
 *
 * @param mem_hex       the memory content in hexadecimal format
 * @param mem_len       the length of the memory content
 * @param mem_val       the memory content to be placed
 */
void hex2mem(uint8_t *mem_hex, size_t mem_len, uint8_t *mem_val);

#endif  // OSD_GDBSERVER_PRIVATE_H
