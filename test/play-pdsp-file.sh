#!/bin/bash

# This fakes a full PDSP test with a single APA all the way through TD.

datadir=/data/fast/bviren/ptmp-dumps/2019-07-05
apa=5

if [ ! -d $datadir ] ; then
    echo "data dir not found: $datadir"
    exit -1
fi

JSONNET=$(which jsonnet)
if [ -z "$JSONNET" ] ; then
    echo "This test needs jsonnet in the PATH"
    exit -1
fi

if [ -z "$LD_LIBRARY_PATH" ] ; then
    echo "This test requires the ptmp-tcs plugin and LD_LIBRARY_PATH is empty"
    exit -1
fi

tmpdir=$(mktemp -d "/tmp/test-pdsp-XXXXX")
testdir=$(dirname $(realpath $BASH_SOURCE))
topdir=$(dirname $testdir)
jdir=$topdir/python/ptmp

jargs="-m $tmpdir -V context=test"

set -x
for cfg in adjacency filesink fileplay
do
    jsonnet $jargs $jdir/$cfg.jsonnet || exit -1
done
set +x

cat <<EOF > $tmpdir/Procfile.tps
filesink: ptmper sink-tps.json
fileplay: ptmper fileplay-apa5.json
EOF

cat <<EOF > $tmpdir/Procfile.tcs
filesink: ptmper sink-tcs.json
tcfinder: ptmper test-tc-apa5.json
fileplay: ptmper fileplay-apa5.json
EOF

cat <<EOF > $tmpdir/Procfile.tds
filesink: ptmper sink-tds.json
tcfinder: ptmper test-tc-apa5.json
tdfinder: ptmper test-td.json
fileplay: ptmper fileplay-apa5.json
EOF


cd $tmpdir
if [ -n "$(which ptmperpy)" ] ; then
    jq -s '.[0].proxies + .[1].proxies + .[2].proxies + .[3].proxies | {proxies:.}' *.json > graph.json
    ptmperpy dotify -o graph.dot graph.json 
    dot -Tpdf -o graph.pdf graph.dot
else
    echo "No ptmperpy, no graph.  Install and setup PTMP's python package if you want it"
fi

echo $tmpdir

