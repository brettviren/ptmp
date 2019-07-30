#!/bin/bash

ptmp_test_dir="$(dirname $(realpath "$BASH_SOURCE"))"
source "$ptmp_test_dir/ptmp-testlib.sh"

check-tpzipper-lateness
