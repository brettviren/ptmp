#!/bin/bash

mydir="$(dirname $BASH_SOURCE)"
topdir="$(dirname $mydir)"
if [ ! -f "$topdir/waf" ] ; then
    echo "This script assumes it is in the PTMP source directory under waftools/"
    exit 1
fi

if [ -z "$ZMQ_VERSION" -o -z "$CZMQ_VERSION" -o -z "$PROTOBUF_VERSION" ] ; then
    cat <<EOF
This script assumes you have your UPS environment set up for the products 'zmq', 'czmq' and 'protobuf'.

Maybe try:

source /cvmfs/fermilab.opensciencegrid.org/products/artdaq/setup
PRODUCTS=$PRODUCTS:/cvmfs/fermilab.opensciencegrid.org/products/larsoft
setup czmq v4_2_0 -q e15
setup protobuf v3_3_1a -q e15

EOF
    exit 1
fi

prefix="$1" 
if [ -z "$prefix" ] ; then
    prefix="$topdir/install"
fi
echo "installing to $prefix"


./waf configure \
      --with-libzmq-lib=$ZMQ_LIB --with-libzmq-include=$ZMQ_INC \
      --with-czmq-lib=$CZMQ_LIB --with-czmq-include=$CZMQ_INC \
      --with-protobuf=$PROTOBUF_FQ_DIR \
      --prefix=$prefix || exit 1

echo "Now, you can run './waf install'"
