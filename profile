#!/bin/bash
set -m

case "$1" in
    trans)
        make clean
        make PROF=1
        python rdcc_proxy.py -t rd -l 0 > /dev/null &
        sleep 0.5
        ./server > /dev/null &
        sleep 0.5
        ./client input.txt > /dev/null &
        wait %2
        mv gmon.out gmon-server.out
        kill %1
        kill %3
        mv gmon.out gmon-client.out
        gprof -b "$2" "gmon-$2.out"
        ;;

    comp)
        make clean
        make comp-test PROF=1
        time ./comp-test "${2:-input.txt}"
        gprof -b comp-test
        cmp "$2" decoded.txt
        ;;
esac
