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
    context="test"
    datadir=/data/fast/bviren/ptmp-dumps/2019-07-19
    jargs="$jargs -V context=test"
else
    context="extvar"
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
tps_json=("sink-tps.json")
echo 'filesink: ptmper sink-tcs.json' > Procfile.tcs
tcs_json=("sink-tcs.json")
echo 'filesink: ptmper sink-tds.json' > Procfile.tds
echo "tdfinder: ptmper ${context}-td.json" >> Procfile.tds
tds_json=("sink-tds.json" "${context}-td.json")

for tc in *-tc-apa?.json
do
    echo "tcfinder: ptmper $tc" >> Procfile.tcs
    tcs_json+=("$tc")
    echo "tcfinder: ptmper $tc" >> Procfile.tds
    tds_json+=("$tc")
done

for fp in fileplay-apa?.json
do
    echo "fileplay: ptmper $fp" >> Procfile.tps
    tps_json+=("$fp")
    echo "fileplay: ptmper $fp" >> Procfile.tcs
    tcs_json+=("$fp")
    echo "fileplay: ptmper $fp" >> Procfile.tds
    tds_json+=("$fp")
done


cd $tmpdir
if [ -n "$(which ptmperpy)" ] ; then
    jq -s '.[0].proxies + .[1].proxies + .[2].proxies + .[3].proxies | {proxies:.}' ${tps_json[*]} > tps-graph.json
    jq -s '.[0].proxies + .[1].proxies + .[2].proxies + .[3].proxies | {proxies:.}' ${tcs_json[*]} > tcs-graph.json
    jq -s '.[0].proxies + .[1].proxies + .[2].proxies + .[3].proxies | {proxies:.}' ${tds_json[*]} > tds-graph.json

    ptmperpy dotify -o tps-graph.dot tps-graph.json 
    ptmperpy dotify -o tcs-graph.dot tcs-graph.json 
    ptmperpy dotify -o tds-graph.dot tds-graph.json 

    dot -Tpdf       -o tps-graph.pdf tps-graph.dot
    dot -Tpdf       -o tcs-graph.pdf tcs-graph.dot
    dot -Tpdf       -o tds-graph.pdf tds-graph.dot
else
    echo "No ptmperpy, no graph.  Install and setup PTMP's python package if you want it"
fi

echo "Config generated.  Now, maybe:"
echo "cd $tmpdir"
echo "shoreman Procfile.tps"


