# Copyright 2017-2019 The Open SoC Debug Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import logging
import os
import sys
import time

from collections.abc import MutableSequence
from enum import IntEnum, unique

cimport cosd
cimport cutil
from cutil cimport va_list, vasprintf, Py_AddPendingCall
from libc.stdint cimport uint16_t
from cpython.mem cimport PyMem_Malloc, PyMem_Free
from libc.stdlib cimport malloc, free
from libc.stdio cimport FILE, fopen, fclose
from libc.errno cimport errno
from libc.string cimport strerror, memmove
from posix.time cimport timespec

if sys.version_info < (3, 7):
    # We receive callbacks from various threads in libosd and need to make sure
    # that the GIL is initialized on each of these threads. Calling
    # PyEval_InitThreads() isn't enough, unfortunately. As workaround, we import
    # cython.parallel, which contains code to always initialize the GIL at the
    # right time.
    # https://github.com/cython/cython/issues/2205#issuecomment-398511174
    # https://github.com/opensocdebug/osd-sw/issues/37
    cimport cython.parallel


def osd_library_version():
     cdef const cosd.osd_version* vers = cosd.osd_version_get()
     version_info = {}
     version_info['major'] = vers.major
     version_info['minor'] = vers.minor
     version_info['micro'] = vers.micro
     version_info['suffix'] = vers.suffix
     return version_info


cdef struct log_item:
    int priority
    const char *file
    int line
    const char *fn
    char *msg
    size_t msg_len

cdef void log_cb(cosd.osd_log_ctx *ctx, int priority, const char *file,
                 int line, const char *fn, const char *format,
                 cosd.va_list args) nogil:
    """
    Process a log entry without holding the GIL

    This function is called from libosd when a log entry should be written.
    It does not hold the GIL, hence no Python objects can be accessed.
    """
    # Get log message as string
    cdef char* msg = NULL
    msg_len = vasprintf(&msg, format, args)
    if msg_len == -1:
        # out of memory: skip log entry
        return

    cdef log_item *item = <log_item*>malloc(sizeof(log_item))
    item.priority = priority
    item.file = file
    item.line = line
    item.fn = fn
    item.msg = msg
    item.msg_len = msg_len

    # Queue callback to process item with GIL held
    Py_AddPendingCall(log_cb_withgil, <void*>item)

cdef int log_cb_withgil(void* item_void) with gil:
    """
    Process a log entry with GIL held

    This function is called from the Python main thread with the GIL held.
    The only argument is the log entry to be processed.

    In this function all Python data structures can be accessed (since the GIL
    is held).
    """
    cdef log_item *item = <log_item*> item_void
    if not item:
        return 0

    try:
        logger = logging.getLogger(__name__)
    except:
        # In the shutdown phase this function is called, but the logging
        # system is already destroyed. Discard messages.
        free(item)
        return 0

    # handle log entry
    u_file = item.file.decode('UTF-8')
    u_fn = item.fn.decode('UTF-8')
    try:
        u_msg = item.msg[:item.msg_len].decode('UTF-8')
    finally:
        free(item.msg)

    # create log record and pass it to the Python logger
    record = logging.LogRecord(name = __name__,
                               level = loglevel_syslog2py(item.priority),
                               pathname = u_file,
                               lineno = item.line,
                               func = u_fn,
                               msg = u_msg,
                               args = '',
                               exc_info = None)

    logger.handle(record)

    free(item)

    return 0

cdef loglevel_py2syslog(py_level):
    """
    Convert Python logging severity levels to syslog levels as defined in
    syslog.h
    """
    if py_level == logging.CRITICAL:
        return 2 # LOG_CRIT
    elif py_level == logging.ERROR:
        return 3 # LOG_ERR
    elif py_level == logging.WARNING:
        return 4 # LOG_WARNING
    elif py_level == logging.INFO:
        return 6 # LOG_INFO
    elif py_level == logging.DEBUG:
        return 7 # LOG_DEBUG
    elif py_level == logging.NOTSET:
        return 0 # LOG_EMERG
    else:
        raise Exception("Unknown loglevel " + str(py_level))


cdef loglevel_syslog2py(syslog_level):
    """
    Convert syslog log severity levels, as defined in syslog.h, to Python ones
    """
    if syslog_level <= 2: # LOG_EMERG, LOG_ALERT, LOG_CRIT
        return logging.CRITICAL
    elif syslog_level == 3: # LOG_ERR
        return logging.ERROR
    elif syslog_level == 4: # LOG_WARNING
        return logging.WARNING
    elif syslog_level <= 6: # LOG_NOTICE, LOG_INFO
        return logging.INFO
    elif syslog_level == 7: # LOG_DEBUG
        return logging.DEBUG
    else:
        raise Exception("Unknown loglevel " + str(syslog_level))


@unique
class Result(IntEnum):
    """Wrapper of osd_result and OSD_ERROR_* defines"""

    OK = 0
    FAILURE = -1
    DEVICE_ERROR = -2
    DEVICE_INVALID_DATA = -3
    COM = -4
    TIMEDOUT = -5
    NOT_CONNECTED = -6
    PARTIAL_RESULT = -7
    ABORTED = -8
    CONNECTION_FAILED = -9
    OOM = -11
    FILE = -12
    MEM_VERIFY_FAILED = -13
    WRONG_MODULE = -14

    def __str__(self):
        # String representations from osd.h, keep in sync!
        _error_strings = {
            self.OK: 'The operation was successful',
            self.FAILURE: 'Generic (unknown) failure',
            self.DEVICE_ERROR: 'debug system returned a failure',
            self.DEVICE_INVALID_DATA: 'received invalid or malformed data from device',
            self.COM: 'failed to communicate with device',
            self.TIMEDOUT: 'operation timed out',
            self.NOT_CONNECTED: 'not connected to the device',
            self.PARTIAL_RESULT: 'this is a partial result, not all requested data was obtained',
            self.ABORTED: 'operation aborted',
            self.CONNECTION_FAILED: 'connection failed',
            self.OOM: 'Out of memory',
            self.FILE: 'file operation failed',
            self.MEM_VERIFY_FAILED: 'memory verification failed ',
            self.WRONG_MODULE: 'unexpected module type'
        }

        try:
            error_str = _error_strings[self.value]
        except:
            error_str = 'unknown error code'

        return '{0} ({1})'.format(error_str, self.value)

    def __init__(self, code):
        self.code = code

class OsdErrorException(Exception):
    def __init__(self, result):
        self.result = result

    def __str__(self):
        return str(self.result)

cdef check_osd_result(rv):
    if rv != 0:
        raise OsdErrorException(Result(rv))


cdef class Log:
    cdef cosd.osd_log_ctx* _cself

    def __cinit__(self):
        logger = logging.getLogger(__name__)
        py_loglevel = logger.getEffectiveLevel()
        syslog_loglevel = loglevel_py2syslog(py_loglevel)

        cosd.osd_log_new(&self._cself, syslog_loglevel, log_cb)
        if self._cself is NULL:
            raise MemoryError()

    def __dealloc__(self):
        if self._cself is not NULL:
            cosd.osd_log_free(&self._cself)

class PacketPayloadView(MutableSequence):
    # Keep this constant in sync with packet.c. (Or read it at runtime, which we
    # avoid here for performance reasons.)
    PACKET_HEADER_WORD_CNT = 3

    def __init__(self, packet):
        self.packet = packet

    def __len__(self):
        return len(self.packet) - self.PACKET_HEADER_WORD_CNT

    def __getitem__(self, index):
        return self.packet[index + self.PACKET_HEADER_WORD_CNT]

    def __setitem__(self, index, value):
        self.packet[index + self.PACKET_HEADER_WORD_CNT] = value

    def __delitem__(self, index):
        del self.packet[index + self.PACKET_HEADER_WORD_CNT]

    def insert(self, index, value):
        self.packet.insert(index + self.PACKET_HEADER_WORD_CNT, value)

cdef class Packet(PacketType, MutableSequence):
    pass

# It would be nice to directly inherit from abc.collections.MutableType, but
# that's not possible currently in Cython 0.28 as we can only inherit from
# types, not from Python objects.
#
cdef class PacketType:
    # These constants need to be declared in PacketType, not in Packet, to
    # avoid a segfault when importing the osd module in (at least) Cython 0.29/
    # Python 3.7.
    TYPE_REG = cosd.OSD_PACKET_TYPE_REG
    TYPE_RES1 = cosd.OSD_PACKET_TYPE_RES1
    TYPE_EVENT = cosd.OSD_PACKET_TYPE_EVENT
    TYPE_RES2 = cosd.OSD_PACKET_TYPE_RES2

    cdef cosd.osd_packet* _cself
    cdef bint _cself_owner

    def __cinit__(self):
        self._cself_owner = False

    def __init__(self):
        if self._cself is NULL:
            self._cself_owner = True
            cosd.osd_packet_new(&self._cself,
                                cosd.osd_packet_sizeconv_payload2data(0))
            if self._cself is NULL:
                raise MemoryError()

    def __dealloc__(self):
        if self._cself is not NULL and self._cself_owner:
            cosd.osd_packet_free(&self._cself)
            self._cself_owner = False

    @staticmethod
    cdef Packet _from_c_ptr(cosd.osd_packet *_c_packet, bint owner=True):
        """ Factory: create Packet from osd_packet* """
        # Call __new__ to bypass __init__ and set _cself manually
        cdef Packet wrapper = Packet.__new__(Packet)
        wrapper._cself = _c_packet
        wrapper._cself_owner = owner
        return wrapper

    # Container API for sequences (lists) with Cython extensions
    # https://docs.python.org/3/reference/datamodel.html#emulating-container-types
    # https://cython.readthedocs.io/en/latest/src/userguide/special_methods.html#sequences-and-mappings
    # https://docs.python.org/3/library/stdtypes.html#mutable-sequence-types
    # https://docs.python.org/3/library/stdtypes.html#list

    def __len__(self):
        return self._cself.data_size_words

    def __getitem__(self, index):
        if index >= self._cself.data_size_words:
            raise IndexError
        return self._cself.data_raw[index]

    def __setitem__(self, index, value):
        if index >= self._cself.data_size_words:
            raise IndexError
        self._cself.data_raw[index] = value

    def __delitem__(self, index):
        if len(self) <= 3:
            raise ValueError("Packet must have at least 3 (header) words.")

        memmove(&self._cself.data_raw[index], &self._cself.data_raw[index + 1],
                (len(self) - index) * sizeof(uint16_t))
        rv = cosd.osd_packet_realloc(&self._cself,
                                     self._cself.data_size_words - 1)
        check_osd_result(rv)

    def __eq__(self, other):
        if not isinstance(other, Packet):
            return NotImplemented

        if not len(self) == len(other):
            return False

        for i in range(len(self)):
            if self[i] != other[i]:
                return False

        return True

    def insert(self, index, value):
        """ insert value before index """

        old_size_words = self._cself.data_size_words

        # enlarge packet by one word
        cosd.osd_packet_realloc(&self._cself, old_size_words + 1)

        # move existing elements out of the way (if any)
        if index < old_size_words:
            memmove(&self._cself.data_raw[index + 1],
                    &self._cself.data_raw[index], (old_size_words - index) * sizeof(uint16_t))

        # set value
        self._cself.data_raw[index] = value

    @property
    def src(self):
        return cosd.osd_packet_get_src(self._cself)

    @property
    def dest(self):
        return cosd.osd_packet_get_dest(self._cself)

    @property
    def type(self):
        return cosd.osd_packet_get_type(self._cself)

    @property
    def type_sub(self):
        return cosd.osd_packet_get_type_sub(self._cself)

    def set_header(self, dest, src, type, type_sub):
        cosd.osd_packet_set_header(self._cself, dest, src, type, type_sub)

    @property
    def payload(self):
        return PacketPayloadView(self)

    def __str__(self):
        cdef char* c_str = NULL
        cosd.osd_packet_to_string(self._cself, &c_str)

        try:
            py_u_str = c_str.decode('UTF-8')
        finally:
            free(c_str)

        return py_u_str


cdef class Hostmod:
    cdef cosd.osd_hostmod_ctx* _cself

    # Event handler must be bound to the class to have it in the same GC scope
    # as _c_event_handler_cb(). Otherwise, the event handler could be garbage-
    # collected before the event handler is called.
    cdef readonly object _event_handler

    def __cinit__(self, Log log, host_controller_address,
                  event_handler = None, *args, **kwargs):
        """ Construct a new Hostmod instance

        Args:
            log: logger
            host_controller_address (str): Address of the host controller to
                connect to.
            event_handler (object): Event handler callback function, called
                whenever an event is received. Set to None to receive event
                packets with Hostmod.event_receive() instead.

                The callback function receives a Packet as only argument.
                The return value is ignored.
                Any exception thrown in the callback function will indicate an
                unsuccessful callback execution and passed on to libosd.
        """
        # *args and **kwargs enables child classes to have more constructor
        # parameters
        py_byte_string = host_controller_address.encode('UTF-8')
        cdef char* c_host_controller_address = py_byte_string

        if event_handler == None:
            rv  = cosd.osd_hostmod_new(&self._cself, log._cself,
                                       c_host_controller_address,
                                       NULL, NULL)
        else:
            self._event_handler = event_handler
            rv  = cosd.osd_hostmod_new(&self._cself, log._cself,
                                       c_host_controller_address,
                                       <cosd.osd_hostmod_event_handler_fn>Hostmod._c_event_handler_cb,
                                       <void*>self)
        check_osd_result(rv)
        if self._cself is NULL:
            raise MemoryError()

    def __dealloc__(self):
        if self._cself is NULL:
            return

        # don't call python methods in here, as they might be partially
        # destructed already
        if cosd.osd_hostmod_is_connected(self._cself):
            rv = cosd.osd_hostmod_disconnect(self._cself)
            check_osd_result(rv)

        cosd.osd_hostmod_free(&self._cself)

    @staticmethod
    cdef cosd.osd_result _c_event_handler_cb(void *self_void, cosd.osd_packet *packet) with gil:
        try:
            self = <object>self_void
            py_packet = Packet._from_c_ptr(packet, owner=True)

            self._event_handler(py_packet)
            return Result.OK
        except:
            return Result.FAILURE

    def connect(self):
        rv = cosd.osd_hostmod_connect(self._cself)
        check_osd_result(rv)

    def disconnect(self):
        rv = cosd.osd_hostmod_disconnect(self._cself)
        check_osd_result(rv)

    def is_connected(self):
        return cosd.osd_hostmod_is_connected(self._cself)

    @property
    def diaddr(self):
        if not self.is_connected:
            return None

        return cosd.osd_hostmod_get_diaddr(self._cself)

    def reg_read(self, diaddr, reg_addr, reg_size_bit = 16, flags = 0):
        cdef uint16_t outvalue
        if reg_size_bit != 16:
            raise Exception("XXX: Extend to support other sizes than 16 bit registers")

        rv = cosd.osd_hostmod_reg_read(self._cself, &outvalue, diaddr, reg_addr,
                                       reg_size_bit, flags)
        check_osd_result(rv)

        return outvalue

    def reg_write(self, data, diaddr, reg_addr, reg_size_bit = 16, flags = 0):
        if reg_size_bit != 16:
            raise Exception("XXX: Extend to support other sizes than 16 bit registers")

        cdef uint16_t c_data = data

        rv = cosd.osd_hostmod_reg_write(self._cself, &c_data, diaddr, reg_addr,
                                        reg_size_bit, flags)
        check_osd_result(rv)

    def get_modules(self, subnet_addr):
        cdef cosd.osd_module_desc *modules = NULL
        cdef size_t modules_len = 0
        try:
            rv = cosd.osd_hostmod_get_modules(self._cself, subnet_addr,
                                              &modules, &modules_len)
            check_osd_result(rv)

            result_list = []
            for m in range(modules_len):
                mod_desc = {}
                mod_desc['addr'] = modules[m].addr
                mod_desc['vendor'] = modules[m].vendor
                mod_desc['type'] = modules[m].type
                mod_desc['version'] = modules[m].version
                result_list.append(mod_desc)
        finally:
            free(modules)

        return result_list

    def mod_describe(self, di_addr):
        cdef cosd.osd_module_desc c_mod_desc
        rv = cosd.osd_hostmod_mod_describe(self._cself, di_addr, &c_mod_desc)
        check_osd_result(rv)

        mod_desc = {}
        mod_desc['addr'] = c_mod_desc.addr
        mod_desc['vendor'] = c_mod_desc.addr
        mod_desc['type'] = c_mod_desc.addr
        mod_desc['version'] = c_mod_desc.addr

        return mod_desc

    def mod_set_event_dest(self, di_addr, flags=0):
        rv = cosd.osd_hostmod_mod_set_event_dest(self._cself, di_addr, flags)
        check_osd_result(rv)

    def mod_set_event_active(self, di_addr, enabled=True, flags=0):
        rv = cosd.osd_hostmod_mod_set_event_active(self._cself, di_addr,
                                                   enabled, flags)
        check_osd_result(rv)

    def get_max_event_words(self, di_addr_target):
        return cosd.osd_hostmod_get_max_event_words(self._cself, di_addr_target)

    def event_send(self, Packet event_pkg):
        rv = cosd.osd_hostmod_event_send(self._cself, event_pkg._cself)
        check_osd_result(rv)

    def event_receive(self, flags=0):
        cdef cosd.osd_packet* c_event_pkg
        rv = cosd.osd_hostmod_event_receive(self._cself, &c_event_pkg, flags)
        check_osd_result(rv)

        py_event_pkg = Packet._from_c_ptr(c_event_pkg, owner=True)

        return py_event_pkg


cdef class GatewayGlip:
    cdef cosd.osd_gateway_glip_ctx* _cself

    def __cinit__(self, Log log, host_controller_address, device_subnet_addr,
                  glip_backend_name, glip_backend_options):

        b_host_controller_address = host_controller_address.encode('UTF-8')
        cdef char* c_host_controller_address = b_host_controller_address

        b_glip_backend_name = glip_backend_name.encode('UTF-8')
        cdef char* c_glip_backend_name = b_glip_backend_name

        c_glip_backend_options_len = len(glip_backend_options)
        cdef cosd.glip_option* c_glip_backend_options = \
            <cosd.glip_option *>PyMem_Malloc(sizeof(cosd.glip_option) * \
                                             c_glip_backend_options_len)

        try:
            index = 0
            for name, value in glip_backend_options.items():
                b_name = name.encode('UTF-8')
                b_value = value.encode('UTF-8')
                c_glip_backend_options[index].name = b_name
                c_glip_backend_options[index].value = b_value
                index += 1

            cosd.osd_gateway_glip_new(&self._cself, log._cself,
                                      c_host_controller_address,
                                      device_subnet_addr,
                                      c_glip_backend_name,
                                      c_glip_backend_options,
                                      c_glip_backend_options_len)

            if self._cself is NULL:
                raise MemoryError()
        finally:
            PyMem_Free(c_glip_backend_options)

    def __dealloc__(self):
        if self._cself is NULL:
            return

        if self.is_connected():
            self.disconnect()

        cosd.osd_gateway_glip_free(&self._cself)

    def connect(self):
        return cosd.osd_gateway_glip_connect(self._cself)

    def disconnect(self):
        return cosd.osd_gateway_glip_disconnect(self._cself)

    def is_connected(self):
        return cosd.osd_gateway_glip_is_connected(self._cself)

    def get_transfer_stats(self):
        cdef cosd.osd_gateway_transfer_stats *stats

        stats = cosd.osd_gateway_glip_get_transfer_stats(self._cself)

        connect_time_float = stats.connect_time.tv_sec + stats.connect_time.tv_nsec * 1e-9
        cur_time = time.clock_gettime(time.CLOCK_MONOTONIC)
        time_elapsed = cur_time - connect_time_float

        return { 'bytes_from_device': stats.bytes_from_device,
                 'bytes_to_device': stats.bytes_to_device,
                 'connected_secs': time_elapsed }


cdef class Hostctrl:
    cdef cosd.osd_hostctrl_ctx* _cself

    def __cinit__(self, Log log, router_address):
        py_byte_string = router_address.encode('UTF-8')
        cdef char* c_router_address = py_byte_string
        cosd.osd_hostctrl_new(&self._cself, log._cself, c_router_address)
        if self._cself is NULL:
            raise MemoryError()

    def __dealloc__(self):
        if self._cself is NULL:
            return

        try:
            if self.is_running():
                self.stop()
        finally:
            cosd.osd_hostctrl_free(&self._cself)

    def start(self):
        rv = cosd.osd_hostctrl_start(self._cself)
        check_osd_result(rv)

    def stop(self):
        rv = cosd.osd_hostctrl_stop(self._cself)
        check_osd_result(rv)

    def is_running(self):
        return cosd.osd_hostctrl_is_running(self._cself)


cdef class Module:
    @staticmethod
    def get_type_short_name(vendor_id, type_id):
        return str(cosd.osd_module_get_type_short_name(vendor_id, type_id))

    @staticmethod
    def get_type_long_name(vendor_id, type_id):
         return str(cosd.osd_module_get_type_long_name(vendor_id, type_id))


cdef class MemoryDescriptor:
    cdef cosd.osd_mem_desc _cself

    def __cinit__(self):
        self._cself.num_regions = 0

    @property
    def regions(self):
        r = []
        for i in range(self._cself.num_regions):
            r.append(self._cself.regions[i])
        return r

    @property
    def addr_width_bit(self):
        return self._cself.addr_width_bit

    @property
    def data_width_bit(self):
        return self._cself.data_width_bit

    @property
    def di_addr(self):
        return self._cself.di_addr

    def __str__(self):
        def sizeof_fmt(num, suffix='B'):
            for unit in ['','Ki','Mi','Gi','Ti','Pi','Ei','Zi']:
                if abs(num) < 1024.0:
                    return "%3.1f %s%s" % (num, unit, suffix)
                num /= 1024.0
            return "%.1f %s%s" % (num, 'Yi', suffix)

        str = "Memory connected to MAM module at DI address %d.\n" %\
            self.di_addr
        str += "address width = %d bit, data width = %d bit\n" %\
            (self.addr_width_bit, self.data_width_bit)
        regions = self.regions
        str += "%d region(s):\n" % len(self.regions)

        r_idx = 0
        for r in self.regions:
            str += "  %d: base address: 0x%x, size: %s\n" %\
                (r_idx, r['baseaddr'], sizeof_fmt(r['memsize']))
            r_idx += 1

        return str

def cl_mam_get_mem_desc(Hostmod hostmod, mam_di_addr):
    mem_desc = MemoryDescriptor()

    cosd.osd_cl_mam_get_mem_desc(hostmod._cself, mam_di_addr, &mem_desc._cself)

    return mem_desc

def cl_mam_write(MemoryDescriptor mem_desc, Hostmod hostmod, data, addr):
    cdef char* c_data = data
    rv = cosd.osd_cl_mam_write(&mem_desc._cself, hostmod._cself, c_data,
                               len(data), addr)
    if rv != 0:
        raise Exception("Memory write failed (%d)" % rv)

def cl_mam_read(MemoryDescriptor mem_desc, Hostmod hostmod, addr, nbyte):
    data = bytearray(nbyte)
    cdef char* c_data = data

    rv = cosd.osd_cl_mam_read(&mem_desc._cself, hostmod._cself, c_data,
                              nbyte, addr)
    if rv != 0:
        raise Exception("Memory read failed (%d)" % rv)

    return data

cdef class MemoryAccess:
    cdef cosd.osd_memaccess_ctx* _cself

    def __cinit__(self, Log log, host_controller_address):
        py_byte_string = host_controller_address.encode('UTF-8')
        cdef char* c_host_controller_address = py_byte_string
        rv = cosd.osd_memaccess_new(&self._cself, log._cself,
                                    c_host_controller_address)
        check_osd_result(rv)
        if self._cself is NULL:
            raise MemoryError()

    def __dealloc__(self):
        if self._cself is NULL:
            return

        if self.is_connected():
            self.disconnect()

        cosd.osd_memaccess_free(&self._cself)

    def connect(self):
        rv = cosd.osd_memaccess_connect(self._cself)
        check_osd_result(rv)

    def disconnect(self):
        rv = cosd.osd_memaccess_disconnect(self._cself)
        check_osd_result(rv)

    def is_connected(self):
        return cosd.osd_memaccess_is_connected(self._cself)

    def cpus_stop(self, subnet_addr):
        rv = cosd.osd_memaccess_cpus_stop(self._cself, subnet_addr)
        check_osd_result(rv)

    def cpus_start(self, subnet_addr):
        rv = cosd.osd_memaccess_cpus_start(self._cself, subnet_addr)
        check_osd_result(rv)

    def find_memories(self, subnet_addr):
        cdef cosd.osd_mem_desc *memories
        cdef size_t num_memories = 0
        try:
            rv = cosd.osd_memaccess_find_memories(self._cself, subnet_addr,
                                                  &memories,
                                                  &num_memories)
            check_osd_result(rv)

            result_list = []
            for m in range(num_memories):
                mem_desc = MemoryDescriptor()
                mem_desc._cself = memories[m]
                result_list.append(mem_desc)
        finally:
            free(memories)

        return result_list

    def loadelf(self, MemoryDescriptor mem_desc, elf_file_path, verify):
        py_byte_string = elf_file_path.encode('UTF-8')
        cdef char* c_elf_file_path = py_byte_string
        cdef int c_verify = verify

        with nogil:
            rv = cosd.osd_memaccess_loadelf(self._cself, &mem_desc._cself,
                                            c_elf_file_path, c_verify)
        check_osd_result(rv)


cdef class SystraceLogger:
    cdef cosd.osd_systracelogger_ctx* _cself
    cdef FILE* _fp_sysprint
    cdef FILE* _fp_event

    cdef _sysprint_file
    cdef _event_file

    def __cinit__(self, Log log, host_controller_address, di_addr):
        self._fp_sysprint = NULL
        self._fp_event = NULL
        self._sysprint_file = None
        self._event_file = None

        b_host_controller_address = host_controller_address.encode('UTF-8')
        rv = cosd.osd_systracelogger_new(&self._cself, log._cself,
                                         b_host_controller_address, di_addr)
        check_osd_result(rv)
        if self._cself is NULL:
            raise MemoryError()

    def __dealloc__(self):
        if self._cself is NULL:
            return

        if self.is_connected():
            self.disconnect()

        cosd.osd_systracelogger_free(&self._cself)

        if self._fp_sysprint:
            fclose(self._fp_sysprint)

        if self._fp_event:
            fclose(self._fp_event)

    def connect(self):
        rv = cosd.osd_systracelogger_connect(self._cself)
        check_osd_result(rv)

    def disconnect(self):
        rv = cosd.osd_systracelogger_disconnect(self._cself)
        check_osd_result(rv)

    def is_connected(self):
        return cosd.osd_systracelogger_is_connected(self._cself)

    def stop(self):
        rv = cosd.osd_systracelogger_stop(self._cself)
        check_osd_result(rv)

    def start(self):
        rv = cosd.osd_systracelogger_start(self._cself)
        check_osd_result(rv)

    @property
    def sysprint_log(self):
        return self._sysprint_file

    @sysprint_log.setter
    def sysprint_log(self, log_filename):
        self._sysprint_file = log_filename

        if self._fp_sysprint:
            fclose(self._fp_sysprint)

        b_log_filename = os.fsencode(log_filename)
        self._fp_sysprint = fopen(b_log_filename, 'w')
        if not self._fp_sysprint:
            raise IOError(errno, strerror(errno).decode('utf-8'), log_filename)

        rv = cosd.osd_systracelogger_set_sysprint_log(self._cself,
                                                     self._fp_sysprint)
        check_osd_result(rv)

    @property
    def event_log(self):
        return self._event_file

    @event_log.setter
    def event_log(self, log_filename):
        self._event_file = log_filename

        if self._fp_event:
            fclose(self._fp_event)

        b_log_filename = os.fsencode(log_filename)
        self._fp_event = fopen(b_log_filename, 'w')
        if not self._fp_event:
            raise IOError(errno, strerror(errno).decode('utf-8'), log_filename)

        rv = cosd.osd_systracelogger_set_event_log(self._cself, self._fp_event)
        check_osd_result(rv)


cdef class CoretraceLogger:
    cdef cosd.osd_coretracelogger_ctx* _cself
    cdef FILE* _fp_log
    cdef _log_file
    cdef _elf_file

    def __cinit__(self, Log log, host_controller_address, di_addr):
        self._fp_log = NULL
        self._log_file = None

        b_host_controller_address = host_controller_address.encode('UTF-8')
        rv = cosd.osd_coretracelogger_new(&self._cself, log._cself,
                                          b_host_controller_address, di_addr)
        check_osd_result(rv)
        if self._cself is NULL:
            raise MemoryError()

    def __dealloc__(self):
        if self._cself is NULL:
            return

        if self.is_connected():
            self.disconnect()

        if self._fp_log:
            fclose(self._fp_log)

        cosd.osd_coretracelogger_free(&self._cself)

    def connect(self):
        rv = cosd.osd_coretracelogger_connect(self._cself)
        check_osd_result(rv)

    def disconnect(self):
        rv = cosd.osd_coretracelogger_disconnect(self._cself)
        check_osd_result(rv)

    def is_connected(self):
        return cosd.osd_coretracelogger_is_connected(self._cself)

    def stop(self):
        rv = cosd.osd_coretracelogger_stop(self._cself)
        check_osd_result(rv)

    def start(self):
        rv = cosd.osd_coretracelogger_start(self._cself)
        check_osd_result(rv)

    @property
    def log_file(self):
        return self._log_file

    @log_file.setter
    def log_file(self, log_filename):
        self._log_file = log_filename

        if self._fp_log:
            fclose(self._fp_log)

        b_log_filename = os.fsencode(log_filename)
        self._fp_log = fopen(b_log_filename, 'w')
        if not self._fp_log:
            raise IOError(errno, strerror(errno).decode('utf-8'), log_filename)

        rv = cosd.osd_coretracelogger_set_log(self._cself, self._fp_log)
        check_osd_result(rv)

    @property
    def elf_file(self):
        return self._elf_file

    @elf_file.setter
    def elf_file(self, elf_filename):
        self._elf_file = elf_filename

        b_filename = os.fsencode(elf_filename)
        rv = cosd.osd_coretracelogger_set_elf(self._cself, b_filename)
        check_osd_result(rv)
