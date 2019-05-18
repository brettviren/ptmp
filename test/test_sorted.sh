#!/bin/bash

echo $BASH_SOURCE
myname=$(basename $BASH_SOURCE .sh)

top="$(dirname $(dirname $BASH_SOURCE))"
sender="$top/build/test/check_send_rates"
sorter="$top/build/test/check_sorted"
recver="$top/build/test/check_recv"

# don't colide with other tests, they might run simultaneously!
baseport=6660

hwm=10
tardy_ms=100
outsock="PUSH"
insock="PULL"

# delay between sender packets in microseconds
delay=1

# 25 is long enough to handle 10k * 10 messages on my laptop
countdown=25
endwait=10000
nsend=10000
maxsender=10
ntotal=$(nproc)
if (( $ntotal > $maxsender )) ; then
    ntotal=$maxsender
fi
senders=""
maxsender=0
while [[ $maxsender -lt $ntotal ]] ; do
    senders="$senders $maxsender"
    maxsender=$(( $maxsender + 1 ))
done
nexpect=$(( $ntotal * $nsend))
echo "ntotal=$ntotal nexpect=$nexpect"
echo "senders: $senders"

set -e
#set -x

err_report() {
    echo "Error on line $1"
    exit -1
}

trap 'err_report $LINENO' ERR

endpoint () {
    local trans=$1 ; shift
    local num=$1 ; shift 

    case $trans in
        ipc) echo "ipc://${myname}${num}.ipc" ;;
        tcp) echo "tcp://127.0.0.1:$(( $baseport + $num ))" ;;
        inproc) echo "inproc://${myname}$num" ;;
        *) exit -1;;
    esac
}

logfile () {
    local name="$1" ; shift
    local trans=$1 ; shift
    local num="$1" ; shift

    echo "$top/build/test/log.${myname}.${name}.${trans}.${num}"
}


cmd_sender () {
    local trans=$1 ; shift
    local num=$1 ; shift
    local ep="$(endpoint $trans $num)"
    local logf="$(logfile sender $trans $num)"
    
    cmd="$sender -E $endwait -d $num -m $hwm -p $outsock -a bind -e $ep -B 1000  -n $nsend -N 1000 uniform -t $delay"
    echo $cmd
    $cmd 2>&1 > $logf
}

cmd_sorter () {
    local trans=$1 ; shift
    local outep="$1" ; shift
    local logf="$(logfile sorter $trans 0)"
    local ep=""
    for n in $@
    do
        ep="$ep -e $(endpoint $trans $n)"
    done
    cmd="$sorter -t $tardy_ms -c $countdown output -m $hwm -p $outsock -a bind -e $outep input -m 1000 -p $insock -a connect  $ep"
    echo $cmd
    $cmd 2>&1 > $logf
    
}

cmd_recver () {
    local trans=$1 ; shift
    local ep="$1" ; shift
    local logf="$(logfile recver $trans 0)"
    cmd="$recver -m $hwm -p $insock -a connect -e $ep -T 1000 -n $nexpect"
    echo $cmd
    $cmd 2>&1 > $logf
}

## obviously can't do inproc between procs
for trans in tcp # ipc tcp
do
    proxep="$(endpoint $trans 100)"
    cmd_sorter $trans $proxep $senders &
    for n in $senders
    do
        cmd_sender $trans $n &
    done
    cmd_recver $trans $proxep || exit -1
    echo "success: $trans"

done
sleep 1

