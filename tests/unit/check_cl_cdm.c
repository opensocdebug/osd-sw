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

    mock_hostmod_expect_reg_read16(32, cdm_diaddr, OSD_REG_CDM_CORE_DATA_WIDTH,
                                   OSD_OK);
    
    struct osd_cdm_desc cdm_desc;
    rv = osd_cl_cdm_get_desc(mock_hostmod_get_ctx(), cdm_diaddr, &cdm_desc);
    ck_assert_int_eq(rv, OSD_OK);
    ck_assert_uint_eq(cdm_desc.core_ctrl, 1);
    ck_assert_uint_eq(cdm_desc.core_reg_upper, 5);
    ck_assert_uint_eq(cdm_desc.core_data_width, 32);
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

#define STALL_BIT 0

START_TEST(test_handle_event)
{
    osd_result rv;

    struct osd_cdm_desc cdm_desc;
    cdm_desc.di_addr = 2;
    cdm_desc.core_ctrl = 16;
    cdm_desc.core_reg_upper = 1;
    cdm_desc.core_data_width = 32;

    struct osd_cdm_event_handler ev_handler;
    ev_handler.cb_fn = event_handler;
    ev_handler.cdm_desc = &cdm_desc;

    struct osd_packet *pkg_trace;
    osd_packet_new(&pkg_trace, osd_packet_sizeconv_payload2data(1));
    rv = osd_packet_set_header(pkg_trace, 1, 2, OSD_PACKET_TYPE_EVENT, 0);
    ck_assert_int_eq(rv, OSD_OK);
    pkg_trace->data.payload[0] = BIT(STALL_BIT); // stall (1 << 0)

    struct osd_cdm_event exp_event;
    exp_event.stall = BIT(STALL_BIT);
    ev_handler.cb_arg = (void*)&exp_event;
    rv = osd_cl_cdm_handle_event((void*)&ev_handler, pkg_trace);
    ck_assert_int_eq(rv, OSD_OK);
}
END_TEST

struct osd_cdm_desc get_cdm_desc(void)
{
    struct osd_cdm_desc cdm_desc = { 0 };
    cdm_desc.di_addr = cdm_diaddr;
    cdm_desc.core_ctrl = 1;
    cdm_desc.core_reg_upper = 0;
    cdm_desc.core_data_width = 32;

    return cdm_desc;
}

START_TEST(test_cpu_read_register_test1)
{
    osd_result rv;
    struct osd_cdm_desc cdm_desc = get_cdm_desc();
    
    uint32_t reg_read_result;
    uint16_t reg_addr = 0x0007;
    uint16_t cdm_reg_addr = 0x8000 + (reg_addr & 0x7fff);
    mock_hostmod_expect_reg_read32(0xabcddead, cdm_diaddr, cdm_reg_addr, OSD_OK);
                                   
    rv = cl_cdm_cpureg_read(mock_hostmod_get_ctx(), &cdm_desc, &reg_read_result, reg_addr, 0); 
    ck_assert_int_eq(rv, OSD_OK);
    ck_assert_uint_eq(reg_read_result, 0xabcddead);
}
END_TEST

START_TEST(test_cpu_read_register_test2)
{
    osd_result rv;
    struct osd_cdm_desc cdm_desc = get_cdm_desc();
    
    uint32_t reg_read_result;
    uint16_t reg_addr = 0xf007;
    uint16_t reg_addr_upper = reg_addr >> 15;
    uint16_t core_upper = cdm_desc.core_reg_upper;
    
    if (core_upper != reg_addr_upper) {
        mock_hostmod_expect_reg_write16(reg_addr_upper, cdm_diaddr, OSD_REG_CDM_CORE_REG_UPPER, 
                                        OSD_OK);
    }
    
    uint16_t cdm_reg_addr = 0x8000 + (reg_addr & 0x7fff);
    mock_hostmod_expect_reg_read32(0xabcddead, cdm_diaddr, cdm_reg_addr, OSD_OK);
                                   
    rv = cl_cdm_cpureg_read(mock_hostmod_get_ctx(), &cdm_desc, &reg_read_result, reg_addr, 0); 
    ck_assert_int_eq(rv, OSD_OK);
    ck_assert_uint_eq(reg_read_result, 0xabcddead);
}
END_TEST

START_TEST(test_cpu_write_register_test1)
{
    osd_result rv;
    struct osd_cdm_desc cdm_desc = get_cdm_desc();
    
    uint16_t reg_addr = 0x0007;
    uint32_t reg_val = 0xabcddead;
    uint16_t cdm_reg_addr = 0x8000 + (reg_addr & 0x7fff);
    mock_hostmod_expect_reg_write32(reg_val, cdm_diaddr, cdm_reg_addr, OSD_OK);
                                   
    rv = cl_cdm_cpureg_write(mock_hostmod_get_ctx(), &cdm_desc, &reg_val, reg_addr, 0);
    ck_assert_int_eq(rv, OSD_OK);
}
END_TEST

START_TEST(test_cpu_write_register_test2)
{
    osd_result rv;
    struct osd_cdm_desc cdm_desc = get_cdm_desc();
    
    uint16_t reg_addr = 0xf007;
    uint32_t reg_val = 0xabcddead;
    uint16_t reg_addr_upper = reg_addr >> 15;
    uint16_t core_upper = cdm_desc.core_reg_upper;
    
    if (core_upper != reg_addr_upper) {
        mock_hostmod_expect_reg_write16(reg_addr_upper, cdm_diaddr, OSD_REG_CDM_CORE_REG_UPPER, 
                                        OSD_OK);
    }

    uint16_t cdm_reg_addr = 0x8000 + (reg_addr & 0x7fff);
    mock_hostmod_expect_reg_write32(reg_val, cdm_diaddr, cdm_reg_addr, OSD_OK);
 
    rv = cl_cdm_cpureg_write(mock_hostmod_get_ctx(), &cdm_desc, &reg_val, reg_addr, 0);
    ck_assert_int_eq(rv, OSD_OK);
}
END_TEST

Suite *suite(void)
{
    Suite *s;
    TCase *tc_core, *tc_rw;
    
    s = suite_create(TEST_SUITE_NAME);

    tc_core = tcase_create("Core Functionality");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_get_desc);
    tcase_add_test(tc_core, test_get_desc_wrong_module);
    tcase_add_test(tc_core, test_handle_event);
    suite_add_tcase(s, tc_core);

    tc_rw = tcase_create("CPU Register read/write");
    tcase_add_checked_fixture(tc_rw, setup, teardown);
    tcase_add_test(tc_rw, test_cpu_read_register_test1);
    tcase_add_test(tc_rw, test_cpu_read_register_test2);
    tcase_add_test(tc_rw, test_cpu_write_register_test1);
    tcase_add_test(tc_rw, test_cpu_write_register_test2);
    suite_add_tcase(s, tc_rw);

    return s;
}
