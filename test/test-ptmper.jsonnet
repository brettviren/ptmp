local input = std.extVar("input");

local input_iota = std.range(0, std.length(input)-1);

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

local test_inout = function(filename, port, sitm = ptmp.sitm.tcp) {
    local bname = basename(filename),    
    local rd = ptmp.czmqat("rd", number = nmsgs, ifile = filename,
                           osocket = sitm('bind','push', port)),
    local wt = ptmp.czmqat("wt", number = nmsgs, ofile = bname + '.copy',
                           isocket = sitm('connect','pull', port)),
    name : "inout-%s"%bname,
    components: [rd, wt],
};

local test_window = function(filename, port, sitm = ptmp.sitm.tcp) {
    local bname = basename(filename),
    local rd = ptmp.czmqat("rd", number = nmsgs, ifile = filename,
                           osocket = sitm('bind','push', port)),
    local wi = ptmp.window("wi",  tspan = 50/0.02, tbuf = 5000/0.02,
                           isocket = sitm('connect','pull', port),
                           osocket = sitm('bind','push', port+1)),
    local wt = ptmp.czmqat("wt", number = nmsgs, 
                           isocket = sitm('connect','pull', port+1)),
    name : "window-%s"%bname,
    components: [rd, wi, wt],

};

local back = function(arr) arr[std.length(arr)-1];

local link_pipeline = function(filename, port, sitm = ptmp.sitm.tcp) {
    local bname = basename(filename),
    local send = ptmp.czmqat("send-"+bname, number = nmsgs, ifile = filename,
                             osocket = sitm('bind','push', port+1)),
    local replay = ptmp.replay("replay-"+bname, 
                               isocket = sitm('connect','pull', port+1),
                               osocket = sitm('bind','push', port+2)),
    local window = ptmp.window("window-"+bname, tspan = 50/0.02, tbuf = 5000/0.02,
                               isocket = sitm('connect','pull', port+2),
                               osocket = sitm('bind','push', port+3)),

    name: "pipeline-"+bname,
    components: [send, replay, window]
};

local single_files = {
    local tests = [[t,f] for t in [test_copy, test_read, test_inout, test_window] for f in input],
    local test_iota = std.range(0, std.length(tests)-1),
    local calls = [tests[ind][0](tests[ind][1], baseport + 100*ind) for ind in test_iota],
    
    ret: {["test-%s-fast.json"%t.name]:ptmp.ptmper(t.components, ttl=0, reprieve=0) for t in calls}
        +
        {["test-%s.json"%t.name]:ptmp.ptmper(t.components, ttl=1, reprieve=1) for t in calls}
}.ret;

//
// multi files
// 

local get_socket = function(actor, which="output") actor.data[which].socket;

local zip_links = function(pipelines, port, sitm = ptmp.sitm.tcp) {
    local addrs = std.flattenArrays([get_socket(back(pl.components)).bind for pl in pipelines]),
    local zipper = ptmp.zipper("zipper", 
                               ptmp.socket('connect', 'pull', addrs),
                               sitm('bind','push', port),
                               sync_time = 10,
                              ),
    components: std.flattenArrays([pl.components for pl in pipelines]) + [zipper],
}.components;



local multi_files = {
    local pipelines = [link_pipeline(input[ind], 7000 + ind*10) for ind in input_iota],

    local zipped = zip_links(pipelines, 8000),

    ret: {'test-zipped.json': ptmp.ptmper(zipped, ttl=1, reprieve=1)},
}.ret;



// output of this file
single_files + multi_files
