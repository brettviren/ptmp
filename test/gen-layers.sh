#!/bin/bash

# this little script runs Jsonnet to generate ptmper and shoreman
# config files based on a PTMP graph structured defined in a "layers"
# Jsonnet file.  This file describes high-level parameters which are
# fodder for expansion by ptmper.jsonnet.

# usage: gen-layers.sh layers.jsonnet [outdir]

# A default "met-layers.jsonnet" is in the same source directory as
# this script.


tstdir=$(dirname $(realpath $BASH_SOURCE))
srcdir=$(dirname "$tstdir")

layers=$1; shift
if [ -z "$layers" ] ; then
    echo "using default test/met-layers.jsonnet"
    layers="$tstdir/met-layers.jsonnet"
fi

outdir=${1:-metcfg}
if [ -d "$outdir" ]  ; then
   echo "Output to existing $outdir"
else
    mkdir "$outdir"
    echo "Output to $outdir"
fi

defargs="-J $srcdir/python/ptmp --tla-code-file layers=$layers"

jsonnet -m $outdir $defargs -e 'local p=import "ptmper.jsonnet"; p.cfggen'

jsonnet -S $defargs -e 'local p=import "ptmper.jsonnet"; p.procgen' > $outdir/Procfile

echo $outdir/Procfile

echo "now maybe do: cd $outdir; shorman"

