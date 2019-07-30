#!/bin/bash

ptmp_test_dir="$(dirname $(realpath "$BASH_SOURCE"))"
source "$ptmp_test_dir/ptmp-testlib.sh"

filename=$(ptmp_get_dump_file 5 6)
if [ -z "$filename" ] ; then
    exit 1
fi
echo $filename
set -x
check-tpwindow-dup -n 1000000 $filename

