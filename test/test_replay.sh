#!/bin/bash

port1=8881
port2=8882

tosend=1000
hwm=1000

top="$(dirname $(dirname $BASH_SOURCE))"
sender="$top/build/test/check_send_rates"
proxy="$top/build/test/check_replay"
recver="$top/build/test/check_recv"
myname="$(basename $BASH_SOURCE .sh)"

trans="tcp"
addr="127.0.0.1"
url="${trans}://${addr}"

logfile () {
    local name="$1" ; shift
    echo "$top/build/test/log.${myname}.${name}.${trans}"
}


cmd_sender () {
    local logf="$(logfile sender)"
    cmd="$sender -E 10000 -d 0 -m $hwm -p PUSH -a bind -e ${url}:$port1 -B 1000 -n $tosend -N 1000 uniform -t10"
    echo $cmd
    $cmd 2>&1 > $logf
}

cmd_proxy() {
    local logf="$(logfile proxy)"
    cmd="$proxy -s 1.0 -c15"
    cmd="$cmd  input -p PULL -a connect -e ${url}:$port1"
    cmd="$cmd output -p PUSH -a bind    -e ${url}:$port2"
    echo $cmd
    $cmd 2>&1 > $logf
}

cmd_receiver () {
    local logf="$(logfile recver)"
    cmd="$recver -n $tosend -p PULL -a connect -e ${url}:$port2"
    echo $cmd
    $cmd 2>&1 > $logf
}    

cmd_proxy &
cmd_receiver &
cmd_sender
