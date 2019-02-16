#!/usr/bin/env python
# encoding: utf-8
# Philipp Bender, 2012

from waflib.Task import Task
from waflib.TaskGen import extension 

"""
A simple tool to integrate protocol buffers into your build system.

    def configure(conf):
        conf.load('compiler_cxx cxx protoc_cxx')

    def build(bld):
    bld.program(source = "main.cpp file1.proto proto/file2.proto", 
            target = "executable") 

"""

class protoc(Task):
    run_str = '${PROTOC} ${SRC} --cpp_out=. -I..'
    color = 'BLUE'
    ext_out = ['.h', 'pb.cc']

@extension('.proto')
def process_protoc(self, node):
    cpp_node = node.change_ext('.pb.cc')
    hpp_node = node.change_ext('.pb.h')
    self.create_task('protoc', node, [cpp_node, hpp_node])
    self.source.append(cpp_node)
    self.env.append_value('INCLUDES', ['.'] )

    self.use = self.to_list(getattr(self, 'use', '')) + ['PROTOBUF']

def configure(conf):
    conf.check_cfg(package="protobuf", uselib_store="PROTOBUF", 
            args=['--cflags', '--libs'])
    conf.find_program('protoc', var='PROTOC')
