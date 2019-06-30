#!/bin/bash

datadirs=". /data/fast/bviren/ptmp-dumps /data /tmp"
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

do_test () {
    local name=${1} ; shift
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
    local args="-m $tmpdir -J $topdir/python/ptmp"

    if [ -n "$files" ] ; then
        local sfiles=${files[@]}
        args+=" --ext-code input=["\"${sfiles// /\",\"}\""]"
    fi

    echo "$args"

    set -x
    for cfg in $(jsonnet $args "$testfile") ; do
        time ptmper $cfg || exit -1
    done
    rm -rf $tmpdir
    #echo "not deleting $tmpdir"
}

do_test ptmper FELIX_BR_601.dump FELIX_BR_603.dump
