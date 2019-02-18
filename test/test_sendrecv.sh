#!/bin/bash

echo $BASH_SOURCE

top="$(dirname $(dirname $BASH_SOURCE))"

set -e

for tran in inproc://sendrecv ipc://sendrecv.fifo tcp://127.0.0.1:23456
do
    for bc in bind connect
    do
        for at in pipe pubsub pushpull
        do
            log="build/log.sendrecv_${bc}_${at}_$(echo $tran | tr ':/.' '_')"
            cmd="$top/build/test/check_sendrecv 100000 $bc $at $tran"
            echo $cmd
            echo $cmd > $log
            time $cmd >> $log 2>&1
        done
    done
done
