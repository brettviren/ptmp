

// jsonnet -V nmsgs=1000 --ext-src "input=["$(printf '"%s"\n' /data/fast/bviren/ptmp-dumps/2019-06-10/FELIX_BR_6*.dump | paste -sd, )"]" thisfile.jsonnet
local nmsgs = std.parseInt(std.extVar("nmsgs"));
local countdown = std.parseInt(std.extVar("countdown"));
local files = std.extVar("input");

local nfiles = std.length(files);
local file_iota = std.range(0,nfiles-1);
local cdargs = "-c %d -z 5000"%(countdown/5);


local g = import "zgraph.jsonnet";

local loaddr = 'tcp://127.0.0.1:%d';
local bvws = {
    eno1: 'tcp://130.199.23.168:%d',
    eno2: 'tcp://130.199.22.40:%d'
};
// set what's used below for upstream/downstream endpoints
local usaddr = loaddr;
local dsaddr = loaddr;

local make_nports = function(name, type, bc, addrpat, firstport) [
    g.port(name%ind, type, [[bc, addrpat%(firstport+ind)]]) for ind in file_iota
];

local fsource_ports    = make_nports('o%d',   'PUSH', 'bind',    usaddr, 6000);
local tap1_ports_in    = make_nports('t1i%d', 'PULL', 'connect', usaddr, 6000);
local tap1_ports_out   = make_nports('t1o%d', 'PUSH', 'bind',    usaddr, 6100);
local replay_ports_in  = make_nports('i%d',   'PULL', 'connect', usaddr, 6100);
local replay_ports_out = make_nports('o%d',   'PUB',  'bind',    usaddr, 6200);
local tap2_ports_in    = make_nports('t2i%d', 'SUB',  'connect', usaddr, 6200);
local tap2_ports_out   = make_nports('t2o%d', 'PUSH', 'bind',    dsaddr, 6300);
local window_ports_in  = make_nports('i%d',   'PULL', 'connect', dsaddr, 6300);
local window_ports_out = make_nports('o%d',   'PUSH', 'bind',    dsaddr, 6400);
local tap3_ports_in    = make_nports('t3i%d', 'PULL', 'connect', dsaddr, 6400);
local tap3_ports_out   = make_nports('t3o%d', 'PUSH', 'bind',    dsaddr, 6500);
local sorted_ports_in  = make_nports('i%d',   'PULL', 'connect', dsaddr, 6500);
local sorted_ports_out = make_nports('o%d',   'PUB',  'bind',    dsaddr, 6600);
local sink_ports_in    = make_nports('i%d',   'SUB',  'connect', dsaddr, 6600);

// The last two are really scalar.  We abuse make_nports() to keep the
// layer pattern visible.
local sorted_ports = sorted_ports_in + sorted_ports_out[:1];
local sink_port = sink_ports_in[0];


local fsources = [
    g.fsource("file%02d"%ind, files[ind], fsource_ports[ind], cliargs="-B 1000 -E 1000")
    for ind in file_iota
    ];

local replays = [
    g.cproxy("replay%02d"%ind, [ replay_ports_in[ind], replay_ports_out[ind] ], cliargs=cdargs)
    for ind in file_iota
    ];

local windows = [
    g.cproxy("window%02d"%ind, [ window_ports_in[ind], window_ports_out[ind] ],
             program='check_window', cliargs=cdargs + " -s 3000 -b 150000")
    for ind in file_iota
    ];

local sorted = g.csorted("sorted", sorted_ports, cliargs=cdargs);

local sink = g.csink('sink', sink_port, cliargs="-n %d"%nmsgs);


local spy = g.pyspy('tap',
                    tap1_ports_in +tap2_ports_in+ tap3_ports_in,
                    tap1_ports_out+tap2_ports_out+tap3_ports_out);

[spy] + windows + [sorted, sink] + fsources + replays
