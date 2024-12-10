#!/usr/bin/env python

from mpsse import *
import sys

if len(sys.argv) < 4:
    print "There should be at least three arguments: dummy, i2c address and value -> exit"
    sys.exit()

i2c = MPSSE(I2C, ONE_HUNDRED_KHZ)
a = chr(2*int(sys.argv[2],0))
i2c.Start()
numberOfValues = len(sys.argv) - 2
for i in range(0,numberOfValues-1):
    a = a + chr(int(sys.argv[3+i],0))
i2c.Write(a)
i2c.Stop()
i2c.Close()


