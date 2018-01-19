Debugging
=========

Run the program with GDB
------------------------
Before `make install` is called all compiled binaries in the source tree are wrapped by libtool.
To run GDB on them, use the following command line:

.. code-block:: sh
   # general
   libtool --mode=execute gdb --args YOUR_TOOL

   # for example
   libtool --mode=execute gdb --args ./src/tools/osd-target-run/osd-target-run -e ~/src/baremetal-apps/hello/hello.elf -vvv

GDB helpers
-----------

GDB can call functions in the program binary when the program is stopped (e.g. when a breakpoint hit).
This can be used to dump useful information from data structures in a readable form.

.. code-block:: sh
   # dump a DI packet
   (gdb) p osd_packet_dump(pkg, stdout)
   Packet of 5 data words:
   DEST = 4096, SRC = 1, TYPE = 2 (OSD_PACKET_TYPE_EVENT), TYPE_SUB = 0
   Packet data (including header):
     0x1000
     0x0001
     0x8000
     0x0000
     0x0000
   $5 = void
