#!/bin/bash

source "$(dirname $BASH_SOURCE)/ptmp-common.sh"

# To make arguments simpler, we assume upstream always binds

# fsource <file> <addr>
fsource () {
    local file="$1"; shift
    local addr="${1:-tcp://127.0.0.1:5678}"; shift
    echo "czmqat ifile -f $ifile osock $(ptmp-sock-args bind PUSH $addr)"
}

# replay PULL <addr1> PUSH <addr2> 
replay () {
    local cmd="check_replay"
    cmd="$cmd  input $(ptmp-sock-args connect $1 $2)"
    cmd="$cmd output $(ptmp-sock-args bind    $3 $4)"
    echo $cmd
}

# window PULL <addr1> PUSH <addr2> 
window () {
    local cmd="check_window"
    cmd="$cmd  input $(ptmp-sock-args connect $1 $2)"
    cmd="$cmd output $(ptmp-sock-args bind    $3 $4)"
    echo $cmd
}

# PULL <addr>
recv () {
    echo "check_recv $(ptmp-sock-args connect $1 $2)"
}

spytap-paced-window () {
    local ifile="${1:-$datadir/FELIX_BR_601.dump}"; shift


    # bind addresses for sources of data
    local addr_base="tcp://127.0.0.1:"
    local port=5678
    local addr_src=$addr_base$port

    port=$(( $port + 1))
    local addr_tap0=$addr_base$port

    port=$(( $port + 1))
    local addr_replay=$addr_base$port

    port=$(( $port + 1))
    local addr_tap1=$addr_base$port

    port=$(( $port + 1))
    local addr_window=$addr_base$port

    port=$(( $port + 1))
    local addr_tap2=$addr_base$port

    local src=$(fsource $ifile $addr_src)
    local p1=$(replay PULL $addr_tap0 PUSH $addr_replay)
    local p2=$(window PULL $addr_tap1 PUSH $addr_window)
    local sink=$(recv PULL $addr_tap2)

    local spytap="ptmp-spy tap"
    spytap="$spytap connect,PULL,$addr_src;bind,PUSH,$addr_tap0"
    spytap="$spytap connect,PULL,$addr_replay;bind,PUSH,$addr_tap1"
    spytap="$spytap connect,PULL,$addr_window;bind,PUSH,$addr_tap2"

    local logbase="log.spytap-paced-window"
    local tokill=""

    set -x
    
    $src > ${logbase}-src 2>&1 &
    tokill="$tokill $!"

    $p1 > ${logbase}-replay 2>&1 &
    tokill="$tokill $!"

    $p2 > ${logbase}-window 2>&1 &
    tokill="$tokill $!"

    $sink > ${logbase}-sink 2>&1 &
    tokill="$tokill $!"

    set +x

    $spytap
    kill -9 $tokill

}

spytap-fast () {
    local ifile="${1:-$datadir/FELIX_BR_601.dump}"; shift

    # bind addresses for sources of data
    local addr_base="tcp://127.0.0.1:"
    local port=5678
    local addr_src=$addr_base$port

    port=$(( $port + 1))
    local addr_tap0=$addr_base$port

    local src=$(fsource $ifile $addr_src)
    local sink=$(recv PULL $addr_tap0)

    local spytap="ptmp-spy tap -o spy-fast.npz"
    spytap="$spytap connect,PULL,$addr_src;bind,PUSH,$addr_tap0"

    local logbase="log.spytap-fast"
    local tokill=""

    set -x
    
    $src > ${logbase}-src 2>&1 &
    tokill="$tokill $!"

    $sink > ${logbase}-sink 2>&1 &
    tokill="$tokill $!"

    set +x

    $spytap
    kill -9 $tokill

}


cmd=$1 ; shift
spytap-$cmd $@
