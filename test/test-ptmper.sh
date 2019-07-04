#!/bin/bash

datadirs=". /data/fast/bviren/ptmp-dumps/2019-06-10 /srv/bv/data /data/bviren /data /tmp"
dataurl="https://www.phy.bnl.gov/~bviren/tmp/ptmp/ptmp-dumps/2019-06-10"

tstdir=$(dirname $(realpath "$BASH_SOURCE"))
topdir=$(dirname "$tstdir")

getfile () {
    if [ -z "$1" ] ; then
        echo "need a file name"
        exit 1
    fi
    local filename="$1" ; shift
    local maybe
    for maybe in $datadirs ; do
        if [ -f $maybe/$filename ] ; then
            echo $maybe/$filename
            return
        fi
    done
    wget -O /tmp/$filename $dataurl/$filename
    echo /tmp/$filename
}

argify_list () {
    local list=$@
    echo "["\"${list// /\",\"}\""]"
}


do_test () {
    local name=${1} ; shift
    local args="$1" ; shift
    local testfile="$tstdir/test-${name}.jsonnet"
    if [ ! -f $testfile ] ; then
        echo "no such test \"$name\""
        exit -1
    fi      

    declare -a files
    for fname in $@ ; do
        files+=( $(getfile $fname) )
    done 

    echo "Using files: ${files[*]}"

    local tmpdir=$(mktemp -d "/tmp/test-ptmper-XXXXX")
    args+=" -m $tmpdir -J $topdir/python/ptmp"
    args+=" --ext-code input="$(argify_list "${files[*]}")

    echo "$args"

    set -x
    for cfg in $(jsonnet $args "$testfile") ; do
        echo "ptmper $cfg"
        time ptmper $cfg || exit -1
    done
    #rm -rf $tmpdir
    echo "not deleting $tmpdir"
}

#do_test ptmper "" FELIX_BR_60{1,3,5,7,9}.dump
do_test ptmper "" FELIX_BR_60{1,3}.dump
#do_test ptmper-monitored "-V output=ptmper.mon" FELIX_BR_60{1,3,5,7,9}.dump > log.ptmper-monitored 2>&1



