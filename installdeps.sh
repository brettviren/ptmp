#!/bin/bash

set -e

insdir="$(readlink -f $1)"; shift
blddir="$(readlink -f $1)"; shift

if [ ! -d "$blddir" ] ; then
    mkdir -p "$blddir"
fi
pushd "$blddir"

PATH="$insdir/bin:$PATH"
LD_LIBRARY_PATH="$insdir/lib:$LD_LIBRARY_PATH"
PKG_CONFIG_PATH="$insdir/lib/pkgconfig:$PKG_CONFIG_PATH"

download () {
    local url="$1"; shift
    local fname="$(basename $url)"
    if [ -f "$fname" ] ; then
        echo "Already have $fname"
        return
    fi
    wget "$url"
}

install-libzmq () {
    if pkg-config libzmq ; then
        echo "have libzmq"
        return
    fi

    download https://github.com/zeromq/libzmq/releases/download/v4.3.1/zeromq-4.3.1.tar.gz
    if [ ! -d zeromq-4.3.1 ] ; then
        tar -xf zeromq-4.3.1.tar.gz
    fi
    pushd zeromq-4.3.1
    ./configure --prefix="$insdir"
    make && make install
    popd
}

install-libczmq () {
    if pkg-config libczmq ; then
        echo "have libczmq"
        return
    fi

    download https://github.com/zeromq/czmq/releases/download/v4.2.0/czmq-4.2.0.tar.gz
    if [ ! -d czmq-4.2.0 ] ; then
        tar -xf czmq-4.2.0.tar.gz
    fi
    pushd czmq-4.2.0
    ./configure --prefix="$insdir"  --with-pkgconfigdir="$PKG_CONFIG_PATH"

    make && make install
    if [ ! -d "$insdir/lib/pkgconfig" ] ; then
        mkdir -p "$insdir/lib/pkgconfig"
    fi
    # why isn't this installed?
    cp src/libczmq.pc "$insdir/lib/pkgconfig"
    popd
}

install-protobuf () {
    if pkg-config protobuf ; then
        echo "have protobuf"
        return
    fi

    # apt-get install -y uuid-dev libsystemd-dev liblz4-dev libcurl4-nss-dev libzmq5-dev libczmq-dev

    download https://github.com/protocolbuffers/protobuf/releases/download/v3.6.1/protobuf-cpp-3.6.1.tar.gz
    if [ ! -d protobuf-3.6.1 ] ; then
        tar -xf protobuf-cpp-3.6.1.tar.gz
    fi
    pushd protobuf-3.6.1
    ./configure --prefix="$insdir"
    make && make install
    popd
}
install-libzmq 
install-libczmq
install-protobuf
