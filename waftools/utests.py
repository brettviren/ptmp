from waflib.Configure import conf

def options(opt):
    opt.load('waf_unit_test')
def configure(cfg):
    cfg.load('waf_unit_test')
def build(bld):
    from waflib.Tools import waf_unit_test
    bld.add_post_fun(waf_unit_test.summary)

@conf
def utesting(bld, name, use=''):
    checksrc = bld.path.ant_glob('test/check_*.cc')
    testsrc = bld.path.ant_glob('test/test_*.cc')
    test_scripts = bld.path.ant_glob('test/test_*.sh') + bld.path.ant_glob('test/test_*.py')

    if getattr(bld.options, "notests", False):
        print ("no tests")
        return
    if not any([testsrc, test_scripts]):
        return

    def get_rpath(uselst):
        ret = set([bld.env["PREFIX"]+"/lib"])
        for one in uselst:
            libpath = bld.env["LIBPATH_"+one]
            for l in libpath:
                ret.add(l)

        ret = list(ret)
        ret.insert(0, bld.path.find_or_declare(bld.out_dir).abspath())
        return ret
    rpath = get_rpath(use + [name])

    for chk in checksrc:        # like tests but don't run
        bld.program(source=[chk],
                    target = chk.change_ext(''),
                    install_path = None,
                    rpath = rpath,
                    includes = ['inc','build','test'],
                    use = use + [name])


    for test_main in testsrc:
        #print 'Building %s test: %s using %s' % (name, test_main, use)
        #print rpath
        bld.program(features = 'test', 
                    source = [test_main], 
                    ut_cwd   = bld.path, 
                    target = test_main.name.replace('.cc',''),
                    install_path = None,
                    rpath = rpath,
                    includes = ['inc','build','test'],
                    use = use + [name])

    bld.add_group()

    for test_script in test_scripts:
        interp = "${BASH}"
        if test_script.abspath().endswith(".py"):
            interp = "${PYTHON}"
        #print 'Building %s test %s script: %s using %s' % (name, interp, test_script, use)
        bld(features="test_scripts",
            ut_cwd   = bld.path, 
            test_scripts_source = test_script,
            test_scripts_template = "pwd && " + interp + " ${SCRIPT}")
