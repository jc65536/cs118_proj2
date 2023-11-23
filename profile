#!/bin/bash

make clean
make comp-test PROF=1
time ./comp-test input.txt
gprof -b comp-test
