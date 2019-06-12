#!/bin/bash
set -x
dumpfile=${1:-"/data/fast/bviren/ptmp-dumps/2019-06-10/FELIX_BR_601.dump"}

topdir=$(dirname $(dirname $(realpath $BASH_SOURCE)))

fsource=czmqat
replay=$topdir/build/test/check_replay
recv=$topdir/build/test/check_recv 
spy=tpset-tap

hwtickperusec=50.0

tokill=""
$fsource -E 10000 \
         ifile -f $dumpfile \
         osock -p PUSH -a connect -e tcp://127.0.0.1:8888 &
tokill="$tokill $!"

$replay -s $hwtickperusec \
        input -p PULL -a bind -e tcp://127.0.0.1:8888 \
        output -p PUSH -a connect -e tcp://127.0.0.1:7777 &
tokill="$tokill $!"

$recv  -p PULL -a connect -e tcp://127.0.0.1:6666 &
tokill="$tokill $!"

sleep 2

echo "kill -9 $tokill"

$spy -n 10 --verbosity 1 \
     input -p PULL -a bind -e tcp://127.0.0.1:7777 \
     output -p PUSH -a bind -e tcp://127.0.0.1:6666

kill -9 $tokill
