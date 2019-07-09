local input = std.extVar("input");
local outdir = std.extVar("outdir");
local ptmp = import "ptmp.jsonnet";

// omnibus config for components.  Not all attributes are used by all
// functions.
local ccfg = ptmp.default_topo_config {
    nmsgs: 400000,
    tspan: 50/0.02,
    tbuf: 5000/0.02,
    rewrite_count: 0,
    socker: ptmp.sitm.inproc,
    port_base: 7000,
    sync_time: 10,
    mon_filename: outdir + "junk.mon",
    filenames: input,
    tap_attach: "pubsub",
};

local jcfg = {
    ttl: 60,
    reprieve: 1,
};



{
    [name+".json"] :  ptmp.ptmper(ptmp[std.strReplace(name, '-','_')](ccfg {
        mon_filename: outdir+"/"+name+".mon"
    }), ttl=jcfg.ttl, reprieve=jcfg.reprieve)
    for name in [
        //"just-files",
        // "just-replay",
        // "just-window",
        "just-zipper",
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


