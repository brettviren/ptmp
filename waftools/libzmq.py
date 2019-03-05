import generic

def options(opt):
    generic._options(opt, 'libzmq')

def configure(cfg):
    generic._configure(cfg, 'libzmq', incs=('zmq.h',), libs=('zmq',),
                       mandatory=True)
