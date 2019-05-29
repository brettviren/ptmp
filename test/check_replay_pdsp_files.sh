#!/bin/bash

# Pierre Lasorak made some TPSet dumps with czmqat run against PDSP's
# hit finder.  This script replays them

filepat="/data/big/bviren/zdumps/HITFINDER_BR_%d.dump"


sender_portbase=7000
replay_portbase=8000
sorted_port=9000

top="$(dirname $(dirname $BASH_SOURCE))"

sender="$top/build/apps/czmqat"
replay="$top/build/test/check_replay"
sorted="$top/build/test/check_sorted"
recver="$top/build/test/check_recv"
myname="$(basename $BASH_SOURCE .sh)"
sinker="$top/build/apps/czmqat"


run_sinker () {
    log="$top/build/test/log.${myname}.sinker"
    out="$top/build/test/dat.${myname}.sinker"
    cmd="$sinker isock -p PULL -a connect -e tcp://127.0.0.1:$sorted_port ofile -f $out"
    $cmd > $log 2>&1 
}

run_sorted () {
    log="$top/build/test/log.${myname}.sorted"
    endpoints=""
    for filen in $@
    do
        port=$(( $replay_portbase + $filen))
        endpoints="$endpoints -e tcp://127.0.0.1:$port"
    done
    cmd="$sorted -c 10 input -p PULL -a connect $endpoints output -p PUSH -a bind -e tcp://127.0.0.1:$sorted_port"
    $cmd > $log 2>&1 &
}

run_replay () {
    local filen=$1 ; shift;
    log="$top/build/test/log.${myname}.replay.$filen"
    in_port=$(( $sender_portbase + $filen ))
    in_addr="tcp://127.0.0.1:$in_port"
    out_port=$(( $replay_portbase + $filen ))
    out_addr="tcp://127.0.0.1:$out_port"
    cmd="$replay -c 10 input -p PULL -a connect -e $in_addr output -p PUSH -a bind -e $out_addr"
    $cmd > $log 2>&1 &
}

run_sender () {
    local filen=$1 ; shift;
    log="$top/build/test/log.${myname}.sender.$filen"
    port=$(( $sender_portbase + $filen ))
    endpoint="tcp://127.0.0.1:$port"
    cmd="$sender -E 10000 -d 0 ifile -f $fname osock -p PUSH -a bind -e $endpoint"
    $cmd > $log 2>&1 &
}


# sender
for filen in {501..510} ; do
    fname="$(printf $filepat $filen)"


    run_replay $filen 
    run_sender $filen 
done

run_sorted
run_sinker

