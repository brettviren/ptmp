local input = std.extVar("input");
local outdir = std.extVar("outdir");
local ptmp = import "ptmp.jsonnet";

{
    // "just-files.json" :
    // ptmp.ptmper(ptmp.just_files(input, outdir + "/just-files.mon", nmsgs=200000),
    //             ttl=30, reprieve=1),
    // "just-replay.json" :
    // ptmp.ptmper(ptmp.just_replay(input, outdir + "/just-replay.mon", nmsgs=200000),
    //             ttl=30, reprieve=1),
    // "just-window.json" :
    // ptmp.ptmper(ptmp.just_window(input, outdir + "/just-window.mon", nmsgs=200000),
    //             ttl=30, reprieve=1),
    "full-chain.json" :
    ptmp.ptmper(ptmp.files_zipped_monitored(input, outdir + "/full-chain.mon", nmsgs=200000),
                ttl=30, reprieve=1),
}

