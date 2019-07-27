import generic

name = __name__

def options(opt):
    generic._options(opt, name)

def configure(cfg):
    generic._configure(cfg, name, incs=('zmq.h',), libs=('zmq',),
                       mandatory=True)
