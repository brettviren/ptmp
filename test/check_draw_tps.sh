#!/bin/bash

# some finite value.  decrease to run faster, increase if there is
# empty data compared to "totalspan"
ntpsets=1000000
# Total time span of plot in hardware clock ticks (here, T in [us] times rate in [MHz])
totalspan=$((5000 * 50))
# time bin, number of 50MHz ticks
tbin=25
# channel range
chanrange="9250 9600"

echo "channels: $chanrange"

dumpfile=/data/fast/bviren/ptmp-dumps/2019-06-10/FELIX_BR_601.dump


czmqat ifile -f $dumpfile osock -p PUSH -a bind -e tcp://127.0.0.1:9999 &
tokill=$!

ptmpy -n 100000 -e tcp://127.0.0.1:9999 -p PULL -a connect \
      draw-tps -c $chanrange -t 0 $totalspan -T $tbin draw-tps-from-file.pdf

kill $tokill

echo "sleeping"
sleep 2
set -x

czmqat ifile -f $dumpfile osock -p PUSH -a bind -e tcp://127.0.0.1:9990 &
tokill1=$!

./build/test/check_window  -s 3000 -b 150000 \
                           input -p PULL -a connect -e tcp://127.0.0.1:9990 \
                           output -p PUSH -a bind -e tcp://127.0.0.1:8880  > /dev/null 2>&1 &
tokill2=$!

ptmpy -n 100000 -e tcp://127.0.0.1:8880 -p PULL -a connect \
      draw-tps -c $chanrange -t 0 $totalspan -T $tbin draw-tps-windowed.pdf

kill $tokill1 $tokill2

