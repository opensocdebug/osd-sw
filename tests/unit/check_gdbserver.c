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

#define TEST_SUITE_NAME "check_gdbserver"

#include "testutil.h"

#include <../../src/libosd/gdbserver-private.h>
#include <osd/osd.h>
#include <osd/reg.h>

#include "mock_host_controller.h"

struct osd_gdbserver_ctx *gdbserver_ctx;
struct osd_log_ctx *log_ctx;

const unsigned int target_subnet_addr = 0;
unsigned int mock_hostmod_diaddr;
unsigned int mock_scm_diaddr;

START_TEST(test1_validate_rsp_packet)
{
    bool ver_checksum;
    char *packet_buffer = "swbreak#ef";
    int packet_len = 10;
    char packet_data[1024];
    int packet_data_len;

    // checks if the obtained packet-data and checksum are valid
    ver_checksum = validate_rsp_packet(packet_buffer, packet_len,
                                       &packet_data_len, packet_data);

    // ver_checksum = 1 indicates valid packet-data
    ck_assert_uint_eq(ver_checksum, 1);
    ck_assert_uint_eq(packet_data_len, 7);
    ck_assert_str_eq(packet_data, "swbreak");
}
END_TEST

START_TEST(test2_validate_rsp_packet)
{
    bool ver_checksum;
    char *packet_buffer = "swbre}]ak#c9";
    int packet_len = 12;
    char packet_data[1024];
    int packet_data_len;

    // checks if the obtained packet-data and checksum are valid
    ver_checksum = validate_rsp_packet(packet_buffer, packet_len,
                                       &packet_data_len, packet_data);

    // ver_checksum = 1 indicates valid packet-data
    ck_assert_uint_eq(ver_checksum, 1);
    ck_assert_uint_eq(packet_data_len, 8);
    ck_assert_str_eq(packet_data, "swbre}ak");
}
END_TEST

START_TEST(test3_validate_rsp_packet)
{
    bool ver_checksum;
    char *packet_buffer = "M23,4:ef0352ab#a4";
    int packet_len = 17;
    char packet_data[1024];
    int packet_data_len;

    // checks if the obtained packet-data and checksum are valid
    ver_checksum = validate_rsp_packet(packet_buffer, packet_len,
                                       &packet_data_len, packet_data);

    // ver_checksum = 1 indicates valid packet-data
    ck_assert_uint_eq(ver_checksum, 1);
    ck_assert_uint_eq(packet_data_len, 14);
    ck_assert_str_eq(packet_data, "M23,4:ef0352ab");
}
END_TEST

START_TEST(test4_validate_rsp_packet)
{
    bool ver_checksum;
    char *packet_buffer = "m23,4#a4";
    int packet_len = 8;
    char packet_data[1024];
    int packet_data_len;

    // checks if the obtained packet-data and checksum are valid
    ver_checksum = validate_rsp_packet(packet_buffer, packet_len,
                                       &packet_data_len, packet_data);

    // ver_checksum = 1 indicates valid packet-data
    // Here, the obtained checksum is not correct
    ck_assert_uint_eq(ver_checksum, 0);
}
END_TEST

START_TEST(test1_encode_rsp_packet)
{
    char packet_data[8] = "swbreak";
    int packet_checksum;
    int packet_data_len = 7;
    char packet_buffer[packet_data_len + 5];

    // encodes the packet-data into RSP packet format: $packet-data#checksum
    encode_rsp_packet(packet_data, packet_data_len, packet_buffer);
    ck_assert_str_eq(packet_buffer, "$swbreak#ef");
}
END_TEST

START_TEST(test2_encode_rsp_packet)
{
    char packet_data[9] = "swbre:ak";
    int packet_checksum;
    int packet_data_len = 8;
    char packet_buffer[packet_data_len + 5];

    // encodes the packet-data into RSP packet format: $packet-data#checksum
    encode_rsp_packet(packet_data, packet_data_len, packet_buffer);
    ck_assert_str_eq(packet_buffer, "$swbre:ak#29");
}
END_TEST

START_TEST(test1_mem_to_hex)
{
    // 2 bytes of data read from the memory
    size_t mem_read_result = 0xaf03;
    uint8_t *mem_content = (uint8_t *)&mem_read_result;
    size_t mem_len = 2;
    uint8_t *mem_content_val = malloc(2 * mem_len + 1);

    // conversion into hexadecimal format
    mem2hex(mem_content, mem_len, mem_content_val);

    ck_assert_str_eq(mem_content_val, "03af");

    free(mem_content_val);
}
END_TEST

START_TEST(test2_mem_to_hex)
{
    // 3 bytes of data read from the memory
    size_t mem_read_result = 0x45e03f;
    uint8_t *mem_content = (uint8_t *)&mem_read_result;
    size_t mem_len = 3;
    uint8_t *mem_content_val = malloc(2 * mem_len + 1);

    // conversion into hexadecimal format
    mem2hex(mem_content, mem_len, mem_content_val);

    ck_assert_str_eq(mem_content_val, "3fe045");

    free(mem_content_val);
}
END_TEST

START_TEST(test1_hex_to_mem)
{
    // 5 bytes of data to be written to the memory
    uint8_t *mem_content = "9f4a4034ef";
    size_t mem_len = 5;
    uint8_t *mem_write_result = malloc(mem_len + 1);

    // conversion from hexadecimal format
    hex2mem(mem_content, mem_len, mem_write_result);

    ck_assert_uint_eq(mem_write_result[0], 159);
    ck_assert_uint_eq(mem_write_result[1], 74);
    ck_assert_uint_eq(mem_write_result[2], 64);
    ck_assert_uint_eq(mem_write_result[3], 52);
    ck_assert_uint_eq(mem_write_result[4], 239);

    free(mem_write_result);
}
END_TEST

START_TEST(test2_hex_to_mem)
{
    // 1 byte of data to be written to the memory
    uint8_t *mem_content = "ef";
    size_t mem_len = 1;
    uint8_t *mem_write_result = malloc(mem_len + 1);

    // conversion from hexadecimal format
    hex2mem(mem_content, mem_len, mem_write_result);

    ck_assert_uint_eq(mem_write_result[0], 239);

    free(mem_write_result);
}
END_TEST

Suite *suite(void)
{
    Suite *s;
    TCase *tc_testing;

    s = suite_create(TEST_SUITE_NAME);

    tc_testing = tcase_create("Testing");

    tcase_add_test(tc_testing, test1_validate_rsp_packet);
    tcase_add_test(tc_testing, test2_validate_rsp_packet);
    tcase_add_test(tc_testing, test3_validate_rsp_packet);
    tcase_add_test(tc_testing, test4_validate_rsp_packet);
    tcase_add_test(tc_testing, test1_encode_rsp_packet);
    tcase_add_test(tc_testing, test2_encode_rsp_packet);
    tcase_add_test(tc_testing, test1_mem_to_hex);
    tcase_add_test(tc_testing, test2_mem_to_hex);
    tcase_add_test(tc_testing, test1_hex_to_mem);
    tcase_add_test(tc_testing, test2_hex_to_mem);
    suite_add_tcase(s, tc_testing);

    return s;
}
