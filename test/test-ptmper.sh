#!/bin/bash

if [ ! -d /data/fast/bviren ] ; then
    echo "This test is required to run on a host with a data directory"
    exit 0
fi

set -x
tstdir=$(dirname $(realpath "$BASH_SOURCE"))
topdir=$(dirname "$tstdir")

tmpdir=$(mktemp -d)

for cfg in $(jsonnet -m "$tmpdir" -J $topdir/python/ptmp $tstdir/test-ptmper.jsonnet) ; do
    ptmper $cfg || exit -1
done
rm -rf $tmpdir

