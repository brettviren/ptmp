local input = std.extVar("input");
local outdir = std.extVar("outdir");
local ptmp = import "ptmp.jsonnet";

// omnibus config for components.  Not all attributes are used by all
// functions.
local cfg = ptmp.topo_defaults {
    filenames: input,
};

local jcfg = {
    ttl: 30,
    reprieve: 1,
};



{
    ["monitor-%s.json"%name] :
    ptmp.ptmper(
        ptmp.monitor(
            ptmp.topo_context(cfg {mon_filename:outdir+'/monitor-%s.mon'%name}))
            .files[std.strReplace(name, '-','_')],
        ttl=jcfg.ttl, reprieve=jcfg.reprieve)
    for name in [
        "raw", "replay",
        "window", "zipper",
    ]
}


    // "just-files.json" :
    // ptmp.ptmper(ptmp.just_files(ccfg { mon_filename: outdir+"/just-files.mon"}),
    //             ttl=jcfg.ttl, reprieve=jcfg.reprieve),

    // "just-replay.json" :
    // ptmp.ptmper(ptmp.just_replay(input, outdir + "/just-replay.mon",  cfg=ccfg),
    //             ttl=jcfg.ttl, reprieve=jcfg.reprieve),

    // "just-window.json" :
    // ptmp.ptmper(ptmp.just_window(input, outdir + "/just-window.mon", cfg=ccfg),
    //             ttl=jcfg.ttl, reprieve=jcfg.reprieve),

    // "just-zipper.json" :
    // ptmp.ptmper(ptmp.just_zipper(input, outdir + "/just-zipper.mon", nmsgs=200000, socker = ptmp.sitm.inproc),
    //             ttl=30, reprieve=1),

    // "full-chain.json" :
    // ptmp.ptmper(ptmp.files_zipped_monitored(input, outdir + "/full-chain.mon", nmsgs=200000),
    //             ttl=30, reprieve=1),


