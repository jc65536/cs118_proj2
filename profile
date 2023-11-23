#!/bin/bash

case "$1" in
    rd)
        make clean
        make PROF=1
        nohup python rdcc_proxy.py -t rd -l 0 &
        time ./server &
        ./client input.txt &
        wait %2
        kill %1
        kill %3
        gprof -b server
        ;;

    comp)
        make clean
        make comp-test PROF=1
        time ./comp-test input.txt
        gprof -b comp-test
        ;;
esac
