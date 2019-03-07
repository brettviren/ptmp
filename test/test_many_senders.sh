#!/bin/bash

echo $BASH_SOURCE

top="$(dirname $(dirname $BASH_SOURCE))"
sender="$top/build/test/check_send_rates"
recver="$top/build/test/check_recv"

nsend=1000
senders="1 2 3 4 5 6 7 8 9"

set -e
#set -x

err_report() {
    echo "Error on line $1"
    exit -1
}

trap 'err_report $LINENO' ERR

endpoint () {
    local trans=$1 ; shift
    local num=$1 ; shift 

    case $trans in
        ipc) echo "ipc://manysenders${num}.ipc" ;;
        tcp) echo "tcp://127.0.0.1:$(( 5550 + $num ))" ;;
        inproc) echo "inproc://manysenders$num" ;;
        *) exit -1;;
    esac
}

logfile () {
    local name="$1" ; shift
    local trans=$1 ; shift
    local num="$1" ; shift

    echo "$top/build/test/log.test_many_senders.${name}.${trans}.${num}"
}


cmd_sender () {
    local trans=$1 ; shift
    local num=$1 ; shift
    local ep="$(endpoint $trans $num)"
    local logf="$(logfile sender $trans $num)"
    
    cmd="$sender -p PUB -a bind -e $ep -B 1000 -E 200 -n $nsend -N 1000 exponential -t 50"
    echo $cmd
    $cmd 2>&1 > $logf
}

cmd_recver () {
    local trans=$1 ; shift
    local logf="$(logfile recver $trans 0)"
    local ep=""
    local expect=0
    for n in $@
    do
        ep="$ep -e $(endpoint $trans $n)"
        expect=$(( $expect + $nsend ))
    done
    cmd="$recver -p SUB -a connect $ep -T 1000 -n $expect"
    echo $cmd
    $cmd 2>&1 > $logf
}

for trans in inproc ipc tcp
do
    for n in $senders
    do
        cmd_sender $trans $n &
    done
    cmd_recver $trans $senders || exit -1
    echo "success: $trans"
done
sleep 1

