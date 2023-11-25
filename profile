#!/bin/bash
set -m

case "$1" in
    rd)
        make clean
        make PROF=1
        python rdcc_proxy.py -t rd -l 0 &
        ./server &
        ./client input.txt > /dev/null 2>&1 &
        wait %2
        kill %1
        kill %3
        gprof -b server
        ;;

    comp)
        make clean
        make comp-test PROF=1
        time ./comp-test rep3.txt
        gprof -b comp-test
        ;;
esac
