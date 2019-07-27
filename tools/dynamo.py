def options(opt):
    pass

def configure(cfg):
    cfg.check(header_name="dlfcn.h", uselib_store='DYNAMO',
              lib=['dl'], mandatory=True)
