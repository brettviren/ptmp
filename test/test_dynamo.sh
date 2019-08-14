#!/bin/bash

tstdir=$(dirname $(realpath $BASH_SOURCE))
topdir=$(dirname $tstdir)
blddir=$topdir/build
dyndir=$blddir/test/dynamo

# set -x

rm -rf $topdir/build/liba.so

LD_LIBRARY_PATH=$blddir $dyndir/testa-nolink && exit 1
echo "expect assertion in the above"

LD_LIBRARY_PATH=$blddir:$dyndir $dyndir/testa-nolink || exit 1

LD_LIBRARY_PATH=$blddir:$dyndir $dyndir/testa-linka || exit 1
LD_LIBRARY_PATH=$blddir:$dyndir $dyndir/testa-linkb || exit 1

cp $dyndir/libb.so $blddir/liba.so

LD_LIBRARY_PATH=$blddir $dyndir/testa-nolink && exit 1
echo "expect assertion in the above"

rm -rf $topdir/build/liba.so
echo "okay"
