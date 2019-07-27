from waflib.Configure import conf

def options(opt):
    opt.load('waf_unit_test')
def configure(cfg):
    cfg.env.CXXFLAGS += ['-std=c++14']
    cfg.load('waf_unit_test')
def build(bld):
    from waflib.Tools import waf_unit_test
    bld.add_post_fun(waf_unit_test.summary)

@conf
def utesting(bld, name, use=''):
    appsrc = bld.path.ant_glob('apps/*.cc')
    checksrc = bld.path.ant_glob('test/check*.cc')
    testsrc = bld.path.ant_glob('test/test*.cc')
    test_scripts = bld.path.ant_glob('test/test*.sh') + bld.path.ant_glob('test/test*.py')

    if getattr(bld.options, "notests", False):
        print ("no tests")
        return
    if not any([checksrc, testsrc, test_scripts]):
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

    for app in appsrc:        # fixme: except for source pattern these are like checks
        bld.program(source=[app],
                    target = app.change_ext(''),
                    install_path = '${PREFIX}/bin/',
                    rpath = rpath,
                    includes = ['inc','build','test'],
                    use = use + [name])

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
