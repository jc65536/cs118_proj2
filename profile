#!/bin/bash

make clean
make comp-test PROF=1
time ./comp-test "$1"
gprof -b comp-test
