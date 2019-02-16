#!/usr/bin/env waf

import sys
from waflib.Utils import to_list

sys.path.append('waftools')

def options(opt):
    opt.load('compiler_c compiler_cxx')
    opt.load('utests')
    opt.add_option('--cxxflags', default='-O2 -ggdb3')

    
def configure(cfg):
    cfg.load('compiler_c compiler_cxx')
    cfg.load('protoc_cxx',)
    cfg.load('utests')

    # cfg.load('boost', tooldir='waftools')
    cfg.check(header_name="dlfcn.h", uselib_store='DYNAMO',
              lib=['dl'], mandatory=True)
    cfg.check_cfg(package='libzmq', uselib_store='ZMQ',
                  args=['--cflags', '--libs'], mandatory=True)
    cfg.check_cfg(package='libczmq', uselib_store='CZMQ',
                  args=['--cflags', '--libs'], use='ZMQ', mandatory=True)
    cfg.env.CXXFLAGS += ['-std=c++17']
    cfg.env.CXXFLAGS += to_list(cfg.options.cxxflags)

def build(bld):
    bld.load('utests')


    uses = ["CZMQ", "PROTOBUF", "DYNAMO"]
    rpath = [bld.env["PREFIX"] + '/lib']
    for u in uses:
        p = bld.env["LIBPATH_%s"%u]
        if p: rpath += p

    src = bld.path.ant_glob("src/*.cc")
    pbs = bld.path.ant_glob("ptmp/*.proto")
    pb_headers = list()
    for pb in pbs:
        bname = 'ptmp/' + pb.name.replace('.proto','.pb')
        pb_headers.append(bld.path.find_or_declare(bname+'.h'))

    bld.shlib(features='c cxx', includes='inc include',
              rpath = rpath,
              source=pbs+src, target='ptmp', use=uses)
    bld.install_files('${PREFIX}/include/ptmp', pb_headers)
    

    bld.utesting('ptmp', uses)
