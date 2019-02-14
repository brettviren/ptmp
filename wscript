#!/usr/bin/env waf

from waflib.Utils import to_list

def options(opt):
    opt.load('compiler_c compiler_cxx')
    #opt.load('boost', tooldir='waftools')
    opt.add_option('--cxxflags', default='-O2 -ggdb3')
    
def configure(cfg):
    cfg.load('compiler_c compiler_cxx')
    # cfg.load('protoc_cxx','waftools')
    # cfg.load('boost', tooldir='waftools')
    # cfg.check(header_name="dlfcn.h", uselib_store='DYNAMO',
    #           lib=['dl'], mandatory=True)
    cfg.check_cfg(package='libzmq', uselib_store='ZMQ',
                  args=['--cflags', '--libs'])
    cfg.check_cfg(package='libczmq', uselib_store='CZMQ',
                  args=['--cflags', '--libs'], use='ZMQ')
    cfg.env.CXXFLAGS += ['-std=c++17']
    cfg.env.CXXFLAGS += to_list(cfg.options.cxxflags)

def build(bld):
    pass
