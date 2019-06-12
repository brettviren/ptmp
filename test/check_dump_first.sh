#!/bin/bash

dumpfilebase="/data/fast/bviren/ptmp-dumps/2019-06-10/FELIX_BR_"

for fn in {601..610}
do
    dumpfile=${dumpfilebase}${fn}.dump
    if [ ! -f $dumpfile ] ; then
        echo "No such file: $dumpfile"
        break;
    fi
    port=99$fn
    czmqat  ifile -f $dumpfile osock -p PUSH -a bind -e tcp://127.0.0.1:$port &
    tokill=$!
    ptmpy -n 1 -e tcp://127.0.0.1:$port -p PULL -a connect dump-messages
    kill -9 $tokill
    
done
