#!/usr/bin/env python
'''
This is NOT a waf tool but generic functions to be called from a waf
tool, in particular by wcb.py.

There's probably a wafier way to do this.

The interpretation of options are very specific so don't change them
unless you really know all the use cases.  The rules are:

If package is optional:

    - omitting all --with-NAME* options will omit use the package

    - explicitly using --with-NAME=false (or "no" or "off") will omit
      use of the package.

If package is mandatory:

    - omitting all --with-NAME* options will use pkg-config to find
      the package.

    - explicitly using --with-NAME=false (or "no" or "off") will
      assert.

In either case:

    - explicitly using --with-NAME=true (or "yes" or "on") will use
      pkg-config to find the package.

    - using --with-NAME* with a path will attempt to locate the
      package without using pkg-config

Note, actually, pgk-config fails often to do its job.  Best to always
use explicit --with-NAME.
'''

from waflib.Utils import to_list

import os.path as osp
def _options(opt, name):
    lower = name.lower()
    opt = opt.add_option_group('%s Options' % name)
    opt.add_option('--with-%s'%lower, type='string', default=None,
                   help="give %s installation location" % name)
    opt.add_option('--with-%s-include'%lower, type='string', 
                   help="give %s include installation location"%name)
    opt.add_option('--with-%s-lib'%lower, type='string', 
                   help="give %s lib installation location"%name)
    return

def _configure(ctx, name, incs=(), libs=(), pcname=None, mandatory=True, uses=""):
    lower = name.lower()
    UPPER = name.upper()
    if pcname is None:
        pcname = lower

    inst = getattr(ctx.options, 'with_'+lower, None)
    inc = getattr(ctx.options, 'with_%s_include'%lower, None)
    lib = getattr(ctx.options, 'with_%s_lib'%lower, None)

    if mandatory:
        if inst:
            assert (inst.lower() not in ['no','off','false'])
    else:                       # optional
        if not any([inst, inc, lib]):
            return
        if inst and inst.lower() in ['no','off','false']:
            return


    # rely on package config
    if not any([inst,inc,lib]) or (inst and inst.lower() in ['yes','on','true']):
        ctx.start_msg('Checking for %s in PKG_CONFIG_PATH' % name)
        args = "--cflags"
        if libs:                # things like eigen may not have libs
            args += " --libs"
        ctx.check_cfg(package=pcname,  uselib_store=UPPER,
                      args=args, mandatory=mandatory)
        if 'HAVE_'+UPPER in ctx.env:
            ctx.end_msg("found")
        else:
            ctx.end_msg("pkg-config failed to find %s" % pcname)
    else:                       # do manual setting

        if incs:
            if not inc and inst:
                inc = osp.join(inst, 'include')
            if inc:
                setattr(ctx.env, 'INCLUDES_'+UPPER, [inc])

        if libs:
            if not lib and inst:
                lib = osp.join(inst, 'lib')
            if lib:
                setattr(ctx.env, 'LIBPATH_'+UPPER, [lib])

    
    # now check, this does some extra work in the caseof pkg-config

    uses = [UPPER] + to_list(uses)

    if incs:
        #print (ctx.env)
        ctx.start_msg("Location for %s headers" % name)
        for tryh in incs:
            ctx.check_cxx(header_name=tryh,
                          use=uses, uselib_store=UPPER, mandatory=mandatory)
        ctx.end_msg(str(getattr(ctx.env, 'INCLUDES_' + UPPER, None)))

    if libs:
        ctx.start_msg("Location for %s libs" % name)
        for tryl in libs:
            ctx.check_cxx(lib=tryl,
                          use=uses, uselib_store=UPPER, mandatory=mandatory)
        ctx.end_msg(str(getattr(ctx.env, 'LIBPATH_' + UPPER, None)))

        ctx.start_msg("Libs for %s" % name)
        ctx.end_msg(str(getattr(ctx.env, 'LIB_' + UPPER)))
