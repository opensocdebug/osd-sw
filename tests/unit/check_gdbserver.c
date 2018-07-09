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
    ver_checksum = validate_rsp_packet(packet_buffer, packet_len,
                                       &packet_data_len, packet_data);
    ck_assert_uint_eq(ver_checksum, 1);
    ck_assert_uint_eq(packet_data_len, 7);
    ck_assert_str_eq(packet_data, "swbreak");
}
END_TEST

START_TEST(test2_validate_rsp_packet)
{
    bool ver_checksum;
    char *packet_buffer = "swbre}]ak#c9";
    int packet_len = 11;
    char packet_data[1024];
    int packet_data_len;
    ver_checksum = validate_rsp_packet(packet_buffer, packet_len,
                                       &packet_data_len, packet_data);
    ck_assert_uint_eq(ver_checksum, 1);
    ck_assert_uint_eq(packet_data_len, 8);
    ck_assert_str_eq(packet_data, "swbre}ak");
}
END_TEST

START_TEST(test1_encode_rsp_packet)
{
    char packet_data[8] = "swbreak";
    int packet_checksum;
    int packet_data_len = 7;
    char packet_buffer[packet_data_len + 5];

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

    encode_rsp_packet(packet_data, packet_data_len, packet_buffer);
    ck_assert_str_eq(packet_buffer, "$swbre:ak#29");
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
    tcase_add_test(tc_testing, test1_encode_rsp_packet);
    tcase_add_test(tc_testing, test2_encode_rsp_packet);
    suite_add_tcase(s, tc_testing);

    return s;
}
