/* Copyright 2017-2018 The Open SoC Debug Project
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

#define TEST_SUITE_NAME "check_cl_cdm"

#include "mock_hostmod.h"
#include "testutil.h"

#include <osd/cl_cdm.h>
#include <osd/osd.h>
#include <osd/reg.h>

struct osd_hostmod_ctx *hostmod_ctx;
struct osd_log_ctx *log_ctx;

// DI address of the CDM module to be tested; chosen arbitrarily
const unsigned int cdm_diaddr = 14;

/**
 * Test fixture: setup (called before each tests)
 */
void setup(void) { mock_hostmod_setup(); }

/**
 * Test fixture: setup (called after each test)
 */
void teardown(void) { mock_hostmod_teardown(); }


START_TEST(test_get_desc)
{
    osd_result rv;

    mock_hostmod_expect_mod_describe(cdm_diaddr, OSD_MODULE_VENDOR_OSD,
                                     OSD_MODULE_TYPE_STD_CDM, 0);

    mock_hostmod_expect_reg_read16(1, cdm_diaddr, OSD_REG_CDM_CORE_CTRL,
                                   OSD_OK);

    mock_hostmod_expect_reg_read16(5, cdm_diaddr, OSD_REG_CDM_CORE_REG_UPPER,
                                   OSD_OK);

    struct osd_cdm_desc cdm_desc;
    rv = osd_cl_cdm_get_desc(mock_hostmod_get_ctx(), cdm_diaddr, &cdm_desc);
    ck_assert_int_eq(rv, OSD_OK);
    ck_assert_uint_eq(cdm_desc.core_ctrl, 1);
    ck_assert_uint_eq(cdm_desc.core_reg_upper, 5);
    ck_assert_uint_eq(cdm_desc.di_addr, cdm_diaddr);
}
END_TEST

START_TEST(test_get_desc_wrong_module)
{
    osd_result rv;

    mock_hostmod_expect_mod_describe(cdm_diaddr, OSD_MODULE_VENDOR_OSD,
                                     OSD_MODULE_TYPE_STD_CTM, 0);

    struct osd_cdm_desc cdm_desc;
    rv = osd_cl_cdm_get_desc(mock_hostmod_get_ctx(), cdm_diaddr, &cdm_desc);
    ck_assert_int_eq(rv, OSD_ERROR_WRONG_MODULE);
}
END_TEST

static void event_handler(void *arg, const struct osd_cdm_desc *cdm_desc,
                          const struct osd_cdm_event *event)
{
    ck_assert(arg);
    ck_assert(cdm_desc);
    ck_assert(event);

    // in this test we pass the expected event as callback argument
    struct osd_cdm_event *exp_event = arg;

    ck_assert_uint_eq(exp_event->stall, event->stall);
    
}

START_TEST(test_handle_event)
{
    osd_result rv;

    struct osd_cdm_desc cdm_desc;
    cdm_desc.di_addr = 2;
    cdm_desc.core_ctrl = 16;
    cdm_desc.core_reg_upper = 1;

    struct osd_cdm_event_handler ev_handler;
    ev_handler.cb_fn = event_handler;
    ev_handler.cdm_desc = &cdm_desc;

    struct osd_packet *pkg_trace;
    osd_packet_new(&pkg_trace, osd_packet_sizeconv_payload2data(1));
    rv = osd_packet_set_header(pkg_trace, 1, 2, OSD_PACKET_TYPE_EVENT, 0);
    ck_assert_int_eq(rv, OSD_OK);
    pkg_trace->data.payload[0] = 1; // stall

    struct osd_cdm_event exp_event;
    exp_event.stall = 1;
    ev_handler.cb_arg = (void*)&exp_event;
    rv = osd_cl_cdm_handle_event((void*)&ev_handler, pkg_trace);
    ck_assert_int_eq(rv, OSD_OK);
}
END_TEST

Suite *suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create(TEST_SUITE_NAME);

    tc_core = tcase_create("Core Functionality");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_get_desc);
    tcase_add_test(tc_core, test_get_desc_wrong_module);
    tcase_add_test(tc_core, test_handle_event);
   
    suite_add_tcase(s, tc_core);

    return s;
}
