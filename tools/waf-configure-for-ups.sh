#!/bin/bash

if [ ! -f wscript ] ; then
    echo "This script must be run from the top level source directory"
    exit -1
fi

if [ -z "$ZEROMQ_VERSION" -o -z "$CZMQ_VERSION" -o -z "$PROTOBUF_VERSION" ] ; then
    cat <<EOF
This script assumes you have your UPS environment set up for the products 'zeromq', 'czmq' and 'protobuf'.

Maybe try:

source /cvmfs/fermilab.opensciencegrid.org/products/artdaq/setup
PRODUCTS=$PRODUCTS:/cvmfs/fermilab.opensciencegrid.org/products/larsoft
setup czmq v4_2_0 -q e15
setup protobuf v3_3_1a -q e15

IF the ZeroMQ product names have been fixed, please update this script.

EOF
    exit 1
fi

prefix="$1" 
if [ -z "$prefix" ] ; then
    prefix="$(pwd)/install"
fi
echo "installing to $prefix"


./tools/waf configure \
      --with-libzmq-lib=$ZEROMQ_LIB --with-libzmq-include=$ZEROMQ_INC \
      --with-libczmq-lib=$CZMQ_LIB --with-libczmq-include=$CZMQ_INC \
      --with-protobuf=$PROTOBUF_FQ_DIR \
      --prefix=$prefix || exit 1


echo "Now maybe run './tools/waf --notests install', etc"
