#!/bin/bash

dataset="2019-07-05"
datadirs=". /data/fast/bviren/ptmp-dumps /srv/bv/data /data/bviren /data /tmp"
dataurl="https://www.phy.bnl.gov/~bviren/tmp/ptmp/ptmp-dumps/$dataset"

tstdir=$(dirname $(realpath "$BASH_SOURCE"))
topdir=$(dirname "$tstdir")
tmpdir=$(mktemp -d "/tmp/test-ptmper-XXXXX")

getfile () {
    if [ -z "$1" ] ; then
        echo "need a file name"
        exit 1
    fi
    local filename="$1" ; shift
    local maybe
    for maybe in $datadirs ; do
        if [ -f $maybe/$dataset/$filename ] ; then
            echo $maybe/$dataset/$filename
            return
        fi
    done
    tmpdir=/tmp/$dataset
    if [ ! -d $tmpdir ] ; then
        mkdir -p $tmpdir
    fi
    wget -O $tmpdir/$filename $dataurl/$filename
    echo $tmpdir/$filename
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

    args+=" -m $tmpdir -J $topdir/python/ptmp"
    args+=" --ext-code input="$(argify_list "${files[*]}")

    echo "$args"

    #set -x
    echo "jsonnet $args $testfile"
    for cfg in $(jsonnet $args "$testfile") ; do
        echo "ptmper $cfg"
        time ptmper $cfg || exit -1
    done
    #rm -rf $tmpdir
}

#do_test ptmper "" FELIX_BR_50{1,3,5,7,9}.dump
#do_test ptmper "" FELIX_BR_50{1,3}.dump
do_test ptmper-monitored "-V outdir=$tmpdir" FELIX_BR_50{1,3,5,7,9}.dump
#do_test ptmper-monitored "-V outdir=$tmpdir" FELIX_BR_501.dump

echo "output in: $tmpdir"


