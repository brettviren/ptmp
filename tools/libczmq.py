import generic

name = __name__

def options(opt):
    generic._options(opt, name)

def configure(cfg):
    generic._configure(cfg, name, incs=('czmq.h',), libs=('czmq',),
                       pcname = name.lower(),
                       uses = 'LIBZMQ', mandatory=True)
