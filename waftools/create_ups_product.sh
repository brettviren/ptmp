#!/bin/bash

usage()
{
    echo "Usage:" >&2
    echo "$(basename $0) installdir" >&2
}

mydir="$(dirname $BASH_SOURCE)"
topdir="$(dirname $mydir)"
if [ ! -f "$topdir/waf" ] ; then
    echo "This script assumes it is in the PTMP source directory under waftools/"
    exit 1
fi

echo mydir is $mydir
echo topdir is $topdir

MYPRODDIR="$1"

if [ -z "${MYPRODDIR}" ]; then
    usage
    exit 1
fi

PRODNAME=ptmp
VERSION=v0_0_1
QUALS=e15

PRODUCT_DEPS="${topdir}/ups/product_deps"

# Make sure UPS is set up, since we'll be using it later
if [ -z "$UPS_DIR" ]; then
    echo "It appears that ups is not set up (\$UPS_DIR is not set). Bailing"
    exit 1
fi

# Unfortunately, ups's 'setup' is a function, and it's not exported to
# subshells, so we can't use it in this script (we *could* manually
# source 'setups', but then we have to hardcode a path here, which is
# ugly). So instead we'll just define setup ourselves, the same way
# that ups does
setup ()
{
    . `ups setup "$@"`
}

# We use get-directory-name from cetpkgsupport below
setup -c cetpkgsupport
# We use build_table from cetbuildtools to make the table file from the product_deps file
setup cetbuildtools v5_14_03

# Sort the qualifiers in the same way as build_table does:
# 'prof'/'debug' at the end, and the rest in alphabetical order
function sort_quals
{
    ret=""
    optimization=""
    for i in $(echo $1 | tr ':' '\n' | sort); do
        if [[ $i = "prof" || $i = "debug" ]]; then
            optimization="$i"
        else
            ret="$ret $i"
        fi
    done
    ret="$ret $optimization"
    # Use printf not echo deliberately here: $ret gets split into
    # words, and printf prints each as "%s\n", so now there are line
    # breaks between the quals
    printf "%s\n" $ret | paste -sd:
}


# The necessary gubbins to find the directory name for the build in
# UPS, so we get something like "slf7.x86_64.e14.prof.s50"
SUBDIR=`get-directory-name subdir` || exit 1
# Sometimes we need the qualifiers colon-separated, sometimes
# dot-separated, so form both
QUALS=$(sort_quals ${QUALS})
QUALSDIR=$(echo ${QUALS} | tr ':' '.')

# Is my installation dir already in the places where UPS will look? If not, add it
if ! echo $PRODUCTS | grep -q ${MYPRODDIR}; then 
    export PRODUCTS=$PRODUCTS:${MYPRODDIR}
fi

# Make the directories that UPS wants, and do the minimum necessary to
# be able to setup the product (ie, copy across the table file). Then
# actually setup the product so that we can copy the files into the
# directories where UPS thinks they should be. This is really a bit of
# extra paranoia, since we basically form those directories ourselves,
# but it allows us to check that everything is working
VERSIONDIR="${MYPRODDIR}/${PRODNAME}/${VERSION}"
mkdir -p "${VERSIONDIR}/ups"                           || exit 1
mkdir -p "${VERSIONDIR}/include"                       || exit 1
mkdir -p "${VERSIONDIR}/${SUBDIR}.${QUALSDIR}"         || exit 1
mkdir -p "${VERSIONDIR}/${SUBDIR}.${QUALSDIR}/lib64"   || exit 1
mkdir -p "${VERSIONDIR}/${SUBDIR}.${QUALSDIR}/bin"     || exit 1
mkdir -p "${VERSIONDIR}/${SUBDIR}.${QUALSDIR}/include" || exit 1

cp ${PRODUCT_DEPS} "${VERSIONDIR}/ups/product_deps" || exit 1

# Make the table file from the product_deps file
build_table ${VERSIONDIR}/ups ${VERSIONDIR}/ups || exit 1

TABLE="${VERSIONDIR}/ups/${PRODNAME}.table"

# Not sure what the best thing to do if the product is already built
# in this configuration. This will hopefully at least cause 'make ups'
# to not fail
if ups exist ${PRODNAME} ${VERSION} -q ${QUALS}; then
    echo
    echo Product already exists with this version. I will undeclare it and re-declare
    echo You may want to manually remove ${VERSIONDIR}'/*' or even
    echo ${VERSIONDIR} and rerun 'make ups'
    echo 
    ups undeclare -z ${MYPRODDIR} -r ${VERSIONDIR} -5 -m ${TABLE} -q ${QUALS} ${PRODNAME} ${VERSION}
fi


echo Declaring with:
echo ups declare -z ${MYPRODDIR} -r ${VERSIONDIR} -5 -m ${TABLE} -q ${QUALS} ${PRODNAME} ${VERSION}
     ups declare -z ${MYPRODDIR} -r ${VERSIONDIR} -5 -m ${TABLE} -q ${QUALS} ${PRODNAME} ${VERSION}

if [ "$?" != "0" ]; then
    echo ups declare failed. Bailing
    exit 1
fi

echo Now ups list says the available builds are:
ups list -aK+ ${PRODNAME}

setup ${PRODNAME} ${VERSION} -q ${QUALS}

if [ "$?" != "0" ]; then
    echo setup failed. Bailing
    exit 1
else
    echo Setting up the new product succeeded
fi

PRODNAME_UC=$(echo $PRODNAME | tr '[:lower:]' '[:upper:]')
PRODNAME_UC_INC="${PRODNAME_UC}_INC"
PRODNAME_UC_LIB="${PRODNAME_UC}_LIB"
PRODNAME_UC_FQ_DIR="${PRODNAME_UC}_FQ_DIR"

if [ ! -d "${!PRODNAME_UC_INC}" ]; then
    echo $PRODNAME_UC_INC is not defined or does not exist. I don\'t know what to do
    exit 1
fi

if [ ! -d "${!PRODNAME_UC_LIB}" ]; then
    echo $PRODNAME_UC_LIB is not defined or does not exist. I don\'t know what to do
    exit 1
fi

if [ ! -d "${!PRODNAME_UC_FQ_DIR}" ]; then
    echo $PRODNAME_UC_FQ_DIR is not defined or does not exist. I don\'t know what to do
    exit 1
fi

${topdir}/waftools/waf-configure-for-ups.sh ${!PRODNAME_UC_FQ_DIR} || exit 1

${topdir}/waf install
