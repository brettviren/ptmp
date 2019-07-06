local input = std.extVar("input");
local outdir = std.extVar("outdir");
local ptmp = import "ptmp.jsonnet";

{
    "full-chain.json" :
    ptmp.ptmper(ptmp.files_zipped_monitored(input, outdir + "/full-chain.mon", nmsgs=200000),
                ttl=30, reprieve=1),
    // "just-files.json" :
    // ptmp.ptmper(ptmp.just_files(input, outdir + "/just-files.mon", nmsgs=200000),
    //             ttl=30, reprieve=1),
}

