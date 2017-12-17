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

#define TEST_SUITE_NAME "check_packet"

#include "testutil.h"

#include <osd/osd.h>
#include <osd/packet.h>

START_TEST(test_packet_header_extractparts)
{
    osd_result rv;
    struct osd_packet *pkg;
    rv = osd_packet_new(&pkg, osd_packet_get_data_size_words_from_payload(0));
    ck_assert_int_eq(rv, OSD_OK);

    pkg->data.dest = 0xa5ab;
    pkg->data.src = 0x1234;
    pkg->data.flags = 0x5557;
    ck_assert_int_eq(osd_packet_get_dest(pkg), 0xa5ab);
    ck_assert_int_eq(osd_packet_get_src(pkg), 0x1234);
    ck_assert_int_eq(osd_packet_get_type(pkg), 0x1);
    ck_assert_int_eq(osd_packet_get_type_sub(pkg), 0x5);

    osd_packet_free(&pkg);
    ck_assert_ptr_eq(pkg, NULL);
}
END_TEST

START_TEST(test_packet_header_set)
{
    osd_result rv;
    struct osd_packet *pkg;
    rv = osd_packet_new(&pkg, osd_packet_get_data_size_words_from_payload(0));
    ck_assert_int_eq(rv, OSD_OK);

    osd_packet_set_header(pkg, 0x1ab, 0x157, OSD_PACKET_TYPE_PLAIN, 0x5);

    ck_assert_int_eq(pkg->data.dest, 0x1ab);
    ck_assert_int_eq(pkg->data.src, 0x157);
    ck_assert_int_eq(pkg->data.flags, 0x5400);

    osd_packet_free(&pkg);
    ck_assert_ptr_eq(pkg, NULL);
}
END_TEST

Suite *suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create(TEST_SUITE_NAME);

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_packet_header_set);
    tcase_add_test(tc_core, test_packet_header_extractparts);
    suite_add_tcase(s, tc_core);

    return s;
}
