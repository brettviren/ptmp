local ptmp = import "ptmp.jsonnet";


local nmsgs=500000;
local datadir="/data/fast/bviren/ptmp-dumps/2019-06-10/";
local dataset="FELIX_BR_601";

local infile=datadir+dataset+".dump";
local outfile=dataset+".copy";

local cp = ptmp.czmqat("cp", number = nmsgs, ifile = infile, ofile = outfile);
local rd = ptmp.czmqat("rd", number = nmsgs, ifile = infile);

local rd2 = ptmp.czmqat("rd", number = nmsgs, ifile = infile,
                        osocket = ptmp.sitm('bind','push',5678));
local wt2 = ptmp.czmqat("wt", number = nmsgs, ofile = outfile,
                        isocket = ptmp.sitm('connect','pull',5678));
{
    "test-ptmper-copy.json": ptmp.ptmper([cp]),
    "test-ptmper-read.json": ptmp.ptmper([rd]),
    "test-ptmper-inout.json": ptmp.ptmper([rd2, wt2]),
}
