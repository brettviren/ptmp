#!/bin/bash

# this may be set externaly, here it's set to where tests often run.
PTMP_TEST_DATAPATH="${PTMP_TEST_DATAPATH:-/data/fast/bviren/ptmp-dumps/2019-07-19:/tmp/ptmp-data}"

PTMP_DATA_URL="${PTMP_DATA_URL:-https://www.phy.bnl.gov/~bviren/tmp/ptmp/ptmp-dumps/2019-07-19}"

# source this in other test scripts to get some common utility functions.
ptmp_test_dir="$(dirname $(realpath "$BASH_SOURCE"))"
ptmp_src_dir="$(dirname $ptmp_test_dir)"

PATH="$ptmp_src_dir/build:$ptmp_src_dir/build/test:$PATH"
export LD_LIBRARY_PATH="$ptmp_src_dir/build"


ptmp_get_dump_file () {
    local apa="${1:-5}" ; shift
    local link="${1:-01}"; shift
    local filename="$(printf FELIX_BR_%d%02d.dump $apa $link)"
    local pdir
    for pdir in ${PTMP_TEST_DATAPATH//:/ }
    do
        if [ -f "$pdir/$filename" ] ; then
            echo "$pdir/$filename"
            return
        fi
    done
    mkdir -p /tmp/ptmp-data
    wget -q -O "/tmp/ptmp-data/$filename" "$PTMP_DATA_URL/$filename"
    echo "downloaded /tmp/ptmp-data/$filename" 1>&2
    echo "/tmp/ptmp-data/$filename"
}

argify_list () {
    local list=$@
    echo "["\"${list// /\",\"}\""]"
}

ptmp_jsonnet () {
    found="$(which jsonnet)"
    if [ -n "$found" ] ; then
        jsonnet $@
        return
    fi
    found="$(which ptmper)"
    if [ -n "$found" ] ; then
        ptmper jsonnet $@
        return
    fi
    found="$(which python3)"
    if [ -n "$found"] ; then
        python3 -m venv ptmp-testlib.venv
        source ptmp-testlib.venv/bin/activate
        (cd $ptmp_src_dir && python setup.py install)
        ptmper jsonnet $@
        return
    fi
    found="$(which virtualenv)"
    if [ -n "$found"] ; then
        virtualenv venv ptmp-testlib.venv
        source ptmp-testlib.venv/bin/activate
        (cd $ptmp_src_dir && python setup.py install)
        ptmper jsonnet $@
        return
    fi
    echo "Can not find jsonnet"
    return 1
}
