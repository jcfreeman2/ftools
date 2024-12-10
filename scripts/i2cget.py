#!/usr/bin/env python

from mpsse import *
import sys

if (len(sys.argv) != 3 and len(sys.argv) != 4):
    print "There should be two or three arguments: dummy, i2c address and value (internal address) -> exit"
    sys.exit()

i2c = MPSSE(I2C, ONE_HUNDRED_KHZ)

# first select internal address if this is specified
writeAddress = chr(2*int(sys.argv[2],0))
if len(sys.argv) == 4:
  aa = writeAddress + chr(int(sys.argv[3],0))
  i2c.Start()
  i2c.Write(aa)
  i2c.Stop

readAddress = chr(2*int(sys.argv[2],0)+1)
i2c.Start()
i2c.Write(readAddress)
if i2c.GetAck() == ACK:
  # print "Ack for "+ hex(ord(readAddress) >> 1)
  data = i2c.Read(1)
  print hex(ord(data))
  i2c.SendNacks()
  i2c.Read(1)
else:
  print "Nack for "+ hex(ord(readAddress) >> 1)
i2c.Stop()
i2c.Close()


