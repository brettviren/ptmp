#!/bin/bash

usage()
{
    echo "Usage:" >&2
    echo "$(basename $0) <ups-products-dir>" >&2
}


mydir="$(dirname $(realpath $BASH_SOURCE))"
topdir="$(dirname $mydir)"

# assure UPS context
DEV_PRODUCTS="$(realpath $1)" ; shift
if [ -z "${DEV_PRODUCTS}" ]; then
    usage
    exit 1
fi
if [ -z "$UPS_DIR" -o -z "$PRODUCTS" ]; then
    echo "It appears that ups is not set up (\$UPS_DIR and/or \$PRODUCTS are not set). Bailing"
    exit 1
fi
QUALS=${1:-e15}
PRODNAME="$(basename $topdir)"
VERSION="v$(git describe --exact-match --tags $(git log -n1 --pretty='%h') | tr '.' '_')"
VERSIONDIR="${DEV_PRODUCTS}/${PRODNAME}/${VERSION}"
echo "Building into to: $VERSIONDIR"
# attempt to create what is not there
if [ ! -d "${DEV_PRODUCTS}" ] ; then
    found=""
    mkdir -p "${DEV_PRODUCTS}" || exit 1
    for maybe in $(echo ${PRODUCTS//:/' '})
    do
        echo $maybe
        if [ -d "$maybe/.upsfiles" ] ; then
            echo "Bootstrapping dev UPS products dir from $maybe"
            cp -a ${maybe}/.ups* ${DEV_PRODUCTS}
            found="$maybe"
            break
        fi
    done
    if [ -z "$found" ] ; then
        echo "Failed to bootstrap $DEV_PRODUCTS"
        exit 1
    fi
fi
# Get PRODUCTS right
PRODUCTS=$(echo -n $PRODUCTS| tr ':' '\n' | grep -v "^${DEV_PRODUCTS}\$"|tr '\n' ':'| sed 's/:$//')
PRODUCTS=$DEV_PRODUCTS:$PRODUCTS


gen_product_deps () {
    # fixme: let dependencies be not hard coded somehow
    cat <<EOF
parent $PRODNAME $VERSION

defaultqual $QUALS

fcldir -
bindir fq_dir
incdir fq_dir
libdir fq_dir lib64

product	    version
zeromq	    v4_3_1
czmq	    v4_2_0
protobuf    v3_5_2
end_product_list

qualifier	zeromq  czmq    protobuf
e15             e15	e15     e15     
end_qualifier_list

EOF
}


# assure WAF context for building
WAF=""
for maybe in "$topdir/waf" "$topdir/waftools/waf"
do
    if [ -x $maybe ] ; then
        WAF=$maybe
        break
    fi
done
if [ -z "$WAF" ] ; then
    echo "Failed to find waf, script must be in subdir of source, not in $mydir"
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



if [ -d "${VERSIONDIR}/${SUBDIR}.${QUALSDIR}" ] ; then
    echo "This product already exists, not overwritting."
    echo "Remove remove and rerun if desired."
    echo "${VERSIONDIR}/${SUBDIR}.${QUALSDIR}"
    exit 1
fi
   

# Make the directories that UPS wants, and do the minimum necessary to
# be able to setup the product (ie, copy across the table file). Then
# actually setup the product so that we can copy the files into the
# directories where UPS thinks they should be. This is really a bit of
# extra paranoia, since we basically form those directories ourselves,
# but it allows us to check that everything is working
mkdir -p "${VERSIONDIR}/ups"                           || exit 1
mkdir -p "${VERSIONDIR}/include"                       || exit 1
mkdir -p "${VERSIONDIR}/${SUBDIR}.${QUALSDIR}/lib64"   || exit 1
mkdir -p "${VERSIONDIR}/${SUBDIR}.${QUALSDIR}/bin"     || exit 1
mkdir -p "${VERSIONDIR}/${SUBDIR}.${QUALSDIR}/include" || exit 1

gen_product_deps > "${VERSIONDIR}/ups/product_deps" || exit 1

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
    ups undeclare -z ${DEV_PRODUCTS} -r ${VERSIONDIR} -5 -m ${TABLE} -q ${QUALS} ${PRODNAME} ${VERSION}
fi


echo Declaring with:
echo ups declare -z ${DEV_PRODUCTS} -r ${VERSIONDIR} -5 -m ${TABLE} -q ${QUALS} ${PRODNAME} ${VERSION}
     ups declare -z ${DEV_PRODUCTS} -r ${VERSIONDIR} -5 -m ${TABLE} -q ${QUALS} ${PRODNAME} ${VERSION}

if [ "$?" != "0" ]; then
    echo ups declare failed. Bailing
    exit 1
fi

echo "Now ups list says the available builds are:"
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



${topdir}/upstools/waf-configure-for-ups.sh ${!PRODNAME_UC_FQ_DIR} || exit 1

${topdir}/waf --notests clean install || exit 1

#    ${topdir}/waf -j1 --alltests || exit 1


