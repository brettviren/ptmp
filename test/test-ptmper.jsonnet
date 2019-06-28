local ptmp = import "ptmp.jsonnet";

local datadir="/data/fast/bviren/ptmp-dumps/2019-06-10";
local testfile="FELIX_BR_601.dump";

local cp = ptmp.api.czmqat(ifile=datadir+"/"+testfile, ofile=testfile+".copy");

{
    "test-ptmper-copy-internal.json": ptmp.cli.ptmper([cp]),
}
