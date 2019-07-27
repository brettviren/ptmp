#!/bin/bash


usage()
{
    cat <<EOF >&2
$1
Usage:
$(basename $BASH_SOURCE) <ups-products-dir> <package-ups-version> <package-ups-quals> <dep> ...

The <package-ups-quals> may be a comma separated list of multiple
qualifier sets.  First one is used as "default".

The UPS products providing the deps must be already "setup".  The
<dep>... list gives these product names.

The version can be a special string "tag" in which case the current
directory is assumed to be a git repo and the tag of the current
commit is used (converted to UPS style spelling).

Example:

  $ setup czmq v4_2_0 -q e15
  $ setup protobuf v3_5_2 -q e15
  $ ./tools/create-ups-product.sh /wcdo/lib/ups v00_10_00 e15 czmq protobuf

EOF
    exit 1
}

DEV_PRODUCTS="$(realpath $1)"; shift
VERSION="$1"; shift
quals_array=(${1//,/ }); shift
pkg_deps="$@"


# sanity checking
if [ -z "$quals_array" ] ; then
    usage "no qualifiers given"
fi
if [ -z "$UPS_DIR" -o -z "$PRODUCTS" ]; then
    usage "Must setup UPS"
fi

tooldir="$(dirname $(realpath $BASH_SOURCE))"
topdir="$(dirname $tooldir)"
pkg_name="$(basename $topdir)"
PACKAGE=$(echo $pkg_name | tr '-' '_')

if [ "$VERSION" = "tag " ] ; then
    gittag=$(git describe --exact-match --tags $(git log -n1 --pretty='%h'))
    if [ "$?" != "0" ]; then
        usage "failed to get git tag"
    fi
    VERSION="v$(echo $gittag | tr '.' '_')"
fi

VERSIONDIR="${DEV_PRODUCTS}/${PACKAGE}/${VERSION}"
echo "Building into to: $VERSIONDIR"

# attempt to create what is not there
if [ ! -d "$DEV_PRODUCTS" ] ; then
    found=""
    mkdir -p "$DEV_PRODUCTS" || exit 1
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
        usage "Failed to bootstrap $DEV_PRODUCTS"
    fi
fi
# Get PRODUCTS right
PRODUCTS=$(echo -n $PRODUCTS| tr ':' '\n' | grep -v "^${DEV_PRODUCTS}\$"|tr '\n' ':'| sed 's/:$//')
PRODUCTS=$DEV_PRODUCTS:$PRODUCTS
QUALS="${quals_array[0]}"

# zeromq_ver=v4_3_1
# czmq_ver=v4_2_0
# protobuf_ver=v3_5_2
# ptmp_ver=v0_1_0
# ptmp_deps="zeromq czmq protobuf"
# ptmp_tcs_ver=v0_0_1
# ptmp_tcs_deps="${ptmp_deps} ptmp"
# qualifiers="e15 e17"

gen_product_deps () {

    cat <<EOF
parent $PACKAGE $VERSION

defaultqual $QUALS

fcldir -
bindir fq_dir
incdir fq_dir
libdir fq_dir lib64

product	    version
EOF
    for dep in $pkg_deps
    do
        dep_ver_var="${dep^^}_VERSION"
        echo -e "$dep\t${!dep_ver_var}"
    done
    cat <<EOF
end_product_list

EOF
    echo -ne "qualifier\t"
    for dep in $pkg_deps
    do
        echo -ne "$dep\t"
    done
    echo

    for qual in $quals_array
    do
        echo -ne "$qual\t\t"
        for one in $pkg_deps
        do
            echo -ne "$qual\t"
        done
        echo
    done
    cat <<EOF
end_qualifier_list

EOF
}


WAF="$tooldir/waf"
if [ ! -x "$WAF" ] ; then
    usage "Failed to find waf at $WAF"
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
[ -z "$CETPKGSUPPORT_DIR" ] && setup -c cetpkgsupport
# We use build_table from cetbuildtools to make the table file from the product_deps file
[ -z "$CETBUILDTOOLS_DIR" ] && setup cetbuildtools v5_14_03

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
    usage "Product exits ${VERSIONDIR}/${SUBDIR}.${QUALSDIR}"
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

tablefile="${VERSIONDIR}/ups/${PACKAGE}.table"

# Not sure what the best thing to do if the product is already built
# in this configuration. This will hopefully at least cause 'make ups'
# to not fail
if ups exist ${PACKAGE} ${VERSION} -q ${QUALS}; then
    echo
    echo Product already exists with this version. I will undeclare it and re-declare
    echo You may want to manually remove ${VERSIONDIR}'/*' or even
    echo ${VERSIONDIR} and rerun 'make ups'
    echo 
    ups undeclare -z ${DEV_PRODUCTS} -r ${VERSIONDIR} -5 -m ${tablefile} -q ${QUALS} ${PACKAGE} ${VERSION}
fi


echo Declaring with:
echo ups declare -z ${DEV_PRODUCTS} -r ${VERSIONDIR} -5 -m ${tablefile} -q ${QUALS} ${PACKAGE} ${VERSION}
     ups declare -z ${DEV_PRODUCTS} -r ${VERSIONDIR} -5 -m ${tablefile} -q ${QUALS} ${PACKAGE} ${VERSION}

if [ "$?" != "0" ]; then
    echo ups declare failed. Bailing
    exit 1
fi

echo "Now ups list says the available builds are:"
ups list -aK+ ${PACKAGE}

echo "PRODUCTS=$PRODUCTS"
echo setup ${PACKAGE} ${VERSION} -q ${QUALS}
     setup ${PACKAGE} ${VERSION} -q ${QUALS}

if [ "$?" != "0" ]; then
    echo setup failed. Bailing
    exit 1
else
    echo Setting up the new product succeeded
fi

PRODNAME_UC=$(echo $PACKAGE | tr '[:lower:]' '[:upper:]')
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


extra_args=""
if [ "$pkg_name" = "ptmp-tcs" ] ; then
    extra_args="--with-ptmp-lib=$PTMP_LIB --with-ptmp-include=$PTMP_INC"
fi

set -x
$WAF configure \
      $extra_args \
      --with-libzmq-lib=$ZEROMQ_LIB --with-libzmq-include=$ZEROMQ_INC \
      --with-libczmq-lib=$CZMQ_LIB --with-libczmq-include=$CZMQ_INC \
      --with-protobuf=$PROTOBUF_FQ_DIR \
      --prefix=${!PRODNAME_UC_FQ_DIR} || usage "configuration of source failed"

$WAF --notests clean install || usage "build failed"




