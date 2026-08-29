#!/usr/bin/env python3
"""Build the minimal monitor-call shim used only by the integration test."""
import sys
image=bytearray(b'\xff'*4096)
def put(address,data): image[address:address+len(data)]=data
put(0x0026,b'\xc3\x00\x01')       # blocking read -> 0100
put(0x0029,b'\xc3\x10\x01')       # key available -> 0110
put(0x002c,b'\xc3\x20\x01')       # clear queue -> 0120
# Increment the monitor's 16-bit 20 ms clock, then return from interrupt.
put(0x0038,b'\xf5\xe5\x21\x10\x60\x34\x20\x02\x23\x34\xe1\xf1\xfb\xed\x4d')
put(0x0100,b'\xdb\xfe\xb7\x28\xfb\xc9')
put(0x0110,b'\xdb\xfd\xb7\xc9')
put(0x0120,b'\xd3\xfc\xc9')
with open(sys.argv[1],'wb') as stream: stream.write(image)
