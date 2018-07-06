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

START_TEST(test_validate_rsp_packet)
{
    osd_result rv;
    char packet_buffer[12] = "swbreak#ef";
    char *buf_p = "swbreak#ef";
    bool ver_checksum;
    int len;
    char buffer[1024];

    rv = validate_rsp_packet(buf_p, &ver_checksum, &len, buffer);
    ck_assert(OSD_SUCCEEDED(rv));
    ck_assert_uint_eq(len, 7);
    ck_assert_str_eq(buffer, "swbreak");
}
END_TEST

START_TEST(test_configure_rsp_packet)
{
    char buffer[8] = "swbreak";
    int packet_checksum;
    int len = 7;
    char packet_buffer[len + 5];

    configure_rsp_packet(buffer, len, packet_buffer);
    ck_assert_str_eq(packet_buffer, "$swbreak#ef");
}
END_TEST

Suite *suite(void)
{
    Suite *s;
    TCase *tc_testing;

    s = suite_create(TEST_SUITE_NAME);

    tc_testing = tcase_create("Testing");

    tcase_add_test(tc_testing, test_validate_rsp_packet);
    tcase_add_test(tc_testing, test_configure_rsp_packet);
    suite_add_tcase(s, tc_testing);

    return s;
}
