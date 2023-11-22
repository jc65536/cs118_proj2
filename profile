#!/bin/bash

make clean
make comp-test PROF=1
./comp-test "$1"
gprof -b comp-test
