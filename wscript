#!/usr/bin/env waf

import os
import sys
from waflib.Utils import to_list

sys.path.append('tools')


# list packages found by tools/*
pkg_deps = ['libzmq','libczmq','protobuf','dynamo']

# version guidance from:
# https://funcptr.net/2014/06/10/build-numbers-in-binaries-using-waf/
APPNAME = "ptmp"
VERSION = "0.0.0"

def options(opt):
    opt.load('compiler_c compiler_cxx')
    opt.load('utests')
    for pkg in pkg_deps:
        opt.load(pkg)
    opt.add_option('--cxxflags', default='-O2 -ggdb3')

def set_version(cfg):
    version = ""
    try:
        gitcmd='git describe --always --dirty --long'
        version = os.popen(gitcmd).read().strip()
    except Exception as e:
        print (e)
        
    if version:
        VERSION = version
    cfg.env.VERSION = VERSION
    cfg.env.APPNAME = APPNAME
    print ("using version: \"%s\"" % cfg.env.VERSION)

    
def configure(cfg):
    set_version(cfg)


    cfg.load('compiler_c compiler_cxx')
    cfg.load('utests')

    for pkg in pkg_deps:
        cfg.load(pkg)

    # we find pthread explictly, it has no tool yet.
    cfg.check_cxx(header_name="pthread.h",
                  lib=['pthread'], uselib_store='PTHREAD')

    #print (cfg.env)
    #cfg.env.LDFLAGS += ['-pg']
    #cfg.env.CXXFLAGS += ['-pg']
    cfg.env.CXXFLAGS += to_list(cfg.options.cxxflags)


def build(bld):
    bld.load('utests')
    bld(features='subst',
        source='inc/ptmp/version.h.in',
        target='inc/ptmp/version.h',
        VERSION = bld.env['VERSION'],
    )

    uses = [p.upper() for p in pkg_deps + ["pthread"]]
    rpath = [bld.env["PREFIX"] + '/lib']
    for u in uses:
        p = bld.env["LIBPATH_%s"%u]
        if p: rpath += p

    src = bld.path.ant_glob("src/*.cc")
    pbs = bld.path.ant_glob("src/ptmp.proto")
    pb_headers = list()
    for pb in pbs:
        bname = 'src/' + pb.name.replace('.proto','.pb')
        pb_headers.append(bld.path.find_or_declare(bname+'.h'))

    bld.shlib(features='c cxx', includes='inc include',
              rpath = rpath,
              source=pbs+src, target='ptmp', use=uses)
    bld.install_files('${PREFIX}/include', 'inc/CLI11.hpp inc/json.hpp')
    bld.install_files('${PREFIX}/include/ptmp', pb_headers)
    bld.install_files('${PREFIX}/include/ptmp', bld.path.ant_glob("inc/ptmp/*.h"))


    bld.utesting('ptmp', uses)
    bld.recurse("test/dynamo")
