#!/bin/bash
# Some bash functions which can be used commonly across several test scripts.

testdir=$(dirname $(realpath $BASH_SOURCE))
srcdir=$(dirname $testdir)
blddir=$srcdir/build
PATH=$blddir:$blddir/test:$blddir/apps

datadir=.
if [ -d /data/fast/bviren/ptmp-dumps/2019-06-10 ] ; then
    datadir=/data/fast/bviren/ptmp-dumps/2019-06-10
fi
    

# activate a virtual env if one found
for maybe in ./*/bin/activate $srcdir/*/bin/activate ; do
    if [ -f $maybe ] ; then
        source $maybe
        break
    fi
done

# convert: <bind|connect> <TYPE> <ADDR> to CLI args convention
ptmp-sock-args () {
    echo " -a $1 -p $2 -e $3"
}

ptmp-sock-desc () {
    echo "${1},${2},${3}"
}

