#!/bin/bash

port1=7771
port2=7772

tosend=10000

top="$(dirname $(dirname $BASH_SOURCE))"
sender="$top/build/test/check_send_rates"
proxy="$top/build/apps/czmqat"
recver="$top/build/test/check_recv"
myname="$(basename $BASH_SOURCE .sh)"

trans="tcp"
addr="127.0.0.1"
url="${trans}://${addr}"

logfile () {
    local name="$1" ; shift
    local num="$1" ; shift
    echo "$top/build/test/log.${myname}.${name}.${trans}.${num}"
}

cmd_proxy() {
    local logf="$(logfile proxy $trans 0)"
    cmd="$proxy -E 10000 -n $tosend isock -p PULL -a connect -e ${url}:$port1 osock -p PUSH -a bind -e ${url}:$port2"
    echo $cmd
    $cmd 2>&1 > $logf
}

cmd_sender () {
    local trans="tcp"
    local logf="$(logfile sender $trans 0)"
    cmd="$sender -E 10000 -d 0 -m 10 -p PUSH -a bind -e ${url}:$port1 -B 1000 -n $tosend -N 1000 uniform -t10"
    echo $cmd
    $cmd 2>&1 > $logf
}

cmd_reciever () {
    local trans="tcp"
    local logf="$(logfile recver $trans 0)"
    cmd="$recver -n $tosend -p PULL -a connect -e ${url}:$port2"
    echo $cmd
    $cmd 2>&1 > $logf
}    

cmd_proxy &
cmd_reciever &
cmd_sender
