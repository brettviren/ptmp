#!/bin/bash

if [ -z "$1" ] ; then
    echo "This test isn't ready yet (tests not programmed to terminate)"
    echo "Simply echoing individual test commands"
fi


set -e

tstdir=$(dirname $BASH_SOURCE)
topdir=$(dirname $tstdir)

cd $topdir

if [ ! -d build ] ; then
    mkdir build
fi

## Run to regenerate test json files.  For now, we don't do this from
## waf to minimize dependencies.
#### jsonnet -m test/json/ test/test-agent-sendrecv.jsonnet
for cfg in test/json/test-{pair-pair,pub-sub,push-pull}-{inproc,ipc,tcp}.json
do
    cmd="./build/apps/ptmp-agent -c $cfg"
    echo $cmd
    if [ -n "$1" ] ; then
        $cmd
    fi
done
