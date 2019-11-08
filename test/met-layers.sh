#!/bin/bash

srcdir=$(dirname $(dirname $(realpath $BASH_SOURCE)))


dir=${1:-metcfg}
if [ -d "$dir" ]  ; then
   echo "Output to existing $dir"
else
    mkdir "$dir"
    echo "Output to $dir"
fi

jsonnet -m $dir -J $srcdir/python/ptmp --tla-code-file layers=met-layers.jsonnet -e 'local p=import "ptmper.jsonnet"; p.cfggen'

jsonnet -S -J $srcdir/python/ptmp --tla-code-file layers=layers.jsonnet -e 'local p=import "ptmper.jsonnet"; p.procgen' > $dir/Procfile

echo $dir/Procfile

echo "now maybe do: cd $dir; shorman"

