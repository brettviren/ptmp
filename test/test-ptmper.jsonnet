local input = std.extVar("input");

local baseport = 7000;

local nmsgs=100000;

local ptmp = import "ptmp.jsonnet";

local basename = function(filepath)  {
    local path=std.split(filepath, '/'),
    ret : std.split(path[std.length(path)-1], '.')[0]
}.ret;

local test_copy = function(filename, port=0) {
    local bname = basename(filename),
    local cp = ptmp.czmqat("cp", number = nmsgs, ifile = filename, ofile = bname + '.copy'),
    name: "copy-%s"%bname,
    components: [cp],
};

local test_read = function(filename, port=0) {
    local bname = basename(filename),
    local rd = ptmp.czmqat("rd", number = nmsgs, ifile = filename),
    name: "read-%s"%bname,
    components: [rd],
};

local test_inout = function(filename, port) {
    local bname = basename(filename),
    local rd = ptmp.czmqat("rd", number = nmsgs, ifile = filename,
                           osocket = ptmp.sitm('bind','push', port)),
    local wt = ptmp.czmqat("wt", number = nmsgs, ofile = bname + '.copy',
                           isocket = ptmp.sitm('connect','pull', port)),
    name : "inout-%s"%bname,
    components: [rd, wt],
};

local test_files = [[t,f] for t in [test_copy, test_read, test_inout] for f in input];

local calls = [test_files[ind][0](test_files[ind][1], baseport + 100*ind) for ind in std.range(0, std.length(test_files)-1)];


// {
//     "test-ptmper-copy.json": ptmp.ptmper([cp]),
//     "test-ptmper-read.json": ptmp.ptmper([rd]),
//     "test-ptmper-inout.json": ptmp.ptmper([rd2, wt2]),
// }

{["test-%s.json"%t.name]:ptmp.ptmper(t.components, ttl=1, reprieve=1) for t in calls}
