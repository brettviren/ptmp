import generic

def options(opt):
    generic._options(opt, 'czmq')

def configure(cfg):
    generic._configure(cfg, 'czmq', incs=('czmq.h',), libs=('czmq',),
                       uses = 'LIBZMQ', mandatory=True)
