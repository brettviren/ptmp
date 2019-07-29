#!/bin/bash

usage () {
    cat <<EOF
usage: $BASH_SOURCE [/path/to/dump/files]
EOF
    exit 1
}


tmpdir=$(mktemp -d "/tmp/test-pdsp-XXXXX")
testdir=$(dirname $(realpath $BASH_SOURCE))
topdir=$(dirname $testdir)
jdir=$topdir/python/ptmp

jargs="-m $tmpdir"

argify_list_str () {
    local list=$@
    echo "["\"${list// /\",\"}\""]"
}
argify_list () {
    local list=$@
    echo "["${list// /,}"]"
}
 
datadir=$1
if [ -z "$datadir" ] ; then
    datadir=/data/fast/bviren/ptmp-dumps/2019-07-05
    jargs="$jargs -V context=test"
else
    apas=()
    infiles=("null")
    for apa in {1..6} ; do
        linkfiles=()
        for link in {01..10} ; do
            linkfile=$datadir/FELIX_BR_${apa}${link}.dump
            if [ ! -f "$linkfile" ] ; then
                echo "no data for APA $apa link $link" 
                continue
            fi

            linkfiles+=($linkfile)
        done
        if [ -z "$linkfiles" ] ; then
            echo "no data for APA $apa"
            infiles+=("null")
            continue
        fi
        infiles+=( $(argify_list_str "${linkfiles[*]}") )
        apas+=( $apa )
    done
    echo "apas=${apas[*]}"
    apas=$(argify_list "${apas[*]}")
    infiles=$(argify_list "${infiles[*]}")
    jargs="$jargs -V context=extvar --ext-code apas=$apas --ext-code infiles=$infiles"    
fi
# echo "$jargs"
# exit

JSONNET=$(which jsonnet)
if [ -z "$JSONNET" ] ; then
    echo "This test needs jsonnet in the PATH"
    exit -1
fi

if [ -z "$LD_LIBRARY_PATH" ] ; then
    echo "This test requires the ptmp-tcs plugin and LD_LIBRARY_PATH is empty"
    exit -1
fi

set -x
for cfg in adjacency filesink fileplay
do
    jsonnet $jargs $jdir/$cfg.jsonnet || exit -1
done
set +x

cd $tmpdir

echo 'filesink: ptmper sink-tps.json' > Procfile.tps
echo 'filesink: ptmper sink-tcs.json' > Procfile.tcs
echo 'filesink: ptmper sink-tds.json' > Procfile.tds

echo 'tdfinder: ptmper test-td.json' > Procfile.tds

for tc in *-tc-apa?.json
do
    echo "tcfinder: ptmper $tc" >> Procfile.tcs
    echo "tcfinder: ptmper $tc" >> Procfile.tds
done

for fp in fileplay-apa?.json
do
    echo "fileplay: ptmper $fp" >> Procfile.tps
    echo "fileplay: ptmper $fp" >> Procfile.tcs
    echo "fileplay: ptmper $fp" >> Procfile.tds
done


cd $tmpdir
if [ -n "$(which ptmperpy)" ] ; then
    jq -s '.[0].proxies + .[1].proxies + .[2].proxies + .[3].proxies | {proxies:.}' *.json > graph.json
    ptmperpy dotify -o graph.dot graph.json 
    dot -Tpdf -o graph.pdf graph.dot
else
    echo "No ptmperpy, no graph.  Install and setup PTMP's python package if you want it"
fi

echo "Config generated.  Now, maybe:"
echo "cd $tmpdir"
echo "shoreman Procfile.tps"


