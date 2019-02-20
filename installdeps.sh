#!/bin/bash


insdir="$(readlink -f $1)"; shift
blddir="$(readlink -f $1)"; shift
options="$1" ; shift

set -e

libzmq_version="4.3.1"
libzmq_repo="libzmq"
czmq_version="4.2.0"

protobuf_version="3.6.1"
protobuf_url="https://github.com/protocolbuffers/protobuf/releases/download/v${protobuf_version}/protobuf-cpp-${protobuf_version}.tar.gz"

if [[ "$options" =~ .*legacy.* ]] ; then
    libzmq_version="3.2.5"
    libzmq_repo="zeromq3-x"
    czmq_version="3.0.2"
    protobuf_version="3.3.1"
    protobuf_url="https://github.com/protocolbuffers/protobuf/archive/v3.3.1.tar.gz"
fi

force () {
    if [[ "$options" =~ .*force.* ]] ; then
        echo yes
    fi
}

if [ ! -d "$blddir" ] ; then
    mkdir -p "$blddir"
fi
pushd "$blddir"

PATH="$insdir/bin:$PATH"
export LD_LIBRARY_PATH="$insdir/lib:$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH="$insdir/lib/pkgconfig:$PKG_CONFIG_PATH"

download () {
    local url="$1"; shift
    local fname="$(basename $url)"
    if [ -f "$fname" -a -z "$(force)" ] ; then
        echo "Already have $fname"
        return
    fi
    wget "$url"
}

install-libzmq () {
    if pkg-config libzmq ; then
        if [ -z "$(force)" ] ; then
           echo "have libzmq"
           return
        fi
    fi
    # https://github.com/zeromq/zeromq3-x/releases/download/v3.2.5/zeromq-3.2.5.tar.gz
    local name="zeromq-${libzmq_version}"
    local fname="${name}.tar.gz"
    local url="https://github.com/zeromq/${libzmq_repo}/releases/download/v${libzmq_version}/${fname}"

    download "$url"
    if [ ! -d "$name" -o -n "$(force)" ] ; then
        tar -xf "$fname"
    fi
    pushd "$name"
    ./configure --prefix="$insdir"
    make && make install
    popd
}

install-libczmq () {
    if pkg-config libczmq ; then
        if [ -z "$(force)" ] ; then
            echo "have libczmq"
            return
        fi
    fi

    local name="czmq-${czmq_version}"
    local fname="${name}.tar.gz"
    local url="https://github.com/zeromq/czmq/releases/download/v${czmq_version}/${fname}"
    download "$url"
    if [ ! -d "$name" -o -n "$(force)" ] ; then
        tar -xf "$fname"
    fi
    pushd "$name"
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
        if [ -z "$(force)" ] ; then
            echo "have protobuf"
            return
        fi
    fi

    # apt-get install -y uuid-dev libsystemd-dev liblz4-dev libcurl4-nss-dev libzmq5-dev libczmq-dev

    local name="protobuf-${protobuf_version}"
    local fname="$(basename $protobuf_url)"

    download "$protobuf_url"
    if [ ! -d "$name" -o -n "$(force)" ] ; then
        tar -xf "$fname"
    fi
    pushd "$name"
    if [[ "$options" =~ .*legacy.* ]] ; then
        ./autogen.sh
    fi
    ./configure --prefix="$insdir"
    make && make install
    popd
}
install-libzmq 
install-libczmq
install-protobuf
