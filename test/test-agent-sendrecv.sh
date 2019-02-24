#!/bin/bash
set -e

tstdir=$(dirname $BASH_SOURCE)
topdir=$(dirname $tstdir)

cd $topdir

if [ ! -d build ] ; then
    mkdir build
fi


for cfg in $(jsonnet -m build test/test-agent-sendrecv.jsonnet)
do
    cmd="./build/apps/ptmp-agent -c $cfg"
    echo $cmd
    $cmd
done
