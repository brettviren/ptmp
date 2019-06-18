#!/bin/bash

outfile=${1:-check-draw-tps-multi.pdf}

set -x

# some finite value.  decrease to run faster, increase if there is
# empty data compared to "totalspan"
ntpsets=100000
# Total time span of plot in hardware clock ticks (here, T in [us] times rate in [MHz])
totalspan=$((1000000 * 50))
# time bin, number of 50MHz ticks
tbin=2500
# channel range
# 602: channels in [10239, 10518]
# 604: channels in [10467, 10468]
# 610: channels in [10077, 10308]
# 601: channels in [9280, 9567]
# 603: channels in [9328, 9615]
# 605: channels in [9472, 9759]
# 607: channels in [9424, 9711]
# 609: channels in [9376, 9663]
chanrange="9250 9800"
#chanrange="9280 9325"
#chanrange="9280 10518"


jobtime_s=100
tardy_ms=1000

dumpfilebase="/data/fast/bviren/ptmp-dumps/2019-06-10/FELIX_BR_"

window=./build/test/check_window 
replay=./build/test/check_replay
sorted=./build/test/check_sorted

tokill=""

tpsorted_endpoints=""


#for fn in {601..609}
for fn in 601 603 605 607 609
do
    oport=9$fn
    czmqat \
           ifile -f ${dumpfilebase}${fn}.dump \
           osock -p PUSH -a bind -e tcp://127.0.0.1:$oport &
    tokill="$! $tokill"


    iport=$oport
    oport=8$fn
    #oport=7777
    $replay -c $jobtime_s -s 50.0 \
            input -p PULL -a connect -e tcp://127.0.0.1:$iport \
            output -p PUSH -a connect -e tcp://127.0.0.1:$oport > log.replay.$fn 2>&1 &
    tokill="$! $tokill"

    iport=$oport
    oport=7$fn
    $window -s 3000 -b 150000 \
            input -p PULL -a bind -e tcp://127.0.0.1:$iport \
            output -p PUSH -a connect -e tcp://127.0.0.1:$oport  > log.window.$fn 2>&1 &
    tokill="$! $tokill"

    tpsorted_endpoints="-e tcp://127.0.0.1:$oport $tpsorted_endpoints"
done


oport=6666
$sorted -t $tardy_ms -c $jobtime_s  \
        input  -p PULL -a bind $tpsorted_endpoints \
        output -p PUSH -a connect -e tcp://127.0.0.1:$oport > log.sorted 2>&1 &
tokill="$! $tokill"
iport=$oport

sleep 2

ptmpy -n $ntpsets  -p PULL -a bind -e tcp://127.0.0.1:$iport \
      draw-tps -c $chanrange -t 0 $totalspan -T $tbin $outfile

kill -9 $tokill
