#!/bin/bash

echo $BASH_SOURCE

top="$(dirname $(dirname $BASH_SOURCE))"
sender="$top/build/test/check_send_rates"
recver="$top/build/test/check_recv"

nsend=1000
senders="1 2 3 4 5 6 7 8 9"
#senders="1"
#set -e
#set -x

err_report() {
    echo "Error on line $1"
    exit 1
}

trap 'err_report $LINENO' ERR

endpoint () {
    local trans=$1 ; shift
    local num=$1 ; shift 

    case $trans in
        ipc) echo "ipc://manysenders${num}.ipc" ;;
        tcp) echo "tcp://127.0.0.1:$(( 5550 + $num ))" ;;
        inproc) echo "inproc://manysenders$num" ;;
        *) exit 2;;
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
    
    cmd="$sender -p PUSH -a bind -e $ep -E 1000 -n $nsend -N 1000 exponential -t 50"
    echo $cmd
    echo $logf
    echo $cmd > $logf
    $cmd >> $logf 2>&1
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
    cmd="$recver -p PULL -a connect $ep -T 1000 -n $expect"
    echo $cmd
    echo $logf
    echo $cmd > $logf
    $cmd >> $logf 2>&1
}

for trans in ipc tcp
do
    tokill=""
    for n in $senders
    do
        cmd_sender $trans $n &
        tokill="$tokill $!"
    done
    cmd_recver $trans $senders || exit 3
    echo "Waiting for $tokill"
    wait $tokill
    echo "success: $trans"
done
sleep 1

