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

jsonnet -m $tmpdir -V 'input=tcp://127.0.0.1:7999' -V "output=$tmpdir/tds.dump" $jdir/filesink.jsonnet
jsonnet -m $tmpdir -V datadir=$datadir -V apa=$apa -V 'output=tcp://127.0.0.1:7%03d' $jdir/fileplay.jsonnet
jsonnet -m $tmpdir -V context=test $jdir/adjacency.jsonnet

cat <<EOF > $tmpdir/Procfile
filesink: ptmper fileplay-sink.json
tcfinder: ptmper test-tc-apa0.json
tdfinder: ptmper test-td.json
fileplay: ptmper fileplay-apa5.json
EOF

cd $tmpdir
jq -s '.[0].proxies + .[1].proxies + .[2].proxies + .[3].proxies | {proxies:.}' *.json > graph.json
ptmperpy dotify -o graph.dot graph.json 
dot -Tpdf -o graph.pdf graph.dot

shoreman Procfile
