#!/bin/bash -x

g++ -c JtagString.cpp
g++ -c HptdcString.cpp
g++ JtagString.o HptdcString.o bis78-jtag.cpp -o bis78-jtag
