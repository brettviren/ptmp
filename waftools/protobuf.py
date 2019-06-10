#!/usr/bin/env python
# encoding: utf-8
# Philipp Bender, 2012

from waflib.Task import Task
from waflib.TaskGen import extension 

import generic

"""
A simple tool to integrate protocol buffers into your build system.

    def configure(conf):
        conf.load('compiler_cxx cxx protoc_cxx')

    def build(bld):
    bld.program(source = "main.cpp file1.proto proto/file2.proto", 
            target = "executable") 

"""

class protoc(Task):
    run_str = '${PROTOC} ${SRC} --cpp_out=. -I.. && cp ${TGT[1].abspath()} ../inc/ptmp/'
    color = 'BLUE'
    ext_out = ['.h', 'pb.cc']

class protopy(Task):
    run_str = '${PROTOC} ${SRC} --python_out=. -I.. && cp ${TGT[0].abspath()} ../python/ptmp/'
    color = 'BLUE'
    ext_out = ['pb2.py']
    


@extension('.proto')
def process_protoc(self, node):
    py_node = node.change_ext('_pb2.py')
    cpp_node = node.change_ext('.pb.cc')
    hpp_node = node.change_ext('.pb.h')
    self.create_task('protopy', node, [py_node])
    self.create_task('protoc', node, [cpp_node, hpp_node])
    self.source.append(cpp_node)
    self.env.append_value('INCLUDES', ['.'] )
    self.use = self.to_list(getattr(self, 'use', '')) + ['PROTOBUF']

    


def options(opt):
    generic._options(opt, 'protobuf')

def configure(cfg):
    generic._configure(cfg, 'protobuf',
                       incs=('google/protobuf/message.h',), libs=('protobuf',),
                       mandatory=True)
    cfg.find_program('protoc', var='PROTOC')
